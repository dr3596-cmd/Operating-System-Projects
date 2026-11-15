#include <pthread.h> //mutex
#include <stdarg.h> //va_list
#include <stdio.h> //fprintf(), fwrite(), fflush()
#include <string.h> //strerror(), strncpy()

#include "log.h"

//1 mutex for all printing --concurrent threads never mix lines. strerror() is only called while holding it (makes that call safe too)
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER; //sets the mutex up at compile time

//FORMAT: "[TAG] [Client #N - ip:port] " prefix
static void print_prefix(FILE *stream, const char *tag, const ClientInfo *client)
{
    fprintf(stream, "[%s] ", tag);

    if (client != NULL) { //client is NULL for server-wide lines
        fprintf(stream, "[Client #%u - %s:%u] ", client->id,client->ip,client->port);
    }
}

int client_info_init(ClientInfo *client, unsigned int id, int socket, const struct sockaddr_in *address) //fill in a ClientInfo from what accept() gives
{
    if (client==NULL || address==NULL)  {
        return -1; //cannot deref null pointers
    }

    client->id=id;
    client->socket=socket;
    client->port=(unsigned int)ntohs(address->sin_port); //network -> host order

    if (inet_ntop(AF_INET, &address->sin_addr,client->ip, sizeof(client->ip)) == NULL) { //if ip addr null
        strncpy(client->ip, "unknown", sizeof(client->ip)-1);
        client->ip[sizeof(client->ip)-1] = '\0'; //add the null term
        return -1;
    }

    return 0;
}

void log_event(const ClientInfo *client, const char *tag, const char *format, ...) //print 1 complete log line
{
    pthread_mutex_lock(&log_mutex); //lock. only this thread prints

    print_prefix(stdout,tag,client);

    va_list args; //variable arg list --a func with "..." cant call fprintf() directly, so we gather the extra args in a va_list and pass them to vfprintf()
    va_start(args,format); //point args to 1st extra arg (after format)
    vfprintf(stdout,format,args); //caller's message
    va_end(args); //cleans up list

    fputc('\n',stdout);
    fflush(stdout); //so fork() never copies buffered text

    pthread_mutex_unlock(&log_mutex);
}

void log_client_connected(const ClientInfo *client) //prints connection msg. thread num = client num
{
    pthread_mutex_lock(&log_mutex);

    fprintf(stdout, "[INFO] Client #%u connected from %s:%u. Assigned to Thread-%u.\n",client->id,client->ip,client->port,client->id);
    fflush(stdout);

    pthread_mutex_unlock(&log_mutex);
}

void log_client_disconnected(const ClientInfo *client) //prints disconnection msg. called by client thread before it closes socket
{
    pthread_mutex_lock(&log_mutex);

    fprintf(stdout, "[INFO] Client #%u disconnected.\n", client->id);
    fflush(stdout);

    pthread_mutex_unlock(&log_mutex);
}

void log_event_with_data(const ClientInfo *client, const char *tag, const char *header, const char *data,size_t length) //print header line + raw output
{
    pthread_mutex_lock(&log_mutex);

    print_prefix(stdout,tag,client);
    fprintf(stdout, "%s\n",header);
    fwrite(data,1,length,stdout); //raw bytes
    fflush(stdout);

    pthread_mutex_unlock(&log_mutex);
}

void log_raw(const char *data, size_t length) //prints more raw output Bs w no prefix, for the later chunks of output after log_event_with_data() printed 1st one
{
    pthread_mutex_lock(&log_mutex);

    fwrite(data,1,length,stdout);
    fflush(stdout);

    pthread_mutex_unlock(&log_mutex);
}

void log_output_end(char last_character) //ends 1 cmds output block
{
    pthread_mutex_lock(&log_mutex);

    if (last_character != '\n') {
        fputc('\n', stdout); //finish an unterminated last line
    }

    fputc('\n', stdout); //blank separator line
    fflush(stdout);

    pthread_mutex_unlock(&log_mutex);
}

void log_error_response(const ClientInfo *client, const char *command, const char *message, size_t length) //prints  "command not found" block in 1 locked operation
{
    pthread_mutex_lock(&log_mutex); //lock

    print_prefix(stdout, "ERROR",client);
    fprintf(stdout, "Command not found: \"%s\"\n", command);

    while (length>0 && message[length-1] == '\n') {
        length--; //keep closing quote on the same line
    }

    print_prefix(stdout, "OUTPUT", client);
    fputs("Sending error message to client: \"", stdout);
    fwrite(message,1,length,stdout);
    fputs("\"\n\n", stdout);   //closing quote, then blank separator line
    fflush(stdout);

    pthread_mutex_unlock(&log_mutex);
}

void log_failure(const ClientInfo *client, const char *context, int err) //report failed system call on stderr
{
    pthread_mutex_lock(&log_mutex);

    print_prefix(stderr, "ERROR",client);
    fprintf(stderr, "%s: %s\n", context, strerror(err));
    fflush(stderr);

    pthread_mutex_unlock(&log_mutex);
}

//hold the log mutex around fork()
void log_lock(void)
{
    pthread_mutex_lock(&log_mutex);
}

void log_unlock(void)
{
    pthread_mutex_unlock(&log_mutex);
}