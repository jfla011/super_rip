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
#include <regex.h>

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
rip_network_t *rip_networks;
thread_config_t *rip_database;


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


int parse_command(char *command, size_t command_len)
{
    /*
    * RETURN VALUES:
    *   -1: Invalid Command
    *    0: Command recognised successfully
    *    1: Command recognised successfully rip db updated
    *    2: Invalid IP address
    *    3: Could not convert IP with inet_pton()
    */

    regex_t compiled_expression;
    
    if (strcmp(command, "quit\n") == 0) {
        fprintf(stdout, "Shutting down super_rip.\n");
        exit(0);
    }

    char cmd_arr[command_len];
    strcpy(cmd_arr, command);

    char *add_network = "ip rip add network";
    if (strncmp(cmd_arr, add_network, strlen(add_network)) == 0 ) {
        char *ip = strtok(cmd_arr, add_network);
        if (ip != NULL){
            ip[strcspn(ip,"\n")] = 0; // Trim newline character
            char *delim = " \n";
            char *metric_str = strtok(NULL, delim);
            uint32_t metric;
            if (metric_str == NULL) {
                metric = 1; // If no metric supplied set to 1
            } else {
                metric = strtol(metric_str, NULL, 10);
            }
            fprintf(stdout, "metric: %i\n", metric);
            char *reg_ex = "[[:digit:]]{1,3}\\.[[:digit:]]{1,3}\\.[[:digit:]]{1,3}\\.0";
            if (regcomp(&compiled_expression, reg_ex, REG_EXTENDED) != 0) {
                perror("ERROR compiling regex 1");
                exit(1);
            }

            if (regexec(&compiled_expression, ip, 0, NULL, 0) == REG_NOMATCH) {
                fprintf(stdout, "Invalid Network address: %s\n", ip);
                return 2;
            }

            struct sockaddr_in sa;
            if (inet_pton(AF_INET, ip, &(sa.sin_addr)) == 0) {
                fprintf(stdout, "Invalid Network address: %s\n", ip);
                return 3;
            }
            fprintf(stdout, "Adding route to database\n");
            rip_network_t *new_rip_networks;
            new_rip_networks = calloc(rip_database->num_networks+1, sizeof(rip_network_t));
            for (int i=0; i<rip_database->num_networks; ++i) {
                new_rip_networks[i] = rip_database->rip_networks[i];
            }
            new_rip_networks[rip_database->num_networks] = get_rip_network(ip, metric);
            rip_database->num_networks ++;
            rip_networks = new_rip_networks;
            rip_database->rip_networks = rip_networks;
            db_state = UPDATE_RIP_DATABASE;


            return 1;
        }
    }
    char *show_network = "show ip rip networks";
    if (strncmp(cmd_arr, show_network, strlen(show_network)) == 0 ) {
        for (int i=0; i<rip_database->num_networks; ++i) {
            struct sockaddr_in ip_addr;
            ip_addr.sin_addr.s_addr = rip_database->rip_networks[i].ip_address;
            char ipstr[INET6_ADDRSTRLEN];

            inet_ntop(AF_INET, &ip_addr.sin_addr, ipstr, sizeof ipstr);
            fprintf(stdout, "Net %i: %s metric %i\n", i+1, ipstr, ntohl(rip_database->rip_networks[i].metric));
        }
        return 0;
    }

    fprintf(stdout, "Invalid command received.\n");
    return -1;
}

    
int start_super_rip ()
{
    fprintf(stdout, "Starting super_rip.\n");
    pthread_t rip_advertise_thread;
    
    // Initial RIP DB setup. Should be removed from this function
    rip_networks = calloc(2, sizeof(rip_network_t));
    rip_networks[0] = get_rip_network("172.25.0.0", 7);
    rip_networks[1] = get_rip_network("10.0.0.0", 4);
    
    rip_database = (thread_config_t*)malloc(sizeof(*rip_database));
    if (!rip_database) {
        perror("OOM");
        exit(1);
    }
    rip_database->num_networks = 2;
    rip_database->rip_networks = rip_networks;

    char command[128];
    memset(command, 0, sizeof command);
    for (;;) {
        if (db_state == WAITING_FOR_UPDATE) {
            fgets(command, 128, stdin);
            fprintf(stdout, "Received %lu chars: %s\n", strlen(command), command);
            if (parse_command(command, strlen(command)) == 1) {
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