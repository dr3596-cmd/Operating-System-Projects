#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PORT 9002

#define default_ip "127.0.0.1" // define the loopback address using this default ip

#include <stddef.h> //size_t

// Sends a command entered by the client to the server
int send_command (int socket_fd, const char *data, int length);

//Recieves exactly the requested number of bytes from teh socket unless the connection
//closes due to an erro r
int recv_all (int socket_fd, void *buffer, size_t length);

//Sends one chunk of output from the server to the client; the server sends a 4-byte length header before sending the actual chunk
//The client reads this header to know how many output bytes it should receive next
int send_chunk (int socket_fd, const char *data, size_t length);

//Sends a special completion marker to the client. This marker in our protocol is represented by a length of 0
//it tells the client that the server has finished sending the output of the current command
int send_marker (int socket_fd);

//Receives one complete command form the client. This function reads incoming data unitl it enouters a nelwine character, which marks 
//the end of a command 
//This is needed becuaste TCP can't determine boundaries between commands 
int receive_command(int socket_fd, char *buffer, size_t capacity); //receive 1 complete newline-terminated cmd

#endif