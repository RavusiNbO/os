#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");
    
    if (argc != 2) {
        fprintf(stderr, "Использование: %s <файл>\n", argv[0]);
        return 1;
    }
    
    // Получаем абсолютный путь
    char absolute_path[4096];
    if (realpath(argv[1], absolute_path) == NULL) {
        perror("Ошибка пути");
        return 1;
    }
    
    printf("Открываю: %s\n", absolute_path);
    
    // Пробуем разные методы по порядку:
    
    // 1. KIOClient (рекомендуется для KDE)
    printf("\n=== 1. Пробую KIOClient (KDE) ===\n");
    if (system("which kioclient5 > /dev/null 2>&1") == 0) {
        char cmd[8192];
        snprintf(cmd, sizeof(cmd), "kioclient5 exec \"%s\" &", absolute_path);
        printf("Команда: %s\n", cmd);
        system(cmd);
        return 0;
    }
    
    // 2. DBus-send с правильным синтаксисом
    printf("\n=== 2. Пробую DBus-send ===\n");
    
    // Экранируем путь для D-Bus (заменяем пробелы на %20)
    char escaped_path[8192];
    char *src = absolute_path;
    char *dst = escaped_path;
    
    // Преобразуем в file:// URL с экранированием
    strcpy(dst, "file://");
    dst += 7;
    
    while (*src) {
        if (*src == ' ') {
            *dst++ = '%';
            *dst++ = '2';
            *dst++ = '0';
        } else {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
    
    printf("Экранированный URI: %s\n", escaped_path);
    
    // Формируем команду dbus-send с ПРАВИЛЬНЫМ синтаксисом
    char cmd1[16384];
    snprintf(cmd1, sizeof(cmd1),
        "dbus-send --session --print-reply --type=method_call "
        "--dest=org.freedesktop.FileManager1 "
        "/org/freedesktop/FileManager1 "
        "org.freedesktop.FileManager1.ShowItems "
        "array:string:\"%s\" string:\"\" 2>&1",
        escaped_path);
    
    printf("Команда: %s\n", cmd1);
    
    // Выполняем и проверяем результат
    FILE *fp = popen(cmd1, "r");
    if (fp) {
        char output[1024];
        int success = 0;
        while (fgets(output, sizeof(output), fp)) {
            printf("Ответ: %s", output);
            if (strstr(output, "method return")) {
                success = 1;
            }
        }
        pclose(fp);
        
        if (success) {
            printf("Файл успешно открыт через FileManager1\n");
            return 0;
        }
    }
    
    // 3. Пробуем OpenItems вместо ShowItems
    printf("\n=== 3. Пробую OpenItems ===\n");
    char cmd2[16384];
    snprintf(cmd2, sizeof(cmd2),
        "dbus-send --session --print-reply --type=method_call "
        "--dest=org.freedesktop.FileManager1 "
        "/org/freedesktop/FileManager1 "
        "org.freedesktop.FileManager1.OpenItems "
        "array:string:\"%s\" string:\"\" 2>&1",
        escaped_path);
    
    printf("Команда: %s\n", cmd2);
    
    fp = popen(cmd2, "r");
    if (fp) {
        char output[1024];
        while (fgets(output, sizeof(output), fp)) {
            printf("Ответ: %s", output);
        }
        pclose(fp);
    }
    
    // 4. Пробуем xdg-open как запасной вариант
    printf("\n=== 4. Пробую xdg-open ===\n");
    char cmd3[8192];
    snprintf(cmd3, sizeof(cmd3), "xdg-open \"%s\" &", absolute_path);
    printf("Команда: %s\n", cmd3);
    system(cmd3);
    
    return 0;
}