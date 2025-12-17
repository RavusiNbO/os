#ifndef DEFLATE_H
#define DEFLATE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Константы для deflate
#define WINDOW_SIZE (32 * 1024)
#define MIN_MATCH 3
#define MAX_MATCH 258
#define MAX_BITS 15
#define LITERAL_CODES 286 // 0-255 literals, 256 EOB, 257-285 length codes
#define DISTANCE_CODES 30 // 0-29 distance codes
#define END_OF_BLOCK 256

// Структуры для работы с битами
typedef struct {
    uint8_t *buffer;
    size_t buffer_size;
    size_t byte_pos;
    uint8_t bit_pos;
    uint8_t current_byte;
} BitWriter;

typedef struct {
    const uint8_t *buffer;
    size_t buffer_size;
    size_t byte_pos;
    uint8_t bit_pos;
    uint8_t current_byte;
} BitReader;

// Структуры для LZ77
typedef struct {
    bool is_literal;
    union {
        uint8_t literal;
        struct {
            uint16_t length;
            uint16_t distance;
        } match;
    } data;
} LZ77Token;

// Структуры для Huffman
typedef struct HuffmanNode {
    uint16_t symbol;
    uint32_t frequency;
    struct HuffmanNode *left;
    struct HuffmanNode *right;
} HuffmanNode;

typedef struct {
    uint16_t code;
    uint8_t length;
} HuffmanCode;

// Функции для работы с битами
void bit_writer_init(BitWriter *writer, uint8_t *buffer, size_t size);
void bit_writer_write_bit(BitWriter *writer, bool bit);
void bit_writer_write_bits(BitWriter *writer, uint32_t value, uint8_t num_bits);
void bit_writer_flush(BitWriter *writer);
size_t bit_writer_get_size(BitWriter *writer);

void bit_reader_init(BitReader *reader, const uint8_t *buffer, size_t size);
bool bit_reader_read_bit(BitReader *reader);
uint32_t bit_reader_read_bits(BitReader *reader, uint8_t num_bits);

// Функции LZ77
LZ77Token* lz77_compress(const uint8_t *data, size_t data_size, size_t *token_count);
uint8_t* lz77_decompress(const LZ77Token *tokens, size_t token_count, size_t *output_size);

// Функции Huffman
HuffmanNode* huffman_build_tree(uint32_t *frequencies, size_t count);
void huffman_get_codes(HuffmanNode *root, HuffmanCode *codes, uint16_t code, uint8_t length);
void huffman_free_tree(HuffmanNode *root);
// Генерирует коды на основе длин (для энкодера)
void huffman_canonical_codes(uint8_t *lengths, size_t count, HuffmanCode *codes);
// Строит дерево декодирования на основе длин (для декодера)
HuffmanNode* huffman_rebuild_tree(const uint8_t *lengths, size_t count);
// Читает символ, используя дерево
int huffman_decode_symbol(BitReader *reader, HuffmanNode *root);

// Функции deflate
int deflate_compress(const uint8_t *input, size_t input_size, uint8_t *output, size_t *output_size);
int deflate_decompress(const uint8_t *input, size_t input_size, uint8_t *output, size_t *output_size);

// Функции архиватора
int archive_directory(const char *dir_path, const char *archive_path);
int extract_archive(const char *archive_path, const char *output_dir);

#endif // DEFLATE_H