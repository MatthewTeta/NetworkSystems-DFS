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

#include <arpa/inet.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "parse_conf.c"
#include "transfer.h"

#define CONFIG_PATH "~/dfc.conf"

// Function prototypes
int handle__GET(serv_t servlist[], int argc, char *argv[]);
int handle__PUT(serv_t servlist[], int argc, char *argv[]);
int handle_LIST(serv_t servlist[]);

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
    // Parse arguments
    if (argc < 2) {
        printUsage(argv);
        exit(1);
    }

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
    cmd = parseArgs(argc, argv);
    if (cmd == INVALID) {
        printUsage(argv);
        exit(1);
    }

    // Create a client identifier for this client
    client_id = time(NULL) & 0xFFFF;

    // Parse the config file to determine the server addresses and ports
    char config_path[PATH_MAX] = {0};
    snprintf(config_path, PATH_MAX, "%s%s", getenv("HOME"), CONFIG_PATH + 1);
    printf("%s\n", config_path);
    serv_t servlist[MAX_SERVERS];
    int    num_servers = parseConfig(config_path, servlist);
    if (num_servers)
        servlist_print(servlist);

    // Create a socket for each server
    for (serv_t *serv = servlist; serv; serv = serv->next) {
        serv->fd = socket(AF_INET, SOCK_STREAM, 0);
        if (serv->fd < 0) {
            perror("socket");
            exit(1);
        }
    }

    // Connect to each server
    for (serv_t *serv = servlist; serv; serv = serv->next) {
        struct sockaddr_in serv_addr;
        serv_addr.sin_family      = AF_INET;
        serv_addr.sin_port        = htons(atoi(serv->port));
        serv_addr.sin_addr.s_addr = inet_addr(serv->ip);

        if (connect(serv->fd, (struct sockaddr *)&serv_addr,
                    sizeof(serv_addr)) >= 0) {
            serv->connected = 1;
            printf("Connected to server %s\n", serv->name);
        } else {
            close(serv->fd);
            serv->connected = 0;
            printf("Failed to connect to server %s\n", serv->name);
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
int handle__GET(serv_t servlist[], int argc, char *argv[]) {
    return EXIT_SUCCESS;
}

/**
 * @brief Handles the PUT command
 *
 */
int handle__PUT(serv_t servlist[], int argc, char *argv[]) {
    return EXIT_SUCCESS;
}

/**
 * @brief Handles the LIST command
 *
 */
int handle_LIST(serv_t servlist[]) {
    serv_t *serv;
    // Send the LIST command to each server
    for (serv = servlist; serv; serv = serv->next) {
        if (!serv->connected)
            continue;
        send(serv->fd, &cmd, FTP_MSG_SIZE, 0);
        ftp_send_msg(serv->fd, FTP_CMD_LS, NULL, 0);
    }
    // Receive the LIST command from each server
    for (serv = servlist; serv; serv = serv->next) {
        if (!serv->connected)
            continue;
        char buf[BUFSIZ];
        recv(serv->fd, buf, BUFSIZ, 0);
        printf("%s\n", buf);
    }

    return EXIT_SUCCESS;
}
