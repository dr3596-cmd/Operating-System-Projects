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

typedef struct{
    int client_socket; //connected socket returned by accept()
    int client_number; //client count in connection order
    char client_label[64]; //Client #N = ip;port
}ClientInfo;

// Function executed by each thread to handle communication with multiple clients

void* Threadfunction(void *arg)
{
    ClientInfo *clinet_info = (ClientInfo *)arg; 
    int client_socket = client_info->client_socket;
    const char *client_label = client_info -> client_label;

    int thread_status = EXIT_SUCCESS; //becomes EXIT_FAILURE if the session ends because of an error
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
            thread_status = EXIT_FAILURE;
            break;
        }

        int exit_requested = (strcmp(command, "exit") == 0);

        log_lock();
        printf("[RECEIVED] [%s] Received command: \"%s\"\n", client_label, command);

        if(exit_requested){
            printf("[INFO] [%s] Client requested disconnect. Closing connection.\n", client_label);
        }

        else{
            printf("[EXECUTING] [%s] Executing command: \"%s\"\n", client_label, command);
        }
        
        fflush(stdout);
        log_unlock();

        if(exit_requested){
            break; // end only this client's session
        }

        int result = execute_remote_command(command, client_socket, client_label);

        if (result==-1) {
            perror("Server: execution or output transfer failed");
            thread_status = EXIT_FAILURE;
            break;
        }
        if (result==1){ // "exit" entered with multiple spaces
            log_lock();
            printf("[INFO] [%s] Exit command received; shutting down.\n", client_label);
            fflush(stdout);
            log_unlock();
            break;
        }

        if (send_marker(client_socket) == -1) { //all output has been forwarded.
            perror("Server: completion send failed");
            thread_status = EXIT_FAILURE;
            break;
        }
    }

    log_lock();

    if(thread_status == EXIT_FAILURE)
    {
        printf("[INFO] Client #%d disconnected (session ended due to an error).\n", client_info->client_number);
    }

    else {
        printf("[INFO] Client #%d disconnected.\n", client_info->client_number);
    }

    fflush(stdout);
    log_unlock();

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

    printf("[INFO] Server started, waiting for client connections...\n"); //listen success. socket is a listening socket
    fflush(stdout);

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
            perror("Server: accept failed");
            continue; //continue to the next iteration
        }

        client_counter++; 

        //Build the client's info 

        ClientInfo *client_info = malloc(sizeof(*client_info));

        if (client_info == NULL) //refuse the client but keep the server running
        {
            perror("Server: client info allocation failed");
            close(client_socket);
            continue;

        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_address.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_address.sin_port);

        client_info -> client_socket = client_socket;
        client_info -> client_number = client_counter;
        snprintf(client_info -> client_label, sizeof(client_info -> client_label),"Client #%d - %s:%d", client_counter, client_ip, client_port);

        log_lock();
        printf("[INFO] Client #%d connected from %s:%d. Assigned to Thread-%d.\n",
            client_counter, client_ip, client_port, client_counter);

        fflush(stdout);
        log_unlock();

        //we need to continue listening to multiple clients
        pthread_t thread; //Declare a thread identifier 

        int thread_error = pthread_create(&thread, NULL, Threadfunction, client_fd);//start a new thread and pass the client's socket

        if (thread_error != 0)
        {
            fprintf(stderr, "pthread create failed: %s\n", strerror(thread_error));
            free(client_fd);
            close(client_socket);
            continue;
        }

        pthread_detach(thread); //detach the thread to allow it to clean up its resources when done

    }

    
}

