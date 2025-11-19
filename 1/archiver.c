#include "archiver.h"


int main()
{
    char *filename = malloc(1023);

    scanf("%1023s", filename);

    compress_directory(filename);
    
    return 0;
}