#ifndef MY_AWESOME_BGP_BGP_CLIENT_H
#define MY_AWESOME_BGP_BGP_CLIENT_H

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "bgp.h"
#include "tree.h"

extern uint32_t my_as;
extern uint8_t console_mode;

class bgp_client_peer : public bgp_peer{
public:
    int sock;
    struct sockaddr_in server_address;
    uint32_t remote_as;
    uint32_t connect_cool_time;
    //bool is_shutdown;
};

bool send_open(bgp_client_peer* peer);
void close_peer(bgp_client_peer* peer);
bool bgp_client_loop(bgp_client_peer* peer);

#endif //MY_AWESOME_BGP_BGP_CLIENT_H
