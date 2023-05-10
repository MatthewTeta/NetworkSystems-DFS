/**
 * @file transfer.h
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief Transfer Protocol for DFS Implementation
 * @version 0.1
 * @date 2023-05-08
 *
 * @copyright Copyright (c) 2023
 */

#ifndef TRANSFER_H
#define TRANSFER_H

#include <linux/limits.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>

#include "common.h"

#define FTP_MSG_SIZE sizeof(ftp_msg_t)

/**
 * Client oriented command naming convention
 * Commands:
 *      GET <filename>: move <filename> file from server to client.
 *      PUT <filename>: move <filename> file from cleint to server.
 *      DELETE <filename>: delete <filename> file from server fs.
 *      LS : list the contents of the server filesystem.
 *      // Internal flow commands
 *      ERROR <message>: Stop any ongoing partial transaction.
 */
#define FTP_CMD_GET   ((uint8_t)0x01)
#define FTP_CMD_PUT   ((uint8_t)0x02)
#define FTP_CMD_LS    ((uint8_t)0x04)
#define FTP_CMD_ERROR ((uint8_t)0x06)
typedef uint8_t ftp_cmd_t;

typedef struct {
    ftp_cmd_t cmd;
    int32_t   nbytes;
    char      path[PATH_MAX];
    char      packet[CHUNK_SIZE];
} ftp_msg_t;

typedef enum {
    FTP_ERR_NONE,
    FTP_ERR_ARGS,
    FTP_ERR_SOCKET,
    FTP_ERR_POLL,
    FTP_ERR_TIMEOUT,
    FTP_ERR_INVALID,
    FTP_ERR_SERVER,
} ftp_err_t;

/**
 * @brief Send arbitrary buffer over the socket
 * Given an arbitrary length buffer, func will break it up into packets
 * and send the chunks through the socket to the address
 */
ftp_err_t ftp_send_data(int fd, FILE *infp);

/**
 * @brief recieve FTP_CMD_DATA chunks from sockfd until either a timeout occurs
 * or an FTP_CMD_ERROR is recieved indicating failure or FTP_CMD_TERM is
 * recieved indicating success.
 */
ftp_err_t ftp_recv_data(int fd, FILE *outfd);

/**
 * @brief Send a single command packet, used for setting up or ending
 * transactions. Use arglen -1 for strings (uses strlen to copy the relevant
 * bit)
 */
ftp_err_t ftp_send_msg(int fd, ftp_cmd_t cmd, const char *arg, ssize_t arglen);

/**
 * @brief Recieve a single command packet, useful for establishing a link (ACK)
 */
ftp_err_t ftp_recv_msg(int fd, ftp_msg_t *ret, int timeout, int send_ack,
                       struct sockaddr *out_addr, socklen_t *out_addr_len);

#endif // TRANSFER_H
