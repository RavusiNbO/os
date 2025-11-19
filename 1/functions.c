#include "archiver.h"


void makeCanonicalCodes(
    unsigned *lengths,  
    unsigned n,               
    uint16_t *codes          
) {
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


void find_smallest_pair(struct tree **arr, unsigned *indSmallest, unsigned *indSmall, size_t size)
{
    struct tree *smallest, *small;

    if (arr[0]->data > arr[1]->data)
    {
        smallest = arr[1];
        *indSmallest = 1;
        small = arr[0];
        *indSmall = 0;

    }
    else{
        small = arr[1];
        *indSmall = 1;
        smallest = arr[0];
        *indSmallest = 0;
    }
    

    for (size_t i = 2 ; i < size; i++)
    {
        if (arr[i]->data < smallest->data)
        {
            small = smallest;
            *indSmall = *indSmallest;
            smallest = arr[i];
            *indSmallest = i;
        }
        else if (arr[i]->data < small->data)
        {
            small = arr[i];
            *indSmall = i;
        }
    }

}

struct tree* merge(struct tree* smallest, struct tree* small) {
    struct tree *newNode = malloc(sizeof(struct tree));
    if (!newNode) return NULL;
    
    newNode->data = small->data + smallest->data;
    newNode->symbol = 0xFFFF;  
    newNode->zeroptr = smallest;
    newNode->oneptr = small;
    return newNode;
}

void refreshArr(struct tree*** arr, unsigned indSmallest, unsigned indSmall, struct tree *newNode, size_t *size) {
    struct tree** newArr = calloc(*size, sizeof(struct tree*));
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

struct tree* build_tree(unsigned *frequencies, size_t size) {
    unsigned indSmallest, indSmall;
    size_t nonzero = 0;

    for (size_t i = 0; i < size; i++)
        if (frequencies[i]) nonzero++;

    if (nonzero == 0) {
        struct tree *t = malloc(sizeof(struct tree));
        t->data = 1;
        t->symbol = 0;
        t->zeroptr = t->oneptr = NULL;
        return t;
    }

    struct tree **arr = calloc(nonzero, sizeof(*arr));
    size_t idx = 0;
    for (size_t i = 0; i < size; i++) {
        if (!frequencies[i]) continue;
        arr[idx] = malloc(sizeof(struct tree));
        arr[idx]->data = frequencies[i];
        arr[idx]->symbol = i;
        arr[idx]->zeroptr = arr[idx]->oneptr = NULL;
        idx++;
    }

    size_t current_size = nonzero; 

    while (current_size > 1) {
        find_smallest_pair(arr, &indSmallest, &indSmall, current_size);
        struct tree *newNode = merge(arr[indSmallest], arr[indSmall]);
        if (!newNode) {
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


bool find_code_in_tree(struct tree* node, uint16_t target, uint16_t code, unsigned depth, uint16_t *out_code, unsigned *out_len) {
    if (!node) return false;
    if (node->zeroptr == NULL && node->oneptr == NULL) {
        if (node->symbol == target) {
            *out_code = code;
            *out_len = depth;
            return true;
        }
        return false;
    }
    if (node->zeroptr) {
        if (find_code_in_tree(node->zeroptr, target, code, depth + 1, out_code, out_len)) return true;
    }
    if (node->oneptr) {
        uint16_t code_with_one = code | (1u << depth);
        if (find_code_in_tree(node->oneptr, target, code_with_one, depth + 1, out_code, out_len)) return true;
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

struct rangedData* to_range(const struct match *matches, size_t size, size_t *outSize)
{
    struct rangedData* arr = malloc(size * 2 * sizeof(struct rangedData));
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



void count_frequencies(struct rangedData* data, unsigned* LLfreq, unsigned* Ofreq, size_t size) 
{
    for (size_t i = 0; i < size; i++) {
        if (data[i].isLL)
            LLfreq[data[i].haffCode]++;
        else
            Ofreq[data[i].haffCode]++;
    }
}

void count_frequencies_for_lengths(struct shortedLength *data, unsigned* freq, size_t size) 
{
    for (size_t i = 0; i < size; i++) {
        freq[data[i].data]++;
    }
}

void flushBuf(uint8_t *buff, struct bitWriter * writer)
{
    buff[writer->buffPos++] = writer->buff;
    writer->buff = 0;
    writer->pos = 0;
}

void write_bits(struct bitWriter *writer, unsigned value, size_t nbits, uint8_t *buff, uint8_t extraVal, uint8_t extraLen)
{
    for (size_t i = 0; i < nbits; ++i) {
        uint8_t bit = (value >> i) & 1u;
        writer->buff |= (bit << writer->pos);
        writer->pos++;
        if (writer->pos == 8) flushBuf(buff, writer);
    }
    for (size_t i = 0; i < extraLen; ++i) {
        uint8_t bit = (extraVal >> i) & 1u;
        writer->buff |= (bit << writer->pos);
        writer->pos++;
        if (writer->pos == 8) flushBuf(buff, writer);
    }
}


void encode_range_data(struct rangedData* data, size_t size, uint8_t *buffer, struct tree *headLL, struct tree *headO, struct bitWriter *writer)
{
    uint16_t encoded;
    unsigned short sizeEncoded;

    for (size_t i = 0 ; i < size; i++)
    {
        uint16_t code16; unsigned codeLen16;
        if (data[i].isLL) {
            if (find_code_in_tree(headLL, data[i].haffCode, 0, 0, &code16, &codeLen16)) {
                write_bits(writer, code16, codeLen16, buffer, data[i].extraVal, data[i].extraLen);
            } 
        } else {
            if (find_code_in_tree(headO, data[i].haffCode, 0, 0, &code16, &codeLen16)) {
                write_bits(writer, code16, codeLen16, buffer, data[i].extraVal, data[i].extraLen);
            } 
        }          

    }
    write_bits(writer, 256, 9, buffer, 0, 0);
}


void encode_lengths(struct shortedLength *shortedLengths, size_t count, uint16_t *tree_codes, unsigned *tree_code_lens, uint8_t *buffer, struct bitWriter *writer) {
    write_bits(writer, count, sizeof(count), buffer, 0, 0);
    for (size_t i = 0; i < count; ++i) {
        uint8_t sym = shortedLengths[i].data;
        uint16_t code = tree_codes[sym];
        uint8_t clen = tree_code_lens[sym];
        write_bits(writer, code, clen, buffer, 0, 0);
        if (sym == 16) write_bits(writer, shortedLengths[i].extra_bits, 2, buffer, 0, 0);
        else if (sym == 17) write_bits(writer, shortedLengths[i].extra_bits, 3, buffer, 0, 0);
        else if (sym == 18) write_bits(writer, shortedLengths[i].extra_bits, 7, buffer, 0, 0);
    }
}




// Считаю длины кодов LL и O деревьев
void tree_bypass(struct tree* head, unsigned *frequencies, unsigned *len, unsigned *maxlen, unsigned *lengths, unsigned *pos)
{
    if (head->zeroptr == NULL && head->oneptr == NULL)
    {
        frequencies[*len]++;
        if (*len > *maxlen) *maxlen = *len;
        if (head->zeroptr || head->oneptr) return; // не записывать для внутренних узлов
        lengths[head->symbol] = *len;
        return;
    }
    if (head->zeroptr)
    {
        (*len)++;
        tree_bypass(head->zeroptr, frequencies, len, maxlen, lengths, pos);
    }
    if (head->oneptr)
    {
        (*len)++;
        tree_bypass(head->oneptr, frequencies, len, maxlen, lengths, pos);
    }
    (*len)--;
}


// нахожу длины каждого кода
void getLenghtsCodeLengths(unsigned *lengths, struct tree* head, size_t length)
{
    if (!head) return;

    if (!head->zeroptr && !head->oneptr) {
        if (head->symbol < 19)
            lengths[head->symbol] = length;
        return;
    }

    if (head->zeroptr)
        getLenghtsCodeLengths(lengths, head->zeroptr, length + 1);
    if (head->oneptr)
        getLenghtsCodeLengths(lengths, head->oneptr, length + 1);
}


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

void count_code_length(unsigned *frequencies, struct tree *headLL, struct tree *headO, unsigned *maxlen, unsigned *lengths, unsigned *pos)
{
    unsigned ll_lengths[288] = {0};
    unsigned o_lengths[30] = {0};

    getLenghtsCodeLengths(ll_lengths, headLL, 0);
    getLenghtsCodeLengths(o_lengths, headO, 0);

    memcpy(lengths, ll_lengths, 288 * sizeof(unsigned));
    memcpy(lengths + 288, o_lengths, 30 * sizeof(unsigned));

}

void repeats_compression(unsigned *lengths, struct shortedLength *shorted, size_t size, size_t *shorted_count)
{
    unsigned dup = lengths[0];
    size_t counter = 1, k = 0;
    size_t repeat;

    for (size_t i = 1; i <= size; i++) {
        if (i < size && lengths[i] == dup) {
            counter++;
            continue;
        }

        if (dup != 0) {
            while (counter >= 3) {
                repeat = (counter > 6) ? 6 : counter;
                shorted[k].data = 16;
                shorted[k++].extra_bits = repeat - 3;
                counter -= repeat;
            }
            while (counter--) shorted[k++].data = dup;
        } else {
            while (counter >= 11) {
                repeat = (counter > 138) ? 138 : counter;
                shorted[k].data = 18;
                shorted[k++].extra_bits = repeat - 11;
                counter -= repeat;
            }
            while (counter >= 3) {
                repeat = (counter > 10) ? 10 : counter;
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
    (*shorted_count) = k;
}

void write_header(struct bitWriter *writer, uint8_t *buff, bool end)
{
    unsigned coding = 0b10;
    write_bits(writer, (int)end, 1, buff, 0, 0);
    write_bits(writer, coding, 2, buff, 0, 0);
    write_bits(writer, HLIT, 5, buff, 0, 0);
    write_bits(writer, HDIST, 5, buff, 0, 0);
    write_bits(writer, HCLEN, 4, buff, 0, 0);
}

void write_lengths_of_lengths(uint16_t *codes, unsigned *lengths, struct bitWriter *writer, uint8_t *buffer)
{
    const int CLEN_ORDER[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    unsigned to_write = (HCLEN + 4); 
    for (unsigned i = 0; i < to_write; ++i) {
        uint8_t val = lengths[CLEN_ORDER[i]]; // 0..7
        write_bits(writer, val, 3, buffer, 0, 0);
    }
}

void write_filename(struct bitWriter *writer, char *name, uint8_t *buffer, size_t len)
{
    memcpy((void*)(buffer + writer->buffPos), (void*)name, len);
    writer->buffPos += len;
    write_bits(writer, 256, 9, buffer, 0, 0);
}


void update_file_tree(struct fileTree *head, char *path, char *parent, uint8_t isDir)
{
    if (strcmp(head->path, parent) == 0)
    {
        struct fileTree *child = malloc(sizeof(struct fileTree));
        child->isDir = isDir;
        child->childsCount = 0;
        child->childs = calloc(20, sizeof(struct fileTree*));
        strcpy(child->path, path);
        head->childs[head->childsCount++] = child;
        return;
    }

    for (size_t i = 0; i < head->childsCount; i++)
    {
        update_file_tree(head->childs[i], path, parent, isDir);
    }
}


// размер длины пути файла - байт
void write_file_tree(struct fileTree *head, FILE *ofile)
{
    size_t pathlen = strlen(head->path);
    fputc(pathlen, ofile);
    fwrite(head->path, 1, pathlen, ofile);
    fputc(head->isDir, ofile);
    fputc(head->childsCount, ofile);
    for (size_t i = 0; i < head->childsCount; i++)
    {
        write_file_tree(head->childs[i], ofile);
    }
}   

void compress_directory(char *filename)
{
    DIR* dir;
    struct dirent* entry;
    FILE* file, *ofile;
    int c;
    size_t i, sizeMatches = 0, sizeData = 0, sizeFile = 0, 
    blocksNumber = 0, blockCounter = 0, currentBlockSize = 0, 
    lastBlockSize = 0, shorted_count, start, to_copy, names_pointer = 0;
    unsigned char* buff = malloc(1024*1024), *archiv = malloc(1024*1024*10), *name = malloc(__DARWIN_MAXPATHLEN);
    struct match *matches;
    unsigned *LLfrequencies, *Ofrequencies, *treesFrequencies, *lengthsFrequencies, 
    *lengths, *codeLengths, maxlen = 0, pos = 0, len = 0,lenLen = 0, lengthsPos = 0,
    *treesFreq, maxlen2 = 0, pos2 = 0;
    unsigned char path[__DARWIN_MAXPATHLEN], parent[__DARWIN_MAXPATHLEN], tempParent[__DARWIN_MAXPATHLEN];
    struct rangedData* rangedData, *rangedBlock;
    struct tree *headLL, *headO, *headTrees;
    uint8_t *buffer, *trees;
    struct shortedLength *shortedLengths;
    uint16_t *codes, *tree_codes;
    bool end;

    strcpy(parent, filename);
    struct fileTree *root = malloc(sizeof(struct fileTree));
    strcpy(root->path, filename);
    root->childs = calloc(20, sizeof(struct fileTree));
    root->childsCount = 0;
    unsigned codeLenFreq[MAX_BITS+1] = {0};
    unsigned lengthsOfLengths[19] = {0};


    dir = opendir(filename);
    struct bitWriter writer = {0, 0, 0};

    


    while ((entry = readdir(dir)) != NULL)
    {

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        snprintf(path, sizeof(path), "%s/%s", filename, entry->d_name);
        if (entry->d_type == DT_DIR) 
        {
            update_file_tree(root, path, parent, true);
            strcpy(tempParent, parent);
            strcpy(parent, path);
            compress_directory(path);
            strcpy(parent, tempParent);
            continue;
        }
        update_file_tree(root, path, parent, false);
        file = fopen(path, "rb");
        if (!file) continue;

        i = 0;
        end = false;
        blockCounter = 0;

        printf("reading file %s\n", path);
        while ((c = getc(file)) != EOF)
        {
            buff[i++] = c;
        }
        sizeFile = i;
        fclose(file);

        

        matches = LZ77(buff, sizeFile, &sizeMatches);
        rangedData = to_range(matches, sizeMatches, &sizeData);
        printf("LZ77 worked\n");
        sizeData += strlen(name) + 2; 

        blocksNumber = sizeData / BLOCK_SIZE + (sizeData % BLOCK_SIZE > 0 ? 1 : 0);
        lastBlockSize = sizeData % BLOCK_SIZE;
        if (lastBlockSize == 0) lastBlockSize = BLOCK_SIZE;

        
        while(!end)
        {
            currentBlockSize = BLOCK_SIZE;
            printf("reading block №: %d of %s\n", ++blockCounter, path);
            if (blockCounter == blocksNumber) 
            {
                end = true;
                currentBlockSize = lastBlockSize;
            }
            start = (blockCounter - 1) * BLOCK_SIZE;
            to_copy = (start + BLOCK_SIZE <= sizeData) ? BLOCK_SIZE : (sizeData - start);
            currentBlockSize = to_copy;
            rangedBlock = malloc(currentBlockSize * sizeof(struct rangedData));
            for (size_t j = 0; j < to_copy; ++j) rangedBlock[j] = rangedData[start + j];
            

            buffer = malloc(currentBlockSize * 200);
            printf("block readed, size = %d\n", currentBlockSize);

            printf("counting frequencies\n");
            LLfrequencies = calloc(288, sizeof(unsigned));
            Ofrequencies  = calloc(30, sizeof(unsigned));
            count_frequencies(rangedBlock, LLfrequencies, Ofrequencies, currentBlockSize);

            
            printf("building trees\n");
            headLL = build_tree(LLfrequencies, 288);
            headO  = build_tree(Ofrequencies, 30);

            

            codeLengths = calloc(318, sizeof(unsigned));


            maxlen = 0, pos = 0;
            count_code_length(codeLenFreq, headLL, headO, &maxlen, codeLengths, &pos);

            printf("repeats compression\n"); 
            shortedLengths = calloc(318, sizeof(struct shortedLength));
            repeats_compression(codeLengths, shortedLengths, 318, &shorted_count);
            
            codes = calloc(318 , sizeof(uint16_t));



            printf("making canonical codes\n");
            makeCanonicalCodes(codeLengths, 318, codes);

            
            printf("building length tree\n");
            treesFreq = calloc(19, sizeof(unsigned));
            count_frequencies_for_lengths(shortedLengths, treesFreq, shorted_count);
            headTrees = build_tree(treesFreq, 19);

            

            memset(lengthsOfLengths, 0, sizeof(lengthsOfLengths));
            getLenghtsCodeLengths(lengthsOfLengths, headTrees, 0);



            tree_codes = calloc(19 , sizeof(uint16_t));
            printf("making canonical trees codes\n");
            makeCanonicalCodes(lengthsOfLengths, 19, tree_codes);
            

            printf("writing data\n");
            write_header(&writer, buffer, end);
            write_lengths_of_lengths(tree_codes, lengthsOfLengths, &writer, buffer); // мб надо писать напрямую без кодирования
            encode_lengths(shortedLengths, shorted_count, tree_codes, lengthsOfLengths, buffer, &writer);
            encode_range_data(rangedBlock, currentBlockSize, buffer, headLL, headO, &writer);
            if (end)
            {
                write_bits(&writer, 256, 8, buffer, 0, 0);
            }
            flushBuf(buffer, &writer);
            
            
            printf("cleaning memory\n");
            delete_tree(headTrees);
            delete_tree(headLL);
            delete_tree(headO);
            free(LLfrequencies);
            free(Ofrequencies);
            free(codeLengths);
            free(rangedBlock);
            memset(codeLenFreq, 0, sizeof(codeLenFreq));
            free(treesFreq);
            free(shortedLengths);
            memset(lengthsOfLengths, 0, sizeof(lengthsOfLengths));
            free(codes);
            

            len=0; 
            maxlen2=0;
            pos2=0;
            maxlen=0;
            pos=0;
            printf("--------------------\n");
            fflush(stdout);
        }

        free(matches);
        free(rangedData);


        

        for (size_t k = 0; k < writer.buffPos; ++k) {
            archiv[BLOCK_SIZE * (blockCounter - 1) + k] = buffer[k];
        }
        
        c = 0;
        
        writer.buffPos = 0;
        writer.pos = 0;
        writer.buff = 0;
        free(buffer);
        printf("====================\n");
    }


    snprintf(path, sizeof(path), "%s_archived", filename);
    ofile = fopen(path, "wb");
    write_file_tree(root, ofile);
    for (size_t k = 0; k < BLOCK_SIZE * (blockCounter - 1) + lastBlockSize; ++k) {
        fputc(archiv[k], ofile);
    }

    fclose(ofile);
    
}
