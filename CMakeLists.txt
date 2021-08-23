cmake_minimum_required(VERSION 3.16)
project(my_awesome_bgp)

set(CMAKE_CXX_STANDARD 17)

add_executable(my_awesome_bgp main.cpp logger.h bgp_client.cpp bgp_client.h)
