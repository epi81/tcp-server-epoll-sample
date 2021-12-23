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

void my_tcp_message_handler(_client_slot *client, char *buffer, ssize_t len){
    buffer[len] = '\0';

    printf("%s:%u sent: %s", client->src_ip, client->src_port, buffer);
    send(client->client_fd, buffer, strlen(buffer), 0);
}


void my_tcp_accept_handler(_client_slot *client){
    printf("%s:%u connected\n", client->src_ip, client->src_port);
}

void my_tcp_close_handler(_client_slot *client){
    printf("%s:%u away\n", client->src_ip, client->src_port);
}


int main(void)
{
    int ret;

    _socket_info si;

    snprintf(si.bind_addr, sizeof(si.bind_addr), "%s", "0.0.0.0");
    si.bind_port = 1234;
    si.tcp_message_handler = &my_tcp_message_handler;
    si.tcp_close_handler = &my_tcp_close_handler;
    si.tcp_accept_handler = &my_tcp_accept_handler;

    ret = epoll_init_socket(&si);
    if (ret != 0)
        goto out;

    ret = epoll_event_loop(&si);

out:
    /*
     * You should write a cleaner here.
     *
     * Close all client file descriptors and release
     * some resources you may have.
     *
     * You may also want to set interrupt handler
     * before the epoll_event_loop.
     *
     * For example, if you get SIGINT or SIGTERM
     * set `state->stop` to true, so that it exits
     * gracefully.
     */
    return ret;
}
