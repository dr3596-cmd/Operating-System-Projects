#include<sys/types.h> // defines pid_t used for process ids
#include<stdio.h> // for printf, sscanf,..
#include<unistd.h> // for fork(),exec() family,..
#include<stdlib.h> // exit, EXIT_SUCCESS
#include<sys/wait.h> // for wait(),...
#include<string.h> //for strcspn(), strcmp(),...
#include<fcntl.h> //for open, close 
#define size 256



int parse_command(char line[], char *args[], int *counter)
{
    char *file; // pointer to the filename after each operator
    char *operator[5];
    //scan line for output redirection operators 

    for (int i=0; line[i]!= '\0'; i++)
    {
        if (line[i] == '>' && line[i+1]=='>')
        {
            //found >>
            operator[0] = line[i];
            operator[1] = line[i+1];
            operator[2] = '\0';

            file = &line[i+2];

        }
        
        else if(line[i] == '>')
        {
            //found >
            operator[0] = line[i];
            operator[1] = '\0';
           
            file = &line[i+1];
        }


        else if (line[i] == '2' && line[i+1] == '>' && line[i+2] == '>')
        {
            //found 2>>
            operator[0] = line[i];
            operator[1] = line[i+1];
            operator[2] = line[i+2];
            operator[3] = '\0';

            file = &line[i+3];

        }
        
        else if(line[i] == '2' && line[i+1] == '>')
        {
            //found 2>
            operator[0] = line[i];
            operator[1] = line[i+1];
            operator[2] = '\0';

            file = &line[i+2];

        }

    }
    

    //parse 
    char *word = strtok(line, " \t"); //split lines on spaces or tabs
    while (word != NULL && counter < size-1)
    {
        args[*counter] = word; // stores the pointer to commands
        (*counter)++;
        word = strtok(NULL, " \t"); //split input by spaces or tabs
    }
    args[*counter] = NULL;

    if (*counter == 0) // user didn't enter a command
    {
        return 0;
    }

    if(strcmp(args[0], "exit") == 0)
    {
        return 1; //if command is exit, stop the shell program completely
    }

    return 2;  // if 2 is returned by the parse coomand, the we can go ahead to do the forking

}
int main(int argc, char *argv[])
{
    //shell setup 
    char line[size];
    char *args[size];
    int status;
 

    while(1)
    {
        printf("$ ");

        if (fgets(line, sizeof(line), stdin)!=NULL)
        {
            int counter = 0;
            line[strcspn(line, "\n")] = '\0'; //strip trailing newline and replace with the null terminator

            int result =parse_command(line, args, &counter); //call the parse command function

            if(result == 0)
            {
                continue; // no command given
            }

            else if (result == 1)
            {
                break; //user entered exit as the command
            }


            pid_t pid = fork(); //fork inside the loop so that each command gets executed by a new child

            if (pid < 0)
            {
                perror ("ERROR: fork failed");
                continue;
            }

            else if (pid == 0) //CHILD
            {
                execvp(args[0],args);

                perror("command failed");

                exit(EXIT_FAILURE);

            }

            else { //PARENT

                waitpid(pid,&status,0); //wait for a specific child by referring to the pid
                if(WIFEXITED(status))
                {
                    printf(" Command not found\n");
                }


            }

        }
        else 
        {
            break;
        }
        
        printf(" \n "); //print new line so the new prompt starts on a separate line

    }
    

    return 0;

}