#include <sys/fcntl.h>
#include "unistd.h"
#include "fcntl.h"
#define BUF_SIZE 1024


int main(int argc, char** argv)
{
    char buf[BUF_SIZE];
    int bytes;
	int fd = open(argv[1], O_RDONLY);
    while((bytes = read(fd, buf, BUF_SIZE)) > 0)
    {
        write(STDOUT_FILENO, buf, bytes);
    }
    close(fd);

    return 0;
}
