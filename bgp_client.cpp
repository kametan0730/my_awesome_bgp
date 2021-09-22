#include <cassert>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>

#include "bgp.h"
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
        close(peer->sock);
        return false;
    }

    set_nonblocking(peer->sock);
    log(log_level::INFO, "Connected to %s", inet_ntoa(peer->server_address.sin_addr));

    //send_open(peer);
    return true;
}

void close_peer(bgp_client_peer* peer){
    if(peer->rib != nullptr){
        log(log_level::DEBUG, "Cleaned table sock %d", peer->sock);
        delete_prefix(peer->rib, true);
        peer->rib = nullptr;
    }
    close(peer->sock);
}

bool loop_established(bgp_client_peer* peer){
    int len;
    unsigned char buff[10000];
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
    uint16_t error = 0;
    while(len < entire_length){
        remain_byte = entire_length - len;
        log(log_level::TRACE, "%d bytes remain", remain_byte);
        append_len = recv(peer->sock, &buff[len], std::min(remain_byte, 1000), 0);
        if(append_len <= 0){
            if(++error > 10000){
                log(log_level::ERROR, "Failed to receive packet");
                exit(EXIT_FAILURE);
            }
            continue;
        }
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
            bgp_update(peer, buff, entire_length);
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
                peer->rib->is_prefix = true;
                peer->rib->prefix = 0;
                peer->rib->prefix_len = 0;
                peer->rib->next_hop = 0;
                peer->rib->parent = nullptr;
                peer->rib->node_0 = nullptr;
                peer->rib->node_1 = nullptr;
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
                }else{
                    peer->connect_cool_time = 100000;
                }
                peer->connect_cool_time = 10000;
            }else{
                peer->connect_cool_time--;
            }
            break;
        case OPEN_CONFIRM:
        case ESTABLISHED:
            if(!loop_established(peer)){
                peer->state = IDLE;
                close_peer(peer);
            }
            break;
    }
    return true;
}