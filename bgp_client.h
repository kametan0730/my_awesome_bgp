#ifndef MY_AWESOME_BGP_BGP_CLIENT_H
#define MY_AWESOME_BGP_BGP_CLIENT_H

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
struct bgp_peer{
    int sock;
    int state;
};
*/

// https://www.iana.org/assignments/bgp-parameters/bgp-parameters.xhtml

enum bgp_message_type{
    OPEN = 1,
    UPDATE = 2,
    NOTIFICATION = 3,
    KEEPALIVE = 4
};

enum bgp_path_attribute_type{
    ORIGIN = 1,
    AS_PATH = 2,
    NEXT_HOP = 3,
    MULTI_EXIT_DISC = 4,
    LOCAL_PREF = 5,
    ATOMIC_AGGREGATE = 6,
    AGGREGATOR = 7,
    COMMUNITY = 8,
    ORIGINATOR_ID = 9,
    CLUSTER_LIST = 10,
    DPA = 11,
    ADVERTISER = 12,
    RCID_PATH = 13 ,
    CLUSTER_ID = 13,
    MP_REACH_NLRI = 14,
    MP_UNREACH_NLRI = 15,
    EXTENDED = 16,
    AS4_PATH = 17,
};

enum bgp_path_attribute_flag{
    EXTENDED_LENGTH = 1 << 4,
    PARTIAL = 1 << 5,
    TRANSITIVE = 1 << 6,
    OPTIONAL = 1 << 7
};

enum bgp_path_attribute_as_path_segment_type{
    AS_SET = 1,
    AS_SEQUENCE = 2,
    AS_CONFED_SEQUENCE = 3,
    AS_CONFED_SET = 4
};

struct bgp_header{
    unsigned char maker[16];
    uint16_t length;
    uint8_t type;
} __attribute__((packed));

struct bgp_open{
    bgp_header header;
    uint8_t version;
    uint16_t my_as;
    uint16_t hold_time;
    uint32_t bgp_id;
    uint8_t opt_length;
} __attribute__((packed));

struct bgp_notification{
    bgp_header header;
    uint8_t error;
    uint16_t error_sub;
    char data[];
} __attribute__((packed));

bool bgp_client_loop(int sock);

#endif //MY_AWESOME_BGP_BGP_CLIENT_H
