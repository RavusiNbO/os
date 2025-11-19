#include "archiver.h"


int main()
{
    char *filename = malloc(MAX_PATH);

    scanf("%1023s", filename);
    compress_directory(filename);
    
    return 0;
}