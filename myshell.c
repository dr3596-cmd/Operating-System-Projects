#include<sys/types.h> // defines pid_t used for process ids
#include<stdio.h> // for printf, sscanf,..
#include<unistd.h> // for fork(),exec() family,..
#include<stdlib.h> // exit, EXIT_SUCCESS
#include<sys/wait.h> // for wait(),...
#include<string.h> //for strcspn(), strcmp(),...
#define SIZE 200
int main(int argc, char *argv[])
{
    //shell setup 
    char line[256];
    char *commands[256];
    int status;
    
    
    

    while(1)
    {
        printf("$ ");

        if (fgets(line, sizeof(line), stdin)!=NULL)
        {
            int counter = 0;
            line[strcspn(line, "\n")] = '\0'; //strip trailing newline

            //parse 
            char *word = strtok(line, " \t"); //split lines on spaces or tabs
            while (word != NULL && counter < SIZE)
            {
                commands[counter] = word; // stores the pointer to commands
                counter++;
                word = strtok(NULL, " \t"); //split input by spaces or tabs
            }
            commands[counter] = NULL;

            if (counter == 0) // user didn't enter a command
            {
                continue;
            }


            pid_t pid = fork(); //fork inside the loop so that each command gets executed by a new child

            if (pid < 0)
            {
                perror ("ERROR: fork failed\n");
                continue;
            }

            else if (pid == 0) //CHILD
            {
                execvp(commands[0],commands);

                perror("command failed\n");

                exit(EXIT_FAILURE);

            }

            else { //PARENT

                wait(&status);

            }

        }
        else 
        {
            break;
        }
        

    }
    

    return 0;

}