#include "stdio.h"
#include <sys/types.h>
#include "sys/stat.h"
#include "unistd.h"
#include "stdlib.h"
#include "dirent.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h"
#define FILE_SIZE 4096
#define BLOCK_SIZE 256
#define WINDOW_SIZE 32*1024
#define MIN_MATCH 3
#define MAX_MATCH 258
#define MAX_BITS 15
#define LENGTHS_SIZE 19
#define HLIT 29
#define HDIST 29
#define HCLEN 15



struct bitReader {
    size_t buffPos;
    unsigned pos;
    unsigned char buff;
};

struct fileTree {
    char path[30];
    struct fileTree **childs;
    uint8_t childsCount;
    uint8_t isDir;
};

typedef enum {LITERAL, MATCH} tok_type;

struct bitWriter{
    uint8_t buff;
    uint8_t pos;
    size_t buffPos;
};


struct rangedData{
    bool isLL;
    uint16_t haffCode;
    uint8_t haffLen;
    uint8_t extraVal;
    uint8_t extraLen;
};

struct shortedLength{
    unsigned data;
    unsigned short extra_bits;
};


struct match{
    tok_type type;
    uint offset;
    uint length;
    unsigned char literal;
};

struct tree{
    uint16_t symbol;
    uint16_t data;
    struct tree* zeroptr;
    struct tree* oneptr;
};


void update_file_tree(struct fileTree *head, char *path, char *parent, uint8_t isDir);


void makeCanonicalCodes(
    unsigned *lengths,  
    unsigned n,               
    uint16_t *codes          
);

int find_best_match(const unsigned char *data, size_t pos, size_t size, unsigned *out_len, unsigned *out_dist);

struct match* LZ77(unsigned char *buf, size_t bytes_read, size_t *sizeMatches);

void find_smallest_pair(struct tree **arr, unsigned *indSmallest, unsigned *indSmall, size_t size);

struct tree* merge(struct tree* smallest, struct tree* small);

void refreshArr(struct tree*** arr, unsigned indSmallest, unsigned indSmall, struct tree *newNode, size_t *size);

struct tree* build_tree(unsigned *frequencies, size_t size);

bool find_code_in_tree(struct tree* node, uint16_t target, uint16_t code, unsigned depth, uint16_t *out_code, unsigned *out_len);

void length_to_code(unsigned length, uint16_t *code, uint16_t *base, uint8_t *extra);

void dist_to_code(unsigned dist, uint16_t *code, uint16_t *base, uint8_t *extra);

struct rangedData* to_range(const struct match *matches, size_t size, size_t *outSize);

void count_frequencies(struct rangedData* data, unsigned* LLfreq, unsigned* Ofreq, size_t size); 

void count_frequencies_for_lengths(struct shortedLength *data, unsigned* freq, size_t size);

void flushBuf(uint8_t *buff, struct bitWriter * writer);

void write_bits(struct bitWriter *writer, unsigned value, size_t nbits, uint8_t *buff, uint8_t extraVal, uint8_t extraLen);

void encode_range_data(struct rangedData* data, size_t size, uint8_t *buffer, struct tree *headLL, struct tree *headO, struct bitWriter *writer);

void encode_lengths(struct shortedLength *shortedLengths, size_t count, uint16_t *tree_codes, unsigned *tree_code_lens, uint8_t *buffer, struct bitWriter *writer);

void tree_bypass(struct tree* head, unsigned *frequencies, unsigned *len, unsigned *maxlen, unsigned *lengths, unsigned *pos);

void getLenghtsCodeLengths(unsigned *lengths, struct tree* head, size_t length);

void delete_tree(struct tree* head);

void count_code_length(unsigned *frequencies, struct tree *headLL, struct tree *headO, unsigned *maxlen, unsigned *lengths, unsigned *pos);

void repeats_compression(unsigned *lengths, struct shortedLength *shorted, size_t size, size_t *shorted_count);

void write_header(struct bitWriter *writer, uint8_t *buff, bool end);

void write_lengths_of_lengths(uint16_t *codes, unsigned *lengths, struct bitWriter *writer, uint8_t *buffer);

void compress_directory(unsigned char filename[256]);

void write_filename(struct bitWriter *writer, char name[30], uint8_t *buffer, size_t len);

