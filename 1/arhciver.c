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
#define MAX_MATCH 258



typedef enum {LITERAL, MATCH} tok_type;

struct rangedData{
    bool isLL;
    uint16_t haffCode;
    uint8_t extraVal;
};


struct match{
    tok_type type;
    uint offset;
    uint length;
    unsigned char literal;
};

struct tree{
    uint16_t data;
    struct LLtree* zeroptr;
    struct LLtree* oneptr;
};

struct Block{
    unsigned end_flag;
    unsigned encoding_type;
    uint32_t data;
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
    *sizeMatches = cur;
    return matches;
}

void find_smallest_pair(struct tree *arr, unsigned *indSmallest, unsigned *indSmall, size_t size)
{
    struct tree *smallest, *small;

    if (arr[0].data > arr[1].data)
    {
        smallest = &arr[1];
        *indSmallest = 1;
        small = &arr[0];
        *indSmall = 0;

    }
    else{
        small = &arr[1];
        *indSmall = 1;
        smallest = &arr[0];
        *indSmallest = 0;
    }
    

    for (size_t i = 2 ; i < size; i++)
    {
        if (arr[i].data < smallest->data)
        {
            small = smallest;
            *indSmall = indSmallest;
            smallest = &arr[i];
            *indSmallest = i;
        }
        else if (arr[i].data < small->data)
        {
            small = &arr[i];
            *indSmall = i;
        }
    }

}

struct tree* merge(struct tree* smallest, struct tree* small)
{
    struct tree *newNode = malloc(sizeof(struct tree));
    newNode->data = small->data + smallest->data;
    newNode->zeroptr = smallest;
    newNode->oneptr = small;
}

struct tree* refreshArr(struct tree** arr, unsigned indSmallest, unsigned indSmall, struct tree *newNode, size_t *size)
{
    struct tree* newArr = calloc(*size - 1, sizeof(struct tree));
    size_t j = 0;
    for (size_t i = 0; i < *size; i ++)
    {
        if (i != indSmallest && i != indSmall)
        {
            newArr[j++] = (*arr)[i];
        }
    }

    newArr[j] = *newNode;
    free(*arr);
    *arr = newArr;

}

struct tree* build_tree(struct tree *arr, unsigned *frequencies, size_t size)
{
    struct tree *newNode, *arr = calloc(size, sizeof(struct tree));
    unsigned indSmallest, indSmall;

    for (size_t i = 0; i < size; i++)
    {
        arr[i].data = frequencies[i];
    }

    for (size_t i = 0 ; i < size - 1; i++)
    {
        find_smallest_pair(arr, &indSmallest, &indSmall, size);
        newNode = merge(&arr[indSmallest], &arr[indSmall]);
        refreshArr(&arr, indSmallest, indSmall, newNode, &size);
    }

    return arr;
}

bool search_tree(struct tree* head, unsigned target, uint8_t *encoded, unsigned *index)
{
    size_t zeros = 0, ones = 0;

    
    if (head->data == target)
    {
        return true;
    }
    if (head->zeroptr)
    {
        encoded[*index++] = 0;
        if (!search_tree(head->zeroptr, target, encoded, index) && head->oneptr)
        {
            encoded[--*index] = 1;
            if (!search_tree(head->oneptr, target, encoded, index))
            {
                return false;
            }
            else return true;
        }
        else return true;
    }
    
}

void length_to_code(unsigned length, uint16_t *code, uint16_t *base, uint8_t *extra) {
    static const uint16_t bases[] = {
      3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
      35,43,51,59,67,83,99,115,131,163,195,227,258
    };
    static const uint8_t extras[] = {
      0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,
      3,3,3,3,4,4,4,4,5,5,5,5,0
    };
    for (int i = 0; i < 29; ++i) {
        uint16_t b = bases[i];
        uint8_t e = extras[i];
        uint16_t max = (i==28) ? 258 : (uint16_t)(bases[i+1] + ((1u<<extras[i]) - 1));
        if (length >= b && length <= max) {
            *code = 257 + i;
            *base = b;
            *extra = e;
            return;
        }
    }
    *code = 285; *base = 258; *extra = 0;
}

void dist_to_code(unsigned dist, uint16_t *code, uint16_t *base, uint8_t *extra) {
    static const uint16_t dbases[] = {
      1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
      257,385,513,769,1025,1537,2049,3073,4097,
      6145,8193,12289,16385,24577
    };
    static const uint8_t dextra[] = {
      0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,
      7,7,8,8,9,9,10,10,11,11,12,12,13,13
    };
    for (int i=0;i<30;i++){
        uint16_t b = dbases[i];
        uint8_t e = dextra[i];
        uint32_t max = (i==29)?32768u:(uint32_t)(dbases[i] + ((1u<<e)-1));
        if (dist >= b && dist <= max) {
            *code = i;
            *base = b;
            *extra = e;
            return;
        }
    }
    *code = 29; *base = 24577; *extra = 13;
}

struct rangedData* to_range(const struct match *matches, size_t size, size_t *outSize)
{
    struct rangedData* arr = malloc(size * 2 * sizeof(struct rangedData));
    size_t k = 0;


    for (size_t i = 0; i < size; i++) {
        if (matches[i].type == LITERAL) {
            arr[k].isLL = true;
            arr[k].haffCode = matches[i].literal;
            arr[k].extraVal = 0;
            k++;
        } else {
            uint16_t lcode, lbase;
            uint8_t lextra;
            length_to_code(matches[i].length, &lcode, &lbase, &lextra);
            arr[k].isLL = true;
            arr[k].haffCode = lcode;
            arr[k].extraVal = matches[i].length - lbase;
            k++;

            uint16_t dcode, dbase;
            uint8_t dextra;
            dist_to_code(matches[i].offset, &dcode, &dbase, &dextra);
            arr[k].isLL = false;
            arr[k].haffCode = dcode;
            arr[k].extraVal = matches[i].offset - dbase;
            k++;
        }
    }

    *outSize = k;
    return arr;
}


void count_frequencies(struct rangedData* data, unsigned* LLfreq, unsigned* Ofreq, size_t size) 
{
    for (size_t i = 0; i < size; i++) {
        if (data[i].isLL)
            LLfreq[data[i].haffCode]++;
        else
            Ofreq[data[i].haffCode]++;
    }
}

void encode_range_data(struct rangedData* data, size_t size, uint8_t *buffer, struct tree *headLL, struct tree *headO)
{
    uint16_t buff; // в нем буду сжимать биты 
    unsigned index;
    uint8_t encoded[288];



    for (size_t i = 0 ; i < size; i++)
    {
        index = 0;
        if (data[i].isLL && search_tree(headLL, data[i].haffCode, encoded, &index))
        {

        }
        else if (search_tree(headO, data[i].haffCode, encoded, index)){

        }
    }



}





int main()
{
    DIR* dir;
    struct dirent* entry;
    unsigned char filename[256];
    FILE* file;
    char c;
    size_t i, sizeMatches = 0, sizeData = 0;
    unsigned char* buff = malloc(1024*1024);
    struct match *matches;
    unsigned *LLfrequencies, *Ofrequencies;
    char path[512];
    struct rangedData* encodedData;
    struct rangedData* rangedData;
    struct tree *arrLL, *arrO, *headLL, *headO;
    uint8_t *buffer;

    
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

        matches = LZ77(buff, i, &sizeMatches);

        LLfrequencies = calloc(288, sizeof(unsigned));
        Ofrequencies = malloc(30 * sizeof(unsigned));
        rangedData = calloc(sizeMatches, sizeof(struct rangedData));
        sizeData = 0;
        

        to_range(matches, sizeMatches, &sizeData);
        count_frequencies(rangedData, LLfrequencies, Ofrequencies, sizeData);
        headLL = build_tree(arrLL, LLfrequencies, 288);
        headO = build_tree(arrO, Ofrequencies, 30);

        buffer = calloc(sizeMatches, sizeof(struct rangedData));




    }

    return 0;
}