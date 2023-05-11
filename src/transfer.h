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

// #define FTP_PACKET_SIZE 1024
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
#define FTP_CMD_LIST  ((uint8_t)0x04)
#define FTP_CMD_DATA  ((uint8_t)0x05)
#define FTP_CMD_TERM  ((uint8_t)0x06)
#define FTP_CMD_ERROR ((uint8_t)0x07)
typedef uint8_t ftp_cmd_t;

typedef struct {
    ftp_cmd_t cmd;
    uint32_t  nbytes;
    uint8_t   packet[FTP_PACKET_SIZE + 1]; // +1 for null terminator
} ftp_msg_t;

typedef enum {
    FTP_ERR_NONE,
    FTP_ERR_ARGS,
    FTP_ERR_SOCKET,
    FTP_ERR_POLL,
    FTP_ERR_TIMEOUT,
    FTP_ERR_INVALID,
    FTP_ERR_SERVER,
    FTP_ERR_CLOSE,
} ftp_err_t;

/**
 * @brief Send arbitrary buffer over the socket
 * Given an arbitrary length buffer, func will break it up into packets
 * and send the chunks through the socket to the address
 *
 * @param outfd File descriptor to write to
 * @param infd File descriptor to read from
 * @return ftp_err_t
 *
 * @note This function will block until the entire buffer is sent.
 */
ftp_err_t ftp_send_data(int outfd, int infd);

/**
 * @brief Send a single chunk of data over the socket
 *
 * @param outfd File descriptor to write to
 * @param infd File descriptor to read from
 * @param nbytes Number of bytes to send
 * @return ftp_err_t
 *
 * @note This function will block until the entire buffer is sent.
 */
ftp_err_t ftp_send_data_chunk(int outfd, int infd, int32_t nbytes);

/**
 * @brief recieve FTP_CMD_DATA chunks from sockfd until either a timeout occurs
 * or an FTP_CMD_ERROR is recieved indicating failure or FTP_CMD_TERM is
 * recieved indicating success.
 */
ftp_err_t ftp_recv_data(int infd, int outfd);

/**
 * @brief Send a single command packet, used for setting up or ending
 * transactions.
 *
 * @param arg Optional argument to send to the receiver.
 * @param len Length of *arg*, if provided. If len is set to -1, the function
 * uses strlen to determine the length of *arg*.
 */
ftp_err_t ftp_send_msg(int outfd, ftp_cmd_t cmd, const char *arg, ssize_t len);

/**
 * @brief Recieve a single command packet
 *
 * @param infd File descriptor to read from
 * @param msg Pointer to a message struct to fill
 */
ftp_err_t ftp_recv_msg(int infd, ftp_msg_t *msg);

/**
 * @brief Return a string representation of the ftp_cmd_t
 *
 */
const char *ftp_cmd_to_str(ftp_cmd_t cmd);

/**
 * @brief Return a string representation of the ftp_err_t
 *
 */
const char *ftp_err_to_str(ftp_err_t err);

/**
 * @brief Print the contents of a ftp_msg_t to the FILE *stream
 *
 */
void ftp_msg_print(FILE *stream, ftp_msg_t *msg);

#endif // TRANSFER_H
