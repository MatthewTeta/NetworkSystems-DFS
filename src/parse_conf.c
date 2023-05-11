/**
 * @brief Parses the configuration file
 *
 */

#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define MAX_SERVERS 16
#define CONFIG_PATH "~/dfc.conf"

typedef struct serv_t serv_t;
struct serv_t {
    char   *name;
    char   *ip;
    char   *port;
    int     id;
    int     fd;
    int     connected;
    serv_t *next;
};

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
    servlist->next      = NULL;
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
                servlist->name = strdup(token);
                break;
            case 2:
                token        = strtok(token, ":");
                servlist->ip = strdup(token);
                break;
            case 3:
                servlist->port = strdup(token);
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
        // Fill out the rest of the serv_t values:
        servlist->fd        = -1;
        servlist->fd        = -1;
        servlist->connected = 0;
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
    if (old != NULL) {
        old->next = NULL;
    }
    free(line);
    fclose(fp);
    return num_servers;
}

void servlist_print(serv_t servlist[]) {
    while (servlist) {
        printf("[%s]\t%s:%s (%d)", servlist->name, servlist->ip, servlist->port,
               servlist->fd);
        if (servlist->connected) {
            printf("(connected)");
        }
        puts("");
        servlist = servlist->next;
    }
}

// int main() {
// 	// Construct a path from the home variable
// 	char path[PATH_MAX] = {0};
// 	snprintf(path, PATH_MAX, "%s%s", getenv("HOME"), CONFIG_PATH + 1);
// 	printf("%s\n", path);
// 	serv_t servlist[MAX_SERVERS];
// 	int num_servers = parseConfig(path, servlist);
// 	if (num_servers) servlist_print(servlist);

// 	return 0;
// }
