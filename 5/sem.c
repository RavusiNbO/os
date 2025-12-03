#include "stdio.h"
#include "pthread.h"
#include "stdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/semaphore.h>
#include <unistd.h>

sem_t accessSem, enterSem, sem[2];
unsigned peopleCount[2] = {0};

enum gender {
    MALE,
    FEMALE,
    NONE
};

enum gender curGen = NONE;

struct threadParams {
    enum gender gen;
};


void* student(void* arg)
{
    struct threadParams *params = (struct threadParams *)arg;
    enum gender gen = params->gen;
    free(params);

    printf("[%s] Attempting to enter...\n", (gen == MALE ? "Male" : "Female"));
    fflush(stdout);

    

    sem_wait(&sem[gen]);
    sem_wait(&accessSem);
    sem_post(&accessSem);
    printf("[%s] count: %u\n", gen == MALE ? "Male" : "Female", peopleCount[gen]);
    fflush(stdout);

    if (peopleCount[gen]++ == 0) 
    {
        sem_wait(&accessSem);
        printf("[%s] acquired accessSem\n", (gen == MALE ? "Male" : "Female"));
        fflush(stdout);
        sem_wait(&enterSem);
        curGen = gen;
        printf("[%s] Room locked for their gender.\n", (gen == MALE ? "Male" : "Female"));
        fflush(stdout);
        sem_post(&accessSem);
        
    }
    

    sem_post(&sem[gen]);
    

    
    printf("[%s] Entered room. (Count: %d)\n", (gen == MALE ? "Male" : "Female"), peopleCount[gen]);
    fflush(stdout);
    sleep(1); 


    sem_wait(&sem[gen]);
    peopleCount[gen]-=1;
    
    printf("<<< [%s] Leaving room.\n", (gen == MALE ? "Male" : "Female"));
    fflush(stdout);
    
    if (peopleCount[gen]-- == 1) 
    {
        printf("!!! [%s Last] Unlocking the door.\n", (gen == MALE ? "Male" : "Female"));
        fflush(stdout);
        sem_post(&enterSem);
    }
    sem_post(&sem[gen]);

    return NULL;
}


int main(int argc, char **argv)
{
    unsigned mens = 5, womans = 5; 
    unsigned cur = 0;
    
    if (argc >= 3) 
    {
        mens = atoi(argv[1]);
        womans = atoi(argv[2]);
    } else if (argc == 2) {
        mens = atoi(argv[1]);
        womans = mens;
    }


    sem_init(&sem[MALE], 0, 1);
    sem_init(&sem[FEMALE], 0, 1);
    sem_init(&enterSem, 0, 1);
    sem_init(&accessSem, 0, 1);
    peopleCount[MALE] = 0;
    peopleCount[FEMALE] = 0;

    unsigned total_threads = mens + womans;
    pthread_t *threads = calloc(total_threads, sizeof(pthread_t));
    
    struct threadParams **all_params = calloc(total_threads, sizeof(struct threadParams*));

    unsigned max_iterations = (mens > womans) ? mens : womans;

    for (size_t i = 0; i < max_iterations; i++)
    {
        if (i < mens)
        {
            all_params[cur] = (struct threadParams *)malloc(sizeof(struct threadParams));
            if (!all_params[cur]) { perror("malloc"); exit(1); }
            
            all_params[cur]->gen = MALE;
            
            if (pthread_create(&threads[cur], NULL, student, (void*)all_params[cur]))
            {
                perror("pthread_create");
                free(all_params[cur]); 
                exit(1);
            }
            cur++;
        }
        
        if (i < womans)
        {
            all_params[cur] = (struct threadParams *)malloc(sizeof(struct threadParams));
            if (!all_params[cur]) { perror("malloc"); exit(1); }
            
            all_params[cur]->gen = FEMALE;
            
            if (pthread_create(&threads[cur], NULL, student, (void*)all_params[cur])) 
            {
                perror("pthread_create");
                fflush(stdout);
                free(all_params[cur]);
                exit(1);
            }
            cur++;
        }
    }
    
    for (size_t i = 0; i < total_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("\nAll students have finished using the bathroom. Simulation complete.\n");
    fflush(stdout);

   
    
    sem_destroy(&sem[0]);
    sem_destroy(&sem[1]);
    sem_destroy(&enterSem);
    sem_destroy(&accessSem);

    return 0;
}