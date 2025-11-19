#include "archiver.h"

void br_init(struct bitReader *r, const unsigned char *data)
{
    r->buffPos = 0;
    r->pos = 0;
    r->buff = data[r->buffPos++];
}


unsigned br_read_bits(struct bitReader *r, const unsigned char *data, size_t nbits)
{
    unsigned val = 0;
    for (size_t i = 0; i < nbits; ++i) {
        unsigned bit = (r->buff >> r->pos) & 1u; 
        val |= (bit << i);                     
        r->pos++;
        if (r->pos == 8) {
            r->pos = 0;
            r->buff = data[r->buffPos++];
        }
    }
    return val;
}

struct fileTree *read_file_tree(struct fileTree *head, unsigned char *archiv, struct bitReader *reader)
{
    head = malloc(sizeof(struct fileTree));
    head->childs = calloc(20, sizeof(struct fileTree*));
    size_t pathlen;
    pathlen = archiv[reader->buffPos++];
    memcpy(head->path, archiv + reader->buffPos, pathlen);
    reader->buffPos += pathlen;
    head->isDir = archiv[reader->buffPos++];
    head->childsCount = archiv[reader->buffPos++];
    for (size_t i = 0; i < head->childsCount; i++)
    {
        head->childs[i] = read_file_tree(head->childs[i], archiv, reader);
    }
    return head;
}

void decode_trees(unsigned char *archiv, struct bitReader *reader, unsigned *codeLengths, uint16_t *treeCodes)
{
    uint16_t buffer = 0;
    size_t c;
    bool br = false;
    uint8_t extraValue = 0;
    unsigned prevValue;

    for (size_t i = 0; i < 318; i++)
    {
        c = 0;
        while(!br)
        {
            buffer |= br_read_bits(reader, archiv, 1) << c++;
            for (size_t j = 0; j < 19; j++)
            {
                if (buffer == treeCodes[j])
                {
                    codeLengths[i] = j;
                    prevValue = buffer;
                    br = true;
                    c = 0;
                    if (buffer == 17)
                    {
                        extraValue = br_read_bits(reader, archiv, 3);
                        for ( ; i < i + extraValue; i++)
                        {
                            codeLengths[i] = 0;
                        }
                        prevValue = 0;
                        break;
                    }
                    if (buffer == 18)
                    {
                        extraValue = br_read_bits(reader, archiv, 7);
                        for ( ; i < i + extraValue; i++)
                        {
                            codeLengths[i] = 0;
                        }
                        prevValue = 0;
                        break;
                    }
                    if (buffer == 16)
                    {
                        extraValue = br_read_bits(reader, archiv, 2);
                        for ( ; i < i + extraValue + 3; i++)
                        {
                            codeLengths[i] = prevValue;
                        }
                        break;
                    }
                    prevValue = buffer;

                }
            }
        }
    }
}

void decode_data(struct bitReader *reader, unsigned char *archiv, uint16_t *codes, struct rangedData *rangedData, size_t *size)
{
    uint16_t buffer = 0;
    uint16_t len = 0;
    uint8_t extraValue = 0;
    while (buffer != 256)
    {
        buffer = br_read_bits(reader, archiv, 1);
        len++;
        for (size_t i = 0; i < 318; i++)
        {
            if (buffer == codes[i])
            {
                rangedData[*size].haffCode = i;
                rangedData[*size].isLL = true;
                rangedData[*size].haffLen = len;
                if (i == 285){
                    rangedData[*size].extraVal = 0;
                    rangedData[*size].extraLen = 0;
                }
                else if (i > 256 && i < 285)
                {
                    rangedData[*size].extraLen = 0;
                    rangedData[*size].extraVal = 0;
                    if (i > 264)
                    {
                        extraValue = br_read_bits(reader, archiv, 1);
                        rangedData[*size].extraVal = extraValue;
                        rangedData[*size].extraLen = 1;
                    }
                    if (i > 268)
                    {
                        extraValue = br_read_bits(reader, archiv, 1);
                        rangedData[*size].extraVal = extraValue;
                        rangedData[*size].extraLen = 2;
                    }
                    if (i > 272)
                    {
                        extraValue = br_read_bits(reader, archiv, 1);
                        rangedData[*size].extraVal = extraValue;
                        rangedData[*size].extraLen = 3;
                    }
                    if (i > 276)
                    {
                        extraValue = br_read_bits(reader, archiv, 1);
                        rangedData[*size].extraVal = extraValue;
                        rangedData[*size].extraLen = 4;
                    }
                    if (i > 280)
                    {
                        extraValue = br_read_bits(reader, archiv, 1);
                        rangedData[*size].extraVal = extraValue;
                        rangedData[*size].extraLen = 5;
                    }
                }
                else if (i > 285)
                {
                    rangedData[*size].isLL = false;
                    if (i > 289)
                    {
                        extraValue = br_read_bits(reader, archiv, 1);
                        rangedData[*size].extraVal = extraValue;
                        rangedData[*size].extraLen = 1;
                    }
                    if (i > 291)
                    {
                        extraValue = br_read_bits(reader, archiv, 1);
                        rangedData[*size].extraVal = extraValue;
                        rangedData[*size].extraLen = 2;
                    }
                    if (i > 293)
                    {
                        extraValue = br_read_bits(reader, archiv, 1);
                        rangedData[*size].extraVal = extraValue;
                        rangedData[*size].extraLen = 3;
                    }
                    if (i > 295)
                    {
                        extraValue = br_read_bits(reader, archiv, 1);
                        rangedData[*size].extraVal = extraValue;
                        rangedData[*size].extraLen = 4;
                    }
                    if (i > 297)
                    {
                        extraValue = br_read_bits(reader, archiv, 1);
                        rangedData[*size].extraVal = extraValue;
                        rangedData[*size].extraLen = 5;
                    }
                    if (i > 299)
                    {
                        extraValue = br_read_bits(reader, archiv, 1);
                        rangedData[*size].extraVal = extraValue;
                        rangedData[*size].extraLen = 6;
                    }
                    if (i > 301)
                    {
                        extraValue = br_read_bits(reader, archiv, 1);
                        rangedData[*size].extraVal = extraValue;
                        rangedData[*size].extraLen = 7;
                    }
                    if (i > 303)
                    {
                        extraValue = br_read_bits(reader, archiv, 1);
                        rangedData[*size].extraVal = extraValue;
                        rangedData[*size].extraLen = 8;
                    }
                    if (i > 305)
                    {
                        extraValue = br_read_bits(reader, archiv, 1);
                        rangedData[*size].extraVal = extraValue;
                        rangedData[*size].extraLen = 9;
                    }
                    if (i > 307)
                    {
                        extraValue = br_read_bits(reader, archiv, 1);
                        rangedData[*size].extraVal = extraValue;
                        rangedData[*size].extraLen = 10;
                    }
                    if (i > 309)
                    {
                        extraValue = br_read_bits(reader, archiv, 1);
                        rangedData[*size].extraVal = extraValue;
                        rangedData[*size].extraLen = 11;
                    }
                    if (i > 311)
                    {
                        extraValue = br_read_bits(reader, archiv, 1);
                        rangedData[*size].extraVal = extraValue;
                        rangedData[*size].extraLen = 12;
                    }
                    if (i > 313)
                    {
                        extraValue = br_read_bits(reader, archiv, 1);
                        rangedData[*size].extraVal = extraValue;
                        rangedData[*size].extraLen = 13;
                    }

                }
                extraValue = 0;
                buffer = 0;
                len = 0;
                *size++;
            }
        }
    }
}

void parseLO(struct rangedData *rangedData, size_t rangedDataSize, struct match *matches, size_t *matchesSize)
{
    uint16_t trueVal;
    for (size_t i = 0 ; i < rangedDataSize; i++)
    {
        trueVal = rangedData[i].haffCode;
        if (rangedData[i].isLL && rangedData[i].haffCode > 255)
        {
            matches[*matchesSize].type = MATCH;
            switch(trueVal)
            {
                case 265:
                    trueVal = 11 + rangedData[i].extraVal;
                    break;
                case 266:
                    trueVal = 13 + rangedData[i].extraVal;
                    break;
                case 267:
                    trueVal = 15 + rangedData[i].extraVal;
                    break;
                case 268:
                    trueVal = 17 + rangedData[i].extraVal;
                    break;
                case 269:
                    trueVal = 19 + rangedData[i].extraVal;
                    break;
                case 270:
                    trueVal = 23 + rangedData[i].extraVal;
                    break;
                case 271:
                    trueVal = 27 + rangedData[i].extraVal;
                    break;
                case 272:
                    trueVal = 31 + rangedData[i].extraVal;
                    break;
                case 273:
                    trueVal = 35 + rangedData[i].extraVal;
                    break;
                case 274:
                    trueVal = 43 + rangedData[i].extraVal;
                    break;
                case 275:
                    trueVal = 51 + rangedData[i].extraVal;
                    break;
                case 276:
                    trueVal = 59 + rangedData[i].extraVal;
                    break;
                case 277:
                    trueVal = 67 + rangedData[i].extraVal;
                    break;
                case 278:
                    trueVal = 83 + rangedData[i].extraVal;
                    break;
                case 279:
                    trueVal = 99 + rangedData[i].extraVal;
                    break;
                case 280:
                    trueVal = 115 + rangedData[i].extraVal;
                    break;
                case 281:
                    trueVal = 131 + rangedData[i].extraVal;
                    break;
                case 282:
                    trueVal = 163 + rangedData[i].extraVal;
                    break;
                case 283:
                    trueVal = 195 + rangedData[i].extraVal;
                    break;
                case 284:
                    trueVal = 227 + rangedData[i].extraVal;
                    break;
                case 257:
                    trueVal = 3;
                    break;
                case 258:
                    trueVal = 4;
                    break;
                case 259:
                    trueVal = 5;
                    break;
                case 260:
                    trueVal = 6;
                    break;
                case 261:
                    trueVal = 7;
                    break;
                case 262:
                    trueVal = 8;
                    break;
                case 263:
                    trueVal = 9;
                    break;
                case 264:
                    trueVal = 10;
                    break;
                case 285:
                    trueVal = 258;
                    break;
            }
            matches[*matchesSize].length = trueVal;
        }
        else if (!rangedData[i].isLL)
        {
            matches[*matchesSize].type = MATCH;
            switch (trueVal)
            {
                case 0:
                    trueVal = 1;
                    break;
                case 1:
                    trueVal = 2;
                    break;
                case 2:
                    trueVal = 3;
                    break;
                case 3:
                    trueVal = 4;
                    break;
                case 4:
                    trueVal = 5 + rangedData[i].extraVal;
                    break;
                case 5:
                    trueVal = 7 + rangedData[i].extraVal;
                    break;
                case 6:
                    trueVal = 9 + rangedData[i].extraVal;
                    break;
                case 7:
                    trueVal = 13 + rangedData[i].extraVal;
                    break;
                case 8:
                    trueVal = 17 + rangedData[i].extraVal;
                    break;
                case 9:
                    trueVal = 25 + rangedData[i].extraVal;
                    break;
                case 10:
                    trueVal = 33 + rangedData[i].extraVal;
                    break;
                case 11:
                    trueVal = 49 + rangedData[i].extraVal;
                    break;
                case 12:
                    trueVal = 65 + rangedData[i].extraVal;
                    break;
                case 13:
                    trueVal = 97 + rangedData[i].extraVal;
                    break;
                case 14:
                    trueVal = 129 + rangedData[i].extraVal;
                    break;
                case 15:
                    trueVal = 193 + rangedData[i].extraVal;
                    break;
                case 16:
                    trueVal = 257 + rangedData[i].extraVal;
                    break;
                case 17:
                    trueVal = 385 + rangedData[i].extraVal;
                    break;
                case 18:
                    trueVal = 513 + rangedData[i].extraVal;
                    break;
                case 19:
                    trueVal = 769 + rangedData[i].extraVal;
                    break;
                case 20:
                    trueVal = 1025 + rangedData[i].extraVal;
                    break;
                case 21:
                    trueVal = 1537 + rangedData[i].extraVal;
                    break;
                case 22:
                    trueVal = 2049 + rangedData[i].extraVal;
                    break;
                case 23:
                    trueVal = 3073 + rangedData[i].extraVal;
                    break;
                case 24:
                    trueVal = 4097 + rangedData[i].extraVal;
                    break;
                case 25:
                    trueVal = 6145 + rangedData[i].extraVal;
                    break;
                case 26:
                    trueVal = 8193 + rangedData[i].extraVal;
                    break;
                case 27:
                    trueVal = 12289 + rangedData[i].extraVal;
                    break;
                case 28:
                    trueVal = 16385 + rangedData[i].extraVal;
                    break;
                case 29:
                    trueVal = 24577 + rangedData[i].extraVal;
                    break;
            }
            matches[*matchesSize].offset = trueVal;
            *matchesSize++;
        }
        else{
            matches[*matchesSize].type = LITERAL;
            matches[*matchesSize].literal = trueVal;
            *matchesSize++;
        }
    }
}

void write_file(uint8_t *file, struct match *matches, size_t size, size_t *pos)
{
    for (size_t i = 0; i < size; i++)
    {
        if (matches[i].type == LITERAL)
        {
            file[(*pos)++] = matches[i].literal;
        }
        else{
            for (size_t j = 0; j < matches[i].length; j++)
            {
                file[*pos] = file[*pos - matches[i].offset + j];
                (*pos)++;
            }
        }
    }
}

void decompress(struct bitReader *reader, unsigned char * archiv, struct fileTree* head)
{
    size_t rangedDataSize = 0, pos = 0, matchesSize = 0;
    uint8_t hclen = 0, hdist = 0, hlit = 0, coding = 0;
    uint8_t bfinal = 0;
    unsigned treesCodelengths[19];
    uint8_t alphabet[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    uint16_t treeCodes[19], codes[318];
    unsigned codeLengths[318];
    struct rangedData *rangedData;
    struct match *matches;
    uint8_t *fileData;

    if (head->isDir)
    {
        mkdir(head->path, 0777);
        for (size_t i = 0; i < head->childsCount; i++)
        {
            decompress(reader, archiv, head);
        }
        return;
    }

    

     while(true){
        rangedData = calloc(2000, sizeof(struct rangedData));
        matches = calloc(2000, sizeof(struct match));
        fileData = malloc(20000);

        bfinal = (uint8_t)br_read_bits(reader, archiv, 1);  
        coding = (uint8_t)br_read_bits(reader, archiv, 2);  
        hlit   = (uint8_t)br_read_bits(reader, archiv, 5); 
        hdist  = (uint8_t)br_read_bits(reader, archiv, 5);  
        hclen  = (uint8_t)br_read_bits(reader, archiv, 4);  
        for (size_t i = 0; i < sizeof(alphabet); i++)
        {
            treesCodelengths[alphabet[i]] = br_read_bits(reader, archiv, 3);
        }


        makeCanonicalCodes(treesCodelengths, sizeof(alphabet), treeCodes);

        decode_trees(archiv, reader, codeLengths, treeCodes);

        makeCanonicalCodes(codeLengths, 318, codes);

        decode_data(reader, archiv, codes, rangedData, &rangedDataSize);

        parseLO(rangedData, rangedDataSize, matches, &matchesSize);

        write_file(fileData, matches, matchesSize, &pos);

        free(rangedData);
        free(matches);
        free(fileData);
        memset(treesCodelengths, 0, sizeof(treesCodelengths));
        memset(treeCodes, 0, sizeof(treeCodes));
        memset(codes, 0, sizeof(codes));
        memset(codeLengths, 0, sizeof(codeLengths));

    }

    FILE *file = fopen(head->path, "wb");
    fwrite(fileData, 1, pos, file);
    fclose(file);

}


int main(int argc, char **argv)
{
    size_t fileSize = 0;
    FILE* file;
    struct fileTree *head;
    unsigned char *archiv = malloc(10 * 1024 * 1024);
    char path[30];    
    struct bitReader reader = {0, 0, 0};
    

    if (argc < 2) return 1;
    file = fopen(argv[1], "rb");
    if (!file) return 1;
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    fread(archiv, 1, fileSize, file);
    fclose(file);

    br_init(&reader, archiv);

    read_file_tree(head, archiv, &reader);

    strcpy(path, head->path);
    
    mkdir(path, 0777);

    for (size_t i = 0; i < head->childsCount; i++)
    {
        decompress(&reader, archiv, head);
    }
    


    return 0;
}