#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include<string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdint.h>
#include <netinet/in.h>
#include <signal.h>
#include <arpa/inet.h>

#include "protocol.h" //port, default_ip, send_command(),...


#define MAX_SIZE 256 // max input line length; newline included

/* The following determines how the client and the server communicate. The client sends the command given by the user
the server answers with chunks of output, each one equivalent to 4-bytes in length + any remaining
then the server ends the answer with a length of 0, so the lcient knows the command is successfully finished*/

int main(int argc, char* argv[])
{

	char line[MAX_SIZE]; //the command types by the user 
	char server_response[MAX_SIZE]; //holds a piece of the server's output at a time
	const char *ip = default_ip; // server address
	int port = PORT; //port required for communication



	//if the server disappears or disconnects, send() must return an error instead of killing the client

	signal (SIGPIPE, SIG_IGN);

	//create a socket
	//use an int to hold the fd for the socket
	int fd_socket;
	
	fd_socket = socket(AF_INET , SOCK_STREAM, 0);

	//check for fail error
	if (fd_socket == -1) {
        printf("socket creation failed..\n");
        exit(EXIT_FAILURE);
    }
    

	//connect to another socket on the other side
	
	//specify an address for the socket we want to connect to
	struct sockaddr_in server_address;

	//initilaize the structure with zero before the connection happens
	memset(&server_address, 0, sizeof(server_address));
	
	//specify address family
	server_address.sin_family = AF_INET;
	
	//specify the port we want to connect to remotely
	//htons converts integer port to right format for the structure
	//9002 just some port we know the OS is not using
	server_address.sin_port = htons(PORT);

	//specify the actual IP address
	//inet_pton converts the text form of the ip_address ("127.0.0.1") into the binary form the structure needs, and 
	//returns 1 only when the text is a valid IPv4 address
	if (inet_pton(AF_INET, ip, &server_address.sin_addr) != 1)
	{
		fprintf(stderr, "Error: Invalid IP address.\n");
		close(fd_socket);
		exit(EXIT_FAILURE);
	} 

	//connect
	//1st : socket 
	//2nd: server address structure , cast to a pointer to a sockaddr struct so pass the address
	//3rd: size of address
	int status = 
	connect(fd_socket, 
			(struct sockaddr *) &server_address,
			sizeof(server_address));

	//check for errors with the connection
	if(status == -1){
		perror("Connect failed");
		close(fd_socket);
		exit(EXIT_FAILURE);
	}

	//now that we have the connect, we either send or receive data
	//since this is the client, we receive data

	//we make an infinite loop that receives results of commands back from the server everytime the client 
	//sends user commands to the server. 

	// loop: read a command, send to server, display the answer

 
    while(1)
    {
        printf("$ "); //client presents a prompt to the user
		fflush(stdout);

        //fgets returns NULL on end of input (Ctrl + D); leaves the shell clean, closes the socket and makes the server shut down too
        if (fgets(line, sizeof(line), stdin)==NULL)
        {
            printf("\n");
            break;
        }

		//Reject commands that do not fit completely in the input buffer. A valid input line 
		//must contin its newline character 

		//discard the resu=t of the line so it can't become a second command

        if (strchr(line,'\n') == NULL && strlen(line) == sizeof(line)-1) { //if buffee full w/o newline, check if more chars belong to this line. dont allow oversized cmd
            int ch=getchar();

            if (ch != '\n' && ch != EOF) {
                while (1) {
                    ch = getchar();
                    if (ch=='\n' || ch==EOF) {
                        break; //else discard this char and read next one
                    }
                }

                fprintf(stderr,"Error: Input line is too long.\n");
                continue;
            }
        }

		line [strcspn(line, "\n")] = '\0'; // remove the newline

		if(line [strspn(line, "\t")] == '\0')
		{
			continue;
		}

		//send the command followed by a newline, which is how the server knows the command is complete
		int length = strlen(line);
		line[length]= '\n';
            
        int bytes_sent = send_command(fd_socket, line, length+1);

		if(bytes_sent == -1)
		{
			perror("Send failed!");
			break;
		}

		line [length] = '\0'; //remove the newline

		//check if the user entered the exit command; we check after sending it so the server also receives the command and 
		//shuts down. Only "exit" alone counts a command to terminate the programs, other variations are cosnidered as 
		//normal commands
		
		char word1[MAX_SIZE];
		char word2[MAX_SIZE];
		int word_count = sscanf(line, "%255s %255s", word1, word2);

		if(word_count == 1 && strcmp(word1, "exist")==0)
		{
			break; //the client disconnects
		}

		//Receive the server's response; a length of 0 means the command results have been 
		//completely sent from the server. A response arrives in chuncls 

		int cmd_finished = 0;
		char last_character = '\n';

		while(!cmd_finished)
		{
			uint32_t network_length;

			//first receive the four-byte length header
			int result = recv_all(fd_socket, &network_length, sizeof(network_length));

			if (result == 0)
			{
				fprintf(stderr, "Server disconnected\n");
				break;
			}

			if (result == -1)
			{
				perror("Receive failed");
				break;
			}

			// Convert the received length into the host's byte order
			uint32_t remaining = ntohl(network_length);

			//A length of 0 indicates that the command is complete

			if(remaining == 0)
			{
				cmd_finished = 1;
				continue;
			}

			while(remaining > 0)
			{
				size_t chunk;

				if(remaining < sizeof(server_response))
				{
					chunk = remaining;
				}

				else {
					chunk = sizeof(server_response);
				}

				int received = recv_all (fd_socket, server_response, chunk);

				if (received == 0)
				{
					fprintf(stderr, " Server disconnected\n");
					break;
				}

				else if (received == -1)
				{
					perror ("Receive failed");
					break;
				}

				// display the bytes received

				fwrite(server_response, 1, chunk, stdout);
				last_character = server_response [chunk - 1];

				remaining = remaining - chunk;

			}

			if (remaining > 0)
			{
				break;
			}

		}

		if(!cmd_finished)
		{
			break;
		}

		if(last_character != '\n')
		{
			printf("\n");
		}

		fflush(stdout);

    }

	
    //close socket after we are done
	close(fd_socket);

	return 0;
}