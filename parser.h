#ifndef PARSER_H
#define PARSER_H

// This header file has everything related to parsing commands and turning 
// lines into Command structs

#define MAX_SIZE 256 // max arguments per command, max commands per line and max line length


typedef struct
{
    char *args[MAX_SIZE]; //command and its arguments
    int counter; //number of arguments 
    char operator[4]; // >, >> (standard output redirection)
    char *file; // filename used by operators
    char err_op[4]; // 2>, 2>> (standard error redirection)
    char *err_file; //filename used by an error operator
} Command;

// The function parses one command (the text between pipes) into *command
//it returns -1 syntax error, 0 if the command is mepty, 1 for exit command and 2 for normal commands

int parse_command(char line[], Command *command);

// The function splits the line when a pipe is found i.e. '|' and parses every piece itno commands[]
// It syores the outcomes in *result and the returns the number of commands found as well
int find_pipes(char line[], Command commands[]);


#endif