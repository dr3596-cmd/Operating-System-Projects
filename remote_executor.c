#include <errno.h> //errno , error consts
#include <fcntl.h> //open(), O_RDONLY.
#include <stdio.h> //fwrite(), fflush(), fprintf(), perror()
#include <stdlib.h> //calloc(), free()
#include <string.h> //strlen(), memcpy()
#include <sys/types.h> //pid_t, ssize_t
#include <sys/wait.h> //waitpid(), child-status macros
#include <unistd.h> //pipe(), fork(), dup2(), read(), close(), _exit()
#include <signal.h> //signal(), SIGPIPE, SIG_DFL

#include "parser.h"
#include "executor.h"
#include "remote_executor.h"
#include "protocol.h"

//worker's outcomes
#define WORKER_COMPLETE 0
#define WORKER_EXIT_REQUESTED 10
#define WORKER_FAILED 20


typedef struct{
    char *data; //collected output
    size_t length; //bytes stored in data
    int started; //check if any output is received
    int error_mode; //was the output an error
}Outputlog;

static int send_command_output(int client_socket, const char *buffer, size_t byte_count) { //send 1 chunk of captured stdout/stderr to client
    return send_chunk(client_socket, buffer, byte_count);
}

static void finish_worker(int exit_code) //finish worker w/o running inherited parent exit handlers
{
    if (fflush(stdout)==EOF) {
        exit_code = WORKER_FAILED;
    }

    if (fflush(stderr)==EOF) {
        exit_code = WORKER_FAILED;
    }

    _exit(exit_code); //doesnt flush stdio, so we flush before
}

static void run_command_worker(const char *command, int capture_read, int capture_write, int client_socket) { //run parsing & execution in worker. called after fork() in child
    close(capture_read); //worker writes captured output, doesnt read the pipe
    close(client_socket); //only supervising process sends responses

    if (dup2(capture_write, STDOUT_FILENO)==-1) { //connect normal command output to the capture pipe
        perror("Worker: stdout redirection failed");
        _exit(WORKER_FAILED);
    }

    if (dup2(capture_write, STDERR_FILENO)==-1) { //connect error output to the same capture pipe
        perror("Worker: stderr redirection failed");
        _exit(WORKER_FAILED);
    }

    close(capture_write); //stdout & stderr now refer to the pipe, so original closed

    if (signal(SIGPIPE,SIG_DFL)==SIG_ERR) { //supervising server ignores SIGPIPE for socket writes
        perror("Worker: SIGPIPE reset failed");
        finish_worker(WORKER_FAILED);
    }

    int input_fd = open("/dev/null", O_RDONLY); //cannot let commands read from the server operator's terminal. /dev/null =>immediate EOF

    if (input_fd==-1) {
        perror("Worker: opening /dev/null failed");
        finish_worker(WORKER_FAILED);
    }

    if (dup2(input_fd, STDIN_FILENO)==-1) {
        perror("Worker: stdin redirection failed");
        close(input_fd);
        finish_worker(WORKER_FAILED);
    }

    //stdin now has its own reference to /dev/null
    close(input_fd);

    long max_fd = sysconf(_SC_OPEN_MAX);

    if (max_fd < 0 || max_fd>4096){
        max_fd = 4096;
    }

    for (int fd = STDERR_FILENO + 1; fd < max_fd; fd++){
        close(fd);
    }

    char line[MAX_SIZE];
    memcpy(line, command, strlen(command) + 1); //work on copy cuz find_pipes() modifies the line when splitting pipelines

    Command *commands = calloc(MAX_SIZE, sizeof(*commands)); //alloc Command array on heap

    if (commands==NULL) {
        perror("Worker: command allocation failed");
        finish_worker(WORKER_FAILED);
    }

    int parse_result = -1; //same parser entry point as local shell
    int command_count = find_pipes(line, commands, &parse_result);

    if (parse_result==1) {
        free(commands); //report exit to parent instead of terminating parent
        finish_worker(WORKER_EXIT_REQUESTED);
    }

    if (parse_result==2) {
        execute_commands(commands, command_count); //Phase 1 execution
    }
    else if (parse_result!=0 && parse_result != -1) {
        fprintf(stderr, "Worker: unexpected parser result.\n");
        free(commands);
        finish_worker(WORKER_FAILED);
    }

    free(commands);
    finish_worker(WORKER_COMPLETE);
}

static void log_output_chunk(Outputlog *output_log, const char *buffer, size_t count){
    if(!output_log->started)
    {
        output_log->started = 1;

        if (strstr(buffer, "Command not found") != NULL)
        {
            output_log->error_mode = 1;
        }
    }

    char *grown = realloc(output_log->data, output_log->length + count);
    if (grown == NULL)
    {
        return;
    }

    memcpy(grown + output_log->length, buffer, count);
    output_log->data = grown;
    output_log->length += count;
}

static void print_output_log(const char *command, const ClientInfo *client, Outputlog *output_log) 
{
    if (!output_log->started) { //command produced no output; nothing to log
        return;
    }

    if(output_log->error_mode)
    {

        log_error_response(client, command, output_log->data, output_log ->length);
        return;
    }

    if(output_log->length == 0){
        log_event(client, "OUTPUT", "Sending output to client:");
        return;
    }

    char lastchr = output_log->data[output_log->length - 1];
    size_t ending_length = (lastchr == '\n') ? 1 : 2;

    char *grown = realloc(output_log->data, output_log->length + ending_length);

    if(grown == NULL)
    {
        log_event_with_data(client, "OUTPUT", "Sending output to client:", output_log->data, output_log->length);
        log_output_end(lastchr);
        return;
    }

    output_log->data = grown;

    if(ending_length == 2)
    {
        output_log->data[output_log->length++] = '\n';
    }

    output_log->data[output_log->length++] = '\n';

    log_event_with_data(client, "OUTPUT","Sending output to client:", output_log->data, output_log->length);
        

}


int execute_remote_command(const char *command, const ClientInfo *client)
{
    if (command==NULL || client==NULL || client->socket<=STDERR_FILENO) {
        errno=EINVAL;
        return -1;
    }

    int client_socket = client->socket;

    if (strlen(command) >= MAX_SIZE) {//leave room for the terminating \0 in the Phase 1 line buffer
        errno = EMSGSIZE;
        return -1;
    }

    log_lock();

    if (fflush(NULL) == EOF) { //flush parent output before fork
        log_unlock();
        return -1;
    }

    int capture_pipe[2];

    if (pipe(capture_pipe) == -1) {
        log_unlock();
        return -1;
    }

    pid_t worker_pid = fork();

    if (worker_pid==-1) {
        int saved_error = errno; //preserve the fork error while cleaning up pipe
        close(capture_pipe[0]);
        close(capture_pipe[1]);
        log_unlock();
        errno = saved_error;
        return -1;
    }

    if (worker_pid==0) {
        run_command_worker(command, capture_pipe[0], capture_pipe[1], client_socket); //child: parse and execute with redirected output
        _exit(WORKER_FAILED); //worker func shouldnt return
    }

    close(capture_pipe[1]); //parent: read only
    log_unlock(); // other threads may print and fork again

    char buffer[4096+1]; //+1 so each chunk can be null-terminated for strstr()
    Outputlog output_log = {0};
    int saved_error = 0;


    while (1) //drain output while worker runs
    {
        ssize_t bytes_read = read(capture_pipe[0], buffer, sizeof(buffer)-1);

        if (bytes_read>0) {
            if (saved_error==0) {
                buffer[bytes_read] = '\0';
                log_output_chunk(&output_log, buffer, (size_t)bytes_read);
                
                if (send_command_output(client_socket, buffer, (size_t)bytes_read)== -1) {
                    if (errno!=0) {
                        saved_error=errno;
                    } else {
                        saved_error=EIO;
                    }
                }
            }
        }
        else if (bytes_read==0) { //all writers have closed the capture pipe
            break;
        } 
        else {
            if (errno==EINTR) {
                continue; //signal interrupted read, retry
            }

            if (saved_error==0) {
                saved_error=errno;
            }

            break;
        }
    }

    close(capture_pipe[0]);

    print_output_log(command, client, &output_log);
    free(output_log.data);

    int worker_status; //collect worker status, no zombies
    pid_t waited;

    do {
        waited = waitpid(worker_pid, &worker_status, 0);
    } 
    while (waited==-1 && errno==EINTR);

    if (waited==-1) {
        return -1;
    }

    if (saved_error != 0) {
        errno = saved_error;
        return -1;
    }

    if (!WIFEXITED(worker_status)) { //worker terminated abnormally
        errno = EIO;
        return -1;
    }

    int worker_exit_code = WEXITSTATUS(worker_status);

    if (worker_exit_code==WORKER_EXIT_REQUESTED) {
        return 1;
    }

    if (worker_exit_code != WORKER_COMPLETE) {
        errno = EIO;
        return -1;
    }

    return 0;
}