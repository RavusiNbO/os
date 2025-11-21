#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>

int main(int argc, char *argv[]) {
    int niceness = 10;
    int opt;
    char *command = NULL;

    while ((opt = getopt(argc, argv, "n:")) != -1) {
        switch (opt) {
            case 'n':
                niceness = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Использование: %s [-n значение] команда [аргументы...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    if (niceness < -20 || niceness > 19) {
        fprintf(stderr, "Диапазон значений nice: от -20 (высший приоритет) до 19 (низший)\n");
        exit(EXIT_FAILURE);
    }
    if (optind >= argc) {
        fprintf(stderr, "Требуется указать команду для выполнения\n");
        exit(EXIT_FAILURE);
    }
    if (setpriority(PRIO_PROCESS, 0, niceness) == -1) {
        perror("Ошибка setpriority");
        exit(EXIT_FAILURE);
    }

    execvp(argv[optind], &argv[optind]);
    
    perror("Ошибка execvp");
    exit(EXIT_FAILURE);
}