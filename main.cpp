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
    log(log_level::INFO, "Router-ID: %s", inet_ntoa(in_addr{.s_addr = router_id}));

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
        peer.connect_cool_loop_time = 0;
        peers.push_back(peer);
    }

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
                        for(int i = 0; i < 1000; ++i){
                            getchar(); // 連続入力の対策はしているが、もしかすると連続する文字列がcから始まるかもしれない. その場合、対策をすり抜けてしまうのでへんなコマンドが実行されないようにここでバッファをクリアする
                        }
                        printf("Switched to command mode\n");
                        printf("> ");
                    }
                }
                is_input_continuous = true;
            }
        }else{
            static std::string cmd_str;
            if(input != -1){
                if(input >= 0x20 and input <= 0x7e){
                    cmd_str.push_back(input);
                }else if(input == 0x0a){
                    if(cmd_str.empty()){
                    }else if(cmd_str == "exit"){
                        console_mode = 0;
                        printf("Switched to log mode\n");
                    }else if(cmd_str == "break"){
                        raise(SIGINT);
                    }else if(cmd_str == "uptime"){
                        now = std::chrono::system_clock::now();
                        console("Uptime %d seconds", std::chrono::duration_cast<std::chrono::seconds>(now-up).count());
                    }else if(cmd_str == "shutdown"){
                        console("Good bye");
                        break;
                    }else if(cmd_str == "test"){
                        for(int i = 0; i < peers.size(); ++i){
                            for(auto &network: conf_json.at("networks")){
                                in_addr address = {};
                                if(inet_aton(network.at("prefix").get<std::string>().c_str(), &address) == 0){
                                    log(log_level::ERROR, "Invalid IP address in config");
                                    exit(EXIT_FAILURE);
                                }
                                attributes a;
                                a.origin = IGP;
                                a.as_path_length = 0;
                                a.next_hop = inet_addr("172.16.3.0");
                                a.med = 100;
                                a.local_pref = 0;
                                send_update_with_nlri(&peers[i], &a, address.s_addr, network.at("prefix-length"));
                            }
                        }
                    }else{
                        command_result_status command_result = execute_command(cmd_str);
                        if(command_result == command_result_status::NOT_FOUND){
                            console("Command not found");
                        }else if(command_result == command_result_status::INVALID_PARAMS){
                            console("Invalid parameters");
                        }
                    }
                    cmd_str.clear();
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