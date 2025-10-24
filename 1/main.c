#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "stdbool.h"
#define MAX_CHILDS 20


int child_pid;
int childs[MAX_CHILDS];


void print_childs(){
	for (int i = 0; i < MAX_CHILDS; ++i){
			printf("%d  ", childs[i]);
	}
	printf("\n");
}
int kill_ChildProc(int pid){
	kill(pid, SIGSTOP);
	for (int i = 0; i < MAX_CHILDS; ++i)
	{
		if (pid == childs[i])
		{
			childs[i] = 0;
			break;
		}
	}
}

int find_free(){
	for (int i = 0; i < MAX_CHILDS; ++i)
	{
		if (childs[i] == 0)
		{
			return i;
		}
	}
}

void handler(int sig)
{
	if (child_pid)
	{
    	kill(child_pid, SIGKILL);
		child_pid = 0;
	}
	else{
		exit(0);
	}
}

int react(char* command[]){
    
    child_pid = fork();
	childs[find_free()] = child_pid;
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
	child_pid = 0;

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
        if (strcmp(command[0], "exit") == 0)
		{
			kill_ChildProc(atoi(command[1]));
			goto free;
		}
		else if (strcmp(command[0], "print") == 0)
		{
			print_childs();	
		}

        if (react(command)) exit(1);
        
		free:

		for (j = 0; j < i; j++) {
            free(command[j]);
        }
    }






}





