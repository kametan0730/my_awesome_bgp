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

std::vector<bgp_client_peer> peers;

void set_nonblocking(int sockfd)
{
    int val;
    val = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, val | O_NONBLOCK);
}


int main(){
    log(log_level::INFO, "Hello BGP!!");
    fcntl(0, F_SETFL, O_NONBLOCK);

    std::ifstream conf_file("config.json", std::ios::in);
    if(!conf_file.is_open()){
        log(log_level::ERROR, "Failed to open config");
        exit(EXIT_FAILURE);
    }
    nlohmann::json conf_json;

    try{
        conf_file >> conf_json;
    }catch(nlohmann::detail::exception e){
        log(log_level::ERROR, "Failed to load config");
        exit(EXIT_FAILURE);
    }
    conf_file.close();

    log(log_level::ERROR, "Succeed to load config");
    //conf_json.at("asn").get<int>();
    log(INFO, "My AS: %d", conf_json.at("asn").get<int>());

    for (auto& neighbor : conf_json.at("neighbors")) {
        bgp_client_peer peer{
            .sock = 0,
            .state = ACTIVE,
            .remote_as = neighbor.at("remote-as")
        };
        peer.server_address.sin_family = AF_INET;

        if(inet_aton(neighbor.at("address").get<std::string>().c_str(), &peer.server_address.sin_addr) == 0){
            log(log_level::ERROR, "Invalid IP address");
            exit(EXIT_FAILURE);
        }
        peers.push_back(peer);
    }

    for (auto & peer : peers) {
        peer.server_address.sin_port = htons(179);
        if((peer.sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 ){
            log(log_level::ERROR, "Failed to create socket");
            exit(EXIT_FAILURE);
        }
        if(connect(peer.sock, (struct sockaddr*) &peer.server_address, sizeof(peer.server_address)) < 0){
            log(log_level::ERROR, "Failed to connect server");
            exit(EXIT_FAILURE);
        }
        set_nonblocking(peer.sock);
        log(log_level::INFO, "Connected to %s", inet_ntoa(peer.server_address.sin_addr));
    }

    while(true){
        if(getchar() == 'q'){
            log(log_level::INFO, "Good bye");
            break;
        }
        for (auto & peer : peers) {
            if(!bgp_client_loop(peer)){
                break;
            }
        }
    }
    return EXIT_SUCCESS;
}