#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "super_rip.h"

// Dummy function to check test suite
int check_test_function(int number)
{
    return ++number;
}

rip_packet_t *get_hc_rip_packet()
{
    rip_network_t rip_network_1;
    rip_packet_t *rip_update;

    memset(&rip_network_1, 0, sizeof(rip_network_1));
    rip_update = calloc(1, sizeof(*rip_update));
    
    rip_network_1.ip_family = 0x0002;      // IPv4
    rip_network_1.ip_family = htons(rip_network_1.ip_family);
    rip_network_1.ip_address = 0x0a000000; // 10.0.0.0
    rip_network_1.ip_address = htonl(rip_network_1.ip_address);
    rip_network_1.metric = 0x00000001;         // 1
    rip_network_1.metric = htonl(rip_network_1.metric);

    rip_update->command = 0x02;             // Response
    rip_update->version = 0x01;             // v1
    
    rip_update->rip_network = rip_network_1;

    return rip_update;
}

rip_packet_t *get_rip_packet_from_network(char *ip_str, uint32_t metric)
{
    rip_network_t rip_network_1;
    rip_packet_t *rip_update;

    memset(&rip_network_1, 0, sizeof(*rip_update));
    rip_update = calloc(1, sizeof(*rip_update));
 
    // Populate hardcoded values
    rip_network_1.ip_family = 0x0002;
    rip_network_1.ip_family = htons(rip_network_1.ip_family);
    rip_update->command = 0x02;
    rip_update->version = 0x01;

    rip_network_1.metric = htonl(metric);

    if (inet_pton(AF_INET, ip_str, &(rip_network_1.ip_address)) <= 0) {
        perror("Error converting address");
        exit(1);
    }

    rip_update->rip_network = rip_network_1;
    return rip_update;

}

int start_super_rip ()
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    struct addrinfo *bc_info;
    int broadcast = 1;
    int rv;
    char ipstr[INET6_ADDRSTRLEN];

    // Set hints to get host addr info
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, RIP_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "gettaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    
    // Set hints to get broadcast addr info
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(BROADCAST_ADDRESS, RIP_PORT, &hints, &bc_info)) != 0) {
        fprintf(stderr, "bc_getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through results and bind to first
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof broadcast) == -1) {
            perror("setsockopt (SO_BROADCAST)");
            exit(1);
        }
        break;
    }

    freeaddrinfo(servinfo);

    char buf[24];
    int numbytes;
//    rip_packet_t *rip_packet = get_hc_rip_packet();
    rip_packet_t *rip_packet = get_rip_packet_from_network("172.16.0.0", 5);

    memcpy(buf, rip_packet, sizeof(*rip_packet));

    for (;;) {
        if ((numbytes = sendto(sockfd, buf, sizeof(buf), 0, bc_info->ai_addr, bc_info->ai_addrlen)) == -1) {
            perror("sendto");
            exit(1);
        }
        struct sockaddr_in *ipv4 = (struct sockaddr_in*)bc_info->ai_addr;
        inet_ntop(bc_info->ai_family, &ipv4->sin_addr, ipstr, sizeof ipstr);
        printf("sent %d bytes to %s\n", numbytes, ipstr);
        sleep(30);
    }

    return 0;
}