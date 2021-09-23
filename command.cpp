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
            node<attribute>* res = search_prefix(peers[i].adj_ribs_in, ntohl(inet_addr(command)));
            char prefix[17];
            char next_hop[17];
            memcpy(&prefix, inet_ntoa(in_addr{.s_addr = htonl(res->prefix)}), 16);
            memcpy(&next_hop, inet_ntoa(in_addr{.s_addr = htonl(res->data->next_hop)}), 16);
            console("%s/%d nexthop %s peer %d", prefix, res->prefix_len, next_hop, i);
        }
    }
    return false;
}
