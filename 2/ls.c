#include <stdio.h>
#include "dirent.h"



int main(int argc, char** argv){
    DIR* dir = opendir(argv[1]);
    struct dirent *file;

    while ((file = readdir(dir)) != NULL){
        printf("%s\n", file->d_name);
    }
    closedir(dir);

    return 1;
}