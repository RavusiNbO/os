#include <sys/msg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>

#define BUFSIZE 512
#define INITIAL_CAPACITY 8192

struct message
{
    long int type;
    char buffer[BUFSIZE];
};


char* receive_chunks(int msg_id, size_t *total_size) {
    struct message chunk;
    size_t current_size = 0;
    size_t capacity = INITIAL_CAPACITY;
    char *buffer = (char*)malloc(capacity);
    ssize_t result;

    do {
        result = msgrcv(msg_id, (void*)(&chunk), BUFSIZE, 1, 0); 

        if (result > 0) {
            if (current_size + result > capacity) {
                capacity *= 2;
                buffer = (char*)realloc(buffer, capacity);
            }
            
            memcpy(buffer + current_size, chunk.buffer, result);
            current_size += result;
        }
        
    } while (result > 0);
    
    *total_size = current_size;
    return buffer;
}


int main(int argc, char **argv)
{
    int msg_id;
    char *file_buffers[2] = {NULL, NULL};
    size_t file_sizes[2] = {0, 0};
    
    if (argc < 3) return 1; 

    msg_id = msgget((key_t)12345, 0666 | IPC_CREAT);

    pid_t child1_id = fork();
    if (child1_id == 0)
    {
        execl("./writer", "./writer", argv[1], NULL);
        return 1;
    } 
    
    file_buffers[0] = receive_chunks(msg_id, &file_sizes[0]);
    waitpid(child1_id, NULL, 0);

    pid_t child2_id = fork();
    if (child2_id == 0)
    {
        execl("./writer", "./writer", argv[2], NULL);
        return 1;
    }
    
    file_buffers[1] = receive_chunks(msg_id, &file_sizes[1]);
    waitpid(child2_id, NULL, 0);

    FILE* f = fopen("output_file.txt", "wb");
    
    size_t len = (file_sizes[0] < file_sizes[1]) ? file_sizes[0] : file_sizes[1];

    for (size_t i = 0; i < len; i++)
    {
        fputc(file_buffers[0][i] ^ file_buffers[1][i], f);
    }

    fclose(f);

    free(file_buffers[0]);
    free(file_buffers[1]);

    msgctl(msg_id, IPC_RMID, NULL);

    return 0;
}