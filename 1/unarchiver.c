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
            buffer |= br_read_bits(reader, archiv, 1) << c;
            for (size_t j = 0; j < 19; j++)
            {
                if (buffer == treeCodes[j])
                {
                    codeLengths[j] = buffer;
                    br = true;
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
                            codeLengths[i] = 0;
                        }
                        break;
                    }
                    prevValue = buffer;

                }
            }
        }
    }
}


int main(int argc, char **argv)
{
    FILE* file;
    size_t fileSize = 0;
    unsigned char *archiv = malloc(10 * 1024 * 1024);
    uint8_t hclen = 0, hdist = 0, hlit = 0, coding = 0;
    uint8_t bfinal = 0;
    struct bitReader reader = {0, 0, 0};
    uint8_t treesCodelengths[19];
    uint8_t alphabet[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    struct fileTree *head;
    uint16_t treeCodes[19], codes[318];
    unsigned codeLengths[318];

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
    while(true){

        bfinal = (uint8_t)br_read_bits(&reader, archiv, 1);  
        coding = (uint8_t)br_read_bits(&reader, archiv, 2);  
        hlit   = (uint8_t)br_read_bits(&reader, archiv, 5); 
        hdist  = (uint8_t)br_read_bits(&reader, archiv, 5);  
        hclen  = (uint8_t)br_read_bits(&reader, archiv, 4);  
        for (size_t i = 0; i < sizeof(alphabet); i++)
        {
            treesCodelengths[alphabet[i]] = br_read_bits(&reader, archiv, 3);
        }


        makeCanonicalCodes(treesCodelengths, sizeof(alphabet), treeCodes); // по ним найти длины кодов -> по длинам каноническое дерево -> декодить остальную часть -> LZ77

        decode_trees(archiv, &reader, codeLengths, treeCodes);

        

        // makeCanonicalCodes(codeLengths, 318, codes);



    }




    return 0;
}