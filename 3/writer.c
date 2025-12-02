#include <sys/msg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/types.h>
#define BUFSIZE 512

struct message
{
    long int type;
    char buffer[BUFSIZE];
};


int main(int argc, char **argv)
{
    FILE* f;
    struct message fileData;
    int msg_id;
    size_t bytesRead;

    if (argc < 2) return 1;
    f = fopen(argv[1], "rb");
    
    fileData.type = 1;

    msg_id = msgget((key_t)12345, 0666 | IPC_CREAT);

    while ((bytesRead = fread(fileData.buffer, 1, BUFSIZE, f)) > 0)
    {
        msgsnd(msg_id, (void*)(&fileData), bytesRead, 0); 
    }
    
    msgsnd(msg_id, (void*)(&fileData), 0, 0);
    
    fclose(f);
    return 0;
}