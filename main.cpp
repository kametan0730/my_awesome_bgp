#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <nlohmann/json.hpp>

#include "bgp.h"
#include "bgp_client.h"
#include "bgp_rib.h"
#include "command.h"
#include "logger.h"
#include "tree.h"

#define CONFIG_PATH "../config.json"
#define LOOP_MINIMUM_US 100 // 0.0001秒

std::vector<bgp_client_peer> peers;

uint32_t my_as;
uint32_t router_id;
uint8_t log_id;
uint8_t console_mode = 0;
node<loc_rib_data>* bgp_loc_rib;

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
        exit(EXIT_FAILURE);
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

    router_id = inet_addr(conf_json.at("router-id").get<std::string>().c_str());
    log(log_level::INFO, "Router-ID: %d", router_id);

    bgp_loc_rib = (node<loc_rib_data>*) malloc(sizeof(node<loc_rib_data>));
    if(bgp_loc_rib == nullptr){
        log(log_level::ERROR, "Failed to allocate memory for loc_rib root");
        exit(EXIT_FAILURE);
    }
    bgp_loc_rib->is_prefix = false;
    bgp_loc_rib->prefix = 0;
    bgp_loc_rib->prefix_len = 0;
    bgp_loc_rib->data = nullptr;
    bgp_loc_rib->parent = nullptr;
    bgp_loc_rib->node_0 = nullptr;
    bgp_loc_rib->node_1 = nullptr;

    for(auto &neighbor: conf_json.at("neighbors")){
        bgp_client_peer peer;
        peer.sock = 0;
        peer.remote_as = neighbor.at("remote-as");
        peer.state = ACTIVE;
        peer.server_address.sin_family = AF_INET;

        if(inet_aton(neighbor.at("address").get<std::string>().c_str(), &peer.server_address.sin_addr) == 0){
            log(log_level::ERROR, "Invalid IP address in config");
            exit(EXIT_FAILURE);
        }
        peer.state = IDLE;

        auto* root = (node<adj_ribs_in_data>*) malloc(sizeof(node<adj_ribs_in_data>));
        if(root == nullptr){
            log(log_level::ERROR, "Failed to allocate memory for adj_ribs_in root");
            exit(EXIT_FAILURE);
        }
        root->is_prefix = false;
        root->parent = nullptr;
        root->node_0 = nullptr;
        root->node_1 = nullptr;
        peer.adj_ribs_in = root;
        peer.connect_cool_time = 0;
        peers.push_back(peer);
    }

    /*
    for(auto &network: conf_json.at("networks")){
        loc_rib_data data{
                .peer = &peers[0],
                .path_attr = {
                        .origin = 0,
                        .next_hop = 0,
                        .med = 0,
                        .local_pref = 200
                }
        };
        in_addr address = {};
        if(inet_aton(network.at("prefix").get<std::string>().c_str(), &address) == 0){
            log(log_level::ERROR, "Invalid IP address in config");
            exit(EXIT_FAILURE);
        }
        add_prefix(bgp_loc_rib, ntohl(address.s_addr), network.at("prefix-length"), data);
    }
    */

    std::chrono::system_clock::time_point start, now;
    uint64_t real_time;
    bool is_input_continuous = false; // ログモードの時に長いテキストを間違えてペーストしてその中にcやbが含まれていると止まってしまうので、連続で入力された場合は無視するために前回のループで入力があったかを保持する
    while(true){
        start = std::chrono::system_clock::now();
        char input = getchar(); // TODO charの範囲外かもしれない
        if(console_mode == 0){
            if(input == -1){
                is_input_continuous = false;
            }else{
                if(!is_input_continuous){
                    if(input == 'q'){ // TODO ログモードからのプログラム終了は廃止したい
                        log(log_level::INFO, "Good bye");
                        break;
                    }else if(input == 'b'){
                        raise(SIGINT);
                    }else if(input == 'c'){
                        console_mode = 1;
                        //for(int i = 0; i < 1000; ++i){
                        //    getchar(); // 連続入力の対策はしているが、もしかすると連続する文字列がcから始まるかもしれない. その場合、対策をすり抜けてしまうのでへんなコマンドが実行されないようにここでバッファをクリアする
                        //}
                        printf("Switched to command mode\n");
                    }
                }
                is_input_continuous = true;
            }
        }else{
            static char command[256];
            static uint8_t offset = 0;
            if(input != -1){
                if(input >= 0x20 and input <= 0x7e){
                    if(offset <= 250){
                        command[offset] = input;
                        command[++offset] = '\0';
                    }
                }else if(input == 0x0a){
                    if(command[0] == '\0'){
                    }else if(strcmp(command, "exit") == 0){
                        console_mode = 0;
                        printf("Switched to log mode\n");
                    }else if(strcmp(command, "break") == 0){
                        raise(SIGINT);
                    }else if(strcmp(command, "uptime") == 0){
                        now = std::chrono::system_clock::now();
                        console("Uptime %d seconds", std::chrono::duration_cast<std::chrono::seconds>(now-up).count());
                    }else if(strcmp(command, "shutdown") == 0){
                        console("Good bye");
                        break;
                    }else{
                        if(!execute_command(command)){
                            console("Command not found");
                        }
                    }
                    memset(command, 0, 255);
                    offset = 0;
                    if(console_mode == 1){ // Exitされていないなら
                        printf("> ");
                    }

                }
            }
        }

        for(int i = 0; i < peers.size(); ++i){
            log_id = i + 1;
            if(!bgp_client_loop(&peers[i])){
            }
        }
        log_id = 0;

        now = std::chrono::system_clock::now();
        real_time = std::chrono::duration_cast<std::chrono::microseconds>(now-start).count();
        if(LOOP_MINIMUM_US > real_time){ // もしこのループにかかった時間がLOOP_MINIMUM_US未満なら
            usleep(LOOP_MINIMUM_US - real_time); //　LOOP_MINIMUM_USに満たない時間分ループが終わるのを待つ
        }
    }

    for(auto & peer : peers){
        close_client_peer(&peer);
    }
    log(log_level::INFO, "Closed all peers");

    for(auto & peer : peers){
        free(peer.adj_ribs_in); // root自体はまだ解放されていないのでここで
    }
    delete_prefix(bgp_loc_rib);
    printf("Safety exited\n");
    return EXIT_SUCCESS;
}