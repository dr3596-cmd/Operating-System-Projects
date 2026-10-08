#ifndef PARSER_H
#define PARSER_H

#include <stddef.h> //size_t

// This header file has everything related to parsing commands and turning 
// lines into Command structs

#define MAX_SIZE 256 // max arguments per command, max commands per line and max line length


typedef struct //file-redirection operation. entries applied in the order entered by the user
{
    int target; //STDIN_FILENO,STDOUT_FILENO,STDERR_FILENO
    int flags; //O_RDONLY, output creation/append flags
    char *file;
} Redirection;

typedef struct //command segment 
{
    char *args[MAX_SIZE]; //command and its arguments
    int counter; //number of arguments 
    Redirection redirections[MAX_SIZE];
    int redirection_count;
    // char operator[4]; // >, >>, 2>, 2>>
    // char *file; //filename used for redirection
    // char *infile; //input redirection
    char text[MAX_SIZE]; //store arg & filename strs
    size_t text_used;
} Command;

// typedef struct
// {
//     char *args[MAX_SIZE]; //command and its arguments
//     int counter; //number of arguments 
//     char operator[4]; // >, >> (standard output redirection)
//     char *file; // filename used by operators
//     char err_op[4]; // 2>, 2>> (standard error redirection)
//     char *err_file; //filename used by an error operator
// } Command;


// The function parses one command (the text between pipes) into *command
//it returns -1 syntax error, 0 if the command is mepty, 1 for exit command and 2 for normal commands

int parse_command(char line[], Command *command);

// The function splits the line when a pipe is found i.e. '|' and parses every piece itno commands[]
// It syores the outcomes in *result and the returns the number of commands found as well
int find_pipes(char line[], Command commands[], int *result);


#endif