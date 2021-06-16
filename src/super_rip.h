#ifndef __SUPER_RIP_H__
#define __SUPER_RIP_H__

#include <stdint.h>

#define RIP_PORT "520"
#define BROADCAST_ADDRESS "192.168.232.200"

typedef struct rip_network {
    uint16_t ip_family;
    uint16_t padding_1;
    uint32_t ip_address;
    uint32_t padding_2;
    uint32_t padding_3;
    uint32_t metric;
} rip_network_t;

typedef struct rip_packet {
    uint8_t command;
    uint8_t version;
    uint16_t padding_1;
    rip_network_t rip_networks[25];
} rip_packet_t;

typedef struct rip_header {
    uint8_t command;
    uint8_t version;
    uint16_t padding_1;
} rip_header_t;


int check_test_function(int number);
rip_network_t get_rip_network(char *ip_str, uint32_t metric);
int build_rip_packet(rip_network_t *rip_networks, int num_networks, char **buf);
int start_super_rip ();

#endif