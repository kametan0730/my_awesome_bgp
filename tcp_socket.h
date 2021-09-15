#ifndef MY_AWESOME_BGP_TCP_SOCKET_H
#define MY_AWESOME_BGP_TCP_SOCKET_H

#include <sys/socket.h>

void set_nonblocking(int sockfd);

int connect_with_timeout(int sockfd, const struct sockaddr *addr, socklen_t addrlen, unsigned int timeout_ms);

#endif //MY_AWESOME_BGP_TCP_SOCKET_H
