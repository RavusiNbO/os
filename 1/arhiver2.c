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
#define MAX_BITS 15
#define LENGTHS_SIZE 19
#define HLIT 31
#define HDIST 29
#define HCLEN 15

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

// ИСПРАВЛЕНО: Добавлено поле symbol для хранения символа в дереве
struct tree{
    uint16_t data;    // частота
    uint16_t symbol;  // символ (для листьев)
    struct tree* zeroptr;
    struct tree* oneptr;
};

void makeCanonicalCodes(const unsigned *lengths, unsigned n, uint16_t *codes) {
    unsigned bl_count[MAX_BITS + 1] = {0};
    unsigned next_code[MAX_BITS + 1] = {0};

    for (unsigned i = 0; i < n; i++) {
        unsigned len = lengths[i];
        if (len != 0)
            bl_count[len]++;
    }

    unsigned code = 0;
    bl_count[0] = 0;
    for (unsigned bits = 1; bits <= MAX_BITS; bits++) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }

    for (unsigned i = 0; i < n; i++) {
        unsigned len = lengths[i];
        if (len != 0) {
            codes[i] = next_code[len];
            next_code[len]++;
        } else {
            codes[i] = 0;
        }
    }
}

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

struct match* LZ77(unsigned char *buf, size_t bytes_read, size_t *sizeMatches) {
    struct match* matches = malloc(bytes_read * sizeof(struct match));
    if (!matches) return NULL;
    
    unsigned len, dist;
    size_t cur = 0;

    for (size_t i = 0; i < bytes_read;) {
        if (find_best_match(buf, i, bytes_read, &len, &dist)) {
            matches[cur].type = MATCH;
            matches[cur].length = len;
            matches[cur].offset = dist;
            i += len;
        } else {
            matches[cur].type = LITERAL;
            matches[cur].literal = buf[i];
            i++;
        }
        cur++;
    }
    *sizeMatches = cur;
    return matches;
}

void find_smallest_pair(struct tree **arr, unsigned *indSmallest, unsigned *indSmall, size_t size) {
    struct tree *smallest, *small;

    if (arr[0]->data > arr[1]->data) {
        smallest = arr[1];
        *indSmallest = 1;
        small = arr[0];
        *indSmall = 0;
    } else {
        small = arr[1];
        *indSmall = 1;
        smallest = arr[0];
        *indSmallest = 0;
    }
    
    for (size_t i = 2; i < size; i++) {
        if (arr[i]->data < smallest->data) {
            small = smallest;
            *indSmall = *indSmallest;
            smallest = arr[i];
            *indSmallest = i;
        } else if (arr[i]->data < small->data) {
            small = arr[i];
            *indSmall = i;
        }
    }
}

struct tree* merge(struct tree* smallest, struct tree* small) {
    struct tree *newNode = malloc(sizeof(struct tree));
    if (!newNode) return NULL;
    
    newNode->data = small->data + smallest->data;
    newNode->symbol = 0xFFFF;  // внутренний узел не имеет символа
    newNode->zeroptr = smallest;
    newNode->oneptr = small;
    return newNode;
}

void refreshArr(struct tree*** arr, unsigned indSmallest, unsigned indSmall, struct tree *newNode, size_t *size) {
    struct tree** newArr = calloc(*size - 1, sizeof(struct tree*));
    if (!newArr) return;
    
    size_t j = 0;
    for (size_t i = 0; i < *size; i++) {
        if (i != indSmallest && i != indSmall) {
            newArr[j++] = (*arr)[i];
        }
    }
    newArr[j] = newNode;
    free(*arr);
    *arr = newArr;
}

// ИСПРАВЛЕНО: Инициализация symbol в дереве
struct tree* build_tree(unsigned *frequencies, size_t size) {
    struct tree **arr = calloc(size, sizeof(struct tree*));
    if (!arr) return NULL;
    
    unsigned indSmallest, indSmall;

    for (size_t i = 0; i < size; i++) {
        arr[i] = malloc(sizeof(struct tree));
        if (!arr[i]) {
            // Освобождаем уже выделенную память при ошибке
            for (size_t j = 0; j < i; j++) free(arr[j]);
            free(arr);
            return NULL;
        }
        arr[i]->data = frequencies[i];
        arr[i]->symbol = i;  // устанавливаем символ
        arr[i]->zeroptr = NULL;
        arr[i]->oneptr = NULL;
    }

    size_t current_size = size;
    while (current_size > 1) {
        find_smallest_pair(arr, &indSmallest, &indSmall, current_size);
        struct tree *newNode = merge(arr[indSmallest], arr[indSmall]);
        if (!newNode) {
            // Обработка ошибки выделения памяти
            for (size_t i = 0; i < current_size; i++) free(arr[i]);
            free(arr);
            return NULL;
        }
        refreshArr(&arr, indSmallest, indSmall, newNode, &current_size);
        current_size--;
    }

    struct tree *root = arr[0];
    free(arr);
    return root;
}

// ИСПРАВЛЕНО: Поиск по symbol вместо data
bool search_tree(struct tree* node, unsigned target, uint16_t *encoded, unsigned *size) {
    if (!node) return false;
    
    // Если это лист, проверяем символ
    if (node->zeroptr == NULL && node->oneptr == NULL) {
        return node->symbol == target;
    }

    if (node->zeroptr) {
        (*size)++;
        if (search_tree(node->zeroptr, target, encoded, size))
            return true;
        (*size)--;
    }

    if (node->oneptr) {
        *encoded |= (1u << *size);
        (*size)++;
        if (search_tree(node->oneptr, target, encoded, size))
            return true;
        *encoded &= ~(1u << *size);
        (*size)--;
    }

    return false;
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

struct rangedData* to_range(const struct match *matches, size_t size, size_t *outSize) {
    struct rangedData* arr = malloc(size * 2 * sizeof(struct rangedData));
    if (!arr) return NULL;
    
    size_t k = 0;

    for (size_t i = 0; i < size; i++) {
        if (matches[i].type == LITERAL) {
            arr[k].isLL = true;
            arr[k].haffCode = matches[i].literal;
            arr[k].extraVal = 0;
            arr[k].haffLen = 8;
            arr[k].extraLen = 0;
            k++;
        } else {
            uint16_t lcode, lbase;
            uint8_t lextra;
            length_to_code(matches[i].length, &lcode, &lbase, &lextra);
            arr[k].isLL = true;
            arr[k].haffCode = lcode;
            arr[k].extraVal = matches[i].length - lbase;
            arr[k].extraLen = lextra;
            k++;

            uint16_t dcode, dbase;
            uint8_t dextra;
            dist_to_code(matches[i].offset, &dcode, &dbase, &dextra);
            arr[k].isLL = false;
            arr[k].haffCode = dcode;
            arr[k].extraVal = matches[i].offset - dbase;
            arr[k].extraLen = dextra;
            k++;
        }
    }

    *outSize = k;
    return arr;
}

void count_frequencies(struct rangedData* data, unsigned* LLfreq, unsigned* Ofreq, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (data[i].isLL)
            LLfreq[data[i].haffCode]++;
        else
            Ofreq[data[i].haffCode]++;
    }
}

void count_frequencies_for_lengths(struct shortedLength *data, unsigned* freq, size_t size) {
    for (size_t i = 0; i < size; i++) {
        freq[data[i].data]++;
    }
}

void flushBuf(uint8_t *buff, struct bitWriter * writer) {
    if (writer->buffPos < FILE_SIZE) {
        buff[writer->buffPos++] = writer->buff;
    }
    writer->buff = 0;
}

// ИСПРАВЛЕНО: Правильный порядок записи битов
void write_bits(struct bitWriter *writer, unsigned value, size_t size, uint8_t *buff, uint8_t extraVal, uint8_t extraLen) {
    // Записываем биты от старшего к младшему
    for (int i = size - 1; i >= 0; i--) {
        uint8_t bit = (value >> i) & 1;
        writer->buff = (writer->buff << 1) | bit;
        writer->pos++;
        
        if (writer->pos == 8) {
            flushBuf(buff, writer);
            writer->pos = 0;
        }
    }
    
    // Дополнительные биты
    if (extraLen > 0) {
        for (int i = extraLen - 1; i >= 0; i--) {
            uint8_t bit = (extraVal >> i) & 1;
            writer->buff = (writer->buff << 1) | bit;
            writer->pos++;
            
            if (writer->pos == 8) {
                flushBuf(buff, writer);
                writer->pos = 0;
            }
        }
    }
}

void encode_range_data(struct rangedData* data, size_t size, uint8_t *buffer, struct tree *headLL, struct tree *headO, struct bitWriter *writer) {
    uint16_t encoded;
    unsigned short sizeEncoded;

    for (size_t i = 0; i < size; i++) {
        encoded = 0;
        sizeEncoded = 0;
        if (data[i].isLL && search_tree(headLL, data[i].haffCode, &encoded, &sizeEncoded)) {
            write_bits(writer, encoded, sizeEncoded, buffer, data[i].extraVal, data[i].extraLen);
        } else if (search_tree(headO, data[i].haffCode, &encoded, &sizeEncoded)) {
            write_bits(writer, encoded, sizeEncoded, buffer, data[i].extraVal, data[i].extraLen);
        }
    }
}

void encode_lenghts(uint16_t* data, size_t size, uint8_t *buffer, struct bitWriter *writer) {
    for (size_t i = 0; i < size; i++) {
        write_bits(writer, data[i], data[i] / 2 + data[i] % 2, buffer, 0, 0);
    }
}

void tree_bypass_lengths(struct tree* head, unsigned *frequencies, unsigned *len, unsigned *maxlen, struct shortedLength *lenghts, unsigned *pos) {
    if (head->zeroptr == NULL && head->oneptr == NULL) {
        frequencies[*len]++;
        if (*len > *maxlen) *maxlen = *len;
        lenghts[(*pos)++].data = *len;
        return;
    }
    if (head->zeroptr) {
        (*len)++;
        tree_bypass_lengths(head->zeroptr, frequencies, len, maxlen, lenghts, pos);
    }
    if (head->oneptr) {
        (*len)++;
        tree_bypass_lengths(head->oneptr, frequencies, len, maxlen, lenghts, pos);
    }
    (*len)--;
}

void tree_bypass(struct tree* head, unsigned *frequencies, unsigned *len, unsigned *maxlen, unsigned *lengths, unsigned *pos) {
    if (head->zeroptr == NULL && head->oneptr == NULL) {
        frequencies[*len]++;
        if (*len > *maxlen) *maxlen = *len;
        lengths[(*pos)++] = *len;
        return;
    }
    if (head->zeroptr) {
        (*len)++;
        tree_bypass(head->zeroptr, frequencies, len, maxlen, lengths, pos);
    }
    if (head->oneptr) {
        (*len)++;
        tree_bypass(head->oneptr, frequencies, len, maxlen, lengths, pos);
    }
    (*len)--;
}

void count_code_length_for_lengths(unsigned *frequencies, struct tree *headLL, struct tree *headO, unsigned *maxlen, struct shortedLength *lengths, unsigned *pos) {
    unsigned len = 0;
    tree_bypass(headLL, frequencies, &len, maxlen, lengths, pos);
    tree_bypass(headO, frequencies, &len, maxlen, lengths, pos);
}

// ИСПРАВЛЕНО: Правильное рекурсивное удаление дерева
void delete_tree(struct tree* head) {
    if (!head) return;
    
    if (head->zeroptr) {
        delete_tree(head->zeroptr);
    }
    if (head->oneptr) {
        delete_tree(head->oneptr);
    }
    
    free(head);
}

void count_code_length(unsigned *frequencies, struct tree *headLL, struct tree *headO, unsigned *maxlen, unsigned *lengths, unsigned *pos) {
    unsigned len = 0;
    tree_bypass(headLL, frequencies, &len, maxlen, lengths, pos);
    tree_bypass(headO, frequencies, &len, maxlen, lengths, pos);
}

void repeats_compression(unsigned *lengths, struct shortedLength *shorted, size_t size) {
    unsigned dup = lengths[0];
    size_t counter = 1, k = 0;

    for (size_t i = 1; i <= size; i++) {
        if (i < size && lengths[i] == dup) {
            counter++;
            continue;
        }

        if (dup != 0) {
            while (counter >= 3) {
                size_t repeat = (counter > 6) ? 6 : counter;
                shorted[k].data = 16;
                shorted[k++].extra_bits = repeat - 3;
                counter -= repeat;
            }
            while (counter--) shorted[k++].data = dup;
        } else {
            while (counter >= 11) {
                size_t repeat = (counter > 138) ? 138 : counter;
                shorted[k].data = 18;
                shorted[k++].extra_bits = repeat - 11;
                counter -= repeat;
            }
            while (counter >= 3) {
                size_t repeat = (counter > 10) ? 10 : counter;
                shorted[k].data = 17;
                shorted[k++].extra_bits = repeat - 3;
                counter -= repeat;
            }
            while (counter--) shorted[k++].data = 0;
        }

        if (i < size) {
            dup = lengths[i];
            counter = 1;
        }
    }
}

void write_header(struct bitWriter *writer, uint8_t *buff, bool end) {
    unsigned hclen = HCLEN;
    unsigned hlit = HLIT;
    unsigned hdist = HDIST;
    unsigned coding = 0b10;
    write_bits(writer, (int)end, 1, buff, 0, 0);
    write_bits(writer, coding, 2, buff, 0, 0);
    write_bits(writer, hlit, 5, buff, 0, 0);     // HLIT: 5 битов
    write_bits(writer, hdist, 5, buff, 0, 0);    // HDIST: 5 битов  
    write_bits(writer, hclen, 4, buff, 0, 0);    // HCLEN: 4 бита
}

int main() {
    DIR* dir;
    struct dirent* entry;
    char filename[256];
    FILE* file, *ofile;
    int c;
    size_t i, sizeMatches = 0, sizeData = 0;
    unsigned char* buff = malloc(1024*1024);
    if (!buff) {
        printf("Memory allocation failed for buff\n");
        return 1;
    }
    
    struct match *matches;
    unsigned *LLfrequencies, *Ofrequencies, *treesFreq;
    unsigned *codeLengths;
    unsigned maxlen = 0, pos = 0, len = 0, maxlen2 = 0, pos2 = 0;
    char path[512];
    struct rangedData* rangedData;
    struct tree *headLL, *headO, *headTrees;
    uint8_t *buffer = NULL;
    struct shortedLength *shortedLengths;
    uint16_t *codes;
    bool end;

    unsigned codeLenFreq[MAX_BITS+1] = {0};
    unsigned lengthsOfLengths[19] = {0};

    printf("Enter directory name: ");
    scanf("%255s", filename);

    dir = opendir(filename);
    if (!dir) {
        printf("Cannot open directory %s\n", filename);
        free(buff);
        return 1;
    }

    struct bitWriter writer = {0, 0, 0};

    while ((entry = readdir(dir)) != NULL) {
        printf("open file: %s\n", entry->d_name);
        
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        
        snprintf(path, sizeof(path), "%s/%s", filename, entry->d_name);
        file = fopen(path, "rb");
        if (!file) {
            printf("Cannot open file: %s\n", path);
            continue;
        }
        
        // ИСПРАВЛЕНО: Правильное чтение файла
        size_t bytes_read = fread(buff, 1, 1024*1024, file);
        fclose(file);
        
        if (bytes_read == 0) {
            printf("Empty file: %s\n", path);
            continue;
        }
        
        printf("file read: %zu bytes\n", bytes_read);

        // Выделяем буфер для выходных данных
        buffer = malloc(FILE_SIZE);
        if (!buffer) {
            printf("Memory allocation failed for buffer\n");
            continue;
        }

        matches = LZ77(buff, bytes_read, &sizeMatches);
        if (!matches) {
            printf("LZ77 failed\n");
            free(buffer);
            continue;
        }
        
        printf("LZ77 worked: %zu matches\n", sizeMatches);

        rangedData = to_range(matches, sizeMatches, &sizeData);
        if (!rangedData) {
            printf("to_range failed\n");
            free(matches);
            free(buffer);
            continue;
        }

        LLfrequencies = calloc(288, sizeof(unsigned));
        Ofrequencies = calloc(30, sizeof(unsigned));
        if (!LLfrequencies || !Ofrequencies) {
            printf("Memory allocation failed for frequencies\n");
            goto cleanup;
        }
        
        count_frequencies(rangedData, LLfrequencies, Ofrequencies, sizeData);
        printf("frequencies counted\n");

        headLL = build_tree(LLfrequencies, 288);
        headO = build_tree(Ofrequencies, 30);
        if (!headLL || !headO) {
            printf("Tree building failed\n");
            goto cleanup;
        }
        printf("trees built\n");

        codeLengths = calloc(318, sizeof(unsigned));
        if (!codeLengths) {
            printf("Memory allocation failed for codeLengths\n");
            goto cleanup;
        }

        maxlen = 0; pos = 0;
        count_code_length(codeLenFreq, headLL, headO, &maxlen, codeLengths, &pos);

        shortedLengths = calloc(318, sizeof(struct shortedLength));
        if (!shortedLengths) {
            printf("Memory allocation failed for shortedLengths\n");
            goto cleanup;
        }
        
        repeats_compression(codeLengths, shortedLengths, 318);
        printf("repeats compressed\n");

        treesFreq = calloc(19, sizeof(unsigned));
        if (!treesFreq) {
            printf("Memory allocation failed for treesFreq\n");
            goto cleanup;
        }
        
        count_frequencies_for_lengths(shortedLengths, treesFreq, 318);
        headTrees = build_tree(treesFreq, 19);
        if (!headTrees) {
            printf("Tree building failed for headTrees\n");
            goto cleanup;
        }
        printf("length tree built\n");

        len = 0; maxlen2 = 0; pos2 = 0;
        tree_bypass(headTrees, treesFreq, &len, &maxlen2, lengthsOfLengths, &pos2);
        printf("tree bypassed\n");

        codes = calloc(19, sizeof(uint16_t));
        
        printf("making canonical codes\n");
        makeCanonicalCodes(lengthsOfLengths, 19, codes);
        printf("Canonical codes built\n");

        write_header(&writer, buffer, true);
        encode_lenghts(codes, 19, buffer, &writer);
        encode_range_data(rangedData, sizeData, buffer, headLL, headO, &writer);
        printf("data written\n");

        // Создаем выходной файл
        char out_path[512];
        snprintf(out_path, sizeof(out_path), "%s.deflate", entry->d_name);
        ofile = fopen(out_path, "wb");
        if (ofile) {
            fwrite(buffer, 1, writer.buffPos, ofile);
            fclose(ofile);
            printf("Output file created: %s\n", out_path);
        } else {
            printf("Cannot create output file: %s\n", out_path);
        }

    cleanup:
        // Освобождаем всю выделенную память
        if (headTrees) delete_tree(headTrees);
        if (headLL) delete_tree(headLL);
        if (headO) delete_tree(headO);
        free(LLfrequencies);
        free(Ofrequencies);
        free(matches);
        free(rangedData);
        free(codeLengths);
        free(treesFreq);
        free(shortedLengths);
        free(codes);
        free(buffer);
        buffer = NULL;
        
        memset(codeLenFreq, 0, sizeof(codeLenFreq));
        memset(lengthsOfLengths, 0, sizeof(lengthsOfLengths));
        
        len = 0; maxlen2 = 0; pos2 = 0;
        maxlen = 0; pos = 0;
    }

    closedir(dir);
    free(buff);
    printf("Compression completed\n");
    return 0;
}