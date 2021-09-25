#ifndef MY_AWESOME_BGP_BGP_CLIENT_H
#define MY_AWESOME_BGP_BGP_CLIENT_H

#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>

#include "bgp.h"

extern uint8_t console_mode;

class bgp_client_peer : public bgp_peer{
public:
    int sock = 0;
    struct sockaddr_in server_address = {};
    uint32_t remote_as = 0;
    uint32_t connect_cool_time = 0;
    bool send(void* buffer, size_t length) override;
};

void close_client_peer(bgp_client_peer* peer);
bool bgp_client_loop(bgp_client_peer* peer);

#endif //MY_AWESOME_BGP_BGP_CLIENT_H
