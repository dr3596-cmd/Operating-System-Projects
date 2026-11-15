#include <arpa/inet.h> //htons(), htonl(), inet_ntop()
#include <errno.h> //errno, EINTR
#include <netinet/in.h> //sockaddr_in, IPv4 addr consts
#include <stdio.h> //printf(), perror(), fflush()
#include <stdlib.h> //EXIT_SUCCESS, EXIT_FAILURE
#include <sys/socket.h> //socket(), bind(), listen(), accept(), recv()
#include <unistd.h> //close()
#include <signal.h> //SIGPIPE handling
#include <pthread.h> //pthread_create(), pthread_detach()
#include <string.h> //strerror()

#include "parser.h"
#include "protocol.h"
#include "remote_executor.h"
#include "log.h" //clinet info and all server log printing done here

// Function executed by each thread to handle communication with multiple clients

void* Threadfunction(void *arg)
{
    ClientInfo *client_info = (ClientInfo *)arg; 
    int client_socket = client_info->socket;
    char command[MAX_SIZE]; // Buffer to store incoming messages from the client

    while (1)
    {
        int received = receive_command(client_socket, command, sizeof(command)); //receive compelte cmd

        if (received==0) {
            printf("[INFO] Client disconnected.\n");
            break;
        }
        if (received==-1) { //end sesh after failure
            log_failure(client_info, "Command reception failed", errno);
            break;
        }

        printf("\n");
        log_event(client_info, "RECEIVED", "Received command: \"%s\"",command);

        if(strcmp(command, "exit")==0){
            log_event(client_info, "INFO", "Client requested disconnect. Closing connection.");
            break; //end only this client's session and keep accepting other clients
        }

        log_event(client_info, "EXECUTING", "Executing command: \"%s\"",command);

        int result = execute_remote_command(command, client_info);

        if (result==-1) {
            log_failure(client_info, "Execution or output transfer failed", errno);
            break;
        }
        if (result==1){ // parser itself reported exit
            log_event(client_info, "INFO", "Client requested disconnect. Closing connection.");
            break;
        }

        if (send_marker(client_socket) == -1) { //all output has been forwarded.
            log_failure(client_info, "Execution or output transfer failed", errno);
            break;
        }
    }

    log_client_disconnected(client_info);
    close(client_socket); //release this client's socket; the server keeps running
    free(client_info); // free this client's details
    return NULL;


}

int main(void) //multithreaded TCP server; one thread per connected client
{
    int client_counter = 0; //keep up with client's number 
    int server_socket, client_socket; //server_socket is the main server socket while client_socket is for each connected client
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

    log_event(NULL, "INFO", "Server started, waiting for client connections...");

    struct sockaddr_in client_address;
    //setting length of the client address structure 
    socklen_t client_length; //socklen_t directly as type (not int)

    while(1){

        //Accept a new connection from a client
        do {
            client_length = sizeof(client_address);
            client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_length); //accept() fills separate struct with the client addr
        }
        while (client_socket == -1 && errno==EINTR);

        if (client_socket == -1) {
            log_failure(NULL, "Accept failed", errno);
            continue; //continue to the next iteration
        }

        client_counter++;  //counted only after a successful accept()

        //Build the client's info 

        ClientInfo *client_info = malloc(sizeof(*client_info)); //each client gets its own struct so threads don't share data

        if (client_info == NULL) //refuse the client but keep the server running
        {
            log_failure(NULL, "Client info allocation failed", errno);
            close(client_socket);
            continue;

        }

        //fill id, socket, ip text and port number
        client_info_init(client_info, client_counter, client_socket, &client_address);

        log_client_connected(client_info);

        //we need to continue listening to multiple clients
        pthread_t thread; //Declare a thread identifier 

        int thread_error = pthread_create(&thread, NULL, Threadfunction, client_info);//start a new thread and pass the client's socket

        if (thread_error != 0)
        {
            log_failure(client_info, "Thread creation failed", thread_error);
            free(client_info);
            close(client_socket);
            continue;
        }

        pthread_detach(thread); //detach the thread to allow it to clean up its resources when done

    }

    
}

