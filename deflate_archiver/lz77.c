#include "deflate.h"
#include <stdlib.h>
#include <string.h>

static int find_longest_match(const uint8_t *data, size_t pos, size_t data_size, 
                               uint16_t *match_length, uint16_t *match_distance) {
    size_t start = (pos > WINDOW_SIZE) ? pos - WINDOW_SIZE : 0;
    int best_len = 0;
    int best_dist = 0;
    
    for (size_t i = start; i < pos; i++) {
        int len = 0;
        while (len < MAX_MATCH && 
               pos + len < data_size && 
               i + len < pos &&
               data[i + len] == data[pos + len]) {
            len++;
        }
        
        if (len >= MIN_MATCH && len > best_len) {
            best_len = len;
            best_dist = (uint16_t)(pos - i);
            if (best_len == MAX_MATCH) break;
        }
    }
    
    if (best_len >= MIN_MATCH) {
        *match_length = (uint16_t)best_len;
        *match_distance = best_dist;
        return 1;
    }
    
    return 0;
}

LZ77Token* lz77_compress(const uint8_t *data, size_t data_size, size_t *token_count) {
    LZ77Token *tokens = malloc(data_size * sizeof(LZ77Token));
    if (!tokens) return NULL;
    
    size_t token_idx = 0;
    size_t pos = 0;
    
    while (pos < data_size) {
        uint16_t match_length, match_distance;
        
        if (find_longest_match(data, pos, data_size, &match_length, &match_distance)) {
            tokens[token_idx].is_literal = false;
            tokens[token_idx].data.match.length = match_length;
            tokens[token_idx].data.match.distance = match_distance;
            pos += match_length;
        } else {
            tokens[token_idx].is_literal = true;
            tokens[token_idx].data.literal = data[pos];
            pos++;
        }
        token_idx++;
    }
    
    *token_count = token_idx;
    return tokens;
}

uint8_t* lz77_decompress(const LZ77Token *tokens, size_t token_count, size_t *output_size) {
    // Сначала вычисляем размер выходных данных
    size_t total_size = 0;
    for (size_t i = 0; i < token_count; i++) {
        if (tokens[i].is_literal) {
            total_size++;
        } else {
            total_size += tokens[i].data.match.length;
        }
    }
    
    uint8_t *output = malloc(total_size);
    if (!output) return NULL;
    
    size_t output_pos = 0;
    
    for (size_t i = 0; i < token_count; i++) {
        if (tokens[i].is_literal) {
            output[output_pos++] = tokens[i].data.literal;
        } else {
            uint16_t length = tokens[i].data.match.length;
            uint16_t distance = tokens[i].data.match.distance;
            
            for (uint16_t j = 0; j < length; j++) {
                output[output_pos] = output[output_pos - distance];
                output_pos++;
            }
        }
    }
    
    *output_size = total_size;
    return output;
}

