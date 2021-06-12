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

#define RIP_PORT "520"
#define BROADCAST_ADDRESS "192.168.232.200"

typedef struct 
{
    uint16_t ip_family;
    uint16_t padding_1;
    uint32_t ip_address;
    uint32_t padding_2;
    uint32_t padding_3;
    uint32_t metric;
} rip_network_t;

typedef struct 
{
    uint8_t command;
    uint8_t version;
    uint16_t padding_1;
    rip_network_t rip_network;
} rip_packet_t;



rip_packet_t *get_hc_rip_packet()
{
    rip_network_t rip_network_1;
    rip_packet_t *rip_update;
//    rip_network_t *rip_networks;

    memset(&rip_network_1, 0, sizeof(rip_network_1));
    rip_update = calloc(1, sizeof(*rip_update));
//    memset(&rip_networks, 0, sizeof(rip_network_1));
    
    rip_network_1.ip_family = 0x0200;      // IPv4
    rip_network_1.ip_address = 0x0000000a; // 10.0.0.0
    rip_network_1.metric = 0x01000000;         // 1

//    rip_networks[1] = rip_network_1;

    rip_update->command = 0x02;             // Response
    rip_update->version = 0x01;             // v1
    
    printf("here\n");
    rip_update->rip_network = rip_network_1;

    return rip_update;


}

int main (void)
{
    printf("main\n");



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
//    char* rip_packet_str = (char*)rip_packet;
//    memset(buf, 0, sizeof(buf));
//    memcpy(buf, &rip_packet, sizeof(rip_packet));
//    char *packet = buf;
//    printf("Packet: %024x\n", *buf);

//    rip_packet_t rip_packet = {0x02,0x01,0x0000,{0x0200,0x0000,0x0000000a,0x00000000,0x00000000,0x01000000}};
    rip_packet_t *rip_packet = get_hc_rip_packet();

    printf("ip_family %04x\n", rip_packet->rip_network.ip_family);  // ->
    printf("ip_family %04x\n", 0x0FC0);
    
    memcpy(buf, rip_packet, sizeof(*rip_packet)); // -& *
    printf("rip_packet size: %ld\n", sizeof(*rip_packet)); // *
    printf("buf size: %ld\n", sizeof(buf));
    char *cp = buf;
    for (int i = 0; i < sizeof(buf); ++i) {
        printf("%02x", buf[i]);
    }
    printf("\n");
    printf("buf as string %s\n", buf);
    printf("buf strlen: %lu\n", strlen(buf));
    

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