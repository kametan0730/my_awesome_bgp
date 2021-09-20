#include <fcntl.h>
#include <unistd.h>
#include <fstream>

#include "bgp_client.h"
#include "logger.h"
#include "tcp_socket.h"
#include "tree.h"

void hex_dump(unsigned char* buffer, int len, bool is_separate = false){
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

bool send_notification(bgp_client_peer* peer, int8_t error, uint16_t error_sub){
    bgp_notification notification;
    memset(notification.header.maker, 0xff, 16);
    notification.header.length = htons(19);
    notification.header.type = NOTIFICATION;
    notification.error = error;
    notification.error_sub = htons(error_sub);
    if(send(peer->sock, &notification, 19, 0) <= 0){
        return false;
    }
    return true;
}


bool send_open(bgp_client_peer* peer){
    bgp_open open;
    memset(open.header.maker, 0xff, 16);
    open.header.length = htons(29);
    open.header.type = OPEN;
    open.version = 4;
    open.my_as = htons(my_as);
    open.hold_time = htons(180);
    open.bgp_id = htonl(373737373);
    open.opt_length = 0;

    if(send(peer->sock, &open, 29, 0) <= 0){
        return false;
    }
    return true;
}

bool try_to_connect(bgp_client_peer* peer){
    peer->server_address.sin_port = htons(179);
    if((peer->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
        log(log_level::ERROR, "Failed to create socket");
        return false;
    }
    if(connect_with_timeout(peer->sock, (struct sockaddr*) &peer->server_address, sizeof(peer->server_address), 1000) < 0){

        if(errno == EINTR){
            log(log_level::ERROR, "Timeout to connect server");
        }else{
            log(log_level::ERROR, "Failed to connect server");
        }
        return false;
    }

    set_nonblocking(peer->sock);
    log(log_level::INFO, "Connected to %s", inet_ntoa(peer->server_address.sin_addr));

    //send_open(peer);
    return true;
}

bool loop_established(bgp_client_peer* peer){
    int len;
    unsigned char buff[100000];
    //printf("\e[m");
    memset(buff, 0x00, 10000);
    len = recv(peer->sock, &buff, 19, 0);
    if(len <= 0){
        return true;
    }
    auto* bgphp = reinterpret_cast<bgp_header*>(buff);

    // hex_dump(buff, 29); // dump header
    int entire_length = ntohs(bgphp->length);
    log(log_level::TRACE, "Receiving %d bytes", entire_length);

    int append_len, remain_byte;
    while(len < entire_length){
        remain_byte = entire_length - len;
        log(log_level::TRACE, "%d bytes remain", remain_byte);
        append_len = recv(peer->sock, &buff[len], std::min(remain_byte, 1000), 0);
        log(log_level::TRACE, "New %d bytes received", append_len);
        len += append_len;
    }

    switch(bgphp->type){
        case OPEN:{
            log(log_level::INFO, "Open Received");
            auto* bgpopp = reinterpret_cast<bgp_open*>(buff);
            log(log_level::INFO, "Version: %d", bgpopp->version);
            log(log_level::INFO, "My AS: %d", ntohs(bgpopp->my_as));
            log(log_level::INFO, "Hold Time: %d", ntohs(bgpopp->hold_time));
            log(log_level::INFO, "BGP Id: %d", ntohl(bgpopp->bgp_id));
            log(log_level::INFO, "Opt Length: %d", bgpopp->opt_length);

            // hex_dump(&buff[29], 54, true);

            int read_length = 29;
            while(read_length < 29 + bgpopp->opt_length){
                int option_type = buff[read_length];
                read_length++;
                int option_length = buff[read_length];
                read_length++;
                switch(option_type){
                    case bgp_open_optional_parameter_type::CAPABILITIES:{
                        uint8_t capability_type = buff[read_length];
                        read_length++;
                        uint8_t capability_length = buff[read_length];
                        read_length++;
                        read_length += capability_length;
                        log(log_level::INFO, "Capability type : %d", capability_type);
                    }
                        break;
                    default:
                        log(log_level::INFO, "Option type : %d", option_type);
                        break;
                }
            }

            if(!send_open(peer)){
                log(log_level::ERROR, "Failed to send packet");
                return false;
            }
        }
            break;
        case UPDATE:{
            log(log_level::INFO, "Update received");
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
                        log(log_level::DEBUG, "Unfeasible %d.0.0.0/%d", buff[read_length + 1], prefix_len);
                        read_length += 2;
                    }else if(prefix_len <= 16){
                        unfeasible_prefix = buff[read_length + 1]*256*256*256 + buff[read_length + 2]*256*256;
                        log(log_level::DEBUG, "Unfeasible %d.%d.0.0/%d", buff[read_length + 1], buff[read_length + 2],
                            prefix_len);
                        read_length += 3;
                    }else if(prefix_len <= 24){
                        unfeasible_prefix = buff[read_length + 1]*256*256*256 + buff[read_length + 2]*256*256 + buff[read_length + 3]*256;
                        log(log_level::DEBUG, "Unfeasible %d.%d.%d.0/%d", buff[read_length + 1], buff[read_length + 2],
                            buff[read_length + 3], prefix_len);
                        read_length += 4;
                    }else if(prefix_len <= 32){
                        unfeasible_prefix = buff[read_length + 1]*256*256*256 + buff[read_length + 2]*256*256 + buff[read_length + 3]*256 + buff[read_length + 4];
                        log(log_level::DEBUG, "Unfeasible %d.%d.%d.%d/%d", buff[read_length + 1], buff[read_length + 2],
                            buff[read_length + 3], buff[read_length + 4], prefix_len);
                        read_length += 5;
                    }else{
                        log(log_level::ERROR, "Invalid packet");
                        exit(1);
                    }
                    node* unfeasible_prefix_node = search_prefix(peer->rib, unfeasible_prefix, prefix_len);
                    if(unfeasible_prefix_node->prefix_len == prefix_len){
                        delete_prefix(unfeasible_prefix_node);
                        log(log_level::DEBUG, "Withdraw success!");
                    }else{
                        log(log_level::ERROR, "Failed to withdraw %s/%d", inet_ntoa(in_addr{.s_addr = htonl(unfeasible_prefix)}), prefix_len);
                    }
                }
            }
            uint32_t next_hop;
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
                        log(log_level::INFO, "Origin %d", origin);
                    }
                        break;
                    case AS_PATH:{
                        uint8_t segment_type = buff[read_length];
                        uint8_t segment_length = buff[read_length + 1];
                        //read_length += 2;
                        //hex_dump(&buff[read_length], 40);
                        for(int i = 0; i < segment_length; i++){
                            uint16_t asn;
                            memcpy(&asn, &buff[read_length + 2 + i * 2], 2);
                            asn = ntohs(asn);
                            //log(log_level::INFO, "AS Path %d", asn);
                        }
                        //hex_dump(&buff[read_length], attribute_len);
                    }
                        break;
                    case NEXT_HOP:{
                        if(attribute_len == 4){
                            log(log_level::INFO, "Next Hop %d.%d.%d.%d", buff[read_length], buff[read_length + 1],
                                buff[read_length + 2], buff[read_length + 3]);
                            next_hop = buff[read_length]*256*256*256 + buff[read_length + 1]*256*256 + buff[read_length + 2]*256 + buff[read_length + 3];

                        }else{
                            //log(log_level::ERROR, "ERR");
                            //    hex_dump(&buff[read_length], attribute_len);
                        }
                    }
                        break;
                    case MULTI_EXIT_DISC:
                        uint32_t med;
                        memcpy(&med, &buff[read_length], 4);
                        med = ntohl(med);
                        log(log_level::INFO, "MED %d", med);
                        break;
                    case LOCAL_PREF:{
                        uint32_t local_pref;
                        memcpy(&local_pref, &buff[read_length], 4);
                        local_pref = ntohl(local_pref);
                        log(log_level::INFO, "Local Pref %d", local_pref);
                    }
                        break;
                    case ATOMIC_AGGREGATE:
                        log(log_level::INFO, "Atomic Aggregate");
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
                log(log_level::DEBUG, "PrefixLen: %d", prefix_len);
                if(prefix_len <= 8){
                    prefix = buff[read_length + 1]*256*256*256;
                    log(log_level::DEBUG, "%d.0.0.0/%d", buff[read_length + 1], prefix_len);
                    read_length += 2;
                }else if(prefix_len <= 16){
                    prefix = buff[read_length + 1]*256*256*256 + buff[read_length + 2]*256*256;
                    log(log_level::DEBUG, "%d.%d.0.0/%d", buff[read_length + 1], buff[read_length + 2], prefix_len);
                    read_length += 3;
                }else if(prefix_len <= 24){
                    prefix = buff[read_length + 1]*256*256*256 + buff[read_length + 2]*256*256 + buff[read_length + 3]*256;
                    log(log_level::DEBUG, "%d.%d.%d.0/%d", buff[read_length + 1], buff[read_length + 2],
                        buff[read_length + 3], prefix_len);
                    read_length += 4;
                }else if(prefix_len <= 32){
                    prefix = buff[read_length + 1]*256*256*256 + buff[read_length + 2]*256*256 + buff[read_length + 3]*256 + buff[read_length + 4];
                    log(log_level::DEBUG, "%d.%d.%d.%d/%d", buff[read_length + 1], buff[read_length + 2],
                        buff[read_length + 3], buff[read_length + 4], prefix_len);
                    read_length += 5;
                }else{
                    log(log_level::ERROR, "Invalid packet");
                    break;
                }
                add_prefix(peer->rib, prefix, prefix_len, next_hop);
            }
        }
            break;
        case NOTIFICATION:{
            log(log_level::NOTICE, "Notification received");
            auto* bgpntp = reinterpret_cast<bgp_notification*>(buff);
            log(log_level::NOTICE, "Error: %d", bgpntp->error);
            log(log_level::NOTICE, "Sub: %d", bgpntp->error_sub);
            return false;
        }
        case KEEPALIVE:
            if(peer->state == OPEN_CONFIRM){
                node* root = (node*) malloc(sizeof(node));
                root->is_prefix = true;
                root->prefix = 0;
                root->prefix_len = 0;
                root->next_hop = 0;
                root->parent = nullptr;
                root->node_0 = nullptr;
                root->node_1 = nullptr;
                peer->rib = root;
                peer->state = ESTABLISHED;
            }
            log(log_level::INFO, "Keepalive Received");
            bgp_header header;
            memset(header.maker, 0xff, 16);
            header.length = htons(19);
            header.type = KEEPALIVE;
            if(send(peer->sock, &header, len, 0) <= 0){
                log(log_level::ERROR, "Failed to send packet");
                return false;
            }
            break;
        default:
            printf("\e[7m");
            log(log_level::WARNING, "Unknown type received %d", bgphp->type);
            break;
    }
    return true;
}

bool bgp_client_loop(bgp_client_peer* peer){
    switch(peer->state){
        case IDLE:
            if(peer->connect_cool_time == 0){
                if(try_to_connect(peer)){
                    peer->state = OPEN_CONFIRM;
                }
                peer->connect_cool_time = 2000;
            }else{
                peer->connect_cool_time--;
            }
            break;
        case OPEN_CONFIRM:
        case ESTABLISHED:
            if(!loop_established(peer)){
                peer->state = IDLE;
                if(peer->rib != nullptr){
                    log(log_level::DEBUG, "Cleaned table sock %d", peer->sock);
                    delete_prefix(peer->rib, true);
                    peer->rib = nullptr;
                }
            }
            break;
    }
    return true;
}