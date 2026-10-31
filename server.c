#include <arpa/inet.h> //htons(), htonl(), inet_ntop()
#include <errno.h> //errno, EINTR
#include <netinet/in.h> //sockaddr_in, IPv4 addr consts
#include <stdio.h> //printf(), perror(), fflush()
#include <stdlib.h> //EXIT_SUCCESS, EXIT_FAILURE
#include <sys/socket.h> //socket(), bind(), listen(), accept(), recv()
#include <unistd.h> //close()

#define PORT 9002

int main(void) //currently 1 TCP connection & observes incoming data
{
    int server_socket = socket(AF_INET, SOCK_STREAM, 0); //create ipv4 TCP socket --returned descriptor is initially unbound

    if (server_socket == -1) {
        perror("Server: socket creation failed");
        return EXIT_FAILURE;
    }

    printf("Server: socket CREATION success.\n");

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

    printf("Server: socket BIND success.\n");


    if (listen(server_socket, 1) == -1) {
        perror("Server: listen failed");
        close(server_socket);
        return EXIT_FAILURE;
    }

    printf("Server: socket LISTEN success on 127.0.0.1:%d.\n", PORT); //listen success. socket is a listening socket
    fflush(stdout);

    struct sockaddr_in client_address;
    socklen_t client_length; //socklen_t directly as type (not int)
    int client_socket;

    do {
        client_length = sizeof(client_address);
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_length); //accept() fills separate struct with the client addr
    }
    while (client_socket == -1 && errno==EINTR);

    if (client_socket == -1) {
        perror("Server: accept failed");
        close(server_socket);
        return EXIT_FAILURE;
    }

    printf("Server: socket ACCEPT success.\n");

    char client_ip[INET_ADDRSTRLEN];

    if (inet_ntop(AF_INET, &client_address.sin_addr,client_ip, sizeof(client_ip))!=NULL) {
        printf("Server: client connected from %s:%u.\n", client_ip, (unsigned int)ntohs(client_address.sin_port));
    }

    close(server_socket); //close listening socket cuz only 1 client sesh required
    fflush(stdout);

    char buffer[1024];
    int server_status=EXIT_SUCCESS;

    while (1)
    {
        ssize_t received = recv(client_socket,buffer,sizeof(buffer),0); // 1 recv result != 1 complete cmd

        if (received>0) {
            printf("Server: RECEIVED %zd bytes.\n",received);
            fflush(stdout);
        }
        else if (received==0) {
            printf("Server: client disconnected.\n"); //peer has closed its sending side 
            break;
        }
        else {
            if (errno==EINTR) {
                continue; //retry a receive interrupted by a signal
            }

            perror("Server: recv failed");
            server_status=EXIT_FAILURE;
            break;
        }
    }

    close(client_socket); //release connected socket before terminating server
    printf("Server: shutting down.\n");

    return server_status;
}
