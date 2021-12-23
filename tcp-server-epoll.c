/*
 * @Author: epi81 <https://github.com/epi81> <epilogue81@gmail.com>
 * based on: https://stackoverflow.com/questions/66916835/c-confused-by-epoll-and-socket-fd-on-linux-systems-and-async-threads
 * by https://stackoverflow.com/users/7275114/ammar-faizi
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "tcp-server-epoll.h"

///////////////////////////////////////////////////////////////////////////////////////
#define EPOLL_MAP_TO_NOP (0u)
#define EPOLL_MAP_SHIFT  (1u) /* Shift to cover reserved value MAP_TO_NOP */

///////////////////////////////////////////////////////////////////////////////////////
static int epoll_add(int epoll_fd, int fd, uint32_t events);
static int epoll_delete(int epoll_fd, int fd);
static const char *convert_addr_ntop(struct sockaddr_in *addr, char *src_ip_buf);
static int accept_new_client(int tcp_fd, _socket_info *si);
static void handle_client_event( int client_fd, uint32_t revents, _socket_info *si );

///////////////////////////////////////////////////////////////////////////////////////
static int epoll_add(int epoll_fd, int fd, uint32_t events){
    int err;
    struct epoll_event event;

    /* Shut the valgrind up! */
    memset(&event, 0, sizeof(struct epoll_event));

    event.events  = events;
    event.data.fd = fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) < 0) {
        err = errno;
        return -1;
    }
    return 0;
}
///////////////////////////////////////////////////////////////////////////////////////
static int epoll_delete(int epoll_fd, int fd)
{
    int err;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
        err = errno;
        return -1;
    }
    return 0;
}
///////////////////////////////////////////////////////////////////////////////////////
static const char *convert_addr_ntop(struct sockaddr_in *addr, char *src_ip_buf){
    int err;
    const char *ret;
    in_addr_t saddr = addr->sin_addr.s_addr;

    ret = inet_ntop(AF_INET, &saddr, src_ip_buf, sizeof("xxx.xxx.xxx.xxx"));
    if (ret == NULL) {
        err = errno;
        err = err ? err : EINVAL;
        return NULL;
    }

    return ret;
}
///////////////////////////////////////////////////////////////////////////////////////
enum {
    ERR_EPOLL_CREATE=-1,
    ERR_SOCKET_CREATE=-2,
    ERR_SOCKET_BIND = -3,
    ERR_SOCKET_LISTEN = -4,
    ERR_EPOLL_ADD = -5,
    ERR_EPOLL_WAIT = -6,
    ERR_SOCKET_ACCEPT = -7,
    ERR_PARSE_SOUCE_ADDRESS = -8

};
///////////////////////////////////////////////////////////////////////////////////////
static int accept_new_client(int tcp_fd, _socket_info *si){
    int err=0;
    int client_fd;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    uint16_t src_port;
    const char *src_ip;
    char src_ip_buf[sizeof("xxx.xxx.xxx.xxx")];
    _tcp_state *state = &si->state;
    const size_t client_slot_num = sizeof(state->clients) / sizeof(*state->clients);


    memset(&addr, 0, sizeof(addr));
    client_fd = accept(tcp_fd, (struct sockaddr *)&addr, &addr_len);
    if (client_fd < 0) {
        if (err == EAGAIN)
            return 0;

        return ERR_SOCKET_ACCEPT;
    }

    src_port = ntohs(addr.sin_port);
    src_ip   = convert_addr_ntop(&addr, src_ip_buf);
    if (!src_ip) {
        err = ERR_PARSE_SOUCE_ADDRESS;
        goto out_close;
    }


    /*
     * Find unused client slot.
     *
     * In real world application, you don't want to iterate
     * the whole array, instead you can use stack data structure
     * to retrieve unused index in O(1).
     *
     */
    for (size_t i = 0; i < client_slot_num; i++) {
        _client_slot *client = &state->clients[i];

        if (!client->is_used) {
            /*
             * We found unused slot.
             */

            client->client_fd = client_fd;
            memcpy(client->src_ip, src_ip_buf, sizeof(src_ip_buf));
            client->src_port = src_port;
            client->is_used = true;
            client->index = i;

            /*
             * We map the client_fd to client array index that we accept
             * here.
             */
            state->client_map[client_fd] = client->index + EPOLL_MAP_SHIFT;

            /*
             * Let's tell to `epoll` to monitor this client file descriptor.
             */
            epoll_add(state->epoll_fd, client_fd, EPOLLIN | EPOLLPRI);

            if( si->tcp_accept_handler ){
                si->tcp_accept_handler(client);
            }

            return 0;
        }
    }

out_close:
    close(client_fd);
    return err;
}

///////////////////////////////////////////////////////////////////////////////////////
static void handle_client_event(int client_fd, uint32_t revents, _socket_info *si ) {
    int err;
    ssize_t recv_ret;
    char buffer[1500];
    const uint32_t err_mask = EPOLLERR | EPOLLHUP;
    /*
     * Read the mapped value to get client index.
     */
    uint32_t index = si->state.client_map[client_fd] - EPOLL_MAP_SHIFT;
    _client_slot *client = &si->state.clients[index];

    if (revents & err_mask)
        goto close_conn;

    recv_ret = recv(client_fd, buffer, sizeof(buffer), 0);
    if (recv_ret == 0)
        goto close_conn;

    if (recv_ret < 0) {
        err = errno;
        if (err == EAGAIN)
            return;

        goto close_conn;
    }


    if( si->tcp_message_handler ){
        si->tcp_message_handler(client, buffer, recv_ret);
    }
    return;


close_conn:
    if( si->tcp_close_handler ){
        si->tcp_close_handler(client);
    }
    epoll_delete(si->state.epoll_fd, client_fd);
    close(client_fd);
    client->is_used = false;
    return;
}

///////////////////////////////////////////////////////////////////////////////////////
int epoll_event_loop(_socket_info *si){
    int err;
    int ret = 0;
    int timeout = 3000; /* in milliseconds */
    int maxevents = 32;
    int epoll_ret;
    int epoll_fd = si->state.epoll_fd;
    struct epoll_event events[32];

    while (!si->state.stop) {

        /*
         * I sleep on `epoll_wait` and the kernel will wake me up
         * when event comes to my monitored file descriptors, or
         * when the timeout reached.
         */
        epoll_ret = epoll_wait(epoll_fd, events, maxevents, timeout);


        if (epoll_ret == 0) {
            /*
             *`epoll_wait` reached its timeout
             */
            continue;
        }


        if (epoll_ret == -1) {
            err = errno;
            if (err == EINTR) { // Something interrupted me
                continue;
            }

            ret = ERR_EPOLL_WAIT;
            break;
        }


        for (int i = 0; i < epoll_ret; i++) {
            int fd = events[i].data.fd;

            if (fd == si->state.tcp_fd) {
                /*
                 * A new client is connecting to us...
                 */
                if (accept_new_client(fd, si) < 0) {
                    ret = -1;
                    goto out;
                }
                continue;
            }

            handle_client_event(fd, events[i].events, si);
        }
    }

out:
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////
int epoll_init_socket(_socket_info *si){
    int ret;
    int err;
    int tcp_fd = -1;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    int epoll_fd;

    const size_t client_slot_num = sizeof(si->state.clients) / sizeof(*si->state.clients);
    const uint16_t client_map_num = sizeof(si->state.client_map) / sizeof(*si->state.client_map);

    for (size_t i = 0; i < client_slot_num; i++) {
        si->state.clients[i].is_used = false;
        si->state.clients[i].client_fd = -1;
    }

    for (uint16_t i = 0; i < client_map_num; i++) {
        si->state.client_map[i] = EPOLL_MAP_TO_NOP;
    }

    /* The epoll_create argument is ignored on modern Linux */
    epoll_fd = epoll_create(255);
    if (epoll_fd < 0) {
        err = errno;
        return ERR_EPOLL_CREATE;
    }

    si->state.epoll_fd = epoll_fd;
    si->state.stop = false;

    tcp_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
    if (tcp_fd < 0) {
        err = errno;
        return ERR_SOCKET_CREATE;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(si->bind_port);
    addr.sin_addr.s_addr = inet_addr(si->bind_addr);

    ret = bind(tcp_fd, (struct sockaddr *)&addr, addr_len);
    if (ret < 0) {
        ret = ERR_SOCKET_BIND;
        err = errno;
        goto out;
    }

    ret = listen(tcp_fd, 10);
    if (ret < 0) {
        ret = ERR_SOCKET_LISTEN;
        err = errno;
        goto out;
    }

    /*
     * Add `tcp_fd` to epoll monitoring.
     *
     * If epoll returned tcp_fd in `events` then a client is
     * trying to connect to us.
     */
    ret = epoll_add(si->state.epoll_fd, tcp_fd, EPOLLIN | EPOLLPRI);
    if (ret < 0) {
        ret = ERR_EPOLL_ADD;
        goto out;
    }

    si->state.tcp_fd = tcp_fd;
    return 0;
out:
    close(tcp_fd);
    return ret;
}
///////////////////////////////////////////////////////////////////////////////////////