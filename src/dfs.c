/**
 * @file dfs.c
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief Distributed File System Server Implementation
 * @version 0.1
 * @date 2023-05-06
 *
 * @copyright Copyright (c) 2023
 *
 * @details The DFS Server will be stateless and dead simple.
 * It will listen for chunks using the TCP protocol and store
 * them in a directory. It will also listen for requests to
 * retrieve chunks and send them back to the client. The server
 * will be able to handle multiple clients at once. The messaging
 * protocol is defined in protocol.h and the README.md file.
 */

#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "transfer.h"

// Global variables
volatile int running      = 1;
volatile int parent       = 1;
volatile int num_children = 0;
int          port         = 8080;

// Function prototypes
void handle_request(int fd, char *ip);
void handle__GET(int fd, ftp_msg_t *msg);
void handle__PUT(int fd, ftp_msg_t *msg);
void handle_LIST(int fd);

// Global variables
// char manif_path[PATH_MAX / 2] = {0};
char chunk_path[PATH_MAX / 2] = {0};

void printUsage(char *argv[]) {
    printf("Usage: %s <directory> <port>\n", argv[0]);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printUsage(argv);
        exit(EXIT_FAILURE);
    }

    char *dir_path = argv[1];
    port           = atoi(argv[2]);
    if (port < 1 || port > 65535) {
        printUsage(argv);
        exit(EXIT_FAILURE);
    }

    printf("Starting server on port %d\n", port);
    printf("Saving files to %s\n", dir_path);

    // Make the directory
    mkdir(dir_path, 0777);
    // Also make the manifests directory and the chunks directory
    // snprintf(manif_path, PATH_MAX / 2, "%s/manif", dir_path);
    snprintf(chunk_path, PATH_MAX / 2, "%s/chunk", dir_path);
    // mkdir(manif_path, 0777);
    mkdir(chunk_path, 0777);

    // Create the socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Bind socket to port
    struct sockaddr_in address = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons(port),
    };
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 3) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Handle incoming connections
    while (running) {
        // Accept connection
        int addrlen = sizeof(address);
        int fd      = accept(server_fd, (struct sockaddr *)&address,
                             (socklen_t *)&addrlen);
        if (fd == -1) {
            // If accept() was interrupted by a signal, try again
            continue;
        }

        // Fork process
        pid_t pid = fork();
        // pid_t pid = 0;
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        // Child process
        if (pid == 0) {
            parent = 0;

            // Close server socket
            close(server_fd);

            // Handle request
            char ip[INET_ADDRSTRLEN] = {0};
            // Populate the IP address field
            if (inet_ntop(AF_INET, &address.sin_addr, ip, INET_ADDRSTRLEN) ==
                NULL) {
                perror("inet_ntop");
                exit(EXIT_FAILURE);
            }
            handle_request(fd, ip);

            // Close client socket
            close(fd);

            // Exit child process
            exit(EXIT_SUCCESS);
        }

        // Parent process
        else {
            num_children++;
            // Close client socket
            close(fd);
        }
    }

    printf("Stopping server...\n");

    // Close server socket
    close(server_fd);

    // Block the SIGCHLD signal
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    // // Kill all children
    // kill(0, SIGTERM);

    // Wait for all children to exit
    while (num_children > 0) {
        wait(NULL);
        num_children--;
    }
    printf("All children exited (%d)\n", num_children);

    // Exit program
    return EXIT_SUCCESS;
}

/**
 * @brief Handle a new client connection (GET, PUT, LS)
 */
void handle_request(int fd, char *ip) {
    while (1) {
        // Receive the transaction initialization header chunk
        ftp_msg_t msg = {0};
        ftp_err_t err = ftp_recv_msg(fd, &msg);
        switch (err) {
        case FTP_ERR_NONE:
            break;
        case FTP_ERR_CLOSE:
            printf("Client %s disconnected\n", ip);
            close(fd);
            return;
        case FTP_ERR_TIMEOUT:
            printf("Client %s timed out\n", ip);
            close(fd);
            return;
        default:
            printf(
                "Error receiving transaction initialization header chunk: %s\n",
                ftp_err_to_str(err));
            return;
        }

        // Handle the transaction initialization header chunk
        switch (msg.cmd) {
        case FTP_CMD_GET:
            printf("GET request from %s\n", ip);
            handle__GET(fd, &msg);
            break;
        case FTP_CMD_PUT:
            printf("PUT request from %s\n", ip);
            handle__PUT(fd, &msg);
            break;
        case FTP_CMD_LIST:
            printf("LS request from %s\n", ip);
            handle_LIST(fd);
            break;
        default:
            printf("Unknown request from %s\n", ip);
            ftp_msg_print(stdout, &msg);
            ftp_send_msg(fd, FTP_CMD_ERROR,
                         "Invalid transaction initialization cmd", -1);
            break;
        }
    }
}

// Handle a GET request
void handle__GET(int fd, ftp_msg_t *msg) {
    char *filename = (char *)msg->packet;
    printf("Filename: %s\n", filename);

    // Open the file
    int file = open(filename, O_RDONLY);
    if (file == -1) {
        // If the file doesn't exist, send an error message
        ftp_send_msg(fd, FTP_CMD_ERROR, "File not found", -1);
        exit(EXIT_FAILURE);
    }

    // Send the file
    ftp_send_data(fd, file);

    // Close the file
    close(file);
}

// Handle a put request
void handle__PUT(int fd, ftp_msg_t *msg) {
    char *filename           = (char *)msg->packet;
    char  filepath[PATH_MAX] = {0};
    snprintf(filepath, PATH_MAX, "%s/%s", chunk_path, filename);
    printf("Filepath: %s\n", filepath);

    // Create the directory just in case
    mkdir(chunk_path, 0777);

    // Create the file
    int file = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (file == -1) {
        // If the file couldn't be created, send an error message
        ftp_send_msg(fd, FTP_CMD_ERROR, "File couldn't be created", -1);
        exit(EXIT_FAILURE);
    }

    // Receive into the file
    ftp_recv_data(fd, file);
    // Close the file
    close(file);
}

// Handle a list request
void handle_LIST(int fd) {
    // Run ls -l
    char cmd[PATH_MAX + 7] = {0};
    snprintf(cmd, PATH_MAX + 7, "ls -l %s", chunk_path);
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        perror("popen");
        exit(EXIT_FAILURE);
    }
    // Send an empty LIST delimiter (FTP_CMD_LIST)
    // ftp_send_msg(fd, FTP_CMD_LIST, NULL, 0);
    // Send the output to the client
    ftp_send_data(fd, fileno(fp));
    pclose(fp);
}
