#ifndef MY_AWESOME_BGP_BGP_RIB_H
#define MY_AWESOME_BGP_BGP_RIB_H

#include "tree.h"

struct bgp_peer; // from bgp.h
struct loc_rib_data;
struct adj_ribs_out_data;

struct attribute{
    uint8_t origin = 0;
    uint8_t as_path_length = 0;
    uint32_t as_path[64]{}; // TODO 可変長のが良い
    uint32_t next_hop = 0;
    uint32_t med = 0;
    uint32_t local_pref = 0;
    // attribute* next; // TODO 複数の属性を持たせられるように
};

struct adj_ribs_in_data{
    attribute path_attr;
    node<loc_rib_data>* installed_loc_rib_node = nullptr;
};

struct loc_rib_data{
    bgp_peer* peer = nullptr;
    attribute* path_attr = nullptr;
    node<adj_ribs_in_data>* source_adj_ribs_in_node = nullptr;
    node<adj_ribs_out_data>* installed_adj_ribs_out_node = nullptr;
};

struct adj_ribs_out_data{
    attribute* path_attr = nullptr;
    node<loc_rib_data>* source_loc_rib_node = nullptr;
};

extern node<loc_rib_data>* bgp_loc_rib;

bool best_path_selection_battle(bgp_peer* my_peer, attribute* my_attr, attribute* opponent_attr);
bool attempt_to_install_bgp_loc_rib(bgp_peer* my_peer, node<adj_ribs_in_data>* route);

#endif //MY_AWESOME_BGP_BGP_RIB_H
