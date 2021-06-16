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


rip_network_t get_rip_network(char *ip_str, uint32_t metric)
{
    rip_network_t rip_network_1;

    memset(&rip_network_1, 0, sizeof(rip_network_t));
 
    // Populate hardcoded values
    rip_network_1.ip_family = 0x0002;
    rip_network_1.ip_family = htons(rip_network_1.ip_family);
    rip_network_1.metric = htonl(metric);

    if (inet_pton(AF_INET, ip_str, &(rip_network_1.ip_address)) <= 0) {
        perror("Error converting address");
        exit(1);
    }
    return rip_network_1;
}


int build_rip_packet(rip_network_t *rip_networks, int num_networks, char **buf)
{
    printf("net: %08x\n", rip_networks[1].ip_address);
    rip_header_t rip_header;
    memset(&rip_header, 0, sizeof(rip_header));
    rip_header.command = 0x02;
    rip_header.version = 0x01;

    *buf = malloc(sizeof(rip_header_t) + num_networks * sizeof(rip_network_t));
    memcpy(*buf, &rip_header, sizeof(rip_header));
    int offset = (int)sizeof(rip_header_t);
    for (int i=0; i<num_networks; ++i) {
        memcpy(*buf + offset, &rip_networks[i], sizeof(rip_network_t));
        offset += sizeof(rip_network_t);
    }
    return 1;
    
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

    char *buf;
    int numbytes;
    rip_network_t *rip_networks = calloc(2, sizeof(rip_network_t));
    int num_networks = 2;
    rip_networks[0] = get_rip_network("172.25.0.0", 7);
    rip_networks[1] = get_rip_network("10.0.0.0", 4);

    if (build_rip_packet(rip_networks, num_networks, &buf) != 1) {
        perror("Error building RIP packet");
        exit(1);
    }


    for (;;) {
        if ((numbytes = sendto(sockfd, buf, 44, 0, bc_info->ai_addr, bc_info->ai_addrlen)) == -1) {
            perror("sendto");
            exit(1);
        }
        struct sockaddr_in *ipv4 = (struct sockaddr_in*)bc_info->ai_addr;
        inet_ntop(bc_info->ai_family, &ipv4->sin_addr, ipstr, sizeof ipstr);
        printf("sent %d bytes to %s\n", numbytes, ipstr);
        sleep(30);
    }
    free(*buf);

    return 0;
}