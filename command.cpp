#include <cstdio>
#include <sstream>
#include <string>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "bgp_client.h"
#include "bgp_rib.h"
#include "command.h"
#include "logger.h"

bgp_peer* get_peer_by_string(const std::string& search){

    for(auto & peer : peers){

    }

    return nullptr;
}

command_result_status route_command(const std::vector<std::string>& command_params){
    if(command_params.size() <= 1){
        return command_result_status::INVALID_PARAMS;
    }
    node<loc_rib_data>* res = search_prefix(bgp_loc_rib, ntohl(inet_addr(command_params[1].c_str()))); // TODO 入力値検証
    char prefix[16];
    char next_hop[16];
    if(res->is_prefix and res->data != nullptr){
        memcpy(&prefix, inet_ntoa(in_addr{.s_addr = htonl(res->prefix)}), 16);
        memcpy(&next_hop, inet_ntoa(in_addr{.s_addr = htonl(res->data->path_attr->next_hop)}), 16);
        console("%s/%d  origin %d, nexthop %s, med %d, local-pref %d, loc_rib", prefix, res->prefix_len,
                res->data->path_attr->origin, next_hop, res->data->path_attr->med,
                res->data->path_attr->local_pref);
    }

    return command_result_status::SUCCESS;
}

command_result_status execute_command(const std::string& command){
    auto command_params = std::vector<std::string>();
    size_t offset = 0;
    while(true){
        size_t separator = command.find(' ', offset);
        if (separator == std::string::npos) {
            command_params.push_back(command.substr(offset));
            break;
        }
        command_params.push_back(command.substr(offset, separator - offset));
        offset = separator + 1;
    }
    size_t param_count = command_params.size();

    if(command_params[0] == "count"){
        for(int i = 0; i < peers.size(); ++i){
            console("Peer %d received %d routes", i, peers[i].route_count);
        }
        return command_result_status::SUCCESS;
    }else if(command_params[0] == "route"){
        if(param_count >= 2){
            for(int i = 0; i < peers.size(); ++i){
                node<adj_ribs_in_data>* res = search_prefix(peers[i].adj_ribs_in, ntohl(inet_addr(command_params[1].c_str()))); // TODO 入力値検証
                char prefix[16];
                char next_hop[16];
                if(res->is_prefix and res->data != nullptr){ // TODO is_prefixがtrueでdataがnullptrの時がどのような場合かよく考える
                    std::string as_path_str;
                    for(int j = 0; j < res->data->path_attr.as_path_length; ++j){
                        std::ostringstream oss;
                        oss << res->data->path_attr.as_path[j];
                        as_path_str.append(oss.str());
                        as_path_str.append(" ");
                    }
                    memcpy(&prefix, inet_ntoa(in_addr{.s_addr = htonl(res->prefix)}), 16);
                    memcpy(&next_hop, inet_ntoa(in_addr{.s_addr = htonl(res->data->path_attr.next_hop)}), 16);
                    console("%s/%d  origin %d, nexthop %s, med %d, local-pref %d, peer %d", prefix, res->prefix_len,
                            res->data->path_attr.origin, next_hop, res->data->path_attr.med,
                            res->data->path_attr.local_pref, i);
                    console("as-path %slength %d", as_path_str.c_str(), res->data->path_attr.as_path_length);
                }
            }

            node<loc_rib_data>* res = search_prefix(bgp_loc_rib, ntohl(inet_addr(command_params[1].c_str()))); // TODO 入力値検証
            char prefix[16];
            char next_hop[16];
            if(res->is_prefix and res->data != nullptr){
                memcpy(&prefix, inet_ntoa(in_addr{.s_addr = htonl(res->prefix)}), 16);
                memcpy(&next_hop, inet_ntoa(in_addr{.s_addr = htonl(res->data->path_attr->next_hop)}), 16);
                console("%s/%d  origin %d, nexthop %s, med %d, local-pref %d, loc_rib", prefix, res->prefix_len,
                        res->data->path_attr->origin, next_hop, res->data->path_attr->med,
                        res->data->path_attr->local_pref);
            }

            return command_result_status::SUCCESS;
        }else{
            return command_result_status::INVALID_PARAMS;
        }
    }

    return command_result_status::NOT_FOUND;
}
