#include <cassert>
#include <fstream>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "bgp_client.h"
#include "bgp_rib.h"
#include "logger.h"
#include "tcp_socket.h"

#define CONNECT_TIMEOUT_MS 1000

#define COOL_LOOP_TIME_CONNECTION_REFUSED 100000
#define COOL_LOOP_TIME_PEER_DOWN 10000

bool bgp_client_peer::send(void* buffer, size_t length){
    if(::send(this->sock, buffer, length, 0) <= 0){
        return false;
    }
    return true;
}

bool try_to_connect(bgp_client_peer* peer){
    peer->server_address.sin_port = getservbyname("bgp", "tcp")->s_port;
    if((peer->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
        log(log_level::ERROR, "Failed to create socket");
        return false;
    }
    if(connect_with_timeout(peer->sock, (struct sockaddr*) &peer->server_address, sizeof(peer->server_address), CONNECT_TIMEOUT_MS) < 0){
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

    // send_open(peer);
    return true;
}

void close_client_peer(bgp_client_peer* peer){
    if(peer->adj_ribs_in != nullptr){ // おそらくalways true
        log(log_level::DEBUG, "Cleaned table sock %d", peer->sock);
        delete_prefix(peer->adj_ribs_in, true); // TODO bgp_loc_ribから経路を削除する
        // peer->adj_ribs_in = nullptr;
    }
    close(peer->sock);
}

bool loop_established(bgp_client_peer* peer){
    int len;
    unsigned char buff[4096];
    //printf("\e[m");
    memset(buff, 0x00, 4096);
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
                abort();
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

            /*
            if(ntohs(bgpopp->my_as) != peer->remote_as){
                send_notification(peer, bgp_error_code::OPEN_MESSAGE_ERROR, bgp_error_sub_code_open::BAD_PEER_AS);
                return false;
            }
            */

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
                        switch(capability_type){
                            case bgp_capability_code::SUPPORT_FOR_4_OCTET_AS_NUMBER_CAPABILITY:
                                peer->is_4_octet_as_supported = true;
                                log(log_level::INFO, "Supported 4-octet AS number!");
                        }
                    }
                        break;
                    default:
                        log(log_level::INFO, "Option type : %d", option_type);
                        break;
                }
            }
            peer->bgp_id = ntohl(bgpopp->bgp_id);

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
        case KEEPALIVE:{
            if(peer->state == OPEN_CONFIRM){
                peer->adj_ribs_in->is_prefix = true;
                peer->adj_ribs_in->prefix = 0;
                peer->adj_ribs_in->prefix_len = 0;
                peer->adj_ribs_in->data = nullptr;
                peer->adj_ribs_in->parent = nullptr;
                peer->adj_ribs_in->node_0 = nullptr;
                peer->adj_ribs_in->node_1 = nullptr;
                peer->state = ESTABLISHED;
            }
            log(log_level::INFO, "Keepalive Received");
            bgp_header header;
            memset(header.maker, 0xff, 16);
            header.length = htons(19);
            header.type = KEEPALIVE;
            if(!peer->send(&header, len)){
                log(log_level::ERROR, "Failed to send packet");
                return false;
            }
        }
            break;
        default:
            log(log_level::ERROR, "Unknown type received %d", bgphp->type);
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
                    peer->connect_cool_time = COOL_LOOP_TIME_CONNECTION_REFUSED;
                }
                peer->connect_cool_time = COOL_LOOP_TIME_PEER_DOWN;
            }else{
                peer->connect_cool_time--;
            }
            break;
        case OPEN_CONFIRM:
        case ESTABLISHED:
            if(!loop_established(peer)){
                peer->state = IDLE;
                close_client_peer(peer);
            }
            break;
    }
    return true;
}