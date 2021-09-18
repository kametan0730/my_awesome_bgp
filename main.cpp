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
#include <chrono>
#include <unistd.h>
#include <nlohmann/json.hpp>

#include "bgp_client.h"
#include "command.h"
#include "logger.h"
#include "tree.h"

#define CONFIG_PATH "../config.json"

std::vector<bgp_client_peer> peers;

uint32_t my_as;
uint8_t log_id;
uint8_t console_mode = 0;

void signal_handler(int sig){
}

int main(){
    std::chrono::system_clock::time_point up = std::chrono::system_clock::now();

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
    }catch(nlohmann::detail::exception &e){
        log(log_level::ERROR, "Failed to load config");
        exit(EXIT_FAILURE);
    }
    conf_file.close();

    log(log_level::INFO, "Succeed to load config");

    my_as = conf_json.at("my_as").get<int>();
    log(log_level::INFO, "My AS: %d", my_as);

    for(auto &neighbor: conf_json.at("neighbors")){
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
        node* root = (node*) malloc(sizeof(node));
        root->is_prefix = true;
        root->prefix = 0;
        root->prefix_len = 0;
        root->next_hop = 0;
        root->parent = nullptr;
        root->node_0 = nullptr;
        root->node_1 = nullptr;
        peer.rib = root;
        peer.connect_cool_time = 0;
        peers.push_back(peer);
    }

    std::chrono::system_clock::time_point start, end;
    uint64_t real_time;
    while(true){
        start = std::chrono::system_clock::now();
        char input = getchar();
        if(console_mode == 0){
            if(input == 'q'){
                log(log_level::INFO, "Good bye");
                break;
            }else if(input == 'b'){
                raise(SIGINT);
            }else if(input == 'c'){
                console_mode = 1;
                printf("Switched into command mode\n");
            }
        }else{
            static char command[256];
            static uint8_t offset = 0;
            if(input != -1){
                if(input > 0x20 and input < 0x7e){
                    command[offset] = input;
                    command[++offset] = '\0';
                }else if(input == 0x0a){
                    if(strcmp(command, "exit") == 0){
                        console_mode = 0;
                        printf("Switched into log mode\n");
                    }else if(strcmp(command, "break") == 0){
                        raise(SIGINT);
                    }else{
                        execute_command(command);
                    }
                    memset(command, 0, 255);
                    offset = 0;
                }
            }
        }

        for(int i = 0; i < peers.size(); ++i){
            log_id = i + 1;
            if(!bgp_client_loop(&peers[i])){
            }
        }
        log_id = 0;

        end = std::chrono::system_clock::now();
        real_time = std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
        if(1000 > real_time){ // もしこのループにかかった時間が0.001秒未満なら
            usleep(1000 - real_time); //　0.001秒に満たない時間分ループが終わるのを待つ
        }
    }
    return EXIT_SUCCESS;
}