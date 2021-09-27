#ifndef MY_AWESOME_BGP_BGP_H
#define MY_AWESOME_BGP_BGP_H

#include "tree.h"

extern uint32_t my_as;
extern uint32_t router_id;

struct adj_ribs_in_data; // from bgp_rib.h

class bgp_peer{
public:
    uint8_t state = 0;
    bool is_4_octet_as_supported = false;
    node<adj_ribs_in_data>* adj_ribs_in = nullptr;
    uint32_t bgp_id = 0;
    uint32_t route_count = 0;
    virtual bool send(void* buffer, size_t length) = 0;
};

enum bgp_peer_state{
    IDLE,
    CONNECT,
    ACTIVE,
    OPEN_SENT,
    OPEN_CONFIRM,
    ESTABLISHED,
};

enum afi{
    IPV4 = 1,
    IPV6 = 2,
};

enum safi{
    UNICAST = 1,
    MULTICAST = 2,
};

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
    AS4_AGGREGATOR = 18,
};

enum bgp_path_attribute_origin{
    IGP = 0,
    EGP = 1,
    INCOMPLETE = 2
};

enum bgp_error_code{
    MESSAGE_HEADER_ERROR = 1,
    OPEN_MESSAGE_ERROR = 2,
    UPDATE_MESSAGE_ERROR = 3,
    HOLD_TIMER_EXPIRED = 4,
    FINITE_STATE_MACHINE_ERROR = 5,
    CEASE = 6,
    ROUTE_REFRESH_MESSAGE_ERROR = 7
};

enum bgp_error_sub_code_open{
    UNSPESIFIC = 0,
    UNSUPPORTED_VERSION_NUMBER = 1,
    BAD_PEER_AS = 2,
    // ...

};

enum bgp_open_optional_parameter_type{
    AUTHENTICATION = 1,
    CAPABILITIES = 2
};

enum bgp_capability_code{
    MULTIPROTOCOL_EXTENSION_FOR_BGP4 = 1,
    ROUTE_REFRESH_CAPABILITY_FOR_BGP4 = 2,
    OUTBOUND_ROUTE_FILTERING_CAPABILITY = 3,
    SUPPORT_FOR_4_OCTET_AS_NUMBER_CAPABILITY = 65
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
    unsigned char option[];
} __attribute__((packed));

struct bgp_notification{
    bgp_header header;
    uint8_t error;
    uint8_t error_sub;
    unsigned char data[];
} __attribute__((packed));

struct bgp_path_attribute_common{
    uint8_t flag;
    uint8_t type;

} __attribute__((packed));

struct attributes;

void hex_dump(unsigned char* buffer, int len, bool is_separate = false);

bool send_update_with_nlri(bgp_peer* peer, attributes* attr, uint32_t prefix, uint8_t prefix_len);
bool send_notification(bgp_peer* peer, int8_t error, uint16_t error_sub);
bool send_open(bgp_peer* peer);

bool bgp_update(bgp_peer* peer,  unsigned char* buff, int entire_length);

#endif //MY_AWESOME_BGP_BGP_H
