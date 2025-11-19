#include "archiver.h"


int main()
{
    char *filename = malloc(256);

    scanf("%255s", filename);

    compress_directory(filename);
    
    return 0;
}