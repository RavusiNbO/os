#include "stdio.h"
#include <sys/types.h>
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
#define MAX_MATCH 256



typedef enum {LITERAL, MATCH} tok_type;

struct encodedData{
    uint16_t base;
    uint16_t haffCode;
    uint8_t extraVal;
};// Кодировать символы по таблице, декодировать также по таблице haffCode+extraVal = targetChar

struct match{
    tok_type type;
    uint offset;
    uint length;
    unsigned char literal;
};

struct LLtree{
    unsigned char data;
    struct LLtree* ptr;
};

struct Block{
    unsigned end_flag;
    unsigned encoding_type;
    uint32_t data;
};

struct Otree{
    uint offset;
    struct Otree* ptr;
};

int find_best_match(const unsigned char *data, size_t pos, size_t size, unsigned *out_len, unsigned *out_dist) {
    size_t start = (pos > WINDOW_SIZE) ? pos - WINDOW_SIZE : 0;
    unsigned best_len = 0;
    unsigned best_dist = 0;
    

    for (size_t j = start; j < pos; ++j) {
        unsigned len = 0;
        while (len < MAX_MATCH && pos + len < size && data[j + len] == data[pos + len]) {
            len++;
        }
        if (len > best_len && len >= MIN_MATCH) {
            best_len = len;
            best_dist = (unsigned)(pos - j);
            if (best_len == MAX_MATCH)
                break; 
        }
    }

    *out_len = best_len;
    *out_dist = best_dist;
    return (best_len >= MIN_MATCH);
}

struct match* LZ77(unsigned char *buf, size_t bytes_read, size_t *sizeMatches)
{
    struct match* matches = malloc(bytes_read * sizeof(struct match));
    unsigned len, dist;
    size_t cur = 0;

    for (size_t i = 0; i < bytes_read ;)
    {
        if (find_best_match(buf, i, bytes_read, &len, &dist))
        {
            matches[cur].type = MATCH;
            matches[cur].length = len;
            matches[cur].offset = dist;
            i+=len;
        }
        else{
            matches[cur].type = LITERAL;
            matches[cur].literal = buf[i];
            i++;
        }
        cur++;
        
    }
    sizeMatches = cur;
    return matches;
}

void find_smallest_LLpair(struct LLtree* LLtree1, struct LLtree* LLtree2);

void find_smallest_Opair(struct Otree* Otree1, struct Otree* Otree2);

void build_LLtree(struct LLtree* LLtree, unsigned *frequencies)
{

}

void build_Otree(struct Otree* Otree, char* file_data);

void count_frequencies(struct match* matches, unsigned* LLfreq, unsigned* Ofreq, size_t size) 
{

    for (size_t i = 0; i < size; ++i){
        if (matches[i].type == LITERAL)
        {
            LLfreq[(unsigned)matches[i].literal]++;
        }
        else{
            LLfreq[(unsigned)matches[i].length]++;
            Ofreq[(unsigned)matches[i].offset]++;
        }
    }
};

void encode();





int main()
{
    DIR* dir;
    struct dirent* entry;
    unsigned char filename[256];
    FILE* file;
    char c;
    size_t i, sizeMatches;
    unsigned char* buff = malloc(1024*1024);
    struct match *matches;
    unsigned *LLfrequencies, *Ofrequencies;
    char path[512];
    
    scanf("%255s", filename);

    

    dir = opendir(filename);

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        snprintf(path, sizeof(path), "%s/%s", filename, entry->d_name);
        file = fopen(path, "rb");
        if (!file) continue;
        i = 0;
        while ((c = getc(file)) != EOF)
        {
            buff[i++] = c;
        }

        matches = LZ77(buff, i, sizeMatches);
        LLfrequencies = calloc(MAX_MATCH, sizeof(unsigned));
        Ofrequencies = malloc(i * sizeof(unsigned));
        count_frequencies(matches, LLfrequencies, Ofrequencies, &sizeMatches);

    }

    return 0;
}