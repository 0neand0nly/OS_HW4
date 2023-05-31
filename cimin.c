//21800637 Jooyoung Jang 
//21600415 Sehyuk Yang
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFSIZE 4096

char* input_file = NULL;
char* output_file = NULL;
char* error_msg = NULL;
char* target_program = NULL;
char * reduced_crash = NULL;

char ** additional_args;

char * reduced_result=NULL;
FILE * out_fp;
pid_t child_pid;



int in_pipe[2];
int e_pipe[2];

void int_handler(int sig);
void alrm_handler(int sig);
void error_handler(char * str);
char* reduce(char* t);
char * run_program(char * candidate);


int main(int argc, char* argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "i:m:o:")) != -1)
    {
        switch (opt)
        {
        case 'i':
            input_file = optarg;
            break;
        case 'm':
            error_msg = optarg;
            break;
        case 'o':
            output_file = optarg;
            break;
        default:
            fprintf(stderr, "Usage: %s -i input_file -m error_msg -o output_file\n", argv[0]);
            exit(EXIT_FAILURE);
        }
        if (input_file != NULL && error_msg != NULL && output_file != NULL)
            break;
    }
    signal(SIGINT,int_handler);
    signal(SIGALRM,alrm_handler);

    additional_args = &argv[optind];
    target_program = additional_args[0];

   #ifdef DEBUG
    printf("input_file: %s\n", input_file);
    printf("error_msg: %s\n", error_msg);
    printf("output_file: %s\n", output_file);
    printf("target program : %s\n", target_program);
   #endif
   FILE *fp;
   if((fp = fopen(input_file, "r"))==NULL)
    error_handler("fopen");
    
    if((out_fp = fopen(output_file, "w"))==NULL)
    error_handler("fopen W");

   char init_crash[BUFSIZE];

   if(fread(init_crash,1,BUFSIZE,fp)==-1)
        error_handler("Reading initial input");

    reduced_crash = reduce(init_crash);
    
    printf("Result: %s\n",reduced_crash);
    fwrite(reduced_crash,1,strlen(reduced_crash),out_fp);
    fclose(out_fp);
    
    return 0;

}


char * reduce(char * t)
{
    char * tm = t;
    int s = strlen(tm) - 1;
    
    while(s>0)
    {
        for(int i=0;i<strlen(tm)-s;i++)
        {
            if(pipe(in_pipe)==-1)
            {
                error_handler("Inpipe open");
            }
            if(pipe(e_pipe)==-1)
            {
                error_handler("Error pipe open");
            }
            char* head;
            char* tail;

           head = strndup(tm,i);
           tail = strndup(&tm[i+s],strlen(tm)-1);

           char * candidate = malloc(strlen(head)+strlen(tail)+1);

           strcpy(candidate,head);
           strcat(candidate,tail);
           reduced_result = candidate;
           free(head);
           free(tail);
           child_pid = fork();

           if(child_pid==0)// error return , receive input
           {
            if(dup2(in_pipe[0],STDIN_FILENO) == -1)
            {
                error_handler("Redirecting pipe for STDIN");
            }
            if(dup2(e_pipe[1],STDERR_FILENO) == -1)
            {
                error_handler("Redirecting pipe for STDERR");
            }
            #ifdef DEBUG
            printf("Candidate: %s\n",candidate);
            #endif
                if(write(in_pipe[1],candidate,strlen(candidate))==-1)
                {
                    error_handler("Writing Thru PIPE_IN");

                }
            close(in_pipe[1]);
            alarm(3);
            if(additional_args[1]!=NULL)
                {
                    if(execv(target_program,additional_args)==-1)
                        error_handler("Execv");
                }
                else{
                    if(execl(target_program,target_program,NULL)==-1)
                        error_handler("Execlp");
                }
           }
           else // read ERROR from pipe/ write input into the pipe
           {
                
                close(in_pipe[0]);
                close(in_pipe[1]);
                close(e_pipe[1]);
                //alarm(3);
                
                
           }
           int status;
           waitpid(child_pid,&status,0);
           char error_buff[BUFSIZE];
           if (WIFEXITED(status))
                {
                    alarm(0);
                    #ifdef DEBUG
                    printf("Child process exited with status %d\n", WEXITSTATUS(status));
                    #endif
                }
            if(read(e_pipe[0],error_buff,BUFSIZE)==-1)
                {
                    error_handler("Reading From pipe");
                }
            #ifdef DEBUG
            printf("Error content: %s\n",error_buff);
            #endif 
            close(e_pipe[0]);
            
            if(strstr(error_buff,error_msg)!=NULL)
            {
                kill(child_pid,SIGKILL);
                char * final_result = reduce(candidate);
                reduced_result = final_result;
                return(final_result);
            }
            free(candidate);               
        }
        for(int i=0;i<strlen(tm)-s;i++)
        {
            if(pipe(in_pipe)==-1)
            {
                error_handler("Inpipe open");
            }
            if(pipe(e_pipe)==-1)
            {
                error_handler("Error pipe open");
            }
           
            char *mid;
           mid = strndup(&tm[i],s);
          

           reduced_result = mid;
           child_pid = fork();

           if(child_pid==0)// error return , receive input
           {
            if(dup2(in_pipe[0],STDIN_FILENO) == -1)
            {
                error_handler("Redirecting pipe for STDIN");
            }
            if(dup2(e_pipe[1],STDERR_FILENO) == -1)
            {
                error_handler("Redirecting pipe for STDERR");
            }
            #ifdef DEBUG
            printf("Candidate: %s\n",mid);
            #endif
                if(write(in_pipe[1],mid,strlen(mid))==-1)
                {
                    error_handler("Writing Thru PIPE_IN");

                }
            close(in_pipe[1]);
            alarm(3);
            if(additional_args[1]!=NULL)
                {
                    if(execv(target_program,additional_args)==-1)
                        error_handler("Execv");
                }
                else{
                    if(execl(target_program,target_program,NULL)==-1)
                        error_handler("Execlp");
                }
           }
           else // read ERROR from pipe/ write input into the pipe
           {
                
                close(in_pipe[0]);
                close(in_pipe[1]);
                close(e_pipe[1]);
                //alarm(3);
                
                
           }
           int status;
           waitpid(child_pid,&status,0);
           char error_buff[BUFSIZE];
           if (WIFEXITED(status))
                {
                    alarm(0);
                    #ifdef DEBUG
                    printf("Child process exited with status %d\n", WEXITSTATUS(status));
                    #endif
                }
            if(read(e_pipe[0],error_buff,BUFSIZE)==-1)
                {
                    error_handler("Reading From pipe");
                }
            #ifdef DEBUG
            printf("Error content: %s\n",error_buff);
            #endif
            close(e_pipe[0]);
            
            if(strstr(error_buff,error_msg)!=NULL)
            {
                kill(child_pid,SIGKILL);
                char * final_result = reduce(mid);
                reduced_result = final_result;
                return(final_result);
            }
            free(mid);               
        }
        s = s-1;
    }
    return tm;
}

void error_handler(char * str)
{
    printf("Error: %s \n",str);
    exit(EXIT_FAILURE);
}


void int_handler(int sig)
{
    if(sig == SIGINT)
    {
        kill(child_pid,SIGKILL);
        printf("Size of current crashing input: %lu",strlen(reduced_result));
        fwrite(reduced_result,1,strlen(reduced_result),out_fp);
        exit(0);
    }
    
}

void alrm_handler(int sig)
{
    if(sig == SIGALRM)
    {
        kill(child_pid,SIGKILL);
        printf("TIMEOUT!\n");
        fwrite(reduced_result,1,strlen(reduced_result),out_fp);
        fclose(out_fp);
        exit(0);
    }
}