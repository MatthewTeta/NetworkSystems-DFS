/**
 * @file common.h
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief Common settings shared between all files
 * @version 0.1
 * @date 2023-05-06
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef COMMON_H
#define COMMON_H

#include <linux/limits.h>

#define MAX_FILES       4096
#define MAX_CLIENTS     16
#define FTP_PACKET_SIZE 65536U // 64Ki bytes
#define CHUNK_MAX       0x8000 // 2Ti byte file max on Ubuntu
#define TIMEOUT_MS      1000   // 1s  timeout
#define REDUNDENCY      2 // Minimum number of servers to store each chunk on
#define NUM_SERVERS     1

#endif // COMMON_H
