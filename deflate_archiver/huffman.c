#include "deflate.h"
#include <stdlib.h>
#include <string.h>

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
        new_node->symbol = 0xFFFF;
        
        nodes[0] = new_node;
        nodes[1] = nodes[node_count - 1];
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
        get_codes_recursive(node->left, codes, code, length + 1);
    }
    if (node->right) {
        get_codes_recursive(node->right, codes, (code | (1 << length)), length + 1);
    }
}

void huffman_get_codes(HuffmanNode *root, HuffmanCode *codes, uint16_t code, uint8_t length) {
    memset(codes, 0, CODE_TABLE_SIZE * sizeof(HuffmanCode));
    get_codes_recursive(root, codes, code, length);
}

void huffman_free_tree(HuffmanNode *root) {
    if (!root) return;
    huffman_free_tree(root->left);
    huffman_free_tree(root->right);
    free(root);
}

void huffman_canonical_codes(uint8_t *lengths, size_t count, uint16_t *codes) {
    uint32_t bl_count[MAX_BITS + 1] = {0};
    uint32_t next_code[MAX_BITS + 1] = {0};
    
    for (size_t i = 0; i < count; i++) {
        if (lengths[i] > 0) {
            bl_count[lengths[i]]++;
        }
    }
    
    uint32_t code = 0;
    for (int bits = 1; bits <= MAX_BITS; bits++) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }
    
    for (size_t i = 0; i < count; i++) {
        if (lengths[i] > 0) {
            codes[i] = (uint16_t)next_code[lengths[i]]++;
        } else {
            codes[i] = 0;
        }
    }
}

