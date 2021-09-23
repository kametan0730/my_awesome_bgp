#include <cstdio>
#include <netinet/in.h>

#include "tree.h"

bool check_bit(uint32_t addr, uint8_t n){
    return (addr >> (31 - n)) & 0b01;
}

void print_address_binary(uint32_t addr){
    for(int i = 31; i >= 0; --i){
        if(check_bit(addr, i)){
            printf("1");
        }else{
            printf("0");
        }
    }
    printf("\n");
}