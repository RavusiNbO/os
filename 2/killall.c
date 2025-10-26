#include "stdlib.h"
#include <dirent.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include "stdio.h"

int is_number(char* str){
    for (int i = 0; str[i]; ++i){
        if (!isdigit(str[i])){
            return 0;
        }
    }


    return 1;
}

int main(int argc, char** argv)
{
    DIR* dir = opendir("/proc");
    struct dirent* cur;
    char path[256], proc_name[256];
    FILE* f;

    while ((cur = readdir(dir)) != 0)
    {
        if (!is_number(cur->d_name)){
            continue;
        }

        snprintf(path, sizeof(path), "/proc/%s/comm", cur->d_name);
        f = fopen(path, "r");
        fgets(proc_name, sizeof(proc_name), f);

        proc_name[strcspn(proc_name, "\n")] = '\0';

        if (strcmp(proc_name, argv[1]) == 0){
            kill(atoi(cur->d_name), SIGTERM);
        }
        memset(path, '\0', sizeof(path));
        memset(proc_name, '\0', sizeof(proc_name));
    }


    return 0;
}



