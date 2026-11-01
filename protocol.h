#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PORT 9002

#define default_ip "127.0.0.1"

#include <stddef.h> //size_t

int send_command (int socket_fd, const char *data, int length);


int recv_all (int socket_fd, void *buffer, size_t length);

int send_chunk (int socket_fd, const char *data, size_t length);

int send_marker (int socket_fd);

int receive_command(int socket_fd, char *buffer, size_t capacity); //receive 1 complete newline-terminated cmd

#endif