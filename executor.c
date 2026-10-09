#include<sys/types.h> // defines pid_t used for process ids
#include<stdio.h> // for printf, sscanf,..
#include<unistd.h> // for fork(),exec() family,..
#include<stdlib.h> // exit, EXIT_SUCCESS
#include<sys/wait.h> // for wait(),...
#include<string.h> //for strcspn(), strcmp(),...
#include<fcntl.h> //for open, close
#include <errno.h> // errno

#include "executor.h"

void execute_commands(Command commands[], int commandctr)
{
    int status;

    //Create pipes
    int pipecount = commandctr - 1; // n pipes
    int fd[MAX_SIZE -1][2]; //we create read and write file descriptors for n commands

    for (int i=0; i<pipecount; i++)
    {
        if(pipe(fd[i]) < 0)
        {
            perror("Error: pipe failed");
            
            for (int j=0; j < i; j++)
            {
                close(fd[j][0]);
                close(fd[j][1]);
            }

            return;
            
        }

    }

    //Create a child for each command


    pid_t pid[MAX_SIZE]; //fork inside the loop so that each command gets executed by a new child

    int forked = 0; // counter for children that already started the execution process

    for (int i=0; i<commandctr; i++)
    {
        pid[i] = fork();// fork multiple children


        if (pid[i] < 0)
        {
            perror ("ERROR: fork failed");
            break; //CLOSES THE PIPES AND WAITS FOR THE CHILDREN ALREADY CREATED
        }

        else if (pid[i] == 0) //CHILD
        {
            //PIPE INPUT; READ

            if (i > 0)
            {
                //this command gets input from the previous pipe
                if(dup2(fd[i-1][0], STDIN_FILENO) < 0)
                {
                    perror("Error: dup2 failed");
                    exit(EXIT_FAILURE);
                }
            }

            // PIPE OUTPUT ; WRITE
            if (i < commandctr - 1)
            {
                //this command sends output to the next pipe
                if(dup2(fd[i][1], STDOUT_FILENO)< 0)
                {
                    perror("Error: dup2 failed");
                    exit(EXIT_FAILURE);
                }
            }

            //Close pipe descriptors

            for (int j=0; j < pipecount; j++)
            {
                close(fd[j][0]);
                close(fd[j][1]);
            }


            Command *cmd = &commands[i];

            for (int r=0; r< cmd->redirection_count; r++) //pipe connection established. apply this cmds redirections in original order
            {
                Redirection *redirection = &cmd->redirections[r]; 

                int fd = open(redirection->file, redirection->flags, 0644);

                if (fd==-1) {
                    perror(redirection->file);
                    _exit(EXIT_FAILURE);
                }

                if (dup2(fd,redirection->target) == -1) {
                    perror("ERROR: redirection dup2 failed");
                    close(fd);
                    _exit(EXIT_FAILURE);
                }

                if (fd != redirection->target) {
                    close(fd);
                }
            }            

            // Execute the child's commands

            execvp(commands[i].args[0],commands[i].args);
            if(errno == ENOENT) // the program does not exist
            {
                if(commandctr > 1)
                {
                    fprintf(stderr, "Error: Command not found in the pipe sequence!\n");
                }

                else {
                    fprintf(stderr, "Error: Command not found!\n");
                }
            }

            else{
                perror("Command execution failed");

            }

            exit(EXIT_FAILURE);

        }

        forked++; //one more child running

    }

    // Parent closes its copies of the pipes

    for (int i=0; i < pipecount; i++)
    {
        close(fd[i][0]);
        close(fd[i][1]);
    }
            
    //Parent waits for all children to execute their commands

    for (int i=0; i < forked; i++)
    { 
        pid_t waited;

        do {
            waited = waitpid(pid[i],&status,0); //wait for a specific child by referring to the pid
        }
        while (waited == -1 && errno==EINTR);

        if (waited == -1) {
            perror("ERROR: waitpid failed");
        }
    }

    
}