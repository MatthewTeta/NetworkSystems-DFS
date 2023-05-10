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
#include <fcntl.h>
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
int handle__GET(serv_t servlist[], char *file_name);
int handle__PUT(serv_t servlist[], char *file_name);
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
        // TODO: Handle multiple files depending on argc
        exit(handle__GET(servlist, argv[2]));
        break;
    case PUT:
        // TODO: Handle multiple files depending on argc
        exit(handle__PUT(servlist, argv[2]));
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
int handle__GET(serv_t servlist[], char *file_name) {
    // Send the GET <filename> command to each server
    for (serv_t *serv = servlist; serv; serv = serv->next) {
        if (!serv->connected)
            continue;
        send(serv->fd, &cmd, FTP_MSG_SIZE, 0);
        ftp_send_msg(serv->fd, FTP_CMD_GET, file_name, -1);
    }

    // TODO: Ask for a list first and determine if we can reconstruct the file

    // Create the file
    int file = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (file < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    // TODO: Fork and recieve from each server in parallel
    // NOTE: This current method with cause the file to be written multiple
    // times, once for each server Receive the file from each server
    for (serv_t *serv = servlist; serv; serv = serv->next) {
        if (!serv->connected)
            continue;
        ftp_recv_data(serv->fd, file);
    }

    // Close the file
    close(file);

    return EXIT_SUCCESS;
}

/**
 * @brief Handles the PUT command
 *
 */
int handle__PUT(serv_t servlist[], char *file_name) {
    // Send the PUT <filename> command to each server
    for (serv_t *serv = servlist; serv; serv = serv->next) {
        if (!serv->connected)
            continue;
        send(serv->fd, &cmd, FTP_MSG_SIZE, 0);
        ftp_send_msg(serv->fd, FTP_CMD_PUT, file_name, -1);
    }

    // Open the file
    int file = open(file_name, O_RDONLY);
    if (file < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    // TODO: Create a manifest and chunk the file here

    // Send the file to each server
    for (serv_t *serv = servlist; serv; serv = serv->next) {
        if (!serv->connected)
            continue;
        ftp_send_data(serv->fd, file);
    }

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
        ftp_send_msg(serv->fd, FTP_CMD_LIST, NULL, 0);
    }

    // Receive the response from each server
    for (serv = servlist; serv; serv = serv->next) {
        if (!serv->connected)
            continue;
        printf("LIST Received from %s\n", serv->name);
        // Receive the file_name
        ftp_msg_t msg = {0};
        ftp_recv_msg(serv->fd, &msg);
        while (1) {
            ftp_err_t err = ftp_recv_msg(serv->fd, &msg);
            switch (msg.cmd) {
            case FTP_CMD_PUT:
                // Read this as a manifest file
                printf("Received manifest file: %s\n", msg.packet);
                ftp_recv_data(serv->fd, STDOUT_FILENO);
                puts("\n");
                break;
            case FTP_CMD_LIST:
                // Read this as a list of files (ls -l popen output)
                printf("Received file list: \n");
                ftp_recv_data(serv->fd, STDOUT_FILENO);
                puts("\n");
                break;
            default:
                printf("Invalid server response: %s\n", ftp_cmd_to_str(msg.cmd));
                return EXIT_FAILURE;
                break;
            }
        }
    }

    return EXIT_SUCCESS;
}
