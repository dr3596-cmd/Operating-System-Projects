#include <arpa/inet.h> //htons(), htonl(), inet_ntop()
#include <errno.h> //errno, EINTR
#include <netinet/in.h> //sockaddr_in, IPv4 addr consts
#include <stdio.h> //printf(), perror(), fflush()
#include <stdlib.h> //EXIT_SUCCESS, EXIT_FAILURE
#include <sys/socket.h> //socket(), bind(), listen(), accept(), recv()
#include <unistd.h> //close()
#include <signal.h> //SIGPIPE handling

#include "parser.h"
#include "protocol.h"
#include "remote_executor.h"

// Function executed by each thread to handle communication with multiple clients

void* Threadfunction(void *client_socket){
    int new_socket = *(int *)arg; //Get the client socket file descriptor

    fflush(stdout);

    char command[MAX_SIZE]; // Buffer to store incoming messages from the client

    while (1)
    {
        int received = receive_command(client_socket, command, sizeof(command)); //receive compelte cmd

        if (received==0) {
            printf("[INFO] Client disconnected.\n");
            break;
        }
        if (received==-1) { //end sesh after failure
            perror("Server: command reception failed");
            server_status = EXIT_FAILURE;
            break;
        }

        
        printf("[RECEIVED] Received command: \"%s\" from client.\n", command);
        printf("[EXECUTING] Executing command: \"%s\"\n", command);
        fflush(stdout);

        int result = execute_remote_command(command, client_socket);

        if (result==-1) {
            perror("Server: execution or output transfer failed");
            server_status = EXIT_FAILURE;
            break;
        }
        if (result==1){
            printf("[INFO] Exit command received; shutting down.\n");
            break;
        }

        if (send_marker(client_socket) == -1) { //all output has been forwarded.
            perror("Server: completion send failed");
            server_status = EXIT_FAILURE;
            break;
        }

        fflush(stdout);
    }

    close(client_socket); //release connected socket before terminating server
    printf("Server: shutting down.\n");

    return server_status;


}

int main(void) //currently 1 TCP connection & observes incoming data
{
    int client_counter = 0;
    int server_socket, client_socket; //server_socket is the main server socket while new_socket is for each connected client
    if (signal(SIGPIPE,SIG_IGN)==SIG_ERR) { //disconnected client => send() reports error
        perror("Server: SIGPIPE setup failed");
        return EXIT_FAILURE;
    }

    server_socket = socket(AF_INET, SOCK_STREAM, 0); //create ipv4 TCP socket --returned descriptor is initially unbound

    if (server_socket == -1) {
        perror("Server: socket creation failed");
        return EXIT_FAILURE;
    }

    int reuse = 1; //can reuse local addr during repeated tests (but no 2nd listener on same endpoint)

    if (setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR, &reuse, sizeof(reuse))==-1) {
        perror("Server: setsockopt failed");
        close(server_socket);
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_address = {0}; //0 initialize struct
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT); //port to network byte order
    server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); //loopback=restricts to clients on this machine

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address))==-1) { //assign local addr & port to socket
        perror("Server: socket bind failed");
        close(server_socket);
        return EXIT_FAILURE;
    }

    //Start listening for incoming connections, with a backlog of 5 (maximu number of pending connections)

    if (listen(server_socket, 5) == -1) {
        perror("Server: listen failed");
        close(server_socket);
        return EXIT_FAILURE;
    }

    printf("[INFO] Server started, waiting for client connections...\n"); //listen success. socket is a listening socket
    fflush(stdout);

    struct sockaddr_in client_address;
    //setting length of the client address structure 
    socklen_t client_length; //socklen_t directly as type (not int)
    int *client_fd= malloc(sizeof(int));
    *client_fd = client_socket;

    while(1){
        //clear the buffer to receive new message form the client
        bzero(buffer, MAX_SIZE);

        //Accept a new connection from a client
        do {
            client_length = sizeof(client_address);
            client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_length); //accept() fills separate struct with the client addr
            client_counter++;
        }
        while (client_socket == -1 && errno==EINTR);

        if (client_socket == -1) {
            perror("Server: accept failed");
            continue; //continue to the next iteration
        }
        printf("[INFO] Client %d connected.\n",client_counter);

        //we need to continue listening to multiple clients
        pthread_t thread; //Declare a thread identifier 
        pthread_create(&thread, NULL, Threadfunction, (void*)&client_fd);//start a new thread and pass the client's socket
        pthread_detach(thread); //detach the thread to allow it to clean up its resources when done

    }

    
}

