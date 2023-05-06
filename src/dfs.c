/**
 * @file dfs.c
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief Distributed File System Server Implementation
 * @version 0.1
 * @date 2023-05-06
 * 
 * @copyright Copyright (c) 2023
 * 
 * @details The DFS Server will be stateless and dead simple.
 * It will listen for chunks using the TCP protocol and store
 * them in a directory. It will also listen for requests to
 * retrieve chunks and send them back to the client. The server
 * will be able to handle multiple clients at once. The messaging
 * protocol is defined in protocol.h and the README.md file.
 */
