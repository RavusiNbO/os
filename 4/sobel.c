#include <stdio.h>
#include <time.h>
#include <math.h>
#include "pthread.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
#define THREADS 32

struct thread_params {
    unsigned char* gray;
    unsigned char* result;
    int width;
    int top;
    int roof;
};


void *SobelFilter(void* args) {
    int kernelX[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };
    int kernelY[3][3] = {
        { 1,  2,  1},
        { 0,  0,  0},
        {-1, -2, -1}
    };

    struct thread_params* params = (struct thread_params*) args;
    
    for (int y = params->roof; y < params->top - 1; y++) {
        for (int x = 0; x < params->width - 1; x++) {
            int gx = 0, gy = 0;

            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int pixel = params->gray[(y + ky) * params->width + (x + kx)];
                    gx += pixel * kernelX[ky + 1][kx + 1];
                    gy += pixel * kernelY[ky + 1][kx + 1];
                }
            }

            int magnitude = (int) sqrt(gx * gx + gy * gy);
            if (magnitude > 255) magnitude = 255;
            params->result[y * params->width + x] = (unsigned char) magnitude;
        }
    }
}



int main() {
    struct timespec start, end;
    int width, height, channels;
    size_t i;
    pthread_t threads[THREADS];
    time_t start_time, end_time, duration;
    unsigned char* img = stbi_load("cat.jpg", &width, &height, &channels, 1);
    unsigned char* result = (unsigned char*) calloc(width * height, sizeof(unsigned char));

    struct thread_params params[THREADS];
    for (i = 0; i < THREADS; ++i) {
        params[i].gray = img;
        params[i].result = result;
        params[i].width = width;
        params[i].top = height / THREADS * (i + 1);
        params[i].roof = height / THREADS * i;

        if (pthread_create(&threads[i], NULL, (void*)SobelFilter, &params[i]))
        {
            printf("Error creating thread %d\n", i);
            return 1;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (i = 0; i < THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double seconds = (end.tv_sec - start.tv_sec) +
                 (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("%f\n", seconds);
	

    stbi_write_png("output_cat_mt.png", width, height, 1, result, width);

    stbi_image_free(img);
    free(result);

    return 0;
}
