/**
 * @file manifest.c
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief Test the generation of manifest files for PUT
 * @version 0.1
 * @date 2023-05-07
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
#include <linux/limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>

#define CHUNK_SIZE 	(1024)	// 1Ki byte chunks
#define REDUNDENCY 	2				// Minimum number of servers to store each chunk on
#define NUM_SERVERS	4

// Global variables
uint16_t client_id;

void printUsage(char *argv[]) {
    printf("Usage: %s <filename>\n", argv[0]);
}

int main(int argc, char *argv[]) {
    // Parse arguments
    if (argc < 2) {
        printUsage(argv);
        exit(1);
    }

	srand(time(NULL));
    client_id = rand() & 0xFFFF;
	printf("client_id: %u\n", client_id);

    // get the absolute path of the file
    char *filepath = realpath(argv[1], NULL);
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

    // Hash the file contents
	char cmd[PATH_MAX + 8];
    char hash[33];
    bzero(cmd, PATH_MAX + 8);
    bzero(hash, 33);
	sprintf(cmd, "md5sum %s", filepath);
    FILE * f_hash = popen(cmd, "r");
    if (f_hash == NULL) {
        perror("popen");
        exit(1);
    }
    if (fread(hash, 1, 32, f_hash) != 32) {
        perror("fread");
        exit(1);
    }
    pclose(f_hash);
    printf("hash: %s\n", hash);

	// Stat the file
    struct stat st;
    if (stat(filepath, &st) == -1) {
        perror("stat");
        exit(1);
    }
	off_t  size = st.st_size;
	time_t mtime = st.st_mtime;
    printf("size: %ld\n", size);
    printf("mtime: %lu\n", mtime);
	
	// Determine number of chunks
	size_t full_chunks = size / CHUNK_SIZE;
	size_t residual_len = size % CHUNK_SIZE;
	size_t num_chunks = full_chunks + (residual_len ? 1 : 0);
	printf("chunks (%lu): (%lu * CHUNK_SIZE) + %lu = %lu\n", num_chunks, full_chunks, residual_len, full_chunks * CHUNK_SIZE + residual_len);

	// TODO: Ensure there are at least 4 servers available for writing

	// Distribute chunks among available servers with REDUNDENCY
	puts("Chunk Map:\t(chunk)\t->\t(serv_id)");
	for (size_t chunk_id = 0; chunk_id < num_chunks; chunk_id++) {
		for (char r = 0; r < REDUNDENCY; r++) {
			size_t serv_id = (hash[0] + chunk_id + r) % NUM_SERVERS;
			char chunk_name[PATH_MAX];
			bzero(chunk_name, PATH_MAX);
			snprintf(chunk_name, PATH_MAX, "%s.%lu.%u.%08lX", hash, mtime, client_id, chunk_id);
			printf("\t\t[%lu]\t->\t{%lu}\t\t%s\n", chunk_id, serv_id, chunk_name);
		}
	}

    return 0;
}

