#ifndef MY_AWESOME_BGP_COMMAND_H
#define MY_AWESOME_BGP_COMMAND_H

#include <vector>

#include "tree.h"

struct bgp_client_peer; // from bgp_client.h
struct loc_rib_data; // from bgp_rib.h

extern std::vector<bgp_client_peer> peers;
extern node<loc_rib_data>* bgp_loc_rib;

bool execute_command(const char* command);

#endif //MY_AWESOME_BGP_COMMAND_H
