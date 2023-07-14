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
#ifdef DEBUG
    printf("%s:%u sent: %s\n", client->src_ip, client->src_port, buffer);
#endif
    send(client->client_fd, buffer, strlen(buffer), 0);
}


void my_tcp_accept_handler(_client_slot *client){
    printf("%s:%u connected\n", client->src_ip, client->src_port);
}

void my_tcp_close_handler(_client_slot *client){
    printf("%s:%u away\n", client->src_ip, client->src_port);
}


int main() {
  _socket_info si;
  
  snprintf(si.bind_addr, sizeof(si.bind_addr), "%s", "0.0.0.0");
  si.bind_port = 1234;
  si.tcp_message_handler = my_tcp_message_handler;
  si.tcp_close_handler = NULL;    // &my_tcp_close_handler;
  si.tcp_accept_handler = NULL;  //&my_tcp_accept_handler;
  
  int ret = epoll_init_socket(&si);
  if (ret != 0) return ret;
  ret = epoll_event_loop(&si);
  
  return ret;
}
