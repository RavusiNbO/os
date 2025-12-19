#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <file>\n", argv[0]);
        return 1;
    }
    
    // Просто используем dbus-send с найденным интерфейсом
    char abs_path[PATH_MAX];
    if (realpath(argv[1], abs_path) == NULL) {
        perror("realpath");
        return 1;
    }
    
    printf("Opening: %s\n", abs_path);
    
    // Используем найденный интерфейс: org.kde.Kate.Application
    // Объект: /MainApplication, метод: openUrl
    // Аргументы: URL и кодировка
    
    char cmd[PATH_MAX + 200];
    snprintf(cmd, sizeof(cmd),
             "dbus-send --session --print-reply "
             "--dest=org.kde.kate-5671 "
             "--type=method_call "
             "/MainApplication org.kde.Kate.Application.openUrl "
             "string:\"file://%s\" string:\"UTF-8\"",
             abs_path);
    
    printf("Command: %s\n", cmd);
    
    int result = system(cmd);
    
    if (result == 0) {
        printf("Success!\n");
        return 0;
    }
    
    // Если не сработало, запускаем Kate напрямую
    printf("D-Bus failed, launching Kate directly\n");
    snprintf(cmd, sizeof(cmd), "kate \"%s\" &", abs_path);
    return system(cmd);
}