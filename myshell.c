#include<sys/types.h> // defines pid_t used for process ids
#include<stdio.h> // for printf, sscanf,..
#include<unistd.h> // for fork(),exec() family,..
#include<stdlib.h> // exit, EXIT_SUCCESS
#include<sys/wait.h> // for wait(),...
#include<string.h> //for strcspn(), strcmp(),...
#include<fcntl.h> //for open, close 
#define size 256

typedef struct
{
    char *args[size]; //command and its arguments
    int counter; //number of arguments 
    char operator[4]; // >, >>, 2>, 2>>
    char *file; //filename used for redirection
} Command;

int parse_command(char line[], Command *command)
{

    //initialize the command
    command->counter = 0;
    command -> file = NULL;
    command -> operator[0] = '\0';

    //scan line for output redirection operators 

    for (int i=0; line[i]!= '\0'; i++)
    {
        //found >>
        if (line[i] == '>' && line[i+1]=='>')
        {
            
            command -> operator[0] = line[i];
            command -> operator[1] = line[i+1];
            command -> operator[2] = '\0';

            command -> file = &line[i+2];

            //command ends before the operator
            line[i] = '\0';

            break;

        }
        
        //found >
        else if(line[i] == '>')
        {
            
            command->operator[0] = line[i];
            command->operator[1] = '\0';
           
            command->file = &line[i+1];

            //command ends before the operator
            line[i] = '\0';

            break;
        }


        //found 2>>
        else if (line[i] == '2' && line[i+1] == '>' && line[i+2] == '>')
        {
            
            command->operator[0] = line[i];
            command->operator[1] = line[i+1];
            command->operator[2] = line[i+2];
            command->operator[3] = '\0';

            command->file = &line[i+3];

            //command ends before the operator
            line[i] = '\0';

            break;

        }
        
        //found 2>
        else if(line[i] == '2' && line[i+1] == '>')
        {
            
            command->operator[0] = line[i];
            command->operator[1] = line[i+1];
            command->operator[2] = '\0';

            command->file = &line[i+2];

            //command ends before the operator
            line[i] = '\0';

            break;

        }

    }

    //remove any potential spaces before filename 

    if(command->file != NULL)
    {
        while(*command->file == ' ' || *command->file == '\t')
        {
            command->file++;
        }
    }

    //Redirection operator was found, but no filename was given
    if(*command->file == '\0')
    {
        return -1;
    }

    

    // Tokenize commands
    char *word = strtok(line, " \t"); //split lines on spaces or tabs

    while (word != NULL && command->counter < size-1)
    {
        command->args[command->counter] = word; // stores the pointer to commands
        command->counter++;

        word = strtok(NULL, " \t"); //split input by spaces or tabs
    }

    command->args[command->counter] = NULL;

    // Empty command 

    if (command->counter == 0) 
    {
        return 0;
    }

    //check for exit command

    if(strcmp(command->args[0], "exit") == 0)
    {
        return 1; //if command is exit, stop the shell program completely
    }

    return 2;  // if 2 is returned by the parse command, then the command is normal and we go ahead with other functions

}

int find_pipes(char line[], Command commands[])
{
    int *result;
    char* command_lines[size];
    int commandctr =1;

    //first command starts at the beginning of the line
    command_lines[0] = line;

    //scan line for pipes 

    for (int i=0; i < strlen(line); i++)
    {
        if (line[i] == '|')
        {
            //found
            //command ends before the operator; replace the pipe with '\0'
            line[i] = '\0';

            command_lines[commandctr] = &line[i+1]; //comannd [1] points to the beginning of the next command
            commandctr++;

        }  

    }

    //Parse each individual command

    for(int i=0; i< commandctr ; i++)
    {
        int parse_result = parse_command(command_lines[i], &commands[i]);

        //Error
        if(parse_result == -1)
        {
            *result = -1;
            return commandctr;
        }

        //Empty command
        else if(parse_result == 0)
        {
            *result = 0; 
            return commandctr;
        }

        //Exit command
        else if (parse_result == 1)
        {
            *result = 1;
            return commandctr;
        }
    }

    //if all commands were valid
    *result = 2;

    return commandctr;

}

int main(int argc, char *argv[])
{
    char line [size];
    Command commands[size];
    int status;
 
    while(1)
    {
        printf("$ ");

        if (fgets(line, sizeof(line), stdin)!=NULL)
        {
            //int counter = 0;
            line[strcspn(line, "\n")] = '\0'; //strip trailing newline and replace with the null terminator

            int commandctr = find_pipes(line,commands);

            int result = parse_command(line, Command*); //call the parse command function

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
                int fd;

                if(strcmp(ops, ">") == 0)
                {

                    fd = = open (file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0)
                    {
                        perror("ERROR: Could not open file");
                        exit(EXIT_FAILURE);

                    }

                    dup2(fd, STDOUT_FILENO);
                    close(fd);

                }

                else if(strcmp(ops, ">>") == 0)
                {
                    fd = = open (file, O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (fd < 0)
                    {
                        perror("ERROR: Could not open file");
                        exit(EXIT_FAILURE);

                    }

                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }

                else if(strcmp(ops, "2>") == 0)
                {
                    fd = = open (file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0)
                    {
                        perror("ERROR: Could not open file");
                        exit(EXIT_FAILURE);

                    }

                    dup2(fd, STDERR_FILENO);
                    close(fd);
                }

                else if(strcmp(ops, "2>>") == 0)
                {
                    fd = = open (file, O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (fd < 0)
                    {
                        perror("ERROR: Could not open file");
                        exit(EXIT_FAILURE);

                    }

                    dup2(fd, STDERR_FILENO);
                    close(fd);
                }

                execvp(args[0],args);

                perror("command failed");

                exit(EXIT_FAILURE);

            }

            else { //PARENT

                int n; //numbe rof commands
                int pipes = n-1;

                int fd[2];


                waitpid(pid,&status,0); //wait for a specific child by referring to the pid
                if(WIFEXITED(status))
                {
                    printf(" Command not found\n");
                }

                else{
                    int fd[2];

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