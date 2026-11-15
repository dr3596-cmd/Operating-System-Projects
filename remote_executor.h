#ifndef REMOTE_EXECUTOR_H
#define REMOTE_EXECUTOR_H

#include <stddef.h> //size_t.

#include "log.h" //ClientInfo (number, ip, port,...) and shared log functions

//parse & execute 1 complete cmd
//returns: 0 if request handled, 1 if exit cmd, -1 if execution-wrapper or output handling failure
int execute_remote_command(const char *command, const ClientInfo *client);



#endif