#include<sys/types.h> // defines pid_t used for process ids
#include<stdio.h> // for printf, sscanf,..
#include<unistd.h> // for fork(),exec() family,..
#include<stdlib.h> // exit, EXIT_SUCCESS
#include<sys/wait.h> // for wait(),...
#include<string.h> //for strcspn(), strcmp(),...
#include<fcntl.h> //for open, close 
#include <errno.h> //errno, EINTR
#define size 256

typedef struct
{
    char *args[size]; //command and its arguments
    int counter; //number of arguments 
    char operator[4]; // >, >>, 2>, 2>>
    char *file; //filename used for redirection
    char *infile; //input redirection
} Command;

int parse_command(char line[], Command *command)
{

    //initialize the command
    command->counter = 0;
    command -> file = NULL;
    command->infile = NULL;
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

        //found <
        else if (line[i] == '<')
        {
            command->infile = &line[i+1];
            line[i] = '\0';
        }

    }

    if (command->infile != NULL) {
        while (*command->infile == ' ' || *command->infile == '\t') {
            command->infile++;
        }
        int inlength = (int)strlen(command->infile);

        while (inlength>0 && (command->infile[inlength-1]==' ' || command->infile[inlength-1]=='\t')){
            command->infile[inlength-1]='\0';
            inlength--;
        }

        if(*command->infile == '\0')
        {
            fprintf(stderr,"Error: Input file not specified.\n");
            return -1;
        }        
    }

    //remove any potential spaces before filename 

    if(command->file != NULL)
    {
        while(*command->file == ' ' || *command->file == '\t')
        {
            command->file++;
        }
    
        size_t filelength = strlen(command->file); //remove whitespace left before a pipe @end of this segment
        while (filelength>0 && (command->file[filelength-1] == ' ' || command->file[filelength-1] == '\t')) {
                command->file[filelength-1] = '\0';
                filelength--;
            }


        //Redirection operator was found, but no filename was given
        if(*command->file == '\0')
        {
            fprintf(stderr,"Error: Output file not specified.\n");
            return -1;
        }
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

int find_pipes(char line[], Command commands[], int *result)
{
    //int *result;
    char* command_lines[size];
    int commandctr =1;

    size_t length = strlen(line); //save len before replacing pipes with \0

    //first command starts at the beginning of the line
    command_lines[0] = line;

    //scan line for pipes 

    for (size_t i=0; i < length; i++)
    {
        if (line[i] == '|')
        {
            if (commandctr >= size) //check capacity before adding segment
            {
                fprintf(stderr,"Error: Too many commands.\n");
                *result = -1;
                return commandctr;
            }

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
        char *segment = command_lines[i];

        while (*segment==' ' || *segment=='\t') { //allow blank line input
            segment++;
        }

        if (*segment=='\0' && commandctr>1) { //blank segment in pipeline not allowed
            if (i == commandctr-1) {
                fprintf(stderr,"Error: Command missing after pipe.\n");
            }
            else {
                fprintf(stderr,"Error: Empty command between pipes.\n");
            }

            *result = -1;
            return commandctr;
        }

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

int main(void)
{
    char line [size];
    Command commands[size];
    //int status;
 
    while(1)
    {
        printf("$ ");
        fflush(stdout); //flush prompt

        if (fgets(line, sizeof(line), stdin)!=NULL)
        {
            //int counter = 0;
            line[strcspn(line, "\n")] = '\0'; //strip trailing newline and replace with the null terminator

            int result;
            int commandctr = find_pipes(line,commands, &result);

            //int result = parse_command(line, Command*); //call the parse command function

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
                continue; //parsing function shouldve displayed the error
            }

            int num_pipes = commandctr-1;
            int pipefd[size][2];
            int pipes_created = 0;
            int pipe_failed = 0;

            for (int p=0; p<num_pipes; p++) { //create pipes before forking --children inherit fds
                if (pipe(pipefd[p]) == -1) {
                    perror("ERROR: pipe failed");
                    pipe_failed=1;
                    break;
                }

                pipes_created++;
            }

            if (pipe_failed) { //if pipe crreation failed, close only those created
                for (int p=0; p<pipes_created; p++) {
                    close(pipefd[p][0]);
                    close(pipefd[p][1]);
                }

                continue;
            }

            pid_t pids[size];
            int children_created=0;

            for (int i=0; i<commandctr; i++) {
                pid_t pid = fork();

                if (pid==-1) { //stop child creation. parent closes all pds after
                    perror("ERROR: fork failed");
                    break;
                }

                if (pid==0) { //CHILD   --every cmd except 1st reads from previous pipe
                    if (i>0) {
                        if (dup2(pipefd[i-1][0],STDIN_FILENO) == -1) {
                            perror("ERROR: pipe input dup2 failed");
                            _exit(EXIT_FAILURE);
                        }
                    }

                    if (i < commandctr-1) { //every smd except last writes to next pipe
                        if (dup2(pipefd[i][1],STDOUT_FILENO) == -1) {
                            perror("ERROR: pipe output dup2 failed");
                            _exit(EXIT_FAILURE);
                        }
                    }

                    for (int p=0; p<num_pipes; p++) //st in/out holds the dups. close original pds in each child
                    {
                        close(pipefd[p][0]);
                        close(pipefd[p][1]);
                    }

                    Command *cmd = &commands[i];

                    if (cmd->infile != NULL) //redirections after pipe connections. so file redirection can replace segment's pipe i/o
                    {
                        int fd = open(cmd->infile,O_RDONLY);

                        if (fd==-1) {
                            perror(cmd->infile);
                            _exit(EXIT_FAILURE);
                        }

                        if (dup2(fd,STDIN_FILENO) == -1)  {
                            perror("ERROR: input redirection dup2 failed");
                            close(fd);
                            _exit(EXIT_FAILURE);
                        }

                        if (fd != STDIN_FILENO) {
                            close(fd);
                        }
                    }

                    if (cmd->file != NULL) {
                        int flags;
                        int target;

                        if (strcmp(cmd->operator,">") == 0) {
                            flags = O_WRONLY | O_CREAT | O_TRUNC;
                            target = STDOUT_FILENO;
                        }
                        else if (strcmp(cmd->operator,">>") == 0) {
                            flags = O_WRONLY | O_CREAT | O_APPEND;
                            target = STDOUT_FILENO;
                        }
                        else if (strcmp(cmd->operator,"2>") == 0)  {
                            flags = O_WRONLY | O_CREAT | O_TRUNC;
                            target = STDERR_FILENO;
                        }
                        else if (strcmp(cmd->operator,"2>>") == 0)  {
                            flags = O_WRONLY | O_CREAT | O_APPEND;
                            target = STDERR_FILENO;
                        }
                        else {
                            fprintf(stderr,"Error: Invalid redirection operator.\n");
                            _exit(EXIT_FAILURE);
                        }

                        int fd = open(cmd->file,flags,0644);

                        if (fd == -1)
                        {
                            perror(cmd->file);
                            _exit(EXIT_FAILURE);
                        }

                        if (dup2(fd, target) == -1)
                        {
                            perror("ERROR: output/error redirection dup2 failed");
                            close(fd);
                            _exit(EXIT_FAILURE);
                        }

                        if (fd != target)
                        {
                            close(fd);
                        }
                    }

                    execvp(cmd->args[0], cmd->args); //for PATH cmds & explicit executable paths

                    if (errno==ENOENT) {//execvp() rets only if failed
                        if (commandctr>1) {
                            fprintf(stderr, "Error: Command not found in pipe sequence: %s\n", cmd->args[0]);
                        }
                        else {
                            fprintf(stderr, "Error: Command not found: %s\n", cmd->args[0]);
                        }

                        _exit(127);
                    }

                    perror(cmd->args[0]);
                    _exit(126);
                }

                pids[children_created] = pid; //PARENT --store only successfully created child pids
                children_created++;
            }

            for (int p=0; p<num_pipes; p++) { //parent doesnt r/w pipeline data 
                close(pipefd[p][0]);
                close(pipefd[p][1]);
            }

            for (int i=0; i<children_created; i++) { //create all children before waiting --wait only for actually stored pids
                int status;
                pid_t waited;

                do {
                    waited = waitpid(pids[i],&status,0);
                }
                while (waited == -1 && errno==EINTR);

                if (waited == -1) {
                    perror("ERROR: waitpid failed");
                }
            }

            // pid_t pid = fork(); //fork inside the loop so that each command gets executed by a new child

            // if (pid < 0)
            // {
            //     perror ("ERROR: fork failed");
            //     continue;
            // }

            // else if (pid == 0) //CHILD
            // {
            //     Command *cmd = &commands[0];

            //     char *ops = cmd->operator;
            //     char *file = cmd->file;
            //     char **args = cmd->args;

            //     int fd;

            //     if (cmd->infile != NULL) {
            //         int fd = open(cmd->infile,O_RDONLY);
            //         if (fd<0) {
            //             fprintf(stderr,"Error: File not found.\n");
            //             exit(EXIT_FAILURE);
            //         }
            //         dup2(fd,STDIN_FILENO);
            //         close(fd);
            //     }

            //     if(strcmp(ops, ">") == 0)
            //     {

            //         fd = open (file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            //         if (fd < 0)
            //         {
            //             perror("ERROR: Could not open file");
            //             exit(EXIT_FAILURE);

            //         }

            //         dup2(fd, STDOUT_FILENO);
            //         close(fd);

            //     }

            //     else if(strcmp(ops, ">>") == 0)
            //     {
            //         fd = open (file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            //         if (fd < 0)
            //         {
            //             perror("ERROR: Could not open file");
            //             exit(EXIT_FAILURE);

            //         }

            //         dup2(fd, STDOUT_FILENO);
            //         close(fd);
            //     }

            //     else if(strcmp(ops, "2>") == 0)
            //     {
            //         fd = open (file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            //         if (fd < 0)
            //         {
            //             perror("ERROR: Could not open file");
            //             exit(EXIT_FAILURE);

            //         }

            //         dup2(fd, STDERR_FILENO);
            //         close(fd);
            //     }

            //     else if(strcmp(ops, "2>>") == 0)
            //     {
            //         fd = open (file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            //         if (fd < 0)
            //         {
            //             perror("ERROR: Could not open file");
            //             exit(EXIT_FAILURE);

            //         }

            //         dup2(fd, STDERR_FILENO);
            //         close(fd);
            //     }

            //     execvp(args[0],args);

            //     perror("command failed");

            //     exit(EXIT_FAILURE);

            // }

            // else { //PARENT

            //     // int n; //numbe rof commands
            //     // int pipes = n-1;

            //     // int fd[2];


            //     // waitpid(pid,&status,0); //wait for a specific child by referring to the pid
            //     // if(WIFEXITED(status))
            //     // {
            //     //     printf(" Command not found\n");
            //     // }

            //     // else{
            //     //     int fd[2];

            //     // }
            //     if (waitpid(pid,&status,0) == -1)
            //     {
            //         perror("ERROR: waitpid failed"); //child reports exec failure through perror()
            //     }


            // }

        }
        else 
        {
            break;
        }
        
        //printf(" \n "); //print new line so the new prompt starts on a separate line //commented since we're flushing prompt

    }
    

    return 0;

}
