#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

void applySobelFilter(int image[][3], int result[][3]) {
    int gx, gy;
    
    int kernelX[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };
    
    int kernelY[3][3] = {
        {1, 2, 1},
        {0, 0, 0},
        {-1, -2, -1}
    };
    
    for (int i = 1; i < 2; i++) {
        for (int j = 1; j < 2; j++) {
            gx = 0;
            gy = 0;
            
            for (int k = -1; k <= 1; k++) {
                for (int l = -1; l <= 1; l++) {
                    gx += image[i + k][j + l] * kernelX[k + 1][l + 1];
                    gy += image[i + k][j + l] * kernelY[k + 1][l + 1];
                }
            }
            
            result[i][j] = sqrt(gx * gx + gy * gy);
        }
    }
}

int main() {
    int image[3][3] = {
        {10, 20, 30},
        {40, 50, 60},
        {70, 80, 90}
    };
    
    int result[3][3];
    
    applySobelFilter(image, result);
    
    // Выводим результат применения фильтра Собела к изображению
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            printf("%d ", result[i][j]);
        }
        printf("\n");
    }
    
    return 0;
}
