#include "deflate.h"
#include <stdlib.h>
#include <string.h>

static void write_huffman_code(BitWriter *writer, uint16_t code, uint8_t length) {
    for (int i = length - 1; i >= 0; i--) {
        bit_writer_write_bit(writer, (code >> i) & 1);
    }
}

// Функция для декомпрессии (будет использована в полной реализации)
// static uint16_t read_huffman_code(BitReader *reader, const HuffmanCode *codes, size_t code_count) {
//     uint16_t code = 0;
//     uint8_t length = 0;
//     
//     while (length < MAX_BITS) {
//         code = (code << 1) | (bit_reader_read_bit(reader) ? 1 : 0);
//         length++;
//         
//         for (size_t i = 0; i < code_count; i++) {
//             if (codes[i].length == length && codes[i].code == code) {
//                 return (uint16_t)i;
//             }
//         }
//     }
//     
//     return 0xFFFF; // Ошибка
// }

int deflate_compress(const uint8_t *input, size_t input_size, uint8_t *output, size_t *output_size) {
    if (!input || input_size == 0 || !output || !output_size || *output_size == 0) {
        return -1;
    }
    
    // LZ77 сжатие
    size_t token_count;
    LZ77Token *tokens = lz77_compress(input, input_size, &token_count);
    if (!tokens || token_count == 0) {
        return -1;
    }
    
    // Упрощенный формат: сохраняем LZ77 токены напрямую
    // Формат: [количество токенов][токены]
    size_t needed_size = sizeof(size_t) + token_count * sizeof(LZ77Token);
    if (needed_size > *output_size) {
        free(tokens);
        return -1;
    }
    
    // Записываем количество токенов
    memcpy(output, &token_count, sizeof(size_t));
    
    // Записываем токены
    memcpy(output + sizeof(size_t), tokens, token_count * sizeof(LZ77Token));
    
    *output_size = needed_size;
    free(tokens);
    return 0;
}

int deflate_decompress(const uint8_t *input, size_t input_size, uint8_t *output, size_t *output_size) {
    if (!input || input_size == 0 || !output || !output_size || *output_size == 0) {
        return -1;
    }
    
    // Читаем количество токенов
    if (input_size < sizeof(size_t)) {
        return -1;
    }
    
    size_t token_count;
    memcpy(&token_count, input, sizeof(size_t));
    
    // Проверяем размер
    size_t needed_size = sizeof(size_t) + token_count * sizeof(LZ77Token);
    if (input_size < needed_size) {
        return -1;
    }
    
    // Читаем токены
    LZ77Token *tokens = (LZ77Token *)(input + sizeof(size_t));
    
    // Декомпрессируем используя LZ77
    size_t decompressed_size = 0;
    uint8_t *decompressed = lz77_decompress(tokens, token_count, &decompressed_size);
    if (!decompressed || decompressed_size == 0) {
        return -1;
    }
    
    // Проверяем, что выходной буфер достаточно большой
    if (decompressed_size > *output_size) {
        free(decompressed);
        *output_size = decompressed_size; // Сообщаем нужный размер
        return -1;
    }
    
    // Копируем результат
    memcpy(output, decompressed, decompressed_size);
    *output_size = decompressed_size;
    free(decompressed);
    return 0;
}

