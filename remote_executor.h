#ifndef REMOTE_EXECUTOR_H
#define REMOTE_EXECUTOR_H

//#include "parser.h" // for the Command type
#include <stddef.h> //size_t.

//parse & execute 1 complete cmd. command must be a null-terminated string shorter than MAX_SIZE.
//returns: 0 if request handled, 1 if exit cmd, -1 if execution-wrapper or output handling failure
int execute_remote_command(const char *command);


#endif