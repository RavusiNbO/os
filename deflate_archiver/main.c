#include "deflate.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  Archive:   %s -c <directory> <archive>\n", argv[0]);
        fprintf(stderr, "  Extract:   %s -x <archive> <output_directory>\n", argv[0]);
        return 1;
    }
    
    if (strcmp(argv[1], "-c") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Error: Archive mode requires directory and archive path\n");
            return 1;
        }
        return archive_directory(argv[2], argv[3]);
    } else if (strcmp(argv[1], "-x") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Error: Extract mode requires archive and output directory\n");
            return 1;
        }
        return extract_archive(argv[2], argv[3]);
    } else {
        fprintf(stderr, "Error: Unknown mode. Use -c for archive or -x for extract\n");
        return 1;
    }
}

