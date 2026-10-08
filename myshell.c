#include<stdio.h> // for printf, sscanf,..
#include<string.h> //for strcspn(), strcmp(),...
#include "parser.h"
#include "executor.h"


int main(void)
{
    char line [MAX_SIZE];
    Command commands[MAX_SIZE];
    int result;
 
    while(1)
    {
        printf("$ ");
        fflush(stdout);

        //fgets returns NULL on end of input; leaves the shell clean
        if (fgets(line, sizeof(line), stdin)!=NULL)
        {
            printf("\n");
            break;
        }
            
        line[strcspn(line, "\n")] = '\0'; //strip trailing newline and replace with the null terminator

        int commandctr = find_pipes(line,commands, &result);

        if(result == 0)
        {
            continue; // no command given
        }

        else if (result == 1)
        {
            break; //user entered exit as the command
        }

        else if (result == -1)
        {
            continue;
        }

        execute_commands(commands, commandctr); //call the execution function
    }

    return 0;


}