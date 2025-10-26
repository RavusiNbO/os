#include "unistd.h"
#include <stdlib.h>


int main(int argc, char** argv){
    nice(atoi(argv[1]));

    return 0;
}
