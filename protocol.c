#include <sys/socket.h> // send, recv
#include <sys/types.h> // ssize_t
#include <stdint.h> //uint32_t
#include <arpa/inet.h> //htonl
#include "protocol.h"


//function to send all bytes of the command to the server
//send() may sent fewer bytes than requested, so we repeat the send process until everything is sent
int send_command(int socket_fd, const char *data, int length)
{
	int total_sent = 0;
	while(total_sent < length)
	{
		int bytes_sent = send(socket_fd, data + total_sent, length - total_sent,0);

		if(bytes_sent <= 0){
			return -1; //if sending fails
		}

		total_sent = total_sent + bytes_sent;

	}
	return total_sent;
} 

// Receives exactly the sent bytes from the server, the client returns 1 if all bytes are received
// 0 if the connection is closed and -1 if an error occured 

int recv_all(int socket_fd, void *buffer, size_t length)
{
	size_t total_received = 0;
	while(total_received < length)
	{
		size_t bytes_received = recv(socket_fd , (char*)buffer + total_received , length - total_received,0);

		if(bytes_received < 0){
			return -1; //if sending fails
		}

		else if(bytes_received == 0)
		{
			return 0;
		}

		total_received = total_received + bytes_received;

	}
	return 1;
} 

int send_chunk (int socket_fd, const char *data, size_t length)
{
    if (length == 0)
    {
        return 0; 
    }

    uint32_t network_length = htonl ((uint32_t)length);

    if (send_command (socket_fd,(const char*)&network_length,sizeof(network_length))==-1)
    {
        return -1;
    }

    return (send_command(socket_fd, data, (int) length)==-1) ? -1 : 0;
}

//A length f 0 tells the client that the command's output is complete


int send_marker (int socket_fd)
{
    uint32_t zero = htonl(0);
    return (send_command(socket_fd, (const char*)&zero, sizeof(zero)) == -1)?-1:0; 
}
