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
        if(search == inet_ntoa(peer.server_address.sin_addr)){
            return &peer;
        }
    }
    return nullptr;
}

std::string as_path_to_string(uint32_t* as_path_list, uint8_t as_path_length){
    std::string as_path_str;
    for(int j = 0; j < as_path_length; ++j){
        std::ostringstream oss;
        oss << as_path_list[j];
        as_path_str.append(oss.str());
        if(j != as_path_length-1){
            as_path_str.append(" ");
        }
    }
    return as_path_str;
}

command_result_status bgp_statistic_command(){
    for(int i = 0; i < peers.size(); ++i){
        console("Peer %d received %d routes", i, peers[i].route_count);
    }
    return command_result_status::SUCCESS;
}

command_result_status bgp_route_command(const std::vector<std::string>& command_params){
    size_t param_count = command_params.size();
    if(param_count < 3){
        return command_result_status::INVALID_PARAMS;
    }

    node<loc_rib_data>* res = search_prefix(bgp_loc_rib, ntohl(inet_addr(command_params[2].c_str()))); // TODO 入力値検証
    char prefix[16];
    char next_hop[16];
    if(res->is_prefix and res->data != nullptr){
        memcpy(&prefix, inet_ntoa(in_addr{.s_addr = htonl(res->prefix)}), 16);
        memcpy(&next_hop, inet_ntoa(in_addr{.s_addr = htonl(res->data->path_attr->next_hop)}), 16);
        console("%s/%d  origin %d, nexthop %s, med %d, local-pref %d, loc_rib", prefix, res->prefix_len,
                res->data->path_attr->origin, next_hop, res->data->path_attr->med,
                res->data->path_attr->local_pref);
        console("as-path %s, length %d", as_path_to_string(res->data->path_attr->as_path, res->data->path_attr->as_path_length).c_str(), res->data->path_attr->as_path_length);
    }

    return command_result_status::SUCCESS;
}

command_result_status bgp_neighbor_route_command(const std::vector<std::string>& command_params, bgp_peer* peer){
    size_t param_count = command_params.size();
    if(param_count < 5){
        return command_result_status::INVALID_PARAMS;
    }

    node<adj_ribs_in_data>* res = search_prefix(peer->adj_ribs_in, ntohl(inet_addr(command_params[4].c_str())));
    char prefix[16];

    char next_hop[16];
    if(res->is_prefix and res->data != nullptr){ // TODO is_prefixがtrueでdataがnullptrの時がどのような場合かよく考える
        memcpy(&prefix, inet_ntoa(in_addr{.s_addr = htonl(res->prefix)}), 16);
        memcpy(&next_hop, inet_ntoa(in_addr{.s_addr = htonl(res->data->path_attr.next_hop)}), 16);
        console("%s/%d  origin %d, nexthop %s, med %d, local-pref %d, peer %d", prefix, res->prefix_len,
                res->data->path_attr.origin, next_hop, res->data->path_attr.med,
                res->data->path_attr.local_pref, peer->index);
        console("as-path %s, length %d", as_path_to_string(res->data->path_attr.as_path, res->data->path_attr.as_path_length).c_str(), res->data->path_attr.as_path_length);
    }

    return command_result_status::SUCCESS;
}

command_result_status bgp_neighbor_command(const std::vector<std::string>& command_params){
    size_t param_count = command_params.size();
    if(param_count < 3){
        return command_result_status::INVALID_PARAMS;
    }
    auto* neighbor_peer = get_peer_by_string(command_params[2]);
    if(neighbor_peer == nullptr){
        console("Neighbor not found");
        return command_result_status::INVALID_PARAMS;
    }

    if(param_count < 4){
        return command_result_status::INVALID_PARAMS;
    }

    if(command_params[3] == "route"){ // bgp neighbor x.x.x.x route x.x.x.x
        return bgp_neighbor_route_command(command_params, neighbor_peer);
    }

    return command_result_status::SUCCESS;
}

command_result_status bgp_command(const std::vector<std::string>& command_params){
    size_t param_count = command_params.size();
    if(param_count < 2){
        return command_result_status::INVALID_PARAMS;
    }
    if(command_params[1] == "route"){ // bgp route
        return bgp_route_command(command_params);
    }else if(command_params[1] == "statistics"){ // bgp statistics
        return bgp_statistic_command();
    }else if(command_params[1] == "neighbor"){ // bgp neighbor
        return bgp_neighbor_command(command_params);
    }

    return command_result_status::INVALID_PARAMS;
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

    if(command_params[0] == "bgp"){
        return bgp_command(command_params);
    }
    return command_result_status::NOT_FOUND;
}
