#include "archiver.h"


int main(int argc, char **argv)
{
    uint8_t *archiv = malloc(1024*1024*10);
    struct bitWriter writer = {0, 0, 0};
    struct fileTree *root;
    FILE *ofile;
    char *path = malloc(MAX_PATH);
    


    compress_directory(argv[1], archiv, &writer, &root);
    
    snprintf(path, MAX_PATH, "%s_archived", argv[1]);
    ofile = fopen(path, "wb");
    fflush(stdout);
    printf("writing file tree\n");
    write_file_tree(root, ofile);
    printf("writing file\n");
    for (size_t k = 0; k < writer.buffPos; ++k) {
        fputc(archiv[k], ofile);
    }

    fclose(ofile);
    
    return 0;
}