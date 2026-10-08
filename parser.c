#include<stdio.h> // for printf, sscanf,..
#include<string.h> //for strcspn(), strcmp(),...

#include "parser.h"



// PARSING

int parse_command(char line[], Command *command)
{

    //initialize the command
    command->counter = 0;
    command -> file = NULL;
    command -> operator[0] = '\0';
    command -> err_file = NULL;
    command-> err_op[0] = '\0';

    //scan line for output redirection operators 

    for (int i=0; i< strlen(line); i++)
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

            i++; //skip the second '>' so it is not read as a new operator;

        }
        
        //found >
        else if(line[i] == '>')
        {
            
            command->operator[0] = line[i];
            command->operator[1] = '\0';
           
            command->file = &line[i+1];

            //command ends before the operator
            line[i] = '\0';

            
        }


        //found 2>>
        else if (line[i] == '2' && line[i+1] == '>' && line[i+2] == '>' && (i == 0 || line[i-1] == ' ' || line[i-1] == '\t'))
        {
            
            command->err_op[0] = line[i];
            command->err_op[1] = line[i+1];
            command->err_op[2] = line[i+2];
            command->err_op[3] = '\0';

            command->err_file = &line[i+3];

            //command ends before the operator
            line[i] = '\0';

            i += 2; //skip the two '>' characters

        }
        
        //found 2>
        else if(line[i] == '2' && line[i+1] == '>' && (i == 0 || line[i-1] == ' ' || line[i-1] == '\t'))
        {
            
            command->err_op[0] = line[i];
            command->err_op[1] = line[i+1];
            command->err_op[2] = '\0';

            command->err_file = &line[i+2];

            //command ends before the operator
            line[i] = '\0';

            i++; //skip the '>'

        }

    }

    //check the output filename 

    if(command->operator[0] != '\0')
    {
        //remove any spaces before filename
        while (*command -> file == ' ' || *command->file == '\t')
        {
            command->file++;
        }
    }

    if(command->file != NULL)
    {
        while(*command->file == ' ' || *command->file == '\t')
        {
            command->file++;
        }

        //Redirection operator was found, but no filename was given
        if(*command->file == '\0')
        {
            fprintf(stderr, "Error: output file not specified. \n");
            return -1;
        }

        //filename ends at the first space or tab
        command->file[strcspn(command->file, " \t")] = '\0';
    }

    //check the error filename 
    if(command->err_op[0] != '\0')
    {
        //remove any spaces before filename
        while (*command -> err_file == ' ' || *command->err_file == '\t')
        {
            command->err_file++;
        }

        //Redirection operator was found, but no filename was given
        if(*command->err_file == '\0')
        {
            fprintf(stderr, "Error: output file not specified. \n");
            return -1;
        }

        //filename ends at the first space or tab
        command->err_file[strcspn(command->err_file, " \t")] = '\0';
    }

    // Tokenize commands
    char *word = strtok(line, " \t"); //split lines on tabs

    while (word != NULL && command->counter < MAX_SIZE-1)
    {

        //Remove quotes around commands so they are not read together
        int length = strlen(word);
        if(length >= 2 && ((word[0] == '"' && word[length - 1] == '"') || (word[length-1] == '\'')))
        {
            word [length -1] = '\0';
            word++;
        }
        command->args[command->counter] = word; // stores the pointer to commands
        command->counter++;

        word = strtok(NULL, " \t"); //split input by tabs
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

int find_pipes(char line[], Command commands[], int *result)
{
    char* command_lines[MAX_SIZE];
    int commandctr =1;

    //first command starts at the beginning of the line
    command_lines[0] = line;

    //scan line for pipes 

    for (int i=0; i < strlen(line); i++)
    {
        if (line[i] == '|')
        {
            if(commandctr >= MAX_SIZE -1)
            {
                fprintf(stderr, "Error: Too many commands in the pipeline.\n");
                *result = -1;
                return commandctr;
            }

            //found
            //command ends before the operator; replace the pipe with '\0'
            line[i] = '\0';

            command_lines[commandctr] = &line[i+1]; //command [1] points to the beginning of the next command
            commandctr++;

        }  

    }

    //Parse each individual command

    for(int i=0; i< commandctr ; i++)
    {
        int parse_result = parse_command(command_lines[i], &commands[i]);

        //Error; parse_command already printed the error
        if(parse_result == -1)
        {
            *result = -1;
            return commandctr;
        }

        //Empty command
        else if(parse_result == 0)
        {
            //no pipes at all; if there is just a blank line which is not an error
            if(commandctr == 1)
            {
                *result = 0; 
                return commandctr;

            }

            //if there are pipes, but some commands are missing

            if(i==0)
            {
                fprintf(stderr, "Error: Command missing before pipe\n");
            }

            else if(i == commandctr - 1)
            {
                fprintf(stderr, "Error: Command missing after pipe\n");
            }

            else{
                fprintf(stderr, "Error: Empty command between pipes\n");
            }
            
            *result = -1;
            return commandctr;
        }


        //Exit command; stops the shell if it is the only command 
        else if (parse_result == 1 && commandctr == 1)
        {
            *result = 1;
            return commandctr;
        }
    }

    //if all commands were valid
    *result = 2;

    return commandctr;

}