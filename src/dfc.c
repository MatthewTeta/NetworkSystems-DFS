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

#define FINF_CHUNK_SUCCESS     ((void *)0)
#define FINF_CHUNK_FAILURE     ((void *)1)
#define FINF_CHUNK_SUCCESS_IDX MAX_SERVERS + 1

typedef struct file_info {
    char     filename[NAME_MAX];
    char     storename[NAME_MAX];
    time_t   stime;
    uint16_t client_id;
    size_t   num_chunks;
    int      reproducible;
    serv_t  *chunk_locs[MAX_CHUNKS]
                      [MAX_SERVERS + 2]; // +1 for NULL, +1 for success status
} file_info_t;

// Function prototypes
int  handle__GET(serv_t servlist[], char *filename);
int  handle__PUT(serv_t servlist[], char *filename);
int  handle_LIST(serv_t servlist[]);
void file_list_insert(char *filename, serv_t *serv);
void file_list_analyze(void);
void file_list_clear(void);
void file_list_print(void);

// Global variables
uint16_t    client_id;
char        tmp_path[NAME_MAX]   = {0};
file_info_t file_info[MAX_FILES] = {0};
size_t      num_files            = 0;
size_t      num_servers          = 0;

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
    puts("");

    cmd = parseArgs(argc, argv);
    if (cmd == INVALID) {
        printUsage(argv);
        exit(1);
    }

    // Create a temporary directory for receiving files into
    mkdir("tmp", 0777);
    snprintf(tmp_path, NAME_MAX, "tmp/client%d", getpid());
    mkdir(tmp_path, 0777);

    // Create a client identifier for this client
    srand(time(NULL));
    client_id = rand() & 0xFFFF;

    // Parse the config file to determine the server addresses and ports
    char config_path[PATH_MAX] = {0};
    snprintf(config_path, PATH_MAX, "%s%s", getenv("HOME"), CONFIG_PATH + 1);
    printf("[INFO]\tLoading server configuration from: %s\n", config_path);
    serv_t servlist[MAX_SERVERS];
    int    num_servers2 = parseConfig(config_path, servlist);
    if (num_servers2 < 0) {
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
        } else {
            close(serv->fd);
            serv->connected = 0;
        }
    }

    // update the server id's
    // This also allows us to index into the servlist array
    serv_t *servlist2 = servlist;
    num_servers       = 0;
    while (servlist2) {
        if (servlist2->connected) {
            servlist2->id = num_servers++;
        }
        servlist2 = servlist2->next;
    }
    // Print the server list
    servlist_print(servlist);

    int rv = EXIT_SUCCESS;
    switch (cmd) {
    case GET:
        while (argc > 2) {
            rv |= handle__GET(servlist, argv[argc - 1]);
            char *status = rv == EXIT_SUCCESS ? "OK" : "FAIL";
            printf("[GET] %4s\t%s", status, argv[argc - 1]);
            argc--;
        }
        break;
    case PUT:
        while (argc > 2) {
            rv |= handle__PUT(servlist, argv[argc - 1]);
            char *status = rv == EXIT_SUCCESS ? "OK" : "FAIL";
            printf("[PUT] %4s\t%s", status, argv[argc - 1]);
            argc--;
        }
        break;
    case LIST:
        rv |= handle_LIST(servlist);
        char *status = rv == EXIT_SUCCESS ? "OK" : "FAIL";
        printf("[LIST] %4s\n", status);
        break;
    default:
        printf("Invalid command\n");
        rv |= EXIT_FAILURE;
        break;
    }

    // Cleanup
    for (serv_t *serv = servlist; serv; serv = serv->next) {
        close(serv->fd);
    }
    remove(tmp_path);

    puts("");
    return rv;
}

int file_info_get_next_match(char *filename, int start) {
    for (size_t i = start; i < num_files; i++) {
        if (strcmp(file_info[i].filename, filename) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Handles the GET command
 *
 */
int handle__GET(serv_t servlist[], char *filename) {
    handle_LIST(servlist);

    // Create the file locally
    int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (file < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    // Find the filename in the file_info array
    int file_id = -1;
    while ((file_id = file_info_get_next_match(filename, file_id + 1)) >= 0) {
        // Check if the file is complete
        if (!file_info[file_id].reproducible)
            continue;

        // Easy access
        file_info_t *finf = &file_info[file_id];
        ftp_err_t    err  = FTP_ERR_NONE;

        printf("[INFO]\tFound file: %s\n", finf->storename);

        // Get each chunk
        for (size_t i = 0; i < finf->num_chunks; i++) {
            // Clear the chunk success array
            finf->chunk_locs[i][FINF_CHUNK_SUCCESS_IDX] = FINF_CHUNK_FAILURE;
            // Try each of the servers which is known to have the file
            for (size_t j = 0; j < MAX_SERVERS; j++) {
                serv_t *serv = finf->chunk_locs[i][j];
                if (!serv || !serv->connected)
                    continue;
                // Send the GET command with the chunk id
                char chunkpath[PATH_MAX] = {0};
                sprintf(chunkpath, "%s.%lu", finf->storename, i);
                ftp_send_msg(serv->fd, FTP_CMD_GET, chunkpath, -1);
                // Wait for the response
                ftp_msg_t msg = {0};
                err           = ftp_recv_msg(serv->fd, &msg);
                // Handle the error
                switch (err) {
                case FTP_ERR_NONE:
                    break;
                case FTP_ERR_CLOSE:
                    fprintf(stderr, "[INFO]\tServer closed connection (%s)\n",
                            serv->name);
                    serv->connected = 0;
                    continue;
                case FTP_ERR_TIMEOUT:
                    fprintf(stderr, "[INFO]\tServer timed out (%s)\n",
                            serv->name);
                    continue;
                default:
                    fprintf(stderr, "Unknown ftp_recv_msg error: %s\n",
                            ftp_err_to_str(err));
                    goto handle__GET_failure;
                }
                // Write the chunk to the file
                lseek(file, i * FTP_PACKET_SIZE, SEEK_SET);
                size_t bytes_written = 0;
                while (bytes_written < msg.nbytes) {
                    ssize_t n = write(file, msg.packet + bytes_written,
                                      msg.nbytes - bytes_written);
                    if (n < 0) {
                        perror("write");
                        goto handle__GET_failure;
                    }
                    bytes_written += n;
                }
                // We got the chunk, so we can stop trying
                finf->chunk_locs[i][FINF_CHUNK_SUCCESS_IDX] =
                    FINF_CHUNK_SUCCESS;
            }
        }

        // Check if we got all the chunks
        for (size_t i = 0; i < finf->num_chunks; i++) {
            if (finf->chunk_locs[i][FINF_CHUNK_SUCCESS_IDX] ==
                FINF_CHUNK_FAILURE) {
                fprintf(stderr, "Failed to get chunk %lu\n", i);
                goto handle__GET_next_file;
            }
        }
        // TODO: Check the file hash
        break;
    handle__GET_next_file:;
    }
    if (file_id < 0) {
        printf("[INFO]\tFile is not available\n");
        return EXIT_FAILURE;
    }

    close(file);
    return EXIT_SUCCESS;

handle__GET_failure:;
    close(file);
    return EXIT_FAILURE;

    // // First we do a LIST to see if we can reconstruct the file
    // // Send the GET <filename> command to each server
    // for (serv_t *serv = servlist; serv; serv = serv->next) {
    //     if (!serv->connected)
    //         continue;
    //     ftp_send_msg(serv->fd, FTP_CMD_GET, filename, -1);
    // }

    // // TODO: Ask for a list first and determine if we can reconstruct the
    // file

    // // Create the file
    // int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    // if (file < 0) {
    //     perror("open");
    //     return EXIT_FAILURE;
    // }

    // // TODO: Fork and recieve from each server in parallel
    // // NOTE: This current method with cause the file to be written multiple
    // // times, once for each server Receive the file from each server
    // for (serv_t *serv = servlist; serv; serv = serv->next) {
    //     if (!serv->connected)
    //         continue;
    //     ftp_recv_data(serv->fd, file);
    // }

    // // Close the file
    // close(file);

    // return EXIT_SUCCESS;
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
    char base_name[NAME_MAX];
    bzero(base_name, NAME_MAX);
    snprintf(base_name, NAME_MAX, "%s.%lu.%u.%lu", filename, stime, client_id,
             num_chunks);

    // Distribute chunks among available servers with REDUNDENCY
    printf("Distributing file %s\n", filepath);
    int fd = open(filepath, O_RDONLY);
    puts("Chunk Map:\t(chunk)\t->\t(serv_id)");
    for (size_t chunk_id = 0; chunk_id < num_chunks; chunk_id++) {
        // Read the chunk from the file
        lseek(fd, chunk_id * FTP_PACKET_SIZE, SEEK_SET);
        uint8_t buf[FTP_PACKET_SIZE] = {0};
        size_t  nread                = 0;
        ssize_t n                    = 0;
        while ((n = read(fd, (char *)buf + nread, FTP_PACKET_SIZE - nread)) >
               0) {
            nread += n;
            if (nread == FTP_PACKET_SIZE) {
                break;
            }
        }
        if (n == -1) {
            perror("read");
            close(fd);
            return EXIT_FAILURE;
        }
        // printf("read (%lu): %s\n", chunk_id, buf);

        // Send the chunk to each of the chosen servers
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
            snprintf(chunk_name, PATH_MAX, "%s.%lu", base_name, chunk_id);
            // printf("\t\t[%lu]\t->\t{%lu}\t\t%s\t\t(%ld)\n", chunk_id,
            // serv_id,
            //        chunk_name, bytes_read);
            printf("\t\t[%lu]\t->\t{%lu}\t\t%s\n", chunk_id, serv_id,
                   chunk_name);

            // Send the chunk to the server
            serv_t *serv = servlist_i[serv_id];
            if (!serv->connected) {
                printf("Server %lu is not connected\n", serv_id);
                close(fd);
                return EXIT_FAILURE;
            }

            // Send the PUT <argpath> command to each server
            ftp_send_msg(serv->fd, FTP_CMD_PUT, chunk_name, -1);

            ftp_send_msg(serv->fd, FTP_CMD_DATA, (char *)buf, nread);
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

    file_list_clear();
    printf("LIST:\t");
    // Receive the response from each server
    for (serv = servlist; serv; serv = serv->next) {
        if (!serv->connected)
            continue;
        printf("[%s]\t", serv->name);
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
        // Recreate the file list from scratch
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
            // Remove the newline
            char *newline = strchr(filename, '\n');
            if (newline) {
                *newline = '\0';
            }
            // Insert the file into the file_list
            file_list_insert(filename, serv);
        }
        fclose(fp);
    }
    puts("");
    file_list_analyze();
    file_list_print();

    return EXIT_SUCCESS;
}

/**
 * @brief Parses a filename and inserts it into the list
 *
 */
void file_list_insert(char *filename, serv_t *serv) {
    // Copy the filename
    char copyname[NAME_MAX];
    char storename[NAME_MAX];
    strncpy(copyname, filename, NAME_MAX);

    // Parse the filename:
    // filename.stime.client_id.num_chunks.chunk_id
    // Search in reverse
    char *dot = strrchr(filename, '.');
    if (!dot)
        goto file_list_insert_error;
    *dot               = '\0';
    char *chunk_id_str = dot + 1;
    strncpy(storename, filename, NAME_MAX);

    dot = strrchr(filename, '.');
    if (!dot)
        goto file_list_insert_error;
    *dot                 = '\0';
    char *num_chunks_str = dot + 1;

    dot = strrchr(filename, '.');
    if (!dot)
        goto file_list_insert_error;
    *dot                = '\0';
    char *client_id_str = dot + 1;

    dot = strrchr(filename, '.');
    if (!dot)
        goto file_list_insert_error;
    *dot            = '\0';
    char *stime_str = dot + 1;

    if (!stime_str || !client_id_str || !num_chunks_str || !chunk_id_str)
        goto file_list_insert_error;

    // Parse the stime
    time_t stime = atoi(stime_str);

    // Parse the client_id
    int client_id = atoi(client_id_str);

    // Parse the num_chunks
    size_t num_chunks = (size_t)atoi(num_chunks_str);

    // Parse the chunk_id
    int chunk_id = atoi(chunk_id_str);

    // Insert the file into the file_list
    file_info_t *info = &file_info[0];
    for (size_t i = 0; i < num_files; i++) {
        if (strcmp(info[i].filename, filename) != 0)
            continue;
        if (info[i].stime != stime)
            continue;
        if (info[i].client_id != client_id)
            continue;
        if (info[i].num_chunks != num_chunks)
            continue;
        // File matches
        // Update the file chunk info
        size_t  j = 0;
        serv_t *n = info[i].chunk_locs[chunk_id][j];
        while (n) {
            if (n->id == serv->id)
                return;
            n = info[i].chunk_locs[chunk_id][++j];
        }
        info[i].chunk_locs[chunk_id][j] = serv;
        return;
    }
    // Insert a new entry
    bzero(info + num_files, sizeof(file_info_t));
    file_info_t *new = &file_info[num_files];
    strncpy(new->filename, filename, NAME_MAX);
    strncpy(new->storename, storename, NAME_MAX);
    new->stime                   = stime;
    new->client_id               = client_id;
    new->num_chunks              = num_chunks;
    new->reproducible            = 0;
    new->chunk_locs[chunk_id][0] = serv;
    num_files++;
    return;

file_list_insert_error:;
    fprintf(stderr, "Error parsing filename: %s\n", filename);
    return;
}

void file_list_clear(void) {
    bzero(file_info, sizeof(file_info_t) * MAX_FILES);
    num_files = 0;
}

/**
 * @brief Determine which files in the file list can be reproduced from the
 * available servers
 *
 */
void file_list_analyze(void) {
    // Iterate through all files
    file_info_t *info = &file_info[0];
    for (size_t i = 0; i < num_files; i++) {
        info[i].reproducible = 1;
        // Iterate through all chunks
        for (size_t j = 0; j < info[i].num_chunks; j++) {
            // Servers begin at index 0
            if (info[i].chunk_locs[j][0] == NULL) {
                // No servers have this chunk -> cannot reproduce
                // printf("File %s chunk %lu cannot be reproduced (%p)\n",
                //        info[i].filename, j, info[i].chunk_locs[j][0]);
                info[i].reproducible = 0;
                break;
            }
        }
    }
}

/**
 * @brief Prints the file list
 *
 */
void file_list_print(void) {
    file_info_t *info = &file_info[0];
    puts("");
    puts("File List:");
    print_line(80, '-');
    puts("reproducible\tnum_chunks\tclient_id\t     stime\tfilename");
    print_line(80, '-');
    for (size_t i = 0; i < num_files; i++) {
        printf("% 12d\t", info[i].reproducible);
        printf("%10lu\t", info[i].num_chunks);
        printf("% 9d\t", info[i].client_id);
        printf("% 10ld\t", info[i].stime);
        printf("%s\n", info[i].filename);
    }
    print_line(80, '-');
    puts("");
}
