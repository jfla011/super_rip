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
#include <pthread.h>

#include "super_rip.h"

typedef struct {
    int num_networks; 
    rip_network_t *rip_networks;
} thread_config_t;

typedef enum {
    WAITING_FOR_UPDATE,
    UPDATE_RIP_DATABASE,
} DatabaseState;

DatabaseState db_state = UPDATE_RIP_DATABASE;


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


void* advertise_rip_routes(void *arg)
{
    fprintf(stdout, "Starting rip advertise thread.\n");
    thread_config_t *rip_database = (thread_config_t*)arg;
    rip_network_t *rip_networks = rip_database->rip_networks;
    int num_networks = rip_database->num_networks;
    int update_size = (int)(sizeof(rip_network_t) * num_networks + sizeof(rip_header_t));

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
        exit(1);
    }
    
    // Set hints to get broadcast addr info
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(BROADCAST_ADDRESS, RIP_PORT, &hints, &bc_info)) != 0) {
        fprintf(stderr, "bc_getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
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

    if (build_rip_packet(rip_networks, num_networks, &buf) != 1) {
        perror("Error building RIP packet");
        exit(1);
    }


    while (db_state == WAITING_FOR_UPDATE) {
        if ((numbytes = sendto(sockfd, buf, update_size, 0, bc_info->ai_addr, bc_info->ai_addrlen)) == -1) {
            perror("sendto");
            exit(1);
        }
        struct sockaddr_in *ipv4 = (struct sockaddr_in*)bc_info->ai_addr;
        inet_ntop(bc_info->ai_family, &ipv4->sin_addr, ipstr, sizeof ipstr);
        printf("sent %d bytes to %s\n", numbytes, ipstr);
        sleep(30);
    }
    free(buf);
    fprintf(stdout, "Exiting rip advertise thread.\n");

    return 0;
}


int parse_command(char *command)
{
    /*
    * RETURN VALUES:
    *    0: Command recognised successfully
    *    1: Invalid command
    */
    char *new_str;
    strcpy(new_str, command);
    const char *delim = " ";
    char *next;

    if (strcmp(new_str, "quit") == 0) {
        fprintf(stdout, "Received exit symbol, shutting down.");
        exit(0);
    } else if (strcmp(strtok(new_str, delim), "ip") == 0) {
        if (strcmp(strtok(new_str, delim), "rip") == 0) {
            fprintf(stdout, "%s\n", new_str);
            return 0;
        }

    }

    fprintf(stdout, "Invalid command received.");
    return 1;
}

int start_super_rip ()
{
    fprintf(stdout, "Starting super_rip.\n");
    pthread_t rip_advertise_thread;
    
    rip_network_t *new_rip_networks;
    rip_network_t *rip_networks = calloc(2, sizeof(rip_network_t));
    rip_networks[0] = get_rip_network("172.25.0.0", 7);
    rip_networks[1] = get_rip_network("10.0.0.0", 4);
    
    thread_config_t *rip_database = (thread_config_t*)malloc(sizeof(*rip_database));
    if (!rip_database) {
        perror("OOM");
        exit(1);
    }
    rip_database->num_networks = 2;
    rip_database->rip_networks = rip_networks;
//    free(rip_networks);

    char command[128];
    memset(command, 0, sizeof command);
    for (;;) {
        if (db_state == WAITING_FOR_UPDATE) {
            fgets(command, 128, stdin);
            fprintf(stdout, "Received: %s\n", command);
            if (strcmp(command, "quit\n") == 0) {
                fprintf(stdout, "Shutting down super_rip.\n");
                break;
            } else if (strcmp(command, "add rip network 203.98.111.0 metric 6\n") == 0) {
                fprintf(stdout, "Adding route to database\n");
                new_rip_networks = calloc(rip_database->num_networks+1, sizeof(rip_network_t));
                for (int i=0; i<rip_database->num_networks; ++i) {
                    new_rip_networks[i] = rip_database->rip_networks[i];
                }
                new_rip_networks[rip_database->num_networks] = get_rip_network("203.98.111.0", 6);
                rip_database->num_networks ++;
                rip_networks = new_rip_networks;
//                free(new_rip_networks);
                rip_database->rip_networks = rip_networks;
                db_state = UPDATE_RIP_DATABASE;
                pthread_join(rip_advertise_thread, NULL);
            }
        } else if (db_state == UPDATE_RIP_DATABASE) {
            db_state = WAITING_FOR_UPDATE;
            pthread_create(&rip_advertise_thread, NULL, advertise_rip_routes, rip_database);
            
        }

    }
    free(rip_database);
    pthread_detach(rip_advertise_thread);
    return 0;

}