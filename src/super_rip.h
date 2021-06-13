#ifndef __SUPER_RIP_H__
#define __SUPER_RIP_H__

#include <stdint.h>

#define RIP_PORT "520"
#define BROADCAST_ADDRESS "192.168.232.200"

typedef struct rip_network rip_network_t;
typedef struct rip_packet rip_packet_t;

int check_test_function(int number);
rip_packet_t *get_hc_rip_packet();
int start_super_rip ();

#endif