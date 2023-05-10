/**
 * @file transfer.c
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief Transfer Protocol for DFS Implementation
 * @version 0.1
 * @date 2023-05-09
 *
 * @copyright Copyright (c) 2023
 */

#include "transfer.h"

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* For Reference:

#define FTP_CMD_GET    ((uint8_t)0x01)
#define FTP_CMD_PUT    ((uint8_t)0x02)
#define FTP_CMD_DELETE ((uint8_t)0x03)
#define FTP_CMD_LIST   ((uint8_t)0x04)
#define FTP_CMD_ERROR  ((uint8_t)0x06)
typedef uint8_t ftp_cmd_t;

typedef struct {
    ftp_cmd_t cmd;
    int32_t   nbytes;
    char      path[PATH_MAX];
    char      packet[CHUNK_SIZE];
} ftp_chunk_t;

typedef enum {
    FTP_ERR_NONE,
    FTP_ERR_ARGS,
    FTP_ERR_SOCKET,
    FTP_ERR_POLL,
    FTP_ERR_TIMEOUT,
    FTP_ERR_INVALID,
    FTP_ERR_SERVER,
} ftp_err_t;

*/

/**
 * @brief Send arbitrary buffer over the socket
 * Given an arbitrary length buffer, func will break it up into packets
 * and send the chunks through the socket to the address
 */
ftp_err_t ftp_send_data(int outfd, int infd) {
    char      buf[FTP_PACKET_SIZE] = {0};
    ftp_err_t err                  = FTP_ERR_NONE;
    while (err == FTP_ERR_NONE) {
        // Read from the input file
        // TODO: Loop until all data is written
        ssize_t nbytes = read(infd, buf, FTP_PACKET_SIZE);
        if (nbytes < 0) {
            perror("Error reading from file");
            return FTP_ERR_ARGS;
        }
        // Send the data to the server
        err = ftp_send_msg(outfd, FTP_CMD_DATA, buf, nbytes);
        if (err != FTP_ERR_NONE) {
            return err;
        }
    }
    // Send the termination message
    return ftp_send_msg(outfd, FTP_CMD_TERM, NULL, 0);
}

/**
 * @brief recieve FTP_CMD_DATA chunks from sockfd until either a timeout occurs
 * or an FTP_CMD_ERROR is recieved indicating failure or FTP_CMD_TERM is
 * recieved indicating success.
 */
ftp_err_t ftp_recv_data(int infd, int outfd) {
    ftp_msg_t msg = {0};
    ftp_err_t err = FTP_ERR_NONE;
    while (err == FTP_ERR_NONE) {
        err = ftp_recv_msg(infd, &msg);
        if (err != FTP_ERR_NONE) {
            return err;
        }
        switch (msg.cmd) {
        case FTP_CMD_DATA:
            // Write the data to the output file
            // TODO: Loop until all data is written
            if (write(outfd, msg.packet, msg.nbytes) < 0) {
                perror("Error writing to file");
                return FTP_ERR_ARGS;
            }
            break;
        case FTP_CMD_TERM:
            return FTP_ERR_NONE;
        case FTP_CMD_ERROR:
            return FTP_ERR_SERVER;
        default:
            return FTP_ERR_INVALID;
        }
    }
    return FTP_ERR_NONE;
}

/**
 * @brief Send a single command packet, used for setting up or ending
 * transactions. Use arglen -1 for strings (uses strlen to copy the relevant
 * bit)
 */
ftp_err_t ftp_send_msg(int outfd, ftp_cmd_t cmd, const char *arg, ssize_t len) {
    ftp_msg_t msg = {0};
    msg.cmd       = cmd;
    if (len > FTP_PACKET_SIZE) {
        return FTP_ERR_ARGS;
    }
    if (len == -1) {
        len = strlen(arg);
    }
    strncpy(msg.packet, arg, len);
    msg.nbytes = len;

    size_t bytes_sent;
    while (bytes_sent < FTP_MSG_SIZE) {
        ssize_t ret =
            send(outfd, &msg + bytes_sent, FTP_MSG_SIZE - bytes_sent, 0);
        if (ret < 0) {
            return FTP_ERR_SOCKET;
        }
        bytes_sent += ret;
    }
    return FTP_ERR_NONE;
}

/**
 * @brief Recieve a single command packet, useful for establishing a link (ACK)
 */
ftp_err_t ftp_recv_msg(int infd, ftp_msg_t *msg) {
    if (infd <= 0 || msg == NULL) {
        return FTP_ERR_ARGS;
    }
    bzero(msg, FTP_MSG_SIZE);
    size_t bytes_recv;
    while (bytes_recv < FTP_MSG_SIZE) {
        struct pollfd fds = {0};
        fds.fd            = infd;
        fds.events        = POLLIN;
        int ret_poll      = poll(&fds, 1, TIMEOUT_MS);
        if (ret_poll < 0) {
            return FTP_ERR_POLL;
        } else if (ret_poll == 0) {
            return FTP_ERR_TIMEOUT;
        }
        ssize_t ret =
            recv(infd, msg + bytes_recv, FTP_MSG_SIZE - bytes_recv, 0);
        if (ret < 0) {
            return FTP_ERR_SOCKET;
        }
        bytes_recv += ret;
    }
    return FTP_ERR_NONE;
}

/**
 * @brief Return a string representation of the ftp_cmd_t
 *
 */
const char *ftp_cmd_to_str(ftp_cmd_t cmd) {
    switch (cmd) {
    case FTP_CMD_GET:
        return "GET";
    case FTP_CMD_PUT:
        return "PUT";
    case FTP_CMD_LIST:
        return "LIST";
    case FTP_CMD_ERROR:
        return "ERROR";
    case FTP_CMD_DATA:
        return "DATA";
    case FTP_CMD_TERM:
        return "TERM";
    default:
        return "INVALID";
    }
}
