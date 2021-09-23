#include "bgp_rib.h"
#include "logger.h"

bool best_path_selection_battle(bgp_peer* my_peer, node<adj_ribs_in_data>* me, node<loc_rib_data>* opponent){
    if(me->data->path_attr.local_pref != opponent->data->path_attr.local_pref){
        if(me->data->path_attr.local_pref > opponent->data->path_attr.local_pref){
            return true;
        }
        return false;
    }

    if(me->data->path_attr.origin != opponent->data->path_attr.origin){
        if(me->data->path_attr.origin < opponent->data->path_attr.origin){
            return true;
        }
        return false;
    }

    if(me->data->path_attr.med != opponent->data->path_attr.med){
        if(me->data->path_attr.med < opponent->data->path_attr.med){
            return true;
        }
        return false;
    }

    if(my_peer->bgp_id != opponent->data->peer->bgp_id){
        if(my_peer->bgp_id < opponent->data->peer->bgp_id){
            return true;
        }
        return false;
    }

    return false;
}

bool attempt_to_install_bgp_loc_rib(bgp_peer* my_peer, node<adj_ribs_in_data>* route){
    node<loc_rib_data>* installed_route = search_prefix(bgp_loc_rib, route->prefix, route->prefix_len, true);
    if(installed_route == nullptr){
        loc_rib_data data;
        data.peer = my_peer;
        memcpy(&data.path_attr, &route->data->path_attr, sizeof(attribute));
        add_prefix(bgp_loc_rib, route->prefix, route->prefix_len, data);
        log(log_level::INFO, "New route installed to loc_rib");
        return true;
    }

    if(best_path_selection_battle(my_peer, route, installed_route)){
        loc_rib_data data;
        data.peer = my_peer;
        memcpy(&data.path_attr, &route->data->path_attr, sizeof(attribute));
        add_prefix(bgp_loc_rib, route->prefix, route->prefix_len, data);
        log(log_level::INFO, "Route updated in loc_rib");
        return true;
    }

    return false;
}