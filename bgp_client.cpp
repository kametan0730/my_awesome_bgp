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
            log(log_level::INFO, "\e[31mOpen Received");
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
            open.my_as = htons(1);
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
            log(log_level::INFO, "\e[32mUpdate received");
            uint16_t unfeasible_routes_length;
            memcpy(&unfeasible_routes_length, &buff[19], 2);
            unfeasible_routes_length = ntohs(unfeasible_routes_length);
            log(log_level::DEBUG, "%d", unfeasible_routes_length);
            uint16_t total_path_attribute_length;
            memcpy(&total_path_attribute_length, &buff[19+2+unfeasible_routes_length], 2);
            total_path_attribute_length = ntohs(total_path_attribute_length);
            log(log_level::DEBUG, "%d", total_path_attribute_length);
            break;
        case NOTIFICATION:
        {
            log(log_level::INFO, "\e[34mNotification received");
            auto* bgpntp = reinterpret_cast<bgp_notification*>(buff);
            log(log_level::INFO, "Error: %d", bgpntp->error);
            log(log_level::INFO, "Sub: %d", bgpntp->error_sub);
        }
        case KEEPALIVE:
            log(log_level::INFO, "\e[35mKeepalive Received");
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
            log(log_level::WARNING, "\e[7mUnknown type received %d", bgphp->type);
            break;
    }
    return true;
}