#include "deflate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PATH 4096

typedef struct FileEntry {
    char path[MAX_PATH];
    bool is_directory;
    size_t data_size;
    uint8_t *data;
    struct FileEntry *next;
} FileEntry;

static FileEntry* read_directory(const char *dir_path) {
    FileEntry *head = NULL;
    FileEntry **tail = &head;
    
    DIR *dir = opendir(dir_path);
    if (!dir) return NULL;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) != 0) continue;
        
        FileEntry *file_entry = malloc(sizeof(FileEntry));
        if (!file_entry) continue;
        
        strncpy(file_entry->path, full_path, MAX_PATH - 1);
        file_entry->path[MAX_PATH - 1] = '\0';
        file_entry->is_directory = S_ISDIR(st.st_mode);
        file_entry->data = NULL;
        file_entry->data_size = 0;
        file_entry->next = NULL;
        
        if (!file_entry->is_directory) {
            FILE *f = fopen(full_path, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                file_entry->data_size = ftell(f);
                fseek(f, 0, SEEK_SET);
                
                if (file_entry->data_size > 0) {
                    file_entry->data = malloc(file_entry->data_size);
                    if (file_entry->data) {
                        size_t bytes_read = fread(file_entry->data, 1, file_entry->data_size, f);
                        if (bytes_read != file_entry->data_size) {
                            fprintf(stderr, "Warning: Only read %zu of %zu bytes from %s\n", 
                                   bytes_read, file_entry->data_size, full_path);
                            file_entry->data_size = bytes_read;
                        }
                    }
                }
                fclose(f);
            }
        }
        
        *tail = file_entry;
        tail = &file_entry->next;
    }
    
    closedir(dir);
    return head;
}

static void free_file_list(FileEntry *head) {
    while (head) {
        FileEntry *next = head->next;
        free(head->data);
        free(head);
        head = next;
    }
}

int archive_directory(const char *dir_path, const char *archive_path) {
    FileEntry *files = read_directory(dir_path);
    if (!files) {
        fprintf(stderr, "Error: Cannot read directory %s\n", dir_path);
        return -1;
    }
    
    FILE *archive = fopen(archive_path, "wb");
    if (!archive) {
        fprintf(stderr, "Error: Cannot create archive %s\n", archive_path);
        free_file_list(files);
        return -1;
    }
    
    // Записываем количество файлов
    size_t file_count = 0;
    for (FileEntry *e = files; e; e = e->next) file_count++;
    fwrite(&file_count, sizeof(size_t), 1, archive);
    
    // Обрабатываем каждый файл
    for (FileEntry *entry = files; entry; entry = entry->next) {
        // Извлекаем относительный путь от исходной директории
        const char *relative_path = entry->path;
        if (strncmp(relative_path, dir_path, strlen(dir_path)) == 0) {
            relative_path += strlen(dir_path);
            if (*relative_path == '/') relative_path++;
        }
        
        // Записываем путь и тип
        uint16_t path_len = (uint16_t)strlen(relative_path);
        fwrite(&path_len, sizeof(uint16_t), 1, archive);
        fwrite(relative_path, 1, path_len, archive);
        fwrite(&entry->is_directory, sizeof(bool), 1, archive);
        
        if (!entry->is_directory && entry->data && entry->data_size > 0) {
            // Сжимаем данные - выделяем достаточно памяти
            size_t compressed_size = entry->data_size * 2 + 1024; // Запас для заголовков
            uint8_t *compressed = malloc(compressed_size);
            if (compressed) {
                if (deflate_compress(entry->data, entry->data_size, compressed, &compressed_size) == 0 && 
                    compressed_size > 0 && compressed_size < entry->data_size) {
                    // Сжатие успешно и данные уменьшились
                    fwrite(&compressed_size, sizeof(size_t), 1, archive);
                    fwrite(compressed, 1, compressed_size, archive);
                } else {
                    // Если сжатие не удалось или не эффективно, сохраняем как есть
                    // Используем специальный маркер: размер с битом "несжато"
                    size_t uncompressed_size = entry->data_size | ((size_t)1 << (sizeof(size_t) * 8 - 1));
                    fwrite(&uncompressed_size, sizeof(size_t), 1, archive);
                    fwrite(entry->data, 1, entry->data_size, archive);
                }
                free(compressed);
            } else {
                // Если не удалось выделить память, сохраняем как есть
                size_t uncompressed_size = entry->data_size | ((size_t)1 << (sizeof(size_t) * 8 - 1));
                fwrite(&uncompressed_size, sizeof(size_t), 1, archive);
                fwrite(entry->data, 1, entry->data_size, archive);
            }
        } else {
            size_t zero = 0;
            fwrite(&zero, sizeof(size_t), 1, archive);
        }
    }
    
    fclose(archive);
    free_file_list(files);
    return 0;
}

int extract_archive(const char *archive_path, const char *output_dir) {
    FILE *archive = fopen(archive_path, "rb");
    if (!archive) {
        fprintf(stderr, "Error: Cannot open archive %s\n", archive_path);
        return -1;
    }
    
    // Создаем выходную директорию
    mkdir(output_dir, 0755);
    
    // Читаем количество файлов
    size_t file_count;
    if (fread(&file_count, sizeof(size_t), 1, archive) != 1) {
        fclose(archive);
        return -1;
    }
    
    // Извлекаем каждый файл
    for (size_t i = 0; i < file_count; i++) {
        // Читаем путь
        uint16_t path_len;
        if (fread(&path_len, sizeof(uint16_t), 1, archive) != 1) break;
        
        char path[MAX_PATH];
        if (fread(path, 1, path_len, archive) != path_len) break;
        path[path_len] = '\0';
        
        // Читаем тип
        bool is_directory;
        if (fread(&is_directory, sizeof(bool), 1, archive) != 1) break;
        
        // Читаем размер данных
        size_t data_size;
        if (fread(&data_size, sizeof(size_t), 1, archive) != 1) break;
        
        // Проверяем, сжаты ли данные (старший бит = 0) или нет (старший бит = 1)
        bool is_compressed = (data_size & ((size_t)1 << (sizeof(size_t) * 8 - 1))) == 0;
        data_size &= ~((size_t)1 << (sizeof(size_t) * 8 - 1)); // Убираем флаг
        
        // Создаем полный путь
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", output_dir, path);
        
        if (is_directory) {
            // Создаем директорию
            mkdir(full_path, 0755);
        } else if (data_size > 0) {
            // Создаем директории для пути, если нужно
            char *dir_sep = strrchr(full_path, '/');
            if (dir_sep) {
                *dir_sep = '\0';
                // Создаем все необходимые директории
                char *p = full_path;
                if (*p == '/') p++;
                while (*p) {
                    if (*p == '/') {
                        *p = '\0';
                        mkdir(full_path, 0755);
                        *p = '/';
                    }
                    p++;
                }
                mkdir(full_path, 0755);
                *dir_sep = '/';
            }
            
            // Читаем сжатые данные
            uint8_t *compressed = malloc(data_size);
            if (!compressed) continue;
            
            size_t bytes_read = fread(compressed, 1, data_size, archive);
            if (bytes_read == data_size) {
                if (is_compressed) {
                    // Декомпрессия
                    size_t decompressed_size = data_size * 10;
                    uint8_t *decompressed = malloc(decompressed_size);
                    if (decompressed) {
                        if (deflate_decompress(compressed, data_size, decompressed, &decompressed_size) == 0 && decompressed_size > 0) {
                            // Сохраняем файл
                            FILE *out = fopen(full_path, "wb");
                            if (out) {
                                size_t written = fwrite(decompressed, 1, decompressed_size, out);
                                fclose(out);
                                if (written == decompressed_size) {
                                    printf("Extracted: %s (decompressed, %zu bytes)\n", full_path, decompressed_size);
                                } else {
                                    fprintf(stderr, "Error: Failed to write all data to %s\n", full_path);
                                }
                            } else {
                                fprintf(stderr, "Error: Cannot create file %s\n", full_path);
                            }
                        } else {
                            fprintf(stderr, "Error: Decompression failed for %s\n", path);
                        }
                        free(decompressed);
                    }
                } else {
                    // Данные не сжаты, сохраняем как есть
                    FILE *out = fopen(full_path, "wb");
                    if (out) {
                        size_t written = fwrite(compressed, 1, data_size, out);
                        fclose(out);
                        if (written == data_size) {
                            printf("Extracted: %s (uncompressed, %zu bytes)\n", full_path, data_size);
                        } else {
                            fprintf(stderr, "Error: Failed to write all data to %s (wrote %zu of %zu)\n", full_path, written, data_size);
                        }
                    } else {
                        fprintf(stderr, "Error: Cannot create file %s\n", full_path);
                    }
                }
            } else {
                fprintf(stderr, "Error: Failed to read data for %s (read %zu of %zu)\n", path, bytes_read, data_size);
            }
            free(compressed);
        }
    }
    
    fclose(archive);
    return 0;
}

