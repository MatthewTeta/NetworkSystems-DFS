/**
 * @brief Parses the configuration file
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define MAX_SERVERS 16

typedef struct {
    char   *name;
    char   *ip;
    char   *port;
    int     fd;
    int     connected;
    serv_t *next;
} serv_t;

/**
 * @brief Parse the configuration file
 * @details The configuration file is a text file with the following format:
 * server <server_name> <server_ip>:<server_port> [# comment]\n+
 * The function will parse the file and populate the servlist array with the
 * server information.
 *
 * @param path File path to config
 * @return int Number of servers parsed
 */
int parseConfig(char *path, serv_t servlist[]) {
    // Open the file
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        perror("fopen");
        return 0;
    }
    int     num_servers = 0;
    serv_t *old         = NULL;
    char   *line        = NULL;
    size_t  len         = 0;
    while (getline(&line, &len, fp) != -1 && num_servers < MAX_SERVERS) {
        // Remove the newline character
        line[strcspn(line, "\n")] = 0;
        // Split the line into tokens
        char *token = strtok(line, " ");
        int   i     = 0;
        while (token != NULL) {
            switch (i) {
            case 0:
                if (strcmp(token, "server") != 0) {
                    goto nextline;
                }
                break;
            case 1:
                servlist->name = token;
                break;
            case 2:
                servlist->ip = token;
                break;
            case 3:
                servlist->port = token;
                break;
            default:
                break;
            }
            token = strtok(NULL, " ");
            i++;
        }
        if (i < 4) {
            goto nextline;
        }
        if (old != NULL) {
            old->next = servlist;
        }
        old = servlist;
        servlist++;
        num_servers++;
    nextline:;
    }
    if (num_servers == MAX_SERVERS) {
        fprintf(stderr, "Warning: Max number of servers reached (%d)\n",
                MAX_SERVERS);
    }
    free(line);
    fclose(fp);
    return num_servers;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <path>\n", argv[0]);
        exit(1);
    }
    char  *path = argv[1];
    serv_t servlist[MAX_SERVERS];
    int    num_servers = parseConfig(path, servlist);
    return 0;
}