#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h" // for the Command type

//This function creates the pipes, forks one child per command, sets up pipes and redirection in 
//each child, execs and waits for all children before returning

void execute_commands(Command commands[], int commandctr);

#endif