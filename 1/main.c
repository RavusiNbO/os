#include "libc.h"
#include "errno.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int child_pid;

void handler(int sig)
{
    kill(child_pid, SIGSTOP);
}

int react(char* command[]){
    
    child_pid = fork();
    if (child_pid == 0)
    {
        execvp(command[0], command);
        perror("execvp");
        exit(1);
    }
    if (waitpid(child_pid, NULL, 0) < 0) {
        perror("waitpid");
        return -1;
    }

    return 0;
}

int main(){
    char buf[512];
    char* command[20];
    char* token;
    size_t i=0, j, count;
    signal(SIGINT, handler);

    while (true)
    {
        fgets(buf, sizeof(buf), stdin);
        
        buf[strcspn(buf, "\n")] = '\0';
        i = 0;
        token = strtok(buf, " ");
        while (token) {
            command[i++] = strdup(token);
            token = strtok(NULL, " ");
        }
        count = i;
        command[i] = NULL;
        

        if (react(command)) exit(1);
        for (j = 0; j < i; j++) {
            free(command[j]);
        }
    }






}





