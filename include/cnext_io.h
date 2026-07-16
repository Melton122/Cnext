#ifndef CNEXT_IO_H
#define CNEXT_IO_H
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static inline CnextString read_file(CnextString path) {
    FILE* file = fopen(path.data, "rb");
    if (!file) return (CnextString){NULL, 0};

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    if (size < 0) { fclose(file); return (CnextString){NULL, 0}; }

    rewind(file);
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) { fclose(file); return (CnextString){NULL, 0}; }

    size_t read = fread(buffer, 1, size, file);
    buffer[read] = '\0';
    fclose(file);

    _cnext_track(buffer);
    return (CnextString){buffer, read};
}

static inline bool write_file(CnextString path, CnextString content) {
    FILE* file = fopen(path.data, "w");
    if (!file) return false;
    fwrite(content.data, 1, content.length, file);
    fclose(file);
    return true;
}

static inline bool append_file(CnextString path, CnextString content) {
    FILE* file = fopen(path.data, "a");
    if (!file) return false;
    fwrite(content.data, 1, content.length, file);
    fclose(file);
    return true;
}

#endif
