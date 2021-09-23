#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <arpa/inet.h>

#include "bgp.h"
#include "logger.h"

bool bgp_update(bgp_peer* peer,  unsigned char* buff, int entire_length){
    uint16_t unfeasible_routes_length;
    memcpy(&unfeasible_routes_length, &buff[19], 2);
    unfeasible_routes_length = ntohs(unfeasible_routes_length);

    //READ_SHORT(buff, 19, unfeasible_routes_length);
    int read_length = 19 + 2;
    if(unfeasible_routes_length != 0){
        //hex_dump(&buff[read_length], unfeasible_routes_length);
        //printf("\n");
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
                exit(1);
            }
            log(log_level::DEBUG, "Unfeasible %s/%d", inet_ntoa(in_addr{.s_addr=ntohl(unfeasible_prefix)}), prefix_len);
            node<attribute>* unfeasible_prefix_node = search_prefix(peer->adj_ribs_in, unfeasible_prefix, prefix_len);
            if(unfeasible_prefix_node->prefix_len == prefix_len){
                delete_prefix(unfeasible_prefix_node);
                log(log_level::DEBUG, "Withdraw success!");
                peer->route_count--;
            }else{
                log(log_level::ERROR, "Failed to withdraw %s/%d", inet_ntoa(in_addr{.s_addr = htonl(unfeasible_prefix)}), prefix_len);
            }
        }
    }
    attribute path_attr;
    uint16_t total_path_attribute_length;
    memcpy(&total_path_attribute_length, &buff[19 + 2 + unfeasible_routes_length], 2);
    total_path_attribute_length = ntohs(total_path_attribute_length);
    read_length = 19 + 2 + unfeasible_routes_length + 2;
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
                path_attr.origin = origin;
                log(log_level::INFO, "Origin %d", origin);
            }
                break;
            case AS_PATH:{
                uint8_t segment_type = buff[read_length];
                uint8_t segment_length = buff[read_length + 1];
                //hex_dump(&buff[read_length], attribute_len);
                if(segment_type == bgp_path_attribute_as_path_segment_type::AS_SEQUENCE){
                    char as_list[256];
                    memset(as_list, 0, 255);
                    char as_str[10];
                    for(int i = 0; i < segment_length; i++){
                        uint16_t asn;
                        memcpy(&asn, &buff[read_length + 2 + i * 2], 2);
                        asn = ntohs(asn);
                        // log(log_level::DEBUG, "AS Path %d", asn);
                        memset(as_str, 0, 10);
                        sprintf(as_str, " %d", asn);
                        strcat(as_list, as_str);
                    }
                    log(log_level::INFO, "AS Path%s", as_list);
                }else{
                    log(log_level::DEBUG, "Unable to interpret segment type %d", segment_type);
                }
            }
                break;
            case NEXT_HOP:{
                assert(attribute_len == 4);
                path_attr.next_hop = buff[read_length]*256*256*256 + buff[read_length + 1]*256*256 + buff[read_length + 2]*256 + buff[read_length + 3];
                log(log_level::INFO, "Next Hop %s", inet_ntoa(in_addr{.s_addr = ntohl(path_attr.next_hop)}));
            }
                break;
            case MULTI_EXIT_DISC:
                uint32_t med;
                memcpy(&med, &buff[read_length], 4);
                med = ntohl(med);
                path_attr.med = med;
                log(log_level::INFO, "MED %d", med);
                break;
            case LOCAL_PREF:{
                uint32_t local_pref;
                memcpy(&local_pref, &buff[read_length], 4);
                local_pref = ntohl(local_pref);
                path_attr.local_pref = local_pref;
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
                //hex_dump(&buff[read_length], attribute_len);
                if(segment_type == bgp_path_attribute_as_path_segment_type::AS_SEQUENCE){
                    char as_list[256];
                    memset(as_list, 0, 255);
                    char as_str[10];
                    for(int i = 0; i < segment_length; i++){
                        uint32_t asn;
                        memcpy(&asn, &buff[read_length + 2 + i * 4], 4);
                        asn = ntohl(asn);
                        // log(log_level::DEBUG, "AS Path %d", asn);

                        memset(as_str, 0, 10);
                        sprintf(as_str, " %d", asn);
                        strcat(as_list, as_str);
                    }
                    log(log_level::INFO, "AS4 Path%s", as_list);
                }else{
                    log(log_level::DEBUG, "Unable to interpret segment type %d", segment_type);
                }
            }
                break;
            default:
                log(log_level::INFO, "Unhandled path attribute type %d", type);
                break;
        }
        read_length += attribute_len;
    }
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
        add_prefix(peer->adj_ribs_in, prefix, prefix_len, path_attr, &is_updated);
        if(!is_updated){
            peer->route_count++;
        }else{
            log(log_level::DEBUG, "Route updated!");
        }
    }
    return true;
}