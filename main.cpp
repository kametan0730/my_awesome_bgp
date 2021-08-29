#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <nlohmann/json.hpp>

#include "bgp_client.h"
#include "logger.h"


int main(int argc, char* argv[]){
    int sock;
    fcntl(0, F_SETFL, O_NONBLOCK);
    struct sockaddr_in server_address;

    server_address.sin_family = AF_INET;

    /*
    std::ifstream fin("config.json", std::ios::out);
    if(!fin.is_open()){
        log(log_level::ERROR, "Failed to open config");
        exit(EXIT_FAILURE);
    }
    nlohmann::json j;

    try{
        fin >> j;
    }catch(nlohmann::detail::exception e){
        log(log_level::ERROR, "Failed to load config");
        exit(EXIT_FAILURE);
    }

    int asn = j["asn"];
*/

    log(log_level::INFO, "Hello BGP!!");

    if(inet_aton(argv[1], &server_address.sin_addr) == 0){
        log(log_level::ERROR, "Invalid IP address");
        exit(EXIT_FAILURE);
    }

    server_address.sin_port = htons(179);
    if((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 ){
        log(log_level::ERROR, "Failed to create socket");
        exit(EXIT_FAILURE);
    }

    if(connect(sock, (struct sockaddr*) &server_address, sizeof(server_address)) < 0){
        log(log_level::ERROR, "Failed to connect server");
        exit(EXIT_FAILURE);
    }
    log(log_level::INFO, "Connected to %s", inet_ntoa(server_address.sin_addr));
    while(true){
        if(getchar() == 'q'){
            close(sock);
            log(log_level::INFO, "Good bye");
            break;
        }
        if(!bgp_client_loop(sock)){
            break;
        }
    }
    return EXIT_SUCCESS;
}