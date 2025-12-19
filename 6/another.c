#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <dbus/dbus.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        fprintf(stderr, "Opens a file in Kate using D-Bus API\n");
        return 1;
    }
    
    if (access(argv[1], F_OK) != 0) {
        perror("File not found");
        return 1;
    }
    
    char abs_path[PATH_MAX];
    if (realpath(argv[1], abs_path) == NULL) {
        perror("Cannot get absolute path");
        return 1;
    }
    
    printf("File: %s\n", abs_path);
    
    // Инициализируем D-Bus
    DBusError error;
    DBusConnection *conn = NULL;
    DBusMessage *msg = NULL;
    DBusMessage *reply = NULL;
    
    dbus_error_init(&error);
    
    // Подключаемся к сессионной шине D-Bus
    conn = dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "D-Bus connection error: %s\n", error.message);
        dbus_error_free(&error);
        return 1;
    }
    
    if (!conn) {
        fprintf(stderr, "Failed to get D-Bus connection\n");
        return 1;
    }
    
    // Формируем file:// URL
    char url[PATH_MAX + 16];
    snprintf(url, sizeof(url), "file://%s", abs_path);
    
    // Создаем D-Bus сообщение для вызова метода openUrl
    msg = dbus_message_new_method_call(
        "org.kde.kate-2508",          // имя службы (service name)
        "/MainApplication",            // путь к объекту (object path)
        "org.kde.Kate.Application",    // интерфейс (interface)
        "openUrl"                      // имя метода (method)
    );
    
    if (!msg) {
        fprintf(stderr, "Failed to create D-Bus message\n");
        dbus_connection_unref(conn);
        return 1;
    }
    
    // Подготавливаем аргументы
    const char *url_ptr = url;        // URL файла
    const char *encoding = "UTF-8";   // Кодировка
    
    // Ключевой момент: dbus_message_append_args требует адресов указателей
    if (!dbus_message_append_args(msg,
                                 DBUS_TYPE_STRING, &url_ptr,
                                 DBUS_TYPE_STRING, &encoding,
                                 DBUS_TYPE_INVALID)) {
        fprintf(stderr, "Failed to append arguments to message\n");
        dbus_message_unref(msg);
        dbus_connection_unref(conn);
        return 1;
    }
    
    // Отправляем сообщение и ждем ответа (таймаут 5 секунд)
    reply = dbus_connection_send_with_reply_and_block(conn, msg, 5000, &error);
    
    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "D-Bus method call error: %s\n", error.message);
        dbus_error_free(&error);
        dbus_message_unref(msg);
        dbus_connection_unref(conn);
        return 1;
    }
    
    if (!reply) {
        fprintf(stderr, "No reply received from Kate\n");
        dbus_message_unref(msg);
        dbus_connection_unref(conn);
        return 1;
    }
    
    // Проверяем тип ответа
    int reply_type = dbus_message_get_type(reply);
    
    if (reply_type == DBUS_MESSAGE_TYPE_METHOD_RETURN) {
        // Метод успешно выполнен, получаем возвращаемое значение
        dbus_bool_t result = FALSE;
        DBusError parse_error;
        
        dbus_error_init(&parse_error);
        
        if (dbus_message_get_args(reply, &parse_error,
                                 DBUS_TYPE_BOOLEAN, &result,
                                 DBUS_TYPE_INVALID)) {
            if (result) {
                printf("SUCCESS: File opened in Kate via D-Bus API\n");
            } else {
                printf("Kate returned false (failed to open file)\n");
            }
        } else {
            fprintf(stderr, "Failed to parse return value: %s\n", parse_error.message);
            dbus_error_free(&parse_error);
        }
    } else if (reply_type == DBUS_MESSAGE_TYPE_ERROR) {
        const char *error_name = dbus_message_get_error_name(reply);
        fprintf(stderr, "Kate returned error: %s\n", error_name ? error_name : "Unknown");
    }
    dbus_message_unref(reply);
    dbus_message_unref(msg);
    dbus_connection_unref(conn);
    
    return 0;
}