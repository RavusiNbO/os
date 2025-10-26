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
	kill(pid, SIGKILL);
	for (int i = 0; i < MAX_CHILDS; ++i)
	{
		if (pid == childs[i])
		{
			childs[i] = 0;
			break;
		}
	}
	child_pid = 0;
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
		kill_ChildProc(child_pid);
	}
	else{
		exit(0);
	}
}

int react(char* command[]){
    
    child_pid = fork();
    char path[256];
    if (child_pid == 0)
    {  
		snprintf(path, sizeof(path), "/usr/local/bin/%s", command[0]);
		execv(path, command);
		execvp(command[0], command);
        perror("execv");
        exit(1);
    }
	childs[find_free()] = child_pid;

    return 0;
}

void SIGCHLD_handler(int sig) {
    int status;
    while (1) {
        int res = waitpid(-1, &status, WNOHANG);
        if (res > 0) {
            for (int i = 0; i < MAX_CHILDS; i++) {
                if (childs[i] == res) {
                    if (child_pid == res) child_pid = 0;
					childs[i] = 0;
                    break;
                }
            }
        } else {
            break;
        }
    }
}

int main(){
    char buf[512];
    char* command[20];
    char* token;
    size_t i=0, j, count;
    signal(SIGINT, handler);
	signal(SIGCHLD, SIGCHLD_handler);

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
			goto free;
		}

        if (react(command)) exit(1);
        
		free:

		for (j = 0; j < i; j++) {
            free(command[j]);
        }
    }

}





