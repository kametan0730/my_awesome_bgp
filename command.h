#ifndef MY_AWESOME_BGP_COMMAND_H
#define MY_AWESOME_BGP_COMMAND_H

#include <vector>

#include "bgp_client.h"

extern std::vector<bgp_client_peer> peers;

bool execute_command(char* command);

#endif //MY_AWESOME_BGP_COMMAND_H
