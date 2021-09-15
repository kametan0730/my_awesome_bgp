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
#include <poll.h>
#include <csignal>
#include <nlohmann/json.hpp>

#include "bgp_client.h"
#include "logger.h"

#define CONFIG_PATH "../config.json"

std::vector<bgp_client_peer> peers;

uint32_t my_as;
uint8_t log_id;

void signal_handler(int sig){
}

int main(){

    struct sigaction act, old_act;

    act.sa_handler = signal_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    act.sa_flags |= SA_INTERRUPT;

    if(sigaction(SIGALRM, &act, &old_act) < 0){
        fprintf(stderr, "Error registering signal disposition\n");
        exit(1);
    }
    log_id = 0;
    log(log_level::INFO, "Hello BGP!!");
    fcntl(0, F_SETFL, O_NONBLOCK);

    std::ifstream conf_file(CONFIG_PATH, std::ios::in);
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

    log(log_level::INFO, "Succeed to load config");

    my_as = conf_json.at("my_as").get<int>();
    log(INFO, "My AS: %d", my_as);

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
        peer.state = IDLE;
        peers.push_back(peer);
    }

    while(true){
        if(getchar() == 'q'){
            log(log_level::INFO, "Good bye");
            break;
        }

        for (int i = 0; i < peers.size(); ++i) {
            log_id = i + 1;
            if(!bgp_client_loop(&peers[i])){
                break;
            }
        }
    }
    return EXIT_SUCCESS;
}