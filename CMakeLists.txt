project(my_awesome_bgp)

cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Populate(
        json
        URL  https://github.com/nlohmann/json/releases/download/v3.10.2/include.zip
)

include_directories(${json_SOURCE_DIR}/single_include)

add_executable(my_awesome_bgp main.cpp logger.h bgp_client.cpp bgp_client.h tcp_socket.cpp tcp_socket.h tree.cpp tree.h command.cpp command.h bgp.cpp bgp.h)
