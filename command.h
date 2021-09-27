#ifndef MY_AWESOME_BGP_COMMAND_H
#define MY_AWESOME_BGP_COMMAND_H

#include <vector>

#include "tree.h"

struct bgp_client_peer; // from bgp_client.h
struct loc_rib_data; // from bgp_rib.h

extern std::vector<bgp_client_peer> peers;
extern node<loc_rib_data>* bgp_loc_rib;

enum class command_result_status{
    SUCCESS,
    NOT_FOUND,
    INVALID_PARAMS,
};

command_result_status execute_command(const std::string& command);

#endif //MY_AWESOME_BGP_COMMAND_H
