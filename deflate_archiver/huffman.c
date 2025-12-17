#include "deflate.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int compare_nodes(const void *a, const void *b) {
    const HuffmanNode *node_a = *(const HuffmanNode **)a;
    const HuffmanNode *node_b = *(const HuffmanNode **)b;
    
    if (node_a->frequency != node_b->frequency) {
        return (int)(node_a->frequency - node_b->frequency);
    }
    return (int)(node_a->symbol - node_b->symbol);
}

HuffmanNode* huffman_build_tree(uint32_t *frequencies, size_t count) {
    HuffmanNode **nodes = malloc(count * sizeof(HuffmanNode*));
    if (!nodes) return NULL;
    
    size_t node_count = 0;
    for (size_t i = 0; i < count; i++) {
        if (frequencies[i] > 0) {
            nodes[node_count] = malloc(sizeof(HuffmanNode));
            if (!nodes[node_count]) {
                for (size_t j = 0; j < node_count; j++) free(nodes[j]);
                free(nodes);
                return NULL;
            }
            nodes[node_count]->symbol = (uint16_t)i;
            nodes[node_count]->frequency = frequencies[i];
            nodes[node_count]->left = NULL;
            nodes[node_count]->right = NULL;
            node_count++;
        }
    }
    
    if (node_count == 0) {
        free(nodes);
        return NULL;
    }
    
    while (node_count > 1) {
        qsort(nodes, node_count, sizeof(HuffmanNode*), compare_nodes);
        
        HuffmanNode *new_node = malloc(sizeof(HuffmanNode));
        if (!new_node) {
            for (size_t i = 0; i < node_count; i++) free(nodes[i]);
            free(nodes);
            return NULL;
        }
        
        new_node->left = nodes[0];
        new_node->right = nodes[1];
        new_node->frequency = nodes[0]->frequency + nodes[1]->frequency;
        new_node->symbol = 0xFFFF; // Внутренний узел
        
        nodes[0] = new_node;
        
        // Сдвигаем массив
        for (size_t i = 1; i < node_count - 1; i++) {
            nodes[i] = nodes[i + 1];
        }
        
        node_count--;
    }
    
    HuffmanNode *root = nodes[0];
    free(nodes);
    return root;
}

static void get_codes_recursive(HuffmanNode *node, HuffmanCode *codes, uint16_t code, uint8_t length) {
    if (!node) return;
    
    if (!node->left && !node->right) {
        codes[node->symbol].code = code;
        codes[node->symbol].length = length;
        return;
    }
    
    if (node->left) {
        get_codes_recursive(node->left, codes, code << 1, length + 1); // Сдвиг влево
    }
    if (node->right) {
        get_codes_recursive(node->right, codes, (code << 1) | 1, length + 1);
    }
}

void huffman_get_codes(HuffmanNode *root, HuffmanCode *codes, uint16_t code, uint8_t length) {
    memset(codes, 0, LITERAL_CODES * sizeof(HuffmanCode)); // Используем макс размер, хотя передаваться может меньше
    get_codes_recursive(root, codes, code, length);
}

void huffman_free_tree(HuffmanNode *root) {
    if (!root) return;
    huffman_free_tree(root->left);
    huffman_free_tree(root->right);
    free(root);
}

// Заполняет массив кодов на основе длин (RFC 1951 Algorithm)
void huffman_canonical_codes(uint8_t *lengths, size_t count, HuffmanCode *codes) {
    uint16_t bl_count[MAX_BITS + 1] = {0};
    uint16_t next_code[MAX_BITS + 1] = {0};
    
    for (size_t i = 0; i < count; i++) {
        if (lengths[i] > 0) {
            bl_count[lengths[i]]++;
        }
    }
    
    uint16_t code = 0;
    for (int bits = 1; bits <= MAX_BITS; bits++) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }
    
    for (size_t i = 0; i < count; i++) {
        if (lengths[i] > 0) {
            codes[i].length = lengths[i];
            codes[i].code = next_code[lengths[i]]++;
        } else {
            codes[i].length = 0;
            codes[i].code = 0;
        }
    }
}

// Восстанавливает дерево декодирования из длин кодов
HuffmanNode* huffman_rebuild_tree(const uint8_t *lengths, size_t count) {
    HuffmanCode *codes = calloc(count, sizeof(HuffmanCode));
    if (!codes) return NULL;
    
    // Генерируем канонические коды, чтобы знать битовые последовательности
    uint16_t bl_count[MAX_BITS + 1] = {0};
    uint16_t next_code[MAX_BITS + 1] = {0};
    
    for (size_t i = 0; i < count; i++) 
        if (lengths[i] > 0) bl_count[lengths[i]]++;
        
    uint16_t code = 0;
    for (int bits = 1; bits <= MAX_BITS; bits++) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }
    
    for (size_t i = 0; i < count; i++) {
        if (lengths[i] > 0) {
            codes[i].length = lengths[i];
            codes[i].code = next_code[lengths[i]]++;
        }
    }

    HuffmanNode *root = calloc(1, sizeof(HuffmanNode));
    root->symbol = 0xFFFF;

    for (size_t i = 0; i < count; i++) {
        if (codes[i].length == 0) continue;
        
        HuffmanNode *current = root;
        uint16_t c = codes[i].code;
        uint8_t len = codes[i].length;
        
        // Идем по битам от старшего к младшему (в рамках длины кода)
        for (int bit = len - 1; bit >= 0; bit--) {
            bool is_set = (c >> bit) & 1;
            
            if (is_set) {
                if (!current->right) {
                    current->right = calloc(1, sizeof(HuffmanNode));
                    current->right->symbol = 0xFFFF;
                }
                current = current->right;
            } else {
                if (!current->left) {
                    current->left = calloc(1, sizeof(HuffmanNode));
                    current->left->symbol = 0xFFFF;
                }
                current = current->left;
            }
        }
        current->symbol = (uint16_t)i; // Лист
    }
    
    free(codes);
    return root;
}

int huffman_decode_symbol(BitReader *reader, HuffmanNode *root) {
    HuffmanNode *current = root;
    while (current->left || current->right) {
        if (bit_reader_read_bit(reader)) {
            current = current->right;
        } else {
            current = current->left;
        }
        if (!current) return -1; // Ошибка пути
    }
    return current->symbol;
}