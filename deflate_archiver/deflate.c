#include "deflate.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Таблицы из спецификации Deflate (RFC 1951)
// Коды длин (257-285)
static const int length_extra_bits[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
static const int length_base[] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};

// Коды расстояний (0-29)
static const int dist_extra_bits[] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};
static const int dist_base[] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};

// Функция записи кода Хаффмана (перевернутый порядок бит для RFC 1951 canonical)
static void write_huffman_code(BitWriter *writer, uint16_t code, uint8_t length) {
    // В deflate биты кодов Хаффмана пишутся от старшего к младшему, 
    // но в рамках байта порядок может варьироваться. 
    // Для канонических кодов мы пишем биты значения code.
    // Наша реализация huffman_rebuild_tree ожидает MSB first (старший бит первым)
    for (int i = length - 1; i >= 0; i--) {
        bit_writer_write_bit(writer, (code >> i) & 1);
    }
}

// Получение кода длины и дополнительных бит
static void get_length_code(uint16_t length, uint16_t *code, uint32_t *extra, uint8_t *extra_bits) {
    if (length == 258) {
        *code = 285;
        *extra_bits = 0;
        return;
    }
    for (int i = 0; i < 29; i++) {
        if (length < length_base[i+1]) {
            *code = 257 + i;
            *extra_bits = length_extra_bits[i];
            *extra = length - length_base[i];
            return;
        }
    }
}

// Получение кода расстояния и дополнительных бит
static void get_dist_code(uint16_t dist, uint16_t *code, uint32_t *extra, uint8_t *extra_bits) {
    for (int i = 0; i < 30; i++) {
        if (dist < dist_base[i+1]) {
            *code = i;
            *extra_bits = dist_extra_bits[i];
            *extra = dist - dist_base[i];
            return;
        }
    }
}

int deflate_compress(const uint8_t *input, size_t input_size, uint8_t *output, size_t *output_size) {
    if (!input || input_size == 0 || !output || !output_size || *output_size == 0) return -1;
    
    // 1. LZ77 Сжатие
    size_t token_count;
    LZ77Token *tokens = lz77_compress(input, input_size, &token_count);
    if (!tokens) return -1;
    
    // 2. Сбор частот для Хаффмана
    uint32_t lit_freq[LITERAL_CODES] = {0};
    uint32_t dist_freq[DISTANCE_CODES] = {0};
    
    lit_freq[END_OF_BLOCK] = 1; // EOB всегда нужен
    
    for (size_t i = 0; i < token_count; i++) {
        if (tokens[i].is_literal) {
            lit_freq[tokens[i].data.literal]++;
        } else {
            uint16_t len_code;
            uint32_t extra;
            uint8_t extra_bits;
            get_length_code(tokens[i].data.match.length, &len_code, &extra, &extra_bits);
            lit_freq[len_code]++;
            
            uint16_t dist_code;
            get_dist_code(tokens[i].data.match.distance, &dist_code, &extra, &extra_bits);
            dist_freq[dist_code]++;
        }
    }
    
    // 3. Построение деревьев
    HuffmanNode *lit_root = huffman_build_tree(lit_freq, LITERAL_CODES);
    HuffmanNode *dist_root = huffman_build_tree(dist_freq, DISTANCE_CODES);
    
    HuffmanCode lit_codes[LITERAL_CODES];
    HuffmanCode dist_codes[DISTANCE_CODES];
    
    // Получаем длины кодов из дерева
    HuffmanCode temp_codes[LITERAL_CODES]; // Временный буфер
    huffman_get_codes(lit_root, temp_codes, 0, 0);
    
    // Преобразуем в канонические коды (важно для декодера)
    uint8_t lit_lengths[LITERAL_CODES];
    for(int i=0; i<LITERAL_CODES; i++) lit_lengths[i] = temp_codes[i].length;
    huffman_canonical_codes(lit_lengths, LITERAL_CODES, lit_codes);
    
    huffman_get_codes(dist_root, temp_codes, 0, 0); // Используем тот же буфер, он достаточен
    uint8_t dist_lengths[DISTANCE_CODES];
    for(int i=0; i<DISTANCE_CODES; i++) dist_lengths[i] = temp_codes[i].length;
    huffman_canonical_codes(dist_lengths, DISTANCE_CODES, dist_codes);
    
    // 4. Запись данных в битовый поток
    BitWriter writer;
    bit_writer_init(&writer, output, *output_size);
    
    // --- ЗАГОЛОВОК (Упрощенный относительно RFC1951, но функциональный) ---
    // В настоящем deflate сами таблицы сжимаются. Мы запишем длины кодов напрямую (4 бита на длину).
    // Чтобы декодер знал, сколько читать.
    
    // Записываем таблицу литералов/длин (286 кодов, по 4 бита = 143 байта)
    // Реальный deflate использует RLE для таблиц, мы для простоты пишем все длины
    for (int i = 0; i < LITERAL_CODES; i++) {
        bit_writer_write_bits(&writer, lit_lengths[i], 4); 
    }
    // Записываем таблицу расстояний (30 кодов, по 4 бита = 15 байт)
    for (int i = 0; i < DISTANCE_CODES; i++) {
        bit_writer_write_bits(&writer, dist_lengths[i], 4);
    }
    
    // --- ДАННЫЕ ---
    for (size_t i = 0; i < token_count; i++) {
        if (tokens[i].is_literal) {
            uint8_t lit = tokens[i].data.literal;
            write_huffman_code(&writer, lit_codes[lit].code, lit_codes[lit].length);
        } else {
            // Match Length
            uint16_t len_code;
            uint32_t len_extra;
            uint8_t len_bits;
            get_length_code(tokens[i].data.match.length, &len_code, &len_extra, &len_bits);
            
            write_huffman_code(&writer, lit_codes[len_code].code, lit_codes[len_code].length);
            if (len_bits > 0) bit_writer_write_bits(&writer, len_extra, len_bits);
            
            // Match Distance
            uint16_t dist_code;
            uint32_t dist_extra;
            uint8_t dist_bits;
            get_dist_code(tokens[i].data.match.distance, &dist_code, &dist_extra, &dist_bits);
            
            write_huffman_code(&writer, dist_codes[dist_code].code, dist_codes[dist_code].length);
            if (dist_bits > 0) bit_writer_write_bits(&writer, dist_extra, dist_bits);
        }
    }
    
    // Write End of Block
    write_huffman_code(&writer, lit_codes[END_OF_BLOCK].code, lit_codes[END_OF_BLOCK].length);
    bit_writer_flush(&writer);
    
    // Очистка
    free(tokens);
    huffman_free_tree(lit_root);
    huffman_free_tree(dist_root);
    
    size_t compressed_sz = bit_writer_get_size(&writer);
    if (compressed_sz >= *output_size) return -1; // Overflow
    
    *output_size = compressed_sz;
    return 0;
}

int deflate_decompress(const uint8_t *input, size_t input_size, uint8_t *output, size_t *output_size) {
    if (!input || !output || !output_size) return -1;
    
    BitReader reader;
    bit_reader_init(&reader, input, input_size);
    
    // 1. Читаем таблицы Хаффмана
    uint8_t lit_lengths[LITERAL_CODES];
    for (int i = 0; i < LITERAL_CODES; i++) {
        lit_lengths[i] = (uint8_t)bit_reader_read_bits(&reader, 4);
    }
    
    uint8_t dist_lengths[DISTANCE_CODES];
    for (int i = 0; i < DISTANCE_CODES; i++) {
        dist_lengths[i] = (uint8_t)bit_reader_read_bits(&reader, 4);
    }
    
    // 2. Восстанавливаем деревья
    HuffmanNode *lit_root = huffman_rebuild_tree(lit_lengths, LITERAL_CODES);
    HuffmanNode *dist_root = huffman_rebuild_tree(dist_lengths, DISTANCE_CODES);
    
    if (!lit_root || !dist_root) {
        huffman_free_tree(lit_root);
        huffman_free_tree(dist_root);
        return -1;
    }
    
    size_t out_pos = 0;
    size_t out_cap = *output_size;
    int status = 0;
    
    // 3. Декодируем поток
    while (1) {
        int symbol = huffman_decode_symbol(&reader, lit_root);
        
        if (symbol < 0) { status = -1; break; } // Error
        
        if (symbol < 256) {
            // Literal
            if (out_pos >= out_cap) { status = -1; break; }
            output[out_pos++] = (uint8_t)symbol;
        } else if (symbol == 256) {
            // End of block
            break; 
        } else {
            // Match (257..285)
            int len_idx = symbol - 257;
            if (len_idx >= 29) { status = -1; break; }
            
            uint32_t len_extra = bit_reader_read_bits(&reader, length_extra_bits[len_idx]);
            uint16_t length = length_base[len_idx] + len_extra;
            
            int dist_symbol = huffman_decode_symbol(&reader, dist_root);
            if (dist_symbol < 0 || dist_symbol >= 30) { status = -1; break; }
            
            uint32_t dist_extra = bit_reader_read_bits(&reader, dist_extra_bits[dist_symbol]);
            uint16_t distance = dist_base[dist_symbol] + dist_extra;
            
            // Копируем LZ77 (с учетом перекрытия)
            for (uint16_t i = 0; i < length; i++) {
                if (out_pos >= out_cap) { status = -1; break; }
                if (out_pos < distance) { status = -1; break; } // Ошибка: ссылка назад за начало буфера
                
                output[out_pos] = output[out_pos - distance];
                out_pos++;
            }
            if (status == -1) break;
        }
    }
    
    huffman_free_tree(lit_root);
    huffman_free_tree(dist_root);
    
    if (status == 0) {
        *output_size = out_pos;
        return 0;
    }
    return -1;
}