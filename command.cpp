#include <cstdio>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "command.h"
#include "logger.h"

bool execute_command(char* command){
    uint32_t param_count = 0;
    char command_param[30][10]; // TODO この辺の適当なコードをどうにかする

    uint8_t separator = 0;
    for(int i = 0; i < 255; ++i){
        if(command[i] == ' ' or command[i] == '\0'){
            if(i - separator == 0){
                if(command[i] != '\0' ){
                    separator++;
                    continue;
                }else{
                    break;
                }
            }
            memcpy(command_param[param_count], command+separator, i - separator);
            command_param[param_count][i - separator] = '\0';
            separator = i+1;
            param_count++;
            if(command[i] == '\0'){
                break;
            }
        }
    }

#ifdef DEBUG
    console("Param count:%d", param_count);
    for(int i=0;i < param_count; i++){
        console("%s", command_param[i]);
    }
#endif

    if(strcmp(command_param[0], "count") == 0){
        for(int i = 0; i < peers.size(); ++i){
            console("Peer %d received %d routes", i, peers[i].route_count);
        }
        return true;
    }else if(strcmp(command_param[0], "route") == 0){
        if(param_count >= 2){
            for(int i = 0; i < peers.size(); ++i){
                node<adj_ribs_in_data>* res = search_prefix(peers[i].adj_ribs_in, ntohl(inet_addr(command_param[1]))); // TODO 入力値検証
                char prefix[17];
                char next_hop[17];
                memcpy(&prefix, inet_ntoa(in_addr{.s_addr = htonl(res->prefix)}), 16);
                memcpy(&next_hop, inet_ntoa(in_addr{.s_addr = htonl(res->data->path_attr.next_hop)}), 16);
                console("%s/%d  origin %d, nexthop %s, med %d, local-pref %d, peer %d", prefix, res->prefix_len,
                        res->data->path_attr.origin, next_hop, res->data->path_attr.med,
                        res->data->path_attr.local_pref, i);

            }
            return true;
        }else{
            return false;
        }
    }

    return false;
}
