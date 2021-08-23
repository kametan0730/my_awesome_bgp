#include "bgp_client.h"
#include "logger.h"

void hex_dump(unsigned char* buffer, int len){
    for (int i = 0; i < len; ++i) {
        printf("%02x", buffer[i]);
    }
    printf("\n");
}

bool bgp_client_loop(int sock){
    int len;
    unsigned char buff[100000];
    printf("\e[m");
    memset(buff, 0x00, 100000);
    len = recv(sock, &buff, 19, 0);
    if(len == 0){
        return true;
    }
    auto* bgphp = reinterpret_cast<bgp_header*>(buff);

    int entire_length = htons(bgphp->length);
    log(log_level::DEBUG, "Receiving %d bytes", entire_length);

    int append_len, remain_byte;
    while(len < entire_length){
        remain_byte = entire_length - len;
        log(log_level::DEBUG, "%d bytes remain", remain_byte);
        append_len = recv(sock, &buff[len], std::min(remain_byte, 1000), 0);
        log(log_level::DEBUG, "New %d bytes received", append_len);
        len += append_len;
    }

    switch(bgphp->type){
        case OPEN:
        {
            printf("\e[31m");
            log(log_level::INFO, "Open Received");
            auto* bgpopp = reinterpret_cast<bgp_open*>(buff);
            log(log_level::INFO, "Version: %d", bgpopp->version);
            log(log_level::INFO, "My AS: %d", ntohs(bgpopp->my_as));
            log(log_level::INFO, "Hold Time: %d", ntohs(bgpopp->hold_time));
            log(log_level::INFO, "BGP Id: %d", ntohl(bgpopp->bgp_id));
            log(log_level::INFO, "Opt Length: %d", bgpopp->opt_length);

            bgp_open open;
            memset(open.header.maker, 0xff, 16);
            open.header.length = htons(29);
            open.header.type = OPEN;
            open.version = 4;
            open.my_as = htons(65017);
            open.hold_time = htons(180);
            open.bgp_id = htons(11111);
            open.opt_length = 0;

            if (send(sock, &open, 29, 0) <= 0) {
                log(log_level::ERROR, "Failed to send packet");
                return false;
            }
        }
            break;
        case UPDATE:
        {
            printf("\e[32m");
            log(log_level::INFO, "Update received");
            uint16_t unfeasible_routes_length;
            memcpy(&unfeasible_routes_length, &buff[19], 2);
            unfeasible_routes_length = ntohs(unfeasible_routes_length);
            log(log_level::DEBUG, "%d", unfeasible_routes_length);
            uint16_t total_path_attribute_length;
            memcpy(&total_path_attribute_length, &buff[19+2+unfeasible_routes_length], 2);
            total_path_attribute_length = ntohs(total_path_attribute_length);
            log(log_level::DEBUG, "%d", total_path_attribute_length);
            int read_length = 19+2+unfeasible_routes_length + 2;
            while(read_length < 19+2+unfeasible_routes_length + 2 + total_path_attribute_length){
                uint8_t flag = buff[read_length];
                uint8_t type = buff[read_length+1];
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
                    case ORIGIN:
                    {
                        uint8_t origin = buff[read_length];
                        log(log_level::INFO, "Origin %d", origin);
                    }
                        break;
                    case AS_PATH:
                    {
                        uint8_t segment_type = buff[read_length];
                        uint8_t segment_length = buff[read_length+1];
                        log(log_level::INFO, "Seg type %d", segment_type);
                        read_length += 2;
                        hex_dump(&buff[read_length], 40);
                        for(int i = 0; i < segment_length; i++){
                            uint16_t asn;
                            memcpy(&asn, &buff[read_length+i*2], 2);
                            asn = ntohs(asn);
                            log(log_level::INFO, "AS Path %d", asn);
                        }
                        //hex_dump(&buff[read_length], attribute_len);
                    }
                        break;
                }
                read_length += attribute_len;
            }
            read_length = 19+2+unfeasible_routes_length+2+total_path_attribute_length;
            while(read_length < entire_length){
                int prefix = buff[read_length];
                if(prefix <= 8){
                    log(log_level::DEBUG, "%d.0.0.0/%d", buff[read_length+1], prefix);
                    read_length += 2;
                }else if(prefix <= 16){
                    log(log_level::DEBUG, "%d.%d.0.0/%d", buff[read_length+1], buff[read_length+2], prefix);
                    read_length += 3;
                }else if(prefix <= 24){
                    log(log_level::DEBUG, "%d.%d.%d.0/%d", buff[read_length+1], buff[read_length+2], buff[read_length+3], prefix);
                    read_length += 4;
                }else if(prefix <= 32){
                    log(log_level::DEBUG, "%d.%d.%d.%d/%d", buff[read_length+1], buff[read_length+2], buff[read_length+3], buff[read_length+4], prefix);

                    read_length += 5;
                }else{
                    log(log_level::ERROR, "Invalid packet");
                }
            }
        }
            break;
        case NOTIFICATION:
        {
            printf("\e[34m");
            log(log_level::INFO, "Notification received");
            auto* bgpntp = reinterpret_cast<bgp_notification*>(buff);
            log(log_level::INFO, "Error: %d", bgpntp->error);
            log(log_level::INFO, "Sub: %d", bgpntp->error_sub);
        }
        case KEEPALIVE:
            printf("\e[35m");
            log(log_level::INFO, "Keepalive Received");
            bgp_header header;
            memset(header.maker, 0xff, 16);
            header.length = htons(19);
            header.type = KEEPALIVE;
            if (send(sock, &header, len, 0) <= 0) {
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