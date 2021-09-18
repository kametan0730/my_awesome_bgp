#include <cstdio>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "command.h"
#include "logger.h"

bool execute_command(char* command){
    //printf("%s\n", command);
    node* res = search_prefix(peers[0].rib, inet_addr(command));
    console("%s/%d nexthop %s", inet_ntoa(in_addr{.s_addr = res->prefix}),res->prefix_len, inet_ntoa(in_addr{.s_addr = res->next_hop}));
    return false;
}
