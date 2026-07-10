#ifndef CNEXT_FILE_H
#define CNEXT_FILE_H
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static inline bool copy_file(CnextString src_path, CnextString dest_path) {
    FILE* src = fopen(src_path.data, "rb");
    if (!src) return false;

    FILE* dest = fopen(dest_path.data, "wb");
    if (!dest) { fclose(src); return false; }

    char buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, n, dest) != n) {
            fclose(src);
            fclose(dest);
            return false;
        }
    }

    fclose(src);
    fclose(dest);
    return true;
}

static inline bool delete_file(CnextString path) {
    return remove(path.data) == 0;
}

static inline bool file_exists(CnextString path) {
    FILE* file = fopen(path.data, "rb");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

static inline long file_size(CnextString path) {
    FILE* file = fopen(path.data, "rb");
    if (!file) return -1;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fclose(file);
    return size;
}

#endif
