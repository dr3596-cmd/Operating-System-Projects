#ifndef LOG_H
#define LOG_H

#include <arpa/inet.h> //inet_ntop(), INET_ADDRSTRLEN
#include <netinet/in.h> // struct sockaddr_in
#include <stddef.h> // size_t

typedef struct { //1 connected client --accept loop makes 1 per connection
    unsigned int id;  //client number, and thread-N label
    int socket; //connected socket for this client
    char ip[INET_ADDRSTRLEN]; //client ip
    unsigned int port; //client port
} ClientInfo;

//fill a ClientInfo from accept's addr -- 0=ok, -1=ip unreadable. pass NULL for client to omit bracketed client part. cannot call while holding the log lock
int client_info_init(ClientInfo *client, unsigned int id, int socket, const struct sockaddr_in *address); 

void log_event(const ClientInfo *client, const char *tag, const char *format, ...); //1 log line: [TAG] [Client #N - ip:port] msg

//"[INFO] Client #N connected from ip:port. Assigned to Thread-N." 
void log_client_connected(const ClientInfo *client);

//"[INFO] Client #N disconnected."
void log_client_disconnected(const ClientInfo *client);

//header line, then raw output bytes, in 1 locked operation
void log_event_with_data(const ClientInfo *client, const char *tag, const char *header, const char *data, size_t length);

//later chunks of the same output (raw bytes)
void log_raw(const char *data, size_t length);

//end of a command's normal output-- finish an unterminated last line, then print blank separator line
void log_output_end(char last_character);

//"command not found" block, ending with the blank line:
//[ERROR] [Client ...] Command not found: "<command>"
//[OUTPUT] [Client ...] Sending error message to client: "<message>"
void log_error_response(const ClientInfo *client, const char *command, const char *message, size_t length);

//system-call failure on stderr: [ERROR] [Client ...] context: reason
void log_failure(const ClientInfo *client, const char *context, int err);

//hold around fork() ONLY. call no other log function in between
void log_lock(void);
void log_unlock(void);

#endif