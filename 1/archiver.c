#include "archiver.h"


int main()
{
    char *filename = malloc(__DARWIN_MAXPATHLEN);

    scanf("%1023s", filename);

    compress_directory(filename);
    
    return 0;
}