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

#include <fcntl.h>
#include <linux/limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "md5.h"

#define CHUNK_SIZE  (65536) // 64Ki byte chunks
#define REDUNDENCY  2       // Minimum number of servers to store each chunk on
#define NUM_SERVERS 4

// Global variables
uint16_t client_id;

void printUsage(char *argv[]) { printf("Usage: %s <filename>\n", argv[0]); }

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

    // Hash the file name
    uint8_t hash[16];
    char    hash_str[33];
    md5String(filename, hash);
    for (int i = 0; i < 16; i++) {
        sprintf(hash_str + (i * 2), "%02x", hash[i]);
    }
    printf("hash: %s\n", hash_str);

    // Hash the file contents
    char cmd[PATH_MAX + 8];
    char checksum[33];
    bzero(cmd, PATH_MAX + 8);
    bzero(checksum, 33);
    sprintf(cmd, "md5sum %s", filepath);
    FILE *f_checksum = popen(cmd, "r");
    if (f_checksum == NULL) {
        perror("popen");
        exit(1);
    }
    if (fread(checksum, 1, 32, f_checksum) != 32) {
        perror("fread");
        exit(1);
    }
    pclose(f_checksum);
    printf("checksum: %s\n", checksum);

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
    size_t full_chunks  = size / CHUNK_SIZE;
    size_t residual_len = size % CHUNK_SIZE;
    size_t num_chunks   = full_chunks + (residual_len ? 1 : 0);
    printf("chunks (%lu): (%lu * CHUNK_SIZE) + %lu = %lu\n", num_chunks,
           full_chunks, residual_len, full_chunks * CHUNK_SIZE + residual_len);

    // TODO: Ensure there are at least 4 servers available for writing

    // Produce URI
    char base_name[PATH_MAX / 2];
    bzero(base_name, PATH_MAX / 2);
    snprintf(base_name, PATH_MAX / 2, "%s.%lu.%u", hash_str, stime, client_id);

    // Generate the manifest path
    char manifest_path[PATH_MAX];
    bzero(manifest_path, PATH_MAX);
    snprintf(manifest_path, PATH_MAX, "manifests/%s", base_name);

    // mkdir("manifests", 0777);

    // Create the manifest file locally:
    FILE *f_manifest = fopen(manifest_path, "w");
    if (f_manifest == NULL) {
        perror("fopen");
        exit(1);
    }
    fprintf(f_manifest, "filename: %s\n", filename);
    fprintf(f_manifest, "basename: %s\n", base_name);
    fprintf(f_manifest, "hash: %s\n", hash_str);
    fprintf(f_manifest, "stime: %lu\n", stime);
    fprintf(f_manifest, "client_id: %u\n", client_id);
    fprintf(f_manifest, "size: %ld\n", size);
    fprintf(f_manifest, "CHUNK_SIZE: %d\n", CHUNK_SIZE);
    fprintf(f_manifest, "full_chunks: %lu\n", full_chunks);
    fprintf(f_manifest, "num_chunks: %lu\n", num_chunks);
    fprintf(f_manifest, "residual_len: %lu\n", residual_len);
    fprintf(f_manifest, "checksum: %s\n", checksum);
    // TODO: Send the manifest to the server
    fclose(f_manifest);
    // Remove the manifest file
    // remove(manifest_path);

    // Distribute chunks among available servers with REDUNDENCY
    printf("Opening the file... (%s)", manifest_path);
    int fd = open(filepath, O_RDONLY);
    puts("Chunk Map:\t(chunk)\t->\t(serv_id)");
    for (size_t chunk_id = 0; chunk_id < num_chunks; chunk_id++) {
        for (char r = 0; r < REDUNDENCY; r++) {
            // Read the chunk
            char chunk[CHUNK_SIZE] = {0};
            // TODO: Loop until all data is written
            ssize_t bytes_read = read(fd, chunk, CHUNK_SIZE);
            if (bytes_read == -1) {
                perror("read");
                exit(1);
            }
            if (bytes_read == 0) {
                break;
            }

            size_t serv_id = (hash[0] + chunk_id + r) % NUM_SERVERS;
            char   chunk_name[PATH_MAX];
            bzero(chunk_name, PATH_MAX);
            snprintf(chunk_name, PATH_MAX, "%s.%04lX", base_name, chunk_id);
            printf("\t\t[%lu]\t->\t{%lu}\t\t%s\t\t(%ld)\n", chunk_id, serv_id,
                   chunk_name, bytes_read);

            // TODO: Send the chunk to the server
        }
    }
    puts("Closing the file...");
    close(fd);

    return 0;
}
