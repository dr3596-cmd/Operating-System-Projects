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


#define MAX_SIZE 256 // max input line length; newline included, for input, we resereve space for newline and 
//the terminating '\0' added by C strings

/* The following determines how the client and the server communicate. 

The client reads a command entered by the user
The cleints sends the command to the server, icnluding a newline
The server executes the command and sends the outpit nalc to the client 

The server sends the output in chunks. Before each chunk, ot sends a 4-byte integer indicating how many bytes belong to that chunk.

The client first reads this 4-byte header, then reads hat many bytes of output 

When a server sends a length of zero, it means the output of the current command is completely finsihed
The ckient displays the received output and returns to the prompt to allow the user to enter the command

*/

int main(void)
{

	char line[MAX_SIZE]; //the command types by the user 
	char server_response[MAX_SIZE]; //holds a piece of the server's output at a time
	const char *ip = default_ip; // server address

	//if the server disappears or disconnects, send() must return an error instead of killing the client

	//Ignoring SIGPIPE allows the client to execute and detect send fialure through its return value instead of getting immediately terminated

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
		//shuts down. Only "exit" alone counts a command to terminate the programs, other exit variations are considered as 
		//normal commands

		// RECEIVE THE SERVER'S RESPONSE
		// cmd_finished tracks whether the server has completely transmitted the output of the current command

		// 0 means the resposne is stilll being received and 1 means the server sent the zero length completion marker
		int cmd_finished = 0;
		char last_character = '\n'; //this remembers the last byte printed form the server's output, it is initialized
		//to newline so that when the server prudces no output, an unnecesary newline is not added

		// Continue receiving until the server indicates that the current command's output is completely finished 
		while(!cmd_finished)
		{
			uint32_t network_length;

			//Receive the 4-byte length header: the server dis sends an integer representing the number
			//of output bytes in the next chunk. recv_all() calls the function menat to receive all the bytes requested for a command
			int result = recv_all(fd_socket, &network_length, sizeof(network_length));

			//if recv_all() returns 0, the server disconnected before completing the expected communication
			if (result == 0)
			{
				fprintf(stderr, "Server disconnected\n");
				break;
			}

			//otherwise a communication error happened
			if (result == -1)
			{
				perror("Receive failed");
				break;
			}

			// Convert the received length into the host's byte order
			uint32_t remaining = ntohl(network_length);

			//Check for the end of response marker
			// A length of 0 means that the server has finished sending the output of the current command
			//No output bytes follow this header so we mark the command as finsihed and return to the outer command loop

			if(remaining == 0)
			{
				cmd_finished = 1;
				continue;
			}

			//Receive the output chunk; here remianing tells us how many bytes belong to thsi chunk. We receive
			//the outpyt in smaller pieces using server_response insead of allocating a large buffer enough to hold the entire command output
			while(remaining > 0)
			{
				size_t chunk;

				//This determines the number of bytes we have to receive, if the number is smaller than the buffer capacity
				//we receive only the remaining bytes, otherwise, we receive one full buffer

				if(remaining < sizeof(server_response))
				{
					chunk = remaining;
				}

				else {
					chunk = sizeof(server_response);
				}

				//recv_all() handles cases where the output arrives pver multiple transmissions

				int received = recv_all (fd_socket, server_response, chunk);

				//if the server disconnects before sending all the expected output bytes, receiving is stopped as well
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

				// display the received output

				fwrite(server_response, 1, chunk, stdout); //This prints exactly chunk bytes and is used becuase the received output is raw data instead
				//of strings terminated by '\0'
				last_character = server_response [chunk - 1]; // this keeps the last byte displayed so we can determine whether the server'soutput ended with a
				//newline or not

				remaining = remaining - chunk;// decerease the remianing by the number of bytes already received. this tracks how much 
				//output is still expected for the current chunk 

			}

			if (remaining > 0)
			{
				break; //if some output bytes are missing, stop reading
			}

		}

		if(!cmd_finished)
		{
			break; //if the zero length completion marker is not received, break the connection
		}

		if(last_character != '\n')
		{
			printf("\n"); //ensure output ends with a newline, so the next command prompt can start on a newline
		}

		fflush(stdout);

    }

	
    //close socket after we are done
	close(fd_socket);

	return 0;
}