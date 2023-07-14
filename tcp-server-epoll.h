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

#define CLIENTS 50000
#define MAP 60000

typedef struct {
    bool                is_used;
    int                 client_fd;
    char                src_ip[sizeof("xxx.xxx.xxx.xxx")];
    uint16_t            src_port;
    uint16_t            index;
}_client_slot;

typedef struct {
    bool                stop;
    int                 tcp_fd;
    int                 epoll_fd;
    uint16_t            client_c;
    _client_slot        clients[CLIENTS];

    /*
     * Map the file descriptor to client_slot array index
     * Note: We assume there is no file descriptor greater than CLIENTS.
     *
     * You must adjust this in production.
     */
    uint32_t            client_map[MAP];
}_tcp_state;

typedef void (*_tcp_message_handler)(_client_slot *client, char *buffer, ssize_t len);
typedef void (*_tcp_close_handler)(_client_slot *client);
typedef void (*_tcp_accept_handler)(_client_slot *client);

typedef struct{
    _tcp_state state;
    char bind_addr[40];
    uint16_t bind_port;
    _tcp_message_handler tcp_message_handler;
    _tcp_close_handler tcp_close_handler;
    _tcp_accept_handler tcp_accept_handler;
}_socket_info;

int epoll_event_loop(_socket_info *state);
int epoll_init_socket(_socket_info *state);
