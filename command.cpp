#include <cstdio>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "command.h"
#include "logger.h"

bool execute_command(char* command){
    if(strcmp(command, "count") == 0){
        for(int i = 0; i < peers.size(); ++i){
            console("Peer %d received %d routes", i, peers[i].route_count);
        }
    }else{
        for(int i = 0; i < peers.size(); ++i){
            node* res = search_prefix(peers[i].rib, ntohl(inet_addr(command)));
            char prefix[16];
            char next_hop[16];
            memcpy(&prefix, inet_ntoa(in_addr{.s_addr = htonl(res->prefix)}), 15);
            memcpy(&next_hop, inet_ntoa(in_addr{.s_addr = htonl(res->next_hop)}), 15);
            console("%s/%d nexthop %s peer %d", prefix, res->prefix_len, next_hop, i);
        }
    }

    return false;
}
