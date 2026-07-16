#ifndef SOURCEMAP_H
#define SOURCEMAP_H

#include <stdbool.h>

typedef struct {
    int source_line;    // Line in .cn source
    int generated_line; // Line in generated .c file
    int source_col;     // Column in .cn source
    int generated_col;  // Column in generated .c file
} SourceMapEntry;

typedef struct {
    SourceMapEntry* entries;
    int count;
    int capacity;
    char* source_filename;
    char* generated_filename;
} SourceMap;

SourceMap* sourcemap_create(const char* source_file, const char* generated_file);
void sourcemap_add(SourceMap* map, int src_line, int src_col, int gen_line, int gen_col);
void sourcemap_write(SourceMap* map, const char* output_path);
void sourcemap_free(SourceMap* map);
int sourcemap_lookup(SourceMap* map, int generated_line);

#endif // SOURCEMAP_H
