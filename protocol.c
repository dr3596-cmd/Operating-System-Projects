#include <sys/socket.h> // send, recv
#include <sys/types.h> // ssize_t
#include <stdint.h> //uint32_t
#include <arpa/inet.h> //htonl
#include "protocol.h"
#include <errno.h> //errno, EINTR, EINVAL, EPROTO, EMSGSIZE


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
		ssize_t bytes_received = recv(socket_fd , (char*)buffer + total_received , length - total_received,0); //ssize_t since recv rets -1 on error

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


int receive_command(int socket_fd, char *buffer, size_t capacity) //receive 1 cmd using clients newline delimiter
{
    if (buffer==NULL || capacity==0) {//dest buffer must have @least room for \0
        errno=EINVAL;
        return -1;
    }

    size_t used = 0;

    buffer[0] = '\0'; //start w empty local str

    while (1)
    {
        char character;

        ssize_t received = recv(socket_fd, &character,1,0); //recv rets signed count. request 1B from cmd stream

        if (received == -1) {
            if (errno==EINTR) {
                continue; //recv interrupted by signal, retry
            }

			return -1; //preserve socket error for caller's perror()
        }

        if (received==0) {
            if (used==0) {
                return 0; //normal disconnect between cmds
            }

            errno=EPROTO; //some cmd bytes arrived but not final newline. dont execute incomplete cmd
            return -1;
        }

        if (character == '\n') {
            buffer[used] = '\0'; //complete cmd -> replace wire delimter with \0
            return 1;
        }

        if (character == '\0') {//embedded null bytes are invalid
            errno=EPROTO;
            return -1;
        }

        if (used >= capacity-1) { //reserve last buffer pos for terminating \0
            errno=EMSGSIZE;
            return -1;
        }

        buffer[used]=character; //store next cmd byte & maintain a valid local string
        used++;
        buffer[used] = '\0';
    }
}