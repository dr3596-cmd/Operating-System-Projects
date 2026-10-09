#include<stdio.h> // for printf, sscanf,..
#include<string.h> //for strcspn(), strcmp(),...
#include<unistd.h> // for fork(),exec() family,..
#include<fcntl.h> //for open, close 


#include "parser.h"

enum TokenType //token type identifies operator / word (cmd name, arg, filename... parse_command() dets word's role from its position)
{
    TOKEN_END,
    TOKEN_WORD,
    TOKEN_INPUT,
    TOKEN_OUTPUT,
    TOKEN_APPEND,
    TOKEN_ERROR,
    TOKEN_ERROR_APPEND,
    TOKEN_INVALID
};

static int next_token(const char **cursor, char word[MAX_SIZE]) //read 1 token from pipe segment
{
    const char *p = *cursor; //cursor points to caller's current pos in segment. updated to let next call continue

    while (*p==' ' || *p=='\t') { //ignore spaces/tabs separating tokens
        p++;
    }

    if (*p=='\0') { //segment end
        *cursor=p;
        return TOKEN_END;
    }

    if (p[0]=='2' && p[1]=='>') { //2> operator
        p+=2; //move pointer

        if (*p=='>')  {
            *cursor = p+1; //to identify append vs truncation
            return TOKEN_ERROR_APPEND; 
        }
        *cursor=p;
        return TOKEN_ERROR;
    }

    if (*p=='<') { // < --input redir
        *cursor = p+1;
        return TOKEN_INPUT;
    }

    if (*p=='>')  { // > --stdout truncation
        p++;

        if (*p=='>') { // >> --stdout append
            *cursor = p+1;
            return TOKEN_APPEND;
        }

        *cursor = p;
        return TOKEN_OUTPUT;
    }

    size_t length = 0;

    char quote = '\0';

    while (*p != '\0') {
        if (quote=='\0') { //unquoted separators terminate the current word
            if (*p == ' ' || *p == '\t' || *p == '<' || *p == '>') {
                break;
            }

            if (*p == '|') {
                fprintf(stderr,"Error: Unexpected pipe in command segment.\n");
                return TOKEN_INVALID;
            }

            if (*p == '\'' || *p == '"') { //start quoted section w/o copying its delimiter
                quote = *p;
                p++;
                continue;
            }
        }
        else if (*p == quote) { //end current quoted section, omit closing delimiter
            quote = '\0';
            p++;
            continue;
        }

        if (*p == '\\' && quote != '\'') { //backslash is literal inside single quotes
            fprintf(stderr,"Error: Backslash escaping is not supported.\n");
            return TOKEN_INVALID;
        }

        if (length>=MAX_SIZE-1) {
            fprintf(stderr,"Error: Token is too long.\n");
            return TOKEN_INVALID;
        }

        word[length] = *p;
        length++;
        p++;
    }

    if (quote != '\0')
    {
        fprintf(stderr, "Error: Unmatched quote.\n");
        return TOKEN_INVALID;
    }

    word[length] = '\0'; //valid string. record where tokenization should resume
    *cursor = p;
    return TOKEN_WORD; //extracted text copied into word
}

static char *store_word(Command *command, const char *word) //copy token into Command storage
{
    size_t bytes = strlen(word)+1; //including \0

    if (bytes>sizeof(command->text) - command->text_used) { //check remaining capacity
        fprintf(stderr, "Error: Command token storage exceeded.\n");
        return NULL;
    }

    char *destination = &command->text[command->text_used]; //write to storage array

    memcpy(destination,word,bytes);
    command->text_used += bytes;

    return destination;
}


int parse_command(char line[], Command *command)
{

    //initialize the command
    command->counter = 0;
    command->redirection_count = 0;
    command->text_used = 0;
    command->args[0] = NULL;

    const char *cursor = line;
    char word[MAX_SIZE];

    //scan line for output redirection operators 

    while(1)
    {
        int token = next_token(&cursor,word);

        if (token==TOKEN_END) {
            break;
        }

        if (token==TOKEN_INVALID) {
            return -1;
        }

        if (token==TOKEN_WORD){ //normal cmd name / arg
            if (command->counter >= MAX_SIZE-1) { //leave 1 entry for null pointer @end
                fprintf(stderr,"Error: Too many arguments.\n");
                return -1;
            }

            char *argument = store_word(command,word);

            if (argument==NULL) {
                return -1;
            }

            command->args[command->counter]=argument;
            command->counter++;

            continue; //this token completed. reads next token from segment
        }

        int filename_token = next_token(&cursor,word); //redirection operator found == next token is filename
        if (filename_token==TOKEN_INVALID)  {
            return -1;
        }

        if (filename_token != TOKEN_WORD) {
            if (token==TOKEN_INPUT) {
                fprintf(stderr,"Error: Input file not specified.\n");
            }
            else if (token==TOKEN_ERROR || token==TOKEN_ERROR_APPEND) {
                fprintf(stderr,"Error: Error output file not specified.\n");
            }
            else {
                fprintf(stderr,"Error: Output file not specified.\n");
            }

            return -1;
        }

        if (command->redirection_count >= MAX_SIZE) { //capacity check before next redir entry
            fprintf(stderr,"Error: Too many redirections.\n");
            return -1;
        }

        char *filename = store_word(command,word);

        if (filename==NULL) {
            return -1;
        }

        Redirection *redirection = &command->redirections[command->redirection_count];

        redirection->file = filename;

        //found >>.
        if (token==TOKEN_APPEND) { // >> appends stoutput to file
            redirection->target = STDOUT_FILENO;
            redirection->flags = O_WRONLY | O_CREAT | O_APPEND; //O_CREAT allows output file creation. O_APPEND preserves content.
        }

        //found >.
        else if (token==TOKEN_OUTPUT) { // > replaces existing content w stoutput
            redirection->target = STDOUT_FILENO;
            redirection->flags = O_WRONLY | O_CREAT | O_TRUNC; //O_TRUNC clears existing content
        }

        //found 2>>.
        else if (token==TOKEN_ERROR_APPEND)  { // 2>> appends sterror to file
            redirection->target = STDERR_FILENO;
            redirection->flags = O_WRONLY | O_CREAT | O_APPEND;
        }

        //found 2>.
        else if (token==TOKEN_ERROR)  { // 2> replaces existing content w sterror
            redirection->target = STDERR_FILENO;
            redirection->flags = O_WRONLY | O_CREAT | O_TRUNC;
        }

        //found <.
        else if (token == TOKEN_INPUT) { // < reads exisitng file through stinput
            redirection->target = STDIN_FILENO;
            redirection->flags = O_RDONLY;
        }

        else {
            fprintf(stderr,"Error: Invalid redirection operator.\n"); //unexpected token
            return -1;
        }

        command->redirection_count++; //every operation in input order
    }

    command->args[command->counter] = NULL;

    // Empty command 

    if (command->counter == 0) 
    {
        if (command->redirection_count > 0) { //redirection exist but no cmd name found --like "> output.txt"
            fprintf(stderr,"Error: Command missing.\n");
            return -1;
        }
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

    size_t length = strlen(line);
    char quote = '\0';

    for (size_t i=0; i < length; i++)
    {
        char current = line[i];
        if (quote != '\0')
        {
            if (current == quote)
            {
                quote = '\0';
            }
            else if (current == '\\' && quote != '\'') {
                fprintf(stderr, "Error: Backslash escaping is not supported.\n");
                *result = -1;
                return commandctr;
            }
            continue; //everything else inside quotes, including '|', is literal
        }

        if (current == '\'' || current == '"')
        {
            quote = current;
            continue;
        }

        if (current == '\\')
        {
            fprintf(stderr, "Error: Backslash escaping is not supported.\n");
            *result = -1;
            return commandctr;
        }

        if (current == '|')
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
        else if (parse_result==1) {
            if (commandctr==1) {
                *result=1;
            }
            else {
                fprintf(stderr,"Error: exit is not supported inside a pipeline.\n"); //exit must be its own separate cmd
                *result= -1;
            }

            return commandctr;
        }
    }

    //if all commands were valid
    *result = 2;

    return commandctr;

}