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
#include <sys/stat.h> // mkdir
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "md5.h"
#include "parse_conf.c"
#include "transfer.h"

#define CONFIG_PATH "~/dfc.conf"

// Function prototypes
int handle__GET(serv_t servlist[], char *filename);
int handle__PUT(serv_t servlist[], char *filename);
int handle_LIST(serv_t servlist[]);

// Global variables
uint16_t client_id;
char     tmp_path[PATH_MAX / 2] = {0};

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

    // Create a temporary directory for receiving files into
    mkdir("tmp", 0777);
    snprintf(tmp_path, PATH_MAX / 2, "tmp/client%d", getpid());
    mkdir(tmp_path, 0777);

    // Create a client identifier for this client
    srand(time(NULL));
    client_id = rand() & 0xFFFF;

    // Parse the config file to determine the server addresses and ports
    char config_path[PATH_MAX] = {0};
    snprintf(config_path, PATH_MAX, "%s%s", getenv("HOME"), CONFIG_PATH + 1);
    printf("%s\n", config_path);
    serv_t servlist[MAX_SERVERS];
    int    num_servers = parseConfig(config_path, servlist);
    if (num_servers < 0) {
        printf("Failed to parse config file\n");
        exit(1);
    }

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

    // Print the server list
    servlist_print(servlist);

    int rv = EXIT_SUCCESS;
    switch (cmd) {
    case GET:
        // TODO: Handle multiple files depending on argc
        rv = handle__GET(servlist, argv[2]);
        break;
    case PUT:
        // TODO: Handle multiple files depending on argc
        rv = handle__PUT(servlist, argv[2]);
        break;
    case LIST:
        rv = handle_LIST(servlist);
        break;
    default:
        printf("Invalid command\n");
        rv = EXIT_FAILURE;
        break;
    }

    // CLeanup
    for (serv_t *serv = servlist; serv; serv = serv->next) {
        close(serv->fd);
    }
    remove(tmp_path);
    return rv;
}

/**
 * @brief Handles the GET command
 *
 */
int handle__GET(serv_t servlist[], char *filename) {
    // Send the GET <filename> command to each server
    for (serv_t *serv = servlist; serv; serv = serv->next) {
        if (!serv->connected)
            continue;
        ftp_send_msg(serv->fd, FTP_CMD_GET, filename, -1);
    }

    // TODO: Ask for a list first and determine if we can reconstruct the file

    // Create the file
    int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
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
int handle__PUT(serv_t servlist[], char *argpath) {
    // // Send the PUT <argpath> command to each server
    // for (serv_t *serv = servlist; serv; serv = serv->next) {
    //     if (!serv->connected)
    //         continue;
    //     ftp_send_msg(serv->fd, FTP_CMD_PUT, argpath, -1);
    // }

    // // Open the file
    // int file = open(argpath, O_RDONLY);
    // if (file < 0) {
    //     perror("open");
    //     return EXIT_FAILURE;
    // }

    // // Send the file to each server
    // for (serv_t *serv = servlist; serv; serv = serv->next) {
    //     if (!serv->connected)
    //         continue;
    //     ftp_send_data(serv->fd, file);
    // }

    // // Close the file
    // close(file);
    // return;

    // -- Determine the file info for distribution --
    // get the absolute path of the file
    char *filepath = realpath(argpath, NULL);
    if (filepath == NULL) {
        perror("realpath");
        exit(1);
    }
    printf("filepath: %s\n", filepath);

    // Get file name
    char *filename = strrchr(filepath, '/');
    if (filename == NULL) {
        perror("strrchr");
        exit(1);
    }
    filename++;
    printf("filename: %s\n", filename);

    // Hash the file name
    uint8_t hash[16];
    char    hash_str[33];
    md5String(filename, hash);
    for (int i = 0; i < 16; i++) {
        sprintf(hash_str + (i * 2), "%02x", hash[i]);
    }
    printf("hash: %s\n", hash_str);

    // Stat the file
    struct stat st;
    if (stat(filepath, &st) == -1) {
        perror("stat");
        exit(1);
    }
    off_t size = st.st_size;
    // time_t mtime = st.st_mtime;
    time_t stime = time(NULL);
    printf("size: %ld\n", size);
    printf("stime: %lu\n", stime);

    // Determine number of chunks
    size_t full_chunks  = size / FTP_PACKET_SIZE;
    size_t residual_len = size % FTP_PACKET_SIZE;
    size_t num_chunks   = full_chunks + (residual_len ? 1 : 0);
    printf("chunks (%lu): (%lu * FTP_PACKET_SIZE) + %lu = %lu\n", num_chunks,
           full_chunks, residual_len,
           full_chunks * FTP_PACKET_SIZE + residual_len);

    // Ensure there are at least NUM_SERVERS servers available for writing
    // This also allows us to index into the servlist array
    serv_t *servlist_i[MAX_SERVERS];
    int     num_servers = 0;
    while (servlist) {
        if (servlist->connected) {
            printf("[%d]: %s\n", num_servers, servlist->name);
            servlist_i[num_servers++] = servlist;
        }
        servlist = servlist->next;
    }
    if (num_servers < NUM_SERVERS) {
        printf("Not enough servers available for writing (%d/%d)\n",
               num_servers, NUM_SERVERS);
        return EXIT_FAILURE;
    }

    // Produce URI
    char base_name[PATH_MAX / 2];
    bzero(base_name, PATH_MAX / 2);
    snprintf(base_name, PATH_MAX / 2, "%s.%lu.%u.%lu", filename, stime,
             client_id, num_chunks);

    // Distribute chunks among available servers with REDUNDENCY
    printf("Distributing file %s", filepath);
    int fd = open(filepath, O_RDONLY);
    puts("Chunk Map:\t(chunk)\t->\t(serv_id)");
    for (size_t chunk_id = 0; chunk_id < num_chunks; chunk_id++) {
        for (char r = 0; r < REDUNDENCY; r++) {
            // // Read the chunk
            // char chunk[FTP_PACKET_SIZE] = {0};
            // // TODO: Loop until all data is written
            // ssize_t bytes_read = read(fd, chunk, FTP_PACKET_SIZE);
            // if (bytes_read == -1) {
            //     perror("read");
            //     return EXIT_FAILURE;
            // }
            // if (bytes_read == 0) {
            //     break;
            // }

            size_t serv_id = (hash[0] + chunk_id + r) % num_servers;
            char   chunk_name[PATH_MAX] = {0};
            snprintf(chunk_name, PATH_MAX, "%s.%04lX", base_name, chunk_id);
            // printf("\t\t[%lu]\t->\t{%lu}\t\t%s\t\t(%ld)\n", chunk_id,
            // serv_id,
            //        chunk_name, bytes_read);
            printf("\t\t[%lu]\t->\t{%lu}\t\t%s\n", chunk_id, serv_id,
                   chunk_name);

            // Send the chunk to the server
            serv_t *serv = servlist_i[serv_id];
            if (!serv->connected) {
                printf("Server %lu is not connected\n", serv_id);
                return EXIT_FAILURE;
            }

            // Send the PUT <argpath> command to each server
            ftp_send_msg(serv->fd, FTP_CMD_PUT, chunk_name, -1);

            // Send the chunk to the server
            lseek(fd, chunk_id * FTP_PACKET_SIZE, SEEK_SET);
            uint8_t buf[FTP_PACKET_SIZE];
            read(fd, chunk_name, FTP_PACKET_SIZE);
            // printf("read (%lu): %s\n", chunk_id, buf);
            ftp_send_msg(serv->fd, FTP_CMD_DATA, (char *)buf, FTP_PACKET_SIZE);
            ftp_send_msg(serv->fd, FTP_CMD_TERM, NULL, 0);
            // ftp_send_data_chunk(serv->fd, fd, FTP_PACKET_SIZE);
        }
    }

    // // Send chunks in parallel
    // for (serv_t *serv = servlist; serv; serv = serv->next) {
    //     if (!serv->connected)
    //         continue;
    //     pid_t pid = fork();
    //     if (pid == 0) {
    //         // Child
    //     } else {
    //         // Parent

    //     }
    // }
    close(fd);

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
        ftp_send_msg(serv->fd, FTP_CMD_LIST, NULL, 0);
    }

    // Receive the response from each server
    for (serv = servlist; serv; serv = serv->next) {
        if (!serv->connected)
            continue;
        printf("Receiving LIST from %s\n", serv->name);
        // Receive each manifest file
        // ftp_msg_t msg = {0};
        // ftp_recv_msg(serv->fd, &msg);
        // while (1) {
        //     ftp_err_t err = ftp_recv_msg(serv->fd, &msg);
        //     switch (msg.cmd) {
        //     case FTP_CMD_PUT:
        //         // Read this as a manifest file
        //         printf("Received manifest file: %s\n", msg.packet);
        //         ftp_recv_data(serv->fd, STDOUT_FILENO);
        //         puts("\n");
        //         break;
        //     case FTP_CMD_LIST:
        //         // Read this as a list of files (ls -l popen output)
        //         printf("Received file list: \n");
        //         ftp_recv_data(serv->fd, STDOUT_FILENO);
        //         puts("\n");
        //         break;
        //     default:
        //         printf("Invalid server response: %s\n",
        //                ftp_cmd_to_str(msg.cmd));
        //         return EXIT_FAILURE;
        //         break;
        //     }
        // }

        // Create a temporary file to store the ls -l output
        char tmp_file[PATH_MAX] = {0};
        snprintf(tmp_file, PATH_MAX, "%s/%04X.tmp", tmp_path, rand() & 0xFFFF);
        int file = open(tmp_file, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (file < 0) {
            perror("open");
            return EXIT_FAILURE;
        }
        ftp_recv_data(serv->fd, file);
        close(file);

        // Read the file and print it
        FILE *fp = fopen(tmp_file, "r");
        if (!fp) {
            perror("fopen");
            return EXIT_FAILURE;
        }
        char   line[PATH_MAX] = {0};
        size_t line_no        = 0;
        while (fgets(line, PATH_MAX, fp)) {
            line_no++;
            // Parse the filename out of the line
            if (line_no == 1) {
                // Skip the first line
                continue;
            }
            char *filename = strrchr(line, ' ') + 1;
            printf("%s\n", filename);
        }
        fclose(fp);
    }

    return EXIT_SUCCESS;
}
