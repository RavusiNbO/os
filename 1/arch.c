// deflate_simple.c
// Простой DEFLATE-компрессор (fixed/static Huffman, наивный LZ77).
// Супер-учебная версия, корректная, но не оптимальная по скорости.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* --- Параметры LZ77 / Deflate --- */
#define WINDOW_SIZE 32768   // 32 KiB
#define MAX_MATCH   258
#define MIN_MATCH   3

/* --- Структуры токенов (LZ77 output) --- */
typedef enum { TOK_LITERAL, TOK_MATCH } tok_type_t;
typedef struct {
    tok_type_t type;
    uint8_t lit;    // valid if literal
    int len;        // valid if match
    int dist;       // valid if match
} token_t;

/* --- Битовый писатель (LSB-first per RFC1951) --- */
typedef struct {
    FILE *f;
    uint8_t bitbuf;
    int bitcount;
} bitwriter_t;

static void bw_init(bitwriter_t *bw, FILE *f) { bw->f = f; bw->bitbuf = 0; bw->bitcount = 0; }
static void bw_write_bits(bitwriter_t *bw, uint32_t bits, int n) {
    // записать n младших битов из bits в поток (LSB-first)
    while (n > 0) {
        int free = 8 - bw->bitcount;
        int take = (n < free) ? n : free;
        bw->bitbuf |= ((bits & ((1u << take) - 1)) << bw->bitcount);
        bw->bitcount += take;
        bits >>= take;
        n -= take;
        if (bw->bitcount == 8) {
            fputc(bw->bitbuf, bw->f);
            bw->bitbuf = 0;
            bw->bitcount = 0;
        }
    }
}
static void bw_flush(bitwriter_t *bw) {
    if (bw->bitcount > 0) {
        fputc(bw->bitbuf, bw->f);
        bw->bitbuf = 0;
        bw->bitcount = 0;
    }
}

/* --- Fixed/static Huffman: canonical codes constructed from known lengths --- */
/*
RFC1951 fixed codes lengths:
 - literal/length alphabet (0..287):
    0-143 -> 8 bits
    144-255 -> 9 bits
    256-279 -> 7 bits
    280-287 -> 8 bits
 - distance alphabet (0..31): all 5 bits
We will build canonical codes from these lengths.
*/

#define LITLEN_ALPH 288
#define DIST_ALPH   32
static uint16_t litlen_code[LITLEN_ALPH]; // code values
static uint8_t  litlen_len[LITLEN_ALPH];  // code lengths
static uint16_t dist_code[DIST_ALPH];
static uint8_t  dist_len[DIST_ALPH];

static void build_fixed_huffman(void) {
    // fill litlen_len according to RFC
    for (int i = 0; i <= 143; ++i) litlen_len[i] = 8;
    for (int i = 144; i <= 255; ++i) litlen_len[i] = 9;
    for (int i = 256; i <= 279; ++i) litlen_len[i] = 7;
    for (int i = 280; i <= 287; ++i) litlen_len[i] = 8;
    // distances
    for (int i = 0; i < DIST_ALPH; ++i) dist_len[i] = 5;

    // canonical code generation
    // 1) count bl_count
    int max_bits = 9; // lit/len max 9, dist max 5
    int bl_count[16] = {0};
    for (int i = 0; i < LITLEN_ALPH; ++i) if (litlen_len[i]) bl_count[litlen_len[i]]++;
    // 2) next_code
    int next_code[16] = {0};
    int code = 0;
    bl_count[0] = 0;
    for (int bits = 1; bits <= max_bits; ++bits) {
        code = (code + bl_count[bits-1]) << 1;
        next_code[bits] = code;
    }
    // 3) assign codes
    for (int n = 0; n < LITLEN_ALPH; ++n) {
        int len = litlen_len[n];
        if (len != 0) {
            litlen_code[n] = next_code[len];
            next_code[len]++;
        }
    }
    // distances
    max_bits = 5;
    memset(bl_count, 0, sizeof(bl_count));
    for (int i = 0; i < DIST_ALPH; ++i) if (dist_len[i]) bl_count[dist_len[i]]++;
    code = 0;
    bl_count[0] = 0;
    for (int bits = 1; bits <= max_bits; ++bits) {
        code = (code + bl_count[bits-1]) << 1;
        next_code[bits] = code;
    }
    for (int n = 0; n < DIST_ALPH; ++n) {
        int len = dist_len[n];
        if (len != 0) {
            dist_code[n] = next_code[len];
            next_code[len]++;
        }
    }
}

/* --- Tables for length and distance extra bits (RFC1951) --- */
static const int length_base[] = {
  3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
  35,43,51,59,67,83,99,115,131,163,195,227,258
};
static const int length_extra[] = {
  0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,
  3,3,3,3,4,4,4,4,5,5,5,5,0
};
// indexes correspond to length codes 257..285 (285 and 286-287 handled by tables)

static const int dist_base[] = {
  1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
  257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
};
static const int dist_extra[] = {
  0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,
  7,7,8,8,9,9,10,10,11,11,12,12,13,13
};

/* --- Наивный LZ77: ищем наилучшее совпадение в окне --- */
static int find_longest(const uint8_t *data, size_t size, size_t pos, int *out_dist) {
    size_t start = (pos >= WINDOW_SIZE) ? pos - WINDOW_SIZE : 0;
    size_t max_len = (size - pos > MAX_MATCH) ? MAX_MATCH : (size - pos);
    if (max_len < MIN_MATCH) return 0;
    int best_len = 0;
    int best_dist = 0;
    for (size_t i = start; i < pos; ++i) {
        if (data[i] != data[pos]) continue;
        int len = 1;
        while (len < max_len && data[i + len] == data[pos + len]) ++len;
        if (len > best_len) {
            best_len = len;
            best_dist = (int)(pos - i);
            if (best_len == (int)max_len) break;
        }
    }
    if (best_len >= MIN_MATCH) { *out_dist = best_dist; return best_len; }
    return 0;
}

/* --- LZ77 pass: получаем массив токенов (динамически) --- */
static token_t *lz77_tokens(const uint8_t *data, size_t size, size_t *out_count) {
    size_t cap = 1024;
    token_t *tokens = malloc(cap * sizeof(token_t));
    size_t tcount = 0;
    size_t pos = 0;
    while (pos < size) {
        int dist = 0;
        int len = find_longest(data, size, pos, &dist);
        if (len >= MIN_MATCH) {
            if (tcount >= cap) { cap *= 2; tokens = realloc(tokens, cap * sizeof(token_t)); }
            tokens[tcount].type = TOK_MATCH;
            tokens[tcount].len = len;
            tokens[tcount].dist = dist;
            tcount++;
            pos += len;
        } else {
            if (tcount >= cap) { cap *= 2; tokens = realloc(tokens, cap * sizeof(token_t)); }
            tokens[tcount].type = TOK_LITERAL;
            tokens[tcount].lit = data[pos];
            tcount++;
            pos++;
        }
    }
    *out_count = tcount;
    return tokens;
}

/* --- Функции вывода символов через static Huffman --- */
static void write_litlen_symbol(bitwriter_t *bw, int sym) {
    // sym in [0..287] (0..255 literal, 256 EOS, 257-285 lengths)
    int len = litlen_len[sym];
    uint16_t code = litlen_code[sym];
    // write code with len bits LSB-first
    bw_write_bits(bw, code, len);
}

static void write_dist_symbol(bitwriter_t *bw, int sym) {
    int len = dist_len[sym];
    uint16_t code = dist_code[sym];
    bw_write_bits(bw, code, len);
}

/* --- Преобразование length->length_code & extra bits (RFC1951) --- */
static void emit_length(bitwriter_t *bw, int length) {
    if (length == 258) {
        // code 285 has no extra bits
        write_litlen_symbol(bw, 285);
        return;
    }
    // find index in length_base table: codes 257..285
    int code = -1;
    for (int i = 0; i < (int)(sizeof(length_base)/sizeof(length_base[0])); ++i) {
        int base = length_base[i];
        int next = (i + 1 < (int)(sizeof(length_base)/sizeof(length_base[0]))) ? length_base[i+1] : 10000;
        if (length >= base && length < next) { code = 257 + i; break; }
    }
    if (code == -1) {
        // fallback (shouldn't happen)
        if (length == 258) { write_litlen_symbol(bw, 285); return; }
        code = 285;
    }
    write_litlen_symbol(bw, code);
    int idx = code - 257;
    int extra = length_extra[idx];
    if (extra > 0) {
        int value = length - length_base[idx];
        bw_write_bits(bw, value, extra);
    }
}

/* --- Преобразование distance->dist_code & extra bits --- */
static void emit_distance(bitwriter_t *bw, int dist) {
    // find code
    int code = -1;
    for (int i = 0; i < (int)(sizeof(dist_base)/sizeof(dist_base[0])); ++i) {
        int base = dist_base[i];
        int next = (i + 1 < (int)(sizeof(dist_base)/sizeof(dist_base[0]))) ? dist_base[i+1] : 1000000;
        if (dist >= base && dist < next) { code = i; break; }
    }
    if (code == -1) code = (int)sizeof(dist_base)/sizeof(dist_base[0]) - 1;
    write_dist_symbol(bw, code);
    int extra = dist_extra[code];
    if (extra > 0) {
        int value = dist - dist_base[code];
        bw_write_bits(bw, value, extra);
    }
}

/* --- Основная процедура: генерирует один static-Huffman DEFLATE block (BFINAL=1) --- */
static int compress_to_deflate(FILE *out, const uint8_t *inbuf, size_t insz) {
    bitwriter_t bw;
    bw_init(&bw, out);

    // Build fixed Huffman (codes and lengths)
    build_fixed_huffman();

    // 1) BFINAL=1, BTYPE=01 (static Huffman)
    bw_write_bits(&bw, 1, 1); // BFINAL = 1
    bw_write_bits(&bw, 1, 2); // BTYPE = 01

    // 2) LZ77 -> tokens
    size_t tcount = 0;
    token_t *tokens = lz77_tokens(inbuf, insz, &tcount);

    // 3) Emit tokens using static Huffman
    for (size_t i = 0; i < tcount; ++i) {
        if (tokens[i].type == TOK_LITERAL) {
            int sym = tokens[i].lit; // 0..255
            write_litlen_symbol(&bw, sym);
        } else {
            // match
            emit_length(&bw, tokens[i].len);
            emit_distance(&bw, tokens[i].dist);
        }
    }
    // End of block symbol 256
    write_litlen_symbol(&bw, 256);

    // flush bits
    bw_flush(&bw);

    free(tokens);
    return 0;
}

/* --- Helper: read entire file into buffer --- */
static uint8_t *read_file(const char *name, size_t *out_size) {
    FILE *f = fopen(name, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    uint8_t *buf = malloc(sz ? (size_t)sz : 1);
    if (!buf) { fclose(f); return NULL; }
    if (sz) fread(buf, 1, (size_t)sz, f);
    fclose(f);
    *out_size = (size_t)sz;
    return buf;
}

/* --- main --- */
int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s input-file output-deflate\n", argv[0]);
        return 1;
    }
    size_t insz;
    uint8_t *inbuf = read_file(argv[1], &insz);
    if (!inbuf) { fprintf(stderr, "Failed to read input\n"); return 1; }
    FILE *out = fopen(argv[2], "wb");
    if (!out) { perror("fopen out"); free(inbuf); return 1; }

    compress_to_deflate(out, inbuf, insz);

    fclose(out);
    free(inbuf);
    return 0;
}
