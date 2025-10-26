#include <sys/fcntl.h>
#include "unistd.h"
#include "fcntl.h"
#define BUF_SIZE 1024


int main(int argc, char** argv)
{
    char buf[BUF_SIZE];
    int fd = open(argv[1], O_RDONLY);
    while(read(fd, buf, BUF_SIZE))
    {
        write(STDOUT_FILENO, buf, BUF_SIZE);
    }
    close(fd);

    return 0;
}