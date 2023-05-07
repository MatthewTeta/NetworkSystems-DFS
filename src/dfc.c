/**
 * @file dfc.c
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief Distributed File System Client Implementation
 * @details See README.md for more details.
 * @version 0.1
 * @date 2023-05-06
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "protocol.h"

#define CONFIG_PATH "~/dfc.conf"

// Function prototypes
int handle__GET(servlist_t *servlist, int argc, char *argv[]);
int handle__PUT(servlist_t *servlist, int argc, char *argv[]);
int handle_LIST(servlist_t *servlist);

// Global variables
uint16_t client_id;

void printUsage(char *argv[]) {
    printf("Usage: %s <command> [filename] ... [filename]\n", argv[0]);
}

enum command {
    INVALID,
    GET,
    PUT,
    LIST,
} cmd = INVALID;

enum command parseArgs(int argc, char *argv[]) {
    enum command cmd = INVALID;
    if (strcmp(argv[1], "get") == 0) {
        cmd = GET;
    } else if (strcmp(argv[1], "put") == 0) {
        cmd = PUT;
    } else if (strcmp(argv[1], "list") == 0) {
        cmd = LIST;
    }
    return cmd;
}

int main(int argc, char *argv[]) {
    // Parse arguments
    if (argc < 2) {
        printUsage(argv);
        exit(1);
    }

    cmd = parseArgs(argc, argv);
    if (cmd == INVALID) {
        printUsage(argv);
        exit(1);
    }

    // Create a client identifier for this client
    client_id = time(NULL) & 0xFFFF;

    // Parse the config file to determine the server addresses and ports
    servlist_t *servlist = parseConfig(CONFIG_PATH);

    // Create a socket for each server
    for (int i = 0; i < servlist->num_servers; i++) {
        servlist->sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (servlist->sockets[i] < 0) {
            perror("socket");
            exit(1);
        }
    }

    // TODO: Maybe move the IP stuff to the parseConfig function
    // Connect to each server
    for (int i = 0; i < servlist->num_servers; i++) {
        struct sockaddr_in serv_addr;
        serv_addr.sin_family      = AF_INET;
        serv_addr.sin_port        = htons(servlist->ports[i]);
        serv_addr.sin_addr.s_addr = inet_addr(servlist->addresses[i]);

        if (connect(servlist->sockets[i], (struct sockaddr *)&serv_addr,
                    sizeof(serv_addr)) >= 0) {
            servlist->connected[i] = 1;
            printf("Connected to server %s\n", servlist->names[i]);
        } else {
            close(servlist->sockets[i]);
            servlist->connected[i] = 0;
            printf("Failed to connect to server %s\n", servlist->names[i]);
        }
    }

    switch (cmd) {
    case GET:
        exit(handle__GET(servlist, argc, argv));
        break;
    case PUT:
        exit(handle__PUT(servlist, argc, argv));
        break;
    case LIST:
        exit(handle_LIST(servlist));
        break;
    default:
        printf("Invalid command\n");
        exit(EXIT_FAILURE);
        break;
    }
}

/**
 * @brief Handles the GET command
 *
 */
int handle__GET(servlist_t *servlist, int argc, char *argv[]) {

    return EXIT_SUCCESS;
}

/**
 * @brief Handles the PUT command
 *
 */
int handle__PUT(servlist_t *servlist, int argc, char *argv[]) {}

/**
 * @brief Handles the LIST command
 *
 */
int handle_LIST(servlist_t *servlist) {
    // Send the LIST command to each server
    for (int i = 0; i < servlist->num_servers; i++) {
        if (servlist->connected[i]) {
            send(servlist->sockets[i], &cmd, sizeof(cmd), 0);
        }
    }

    // Receive the LIST command from each server
    for (int i = 0; i < servlist->num_servers; i++) {
        if (servlist->connected[i]) {
            char buf[BUFSIZ];
            recv(servlist->sockets[i], buf, BUFSIZ, 0);
            printf("%s\n", buf);
        }
    }

    return EXIT_SUCCESS;
}
