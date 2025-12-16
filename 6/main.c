#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <dbus/dbus.h>

// Функция для получения абсолютного пути
char* get_absolute_path(const char* filename) {
    char* path = realpath(filename, NULL);
    if (!path) {
        perror("realpath");
        return NULL;
    }
    return path;
}

// Функция открытия файла через DBus
int open_file_with_dbus(const char* filename) {
    DBusError err;
    DBusConnection* conn = NULL;
    DBusMessage* msg = NULL;
    DBusMessageIter args;
    int ret = 0;
    
    // Инициализируем ошибки
    dbus_error_init(&err);
    
    // Подключаемся к сессионной шине DBus
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "DBus Connection Error: %s\n", err.message);
        dbus_error_free(&err);
        return -1;
    }
    
    if (!conn) {
        fprintf(stderr, "Failed to get DBus connection\n");
        return -1;
    }
    
    // Получаем абсолютный путь к файлу
    char* abs_path = get_absolute_path(filename);
    if (!abs_path) {
        fprintf(stderr, "Failed to get absolute path for: %s\n", filename);
        dbus_connection_unref(conn);
        return -1;
    }
    
    // Создаем сообщение для вызова метода
    // Используем xdg-open через DBus
    msg = dbus_message_new_method_call(
        "org.freedesktop.DBus",        // целевая служба
        "/org/freedesktop/DBus",       // объект
        "org.freedesktop.DBus",        // интерфейс
        "StartServiceByName"           // метод
    );
    
    if (!msg) {
        fprintf(stderr, "Failed to create DBus message\n");
        free(abs_path);
        dbus_connection_unref(conn);
        return -1;
    }
    
    // Подготавливаем аргументы
    dbus_message_iter_init_append(msg, &args);
    
    // Первый аргумент: имя службы
    const char* service_name = "org.freedesktop.FileManager1";
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &service_name)) {
        fprintf(stderr, "Failed to append service name\n");
        free(abs_path);
        dbus_message_unref(msg);
        dbus_connection_unref(conn);
        return -1;
    }
    
    // Второй аргумент: флаги (0)
    dbus_uint32_t flags = 0;
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &flags)) {
        fprintf(stderr, "Failed to append flags\n");
        free(abs_path);
        dbus_message_unref(msg);
        dbus_connection_unref(conn);
        return -1;
    }
    
    // Отправляем сообщение
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(
        conn, msg, 5000, &err); // 5 секунд таймаут
    
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "DBus Error: %s\n", err.message);
        dbus_error_free(&err);
        free(abs_path);
        dbus_message_unref(msg);
        dbus_connection_unref(conn);
        return -1;
    }
    
    if (reply) {
        dbus_message_unref(reply);
    }
    
    // Теперь открываем файл через FileManager1
    dbus_message_unref(msg);
    msg = dbus_message_new_method_call(
        "org.freedesktop.FileManager1",
        "/org/freedesktop/FileManager1",
        "org.freedesktop.FileManager1",
        "ShowItems"
    );
    
    if (!msg) {
        fprintf(stderr, "Failed to create FileManager1 message\n");
        free(abs_path);
        dbus_connection_unref(conn);
        return -1;
    }
    
    // Подготавливаем аргументы для ShowItems
    dbus_message_iter_init_append(msg, &args);
    
    // Первый аргумент: массив URI
    DBusMessageIter array_iter;
    if (!dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, 
                                         DBUS_TYPE_STRING_AS_STRING, &array_iter)) {
        fprintf(stderr, "Failed to open array container\n");
        free(abs_path);
        dbus_message_unref(msg);
        dbus_connection_unref(conn);
        return -1;
    }
    
    // Формируем file:// URI
    char uri[1024];
    snprintf(uri, sizeof(uri), "file://%s", abs_path);
    
    const char* uri_ptr = uri;
    if (!dbus_message_iter_append_basic(&array_iter, DBUS_TYPE_STRING, &uri_ptr)) {
        fprintf(stderr, "Failed to append URI to array\n");
        free(abs_path);
        dbus_message_iter_abandon_container(&args, &array_iter);
        dbus_message_unref(msg);
        dbus_connection_unref(conn);
        return -1;
    }
    
    dbus_message_iter_close_container(&args, &array_iter);
    
    // Второй аргумент: пустая строка (startup_id)
    const char* startup_id = "";
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &startup_id)) {
        fprintf(stderr, "Failed to append startup_id\n");
        free(abs_path);
        dbus_message_unref(msg);
        dbus_connection_unref(conn);
        return -1;
    }
    
    // Отправляем сообщение
    reply = dbus_connection_send_with_reply_and_block(conn, msg, 5000, &err);
    
    if (dbus_error_is_set(&err)) {
        // Пробуем альтернативный метод через xdg-open
        fprintf(stderr, "FileManager1 Error: %s\n", err.message);
        fprintf(stderr, "Trying alternative method...\n");
        
        dbus_error_free(&err);
        dbus_message_unref(msg);
        
        // Альтернатива: используем команду через DBus
        msg = dbus_message_new_method_call(
            "org.freedesktop.DBus",
            "/org/freedesktop/DBus",
            "org.freedesktop.DBus",
            "StartServiceByName"
        );
        
        if (msg) {
            dbus_message_iter_init_append(msg, &args);
            const char* xdg_service = "org.freedesktop.PackageKit";
            dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &xdg_service);
            dbus_uint32_t xdg_flags = 0;
            dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &xdg_flags);
            
            reply = dbus_connection_send_with_reply_and_block(conn, msg, 5000, &err);
            
            if (reply) {
                dbus_message_unref(reply);
            }
            
            // Просто запускаем xdg-open через system()
            char command[1024];
            snprintf(command, sizeof(command), "xdg-open \"%s\" &", abs_path);
            printf("Executing: %s\n", command);
            system(command);
        }
    } else {
        printf("File opened successfully via FileManager1: %s\n", filename);
        if (reply) {
            dbus_message_unref(reply);
        }
    }
    
    // Освобождаем ресурсы
    free(abs_path);
    if (msg) dbus_message_unref(msg);
    dbus_connection_unref(conn);
    
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        fprintf(stderr, "Example: %s ./1/archiver.c\n", argv[0]);
        return 1;
    }
    
    const char* filename = argv[1];
    
    // Проверяем существование файла
    struct stat st;
    if (stat(filename, &st) != 0) {
        perror("Error accessing file");
        return 1;
    }
    
    // Проверяем, является ли файлом
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "Error: %s is not a regular file\n", filename);
        return 1;
    }
    
    printf("Opening file: %s\n", filename);
    
    // Открываем файл через DBus
    int result = open_file_with_dbus(filename);
    
    if (result != 0) {
        fprintf(stderr, "Failed to open file via DBus. Trying fallback...\n");
        
        // Фолбэк: используем xdg-open напрямую
        char command[1024];
        snprintf(command, sizeof(command), "xdg-open \"%s\" >/dev/null 2>&1 &", filename);
        int ret = system(command);
        
        if (ret == 0) {
            printf("File opened using xdg-open fallback\n");
        } else {
            fprintf(stderr, "Failed to open file\n");
            return 1;
        }
    }
    
    return 0;
}