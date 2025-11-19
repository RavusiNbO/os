#include "archiver.h"


int main()
{
    char *filename = malloc(MAX_PATH);
    uint8_t *archiv = malloc(1024*1024*10);
    struct bitWriter writer = {0, 0, 0};
    struct fileTree *root;
    FILE *ofile;
    char *path = malloc(MAX_PATH);


    scanf("%1023s", filename);
    compress_directory(filename, archiv, &writer, root);
    flushBuf(archiv, &writer);

    snprintf(path, MAX_PATH, "%s_archived", filename);
    ofile = fopen(path, "wb");

    printf("writing file tree\n");
    write_file_tree(root, ofile);
    printf("writing archiv\n");
    for (size_t k = 0; k < writer.buffPos; ++k) {
        fputc(archiv[k], ofile);
    }

    fclose(ofile);
    
    return 0;
}