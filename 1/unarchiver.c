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

int main(int argc, char **argv)
{
    FILE* file;
    size_t fileSize = 0;
    unsigned char *archiv = malloc(10 * 1024 * 1024);
    uint8_t hclen = 0, hdist = 0, hlit = 0, coding = 0;
    uint8_t bfinal = 0;
    struct bitReader reader = {0, 0, 0};
    uint8_t TreesCodelengths

    if (argc < 2) return 1;
    file = fopen(argv[1], "rb");
    if (!file) return 1;
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    fread(archiv, 1, fileSize, file);
    fclose(file);

    br_init(&reader, archiv);
    bfinal = (uint8_t)br_read_bits(&reader, archiv, 1);  
    coding = (uint8_t)br_read_bits(&reader, archiv, 2);  
    hlit   = (uint8_t)br_read_bits(&reader, archiv, 5); 
    hdist  = (uint8_t)br_read_bits(&reader, archiv, 5);  
    hclen  = (uint8_t)br_read_bits(&reader, archiv, 4);  



    return 0;
}
