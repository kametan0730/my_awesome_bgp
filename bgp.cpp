#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <arpa/inet.h>

#include "bgp.h"
#include "bgp_rib.h"
#include "logger.h"

#define BGP_VERSION 4
#define BGP_HOLD_TIME 180

#define BGP_HEADER_SIZE 19
#define BGP_OPEN_MESSAGE_SIZE 29

void hex_dump(unsigned char* buffer, int len, bool is_separate){
    if(console_mode == 1){
        return;
    }
    if(is_separate) printf("|");
    for(int i = 0; i < len; ++i){
        if(is_separate){
            printf("%02x|", buffer[i]);
        }else{
            printf("%02x", buffer[i]);
        }
    }
    printf("\n");
}

bool send_notification(bgp_peer* peer, int8_t error, uint16_t error_sub){
    bgp_notification notification;
    memset(notification.header.maker, 0xff, 16);
    notification.header.length = htons(19);
    notification.header.type = NOTIFICATION;
    notification.error = error;
    notification.error_sub = htons(error_sub);
    return peer->send(&notification, 19);
}

size_t encode_bgp_path_attributes_to_buffer(bgp_peer* peer, attributes* attr, unsigned char* buffer, size_t start_point){
    size_t pointer = start_point;
    uint8_t flag = 0;
    flag |= TRANSITIVE;

    /** Origin **/
    buffer[pointer++] = flag;
    buffer[pointer++] = ORIGIN;
    buffer[pointer++] = 1;
    buffer[pointer++] = attr->origin;

    /** AS Path **/
    flag = 0;
    flag |= TRANSITIVE;
    flag |= EXTENDED_LENGTH;
    buffer[pointer++] = flag;
    buffer[pointer++] = AS_PATH;
    uint16_t ex_len;
    if(attr->as_path_length == 0){
        ex_len = 0;
        memcpy(&buffer[pointer], &ex_len, 2);
        pointer += 2;
    }else{
        ex_len = htons(attr->as_path_length * ((peer->is_4_octet_as_supported) ? 4 : 2) + 2);
        memcpy(&buffer[pointer], &ex_len, 2);
        pointer += 2;
        buffer[pointer++] = AS_SEQUENCE;
        buffer[pointer++] = attr->as_path_length;
        if(peer->is_4_octet_as_supported){
            for(int i = 0; i < attr->as_path_length; ++i){
                uint32_t asn = htonl(attr->as_path[i]);
                memcpy(&buffer[pointer], &asn, 4);
                pointer += 4;
            }
        }else{
            for(int i = 0; i < attr->as_path_length; ++i){
                uint16_t asn = htons(attr->as_path[i]);
                memcpy(&buffer[pointer], &asn, 2);
                pointer += 2;
            }
        }
    }

    flag = 0;
    flag |= TRANSITIVE;
    buffer[pointer++] = flag;
    buffer[pointer++] = NEXT_HOP;
    buffer[pointer++] = 4;
    memcpy(&buffer[pointer], &attr->next_hop, 4);
    pointer += 4;

    flag = 0;
    flag |= OPTIONAL;
    buffer[pointer++] = flag;
    buffer[pointer++] = MULTI_EXIT_DISC;
    buffer[pointer++] = 4;
    uint32_t med = htonl(attr->med);
    memcpy(&buffer[pointer], &med, 4);
    pointer += 4;

    flag = 0;
    flag |= TRANSITIVE;
    buffer[pointer++] = flag;
    buffer[pointer++] = LOCAL_PREF;
    buffer[pointer++] = 4;
    uint32_t local_pref = htonl(attr->local_pref);
    memcpy(&buffer[pointer], &local_pref, 4);
    pointer += 4;

    return pointer - start_point;
}

bool send_update_with_nlri(bgp_peer* peer, attributes* attr, uint32_t prefix, uint8_t prefix_len){
    unsigned char buffer[1000];
    memset(buffer, 0, 1000);
    uint16_t pointer = BGP_HEADER_SIZE;

    buffer[pointer++] = 0; // withdraw routes length (2byte)
    buffer[pointer++] = 0;

    pointer += 2;
    size_t attr_size = encode_bgp_path_attributes_to_buffer(peer, attr, buffer, pointer);
    size_t attr_size_ordered = htons(attr_size);
    memcpy(&buffer[pointer-2], &attr_size_ordered, 2);
    pointer += attr_size;

    buffer[pointer++] = prefix_len;
    if(prefix_len <= 8){
        memcpy(&buffer[pointer], &prefix, 1);
        pointer += 1;
    }else if(prefix_len <= 16){
        memcpy(&buffer[pointer], &prefix, 2);
        pointer += 2;
    }else if(prefix_len <= 24){
        memcpy(&buffer[pointer], &prefix, 3);
        pointer += 3;
    }else if(prefix_len <= 32){
        memcpy(&buffer[pointer], &prefix, 4);
        pointer += 4;
    }

    bgp_header header;
    memset(header.maker, 0xff, 16);
    header.type = UPDATE;
    header.length = htons(pointer);
    memcpy(&buffer[0], &header, BGP_HEADER_SIZE);
    //hex_dump(buffer, pointer);
    return peer->send(buffer, pointer);
}

size_t encode_bgp_capabilities_to_buffer(unsigned char* buffer, size_t start_point){
    size_t pointer = start_point;

    /** MP BGP **/
    buffer[pointer++] = CAPABILITIES;
    buffer[pointer++] = 6;
    buffer[pointer++] = MULTIPROTOCOL_EXTENSION_FOR_BGP4;
    buffer[pointer++] = 4;
    uint16_t afi = htons(afi::IPV4);
    memcpy(&buffer[pointer], &afi, 2);
    pointer += 2;
    buffer[pointer++] = 0; // Reserved
    buffer[pointer++] = safi::UNICAST;

    /** 4-octet AS number **/
    buffer[pointer++] = CAPABILITIES;
    buffer[pointer++] = 6;
    buffer[pointer++] = SUPPORT_FOR_4_OCTET_AS_NUMBER_CAPABILITY;
    buffer[pointer++] = 4;
    uint32_t asn = htonl(my_as);
    memcpy(&buffer[pointer], &asn, 4);
    pointer += 4;

    return pointer - start_point;
}

bool send_open(bgp_peer* peer){

    unsigned char buffer[1000];
    memset(buffer, 0, 1000);
    uint16_t pointer = BGP_OPEN_MESSAGE_SIZE;

    uint16_t opt_len = encode_bgp_capabilities_to_buffer(buffer, pointer);
    pointer += opt_len;

    bgp_open open;
    memset(open.header.maker, 0xff, 16);
    open.header.length = htons(pointer);
    open.header.type = OPEN;
    open.version = BGP_VERSION;
    open.my_as = htons(my_as);
    open.hold_time = htons(BGP_HOLD_TIME);
    open.bgp_id = htonl(router_id);
    open.opt_length = opt_len;

    memcpy(&buffer[0], &open, BGP_OPEN_MESSAGE_SIZE);
    hex_dump(buffer, pointer);
    return peer->send(buffer, pointer);
}

bool bgp_update_handle_unfeasible_prefix(bgp_peer* peer, const unsigned char* buff, uint16_t unfeasible_routes_length){

    uint32_t read_length = 21; // 19+2
    if(unfeasible_routes_length != 0){
        uint32_t unfeasible_prefix;
        while(read_length < 19 + 2 + unfeasible_routes_length){
            int prefix_len = buff[read_length];
            if(prefix_len <= 8){
                unfeasible_prefix = buff[read_length + 1]*256*256*256;
                read_length += 2;
            }else if(prefix_len <= 16){
                unfeasible_prefix = buff[read_length + 1]*256*256*256 + buff[read_length + 2]*256*256;
                read_length += 3;
            }else if(prefix_len <= 24){
                unfeasible_prefix = buff[read_length + 1]*256*256*256 + buff[read_length + 2]*256*256 + buff[read_length + 3]*256;
                read_length += 4;
            }else if(prefix_len <= 32){
                unfeasible_prefix = buff[read_length + 1]*256*256*256 + buff[read_length + 2]*256*256 + buff[read_length + 3]*256 + buff[read_length + 4];
                read_length += 5;
            }else{
                log(log_level::ERROR, "Invalid packet");
                abort(); // TODO 安定してきたら、return falseにする
            }
            log(log_level::DEBUG, "Unfeasible %s/%d", inet_ntoa(in_addr{.s_addr=ntohl(unfeasible_prefix)}), prefix_len);
            node<adj_ribs_in_data>* unfeasible_prefix_node = search_prefix(peer->adj_ribs_in, unfeasible_prefix, prefix_len, true);
            if(unfeasible_prefix_node != nullptr){
                if(unfeasible_prefix_node->data->installed_loc_rib_node != nullptr){
                    delete_prefix(unfeasible_prefix_node->data->installed_loc_rib_node);
                    unfeasible_prefix_node->data->installed_loc_rib_node = nullptr;
                    log(log_level::DEBUG, "Withdrawn from loc_rib!");
                }
                delete_prefix(unfeasible_prefix_node);
                log(log_level::DEBUG, "Withdraw success!");
                peer->route_count--;
            }else{
                log(log_level::ERROR, "Failed to withdraw %s/%d", inet_ntoa(in_addr{.s_addr = htonl(unfeasible_prefix)}), prefix_len);
                abort();
            }
        }
    }
    return true;
}

bool bgp_update_handle_path_attribute(bgp_peer* peer, const unsigned char* buff, uint16_t unfeasible_routes_length, uint16_t total_path_attribute_length, adj_ribs_in_data& route_data){

    int read_length = 19 + 2 + unfeasible_routes_length + 2;
    while(read_length < 19 + 2 + unfeasible_routes_length + 2 + total_path_attribute_length){
        uint8_t flag = buff[read_length];
        uint8_t type = buff[read_length + 1];
        read_length += 2;
        uint16_t attribute_len;
        if(!(flag & EXTENDED_LENGTH)){
            attribute_len = buff[read_length];
            read_length += 1;
        }else{
            memcpy(&attribute_len, &buff[read_length], 2);
            attribute_len = ntohs(attribute_len);
            read_length += 2;
        }
        switch(type){
            case ORIGIN:{
                uint8_t origin = buff[read_length];
                route_data.path_attr.origin = origin;
                log(log_level::INFO, "Origin %d", origin);
            }
                break;
            case AS_PATH:{
                uint8_t segment_type = buff[read_length];
                uint8_t segment_length = buff[read_length + 1];
                if(segment_type == bgp_path_attribute_as_path_segment_type::AS_SEQUENCE){
                    std::string as_list_str;
                    bool is_overflowed = false;
                    for(int i = 0; i < segment_length; i++){
                        std::ostringstream oss;
                        as_list_str.append(" ");
                        if(peer->is_4_octet_as_supported){
                            uint32_t asn;
                            memcpy(&asn, &buff[read_length + 2 + i * 4], 4);
                            asn = ntohl(asn);
                            oss << asn;
                            if(route_data.path_attr.as_path_length < 64){
                                route_data.path_attr.as_path[route_data.path_attr.as_path_length++] = asn;
                            }else{
                                is_overflowed = true;
                            }
                        }else{
                            uint16_t asn;
                            memcpy(&asn, &buff[read_length + 2 + i * 2], 2);
                            asn = ntohs(asn);
                            oss << asn;
                            if(route_data.path_attr.as_path_length < 64){
                                route_data.path_attr.as_path[route_data.path_attr.as_path_length++] = asn;
                            }else{
                                is_overflowed = true;
                            }
                        }
                        as_list_str.append(oss.str());
                    }
                    if(is_overflowed){
                        log(log_level::WARNING, "Overflowed AS PATH %d", segment_length);

                    }
                    log(log_level::INFO, "AS Path%s", as_list_str.c_str());
                }else{
                    log(log_level::DEBUG, "Unable to interpret segment type %d", segment_type);
                }
            }
                break;
            case NEXT_HOP:{
                assert(attribute_len == 4);
                route_data.path_attr.next_hop = buff[read_length]*256*256*256 + buff[read_length + 1]*256*256 + buff[read_length + 2]*256 + buff[read_length + 3];
                log(log_level::INFO, "Next Hop %s", inet_ntoa(in_addr{.s_addr = ntohl(route_data.path_attr.next_hop)}));
            }
                break;
            case MULTI_EXIT_DISC:
                uint32_t med;
                memcpy(&med, &buff[read_length], 4);
                med = ntohl(med);
                route_data.path_attr.med = med;
                log(log_level::INFO, "MED %d", med);
                break;
            case LOCAL_PREF:{
                uint32_t local_pref;
                memcpy(&local_pref, &buff[read_length], 4);
                local_pref = ntohl(local_pref);
                route_data.path_attr.local_pref = local_pref;
                log(log_level::INFO, "Local Pref %d", local_pref);
            }
                break;
            case ATOMIC_AGGREGATE:
                log(log_level::INFO, "Atomic Aggregate");
                break;
            case AS4_PATH:
            {
                uint8_t segment_type = buff[read_length];
                uint8_t segment_length = buff[read_length + 1];
                if(segment_type == bgp_path_attribute_as_path_segment_type::AS_SEQUENCE){
                    std::string as_list_str;
                    for(int i = 0; i < segment_length; i++){
                        uint32_t asn;
                        memcpy(&asn, &buff[read_length + 2 + i * 4], 4);
                        asn = ntohl(asn);
                        std::ostringstream oss;
                        oss << asn;
                        as_list_str.append(" ");
                        as_list_str.append(oss.str());
                        /*
                        if(route_data.path_attr.as4_path_length < 64){
                            route_data.path_attr.as4_path[route_data.path_attr.as_path_length++] = asn;
                        }else{
                            log(log_level::WARNING, "Overflowed AS4 PATH %d", segment_length);
                        }*/
                    }
                    log(log_level::INFO, "AS4 Path%s", as_list_str.c_str());
                }else{
                    log(log_level::DEBUG, "Unable to interpret segment type %d", segment_type);
                }
            }
                break;
            default:
                log(log_level::INFO, "Unhandled path attributes type %d", type);
                break;
        }
        read_length += attribute_len;
    }
    return true;
}

bool bgp_update(bgp_peer* peer,  unsigned char* buff, int entire_length){
    uint16_t unfeasible_routes_length;
    memcpy(&unfeasible_routes_length, &buff[19], 2);
    unfeasible_routes_length = ntohs(unfeasible_routes_length);
    int read_length;

    bgp_update_handle_unfeasible_prefix(peer, buff, unfeasible_routes_length);

    adj_ribs_in_data route_data;
    uint16_t total_path_attribute_length;
    memcpy(&total_path_attribute_length, &buff[19 + 2 + unfeasible_routes_length], 2);
    total_path_attribute_length = ntohs(total_path_attribute_length);

    bgp_update_handle_path_attribute(peer, buff, unfeasible_routes_length, total_path_attribute_length, route_data);

    read_length = 19 + 2 + unfeasible_routes_length + 2 + total_path_attribute_length;

    while(read_length < entire_length){
        uint32_t prefix;
        int prefix_len = buff[read_length];
        if(prefix_len <= 8){
            prefix = buff[read_length + 1]*256*256*256;
            read_length += 2;
        }else if(prefix_len <= 16){
            prefix = buff[read_length + 1]*256*256*256 + buff[read_length + 2]*256*256;
            read_length += 3;
        }else if(prefix_len <= 24){
            prefix = buff[read_length + 1]*256*256*256 + buff[read_length + 2]*256*256 + buff[read_length + 3]*256;
            read_length += 4;
        }else if(prefix_len <= 32){
            prefix = buff[read_length + 1]*256*256*256 + buff[read_length + 2]*256*256 + buff[read_length + 3]*256 + buff[read_length + 4];
            read_length += 5;
        }else{
            log(log_level::ERROR, "Invalid packet");
            break;
        }
        log(log_level::DEBUG, "%s/%d", inet_ntoa(in_addr{.s_addr=ntohl(prefix)}), prefix_len);
        bool is_updated;
        node<adj_ribs_in_data>* added = add_prefix(peer->adj_ribs_in, prefix, prefix_len, route_data, &is_updated);
        if(!is_updated){
            peer->route_count++;
        }else{
            log(log_level::DEBUG, "Route updated!");
        }
        attempt_to_install_bgp_loc_rib(peer, added);
    }
    return true;
}