#ifndef MY_AWESOME_BGP_BGP_RIB_H
#define MY_AWESOME_BGP_BGP_RIB_H

#include "bgp.h"
#include "tree.h"

struct attribute{
    uint8_t origin = 0;
    uint32_t next_hop = 0;
    uint32_t med = 0;
    uint32_t local_pref = 0;
    // attribute* next; // TODO 複数の属性を持たせられるように
};

struct adj_ribs_in_data{
    attribute path_attr;
};

struct bgp_peer;

struct loc_rib_data{
    bgp_peer* peer;
    attribute path_attr;
};

struct adj_ribs_out_data{
    attribute path_attr;
};

extern node<loc_rib_data>* bgp_loc_rib;

bool best_path_selection_battle(bgp_peer* my_peer, attribute* my_attr, attribute* opponent_attr);
bool attempt_to_install_bgp_loc_rib(bgp_peer* my_peer, node<adj_ribs_in_data>* route);

#endif //MY_AWESOME_BGP_BGP_RIB_H
