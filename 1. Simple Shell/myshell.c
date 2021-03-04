#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include "myshell_parser.h"

#define TRUE 1

//Child signal handler
void signalHandler(){
    pid_t pid;
    int status;
    while(TRUE){
        pid = waitpid(-1, &status, WNOHANG);
        if(pid <= 0){
            return;
        }
    }
}

//Function that executes the command line
int execCommand(struct pipeline *my_pipeline, int input, int first, int last, bool background){
    pid_t child_pid;
    int fd[2];
    int status;

    //Creating a pipe
    pipe(fd);

    //Forking a process
    child_pid = fork();
    
    //Child
    if (child_pid == 0){
        if(my_pipeline->commands->redirect_out_path){            
            if((fd[1] = creat(my_pipeline->commands->redirect_out_path, 0644)) < 0){
                perror("ERROR");
                exit(0);
            }
        }
        if(my_pipeline->commands->redirect_in_path){
            if((fd[1] = open(my_pipeline->commands->redirect_in_path, O_RDONLY, 0)) < 0){
                perror("ERROR");
                exit(0);
            }
        }
        
        if(background){
            //close(STDOUT_FILENO);
            close(fd[1]);
        }
        
        if(first == 1 && last == 0 && input == 0){
            dup2(fd[1], STDOUT_FILENO);
        }
        else if(first == 0 && last == 0 && input != 0){
            dup2(input, STDIN_FILENO);
            dup2(fd[1], STDOUT_FILENO);
        }
        else{
            if(my_pipeline->commands->redirect_out_path){
                dup2(fd[1], STDOUT_FILENO);
            }
            if(my_pipeline->commands->redirect_in_path){
                dup2(fd[1], STDIN_FILENO);
            }
            dup2(input, STDIN_FILENO);
        }
        
        
        
        if(execvp(my_pipeline->commands->command_args[0], my_pipeline->commands->command_args) == -1){
            perror("ERROR");
            exit(1);
        }

        close(fd[1]);
        close(STDIN_FILENO);
        dup(STDIN_FILENO);
        close(fd[0]);
        
    }
    //Parent
    else{
        //Waiting for the child processes to complete
        if(!background){
            wait(&status);
            //waitpid(-1, &status, WNOHANG);
        }

        //Closing the write end of the pipe
        close(fd[1]);
    }
    
    //Closing the input
    if(input != 0){
        close(input);
    }

    //Closing the read end of the pipe
    if(last == 1){
        close(fd[0]);
    }
    
    //Returning the read end of the pipe to the next pipe as an input
    return fd[0];
}



int main(int argc, char* argv[]){
    char buf[1024];
    struct pipeline* my_pipeline;
    int noPrompt = 0;
    char *chars = "-n";
    
    if((argc > 1) && (strcmp(argv[1], chars) == 0)){
        noPrompt = 1;
    }

    //Waiting for the child processes to complete
    signal(SIGCHLD, signalHandler);

    while(TRUE){
        
        if(noPrompt == 1){
            
        }
        else{
            printf("myshell$ ");
        }
        fflush(NULL);
        
        const char *command_line;
        //Option for the Ctrl^D command to exit the shell
        if((command_line = fgets(buf, MAX_LINE_LENGTH, stdin)) == NULL){
            break;
        }
        else{
            my_pipeline = pipeline_build(command_line);
            int input = 0, first = 1;

            //Executing a pipeline
            while(my_pipeline->commands->next != NULL){
                input = execCommand(my_pipeline, input, first, 0, my_pipeline->is_background);
                first = 0;
                my_pipeline->commands = my_pipeline->commands->next;
                
            }
            
            input = execCommand(my_pipeline, input, first, 1, my_pipeline->is_background);
            
        }
    }

    //Freeing the memory that is taken by the pipeline
    pipeline_free(my_pipeline);
    
    return 0;
}