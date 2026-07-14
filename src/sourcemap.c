#include "sourcemap.h"
#include "checked_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SourceMap* sourcemap_create(const char* source_file, const char* generated_file) {
    SourceMap* map = (SourceMap*)checked_calloc(1, sizeof(SourceMap));
    map->capacity = 256;
    map->entries = (SourceMapEntry*)checked_malloc(sizeof(SourceMapEntry) * map->capacity);
    map->count = 0;
    map->source_filename = checked_strdup(source_file);
    map->generated_filename = checked_strdup(generated_file);
    return map;
}

void sourcemap_add(SourceMap* map, int src_line, int src_col, int gen_line, int gen_col) {
    if (!map) return;
    if (map->count >= map->capacity) {
        map->capacity *= 2;
        map->entries = (SourceMapEntry*)checked_realloc(map->entries, sizeof(SourceMapEntry) * map->capacity);
    }
    map->entries[map->count].source_line = src_line;
    map->entries[map->count].source_col = src_col;
    map->entries[map->count].generated_line = gen_line;
    map->entries[map->count].generated_col = gen_col;
    map->count++;
}

void sourcemap_write(SourceMap* map, const char* output_path) {
    if (!map) return;
    FILE* f = fopen(output_path, "w");
    if (!f) return;

    fprintf(f, "{\n");
    fprintf(f, "  \"source\": \"%s\",\n", map->source_filename);
    fprintf(f, "  \"generated\": \"%s\",\n", map->generated_filename);
    fprintf(f, "  \"mappings\": [\n");
    for (int i = 0; i < map->count; i++) {
        fprintf(f, "    {\"src\":%d,\"scol\":%d,\"gen\":%d,\"gcol\":%d}%s\n",
            map->entries[i].source_line, map->entries[i].source_col,
            map->entries[i].generated_line, map->entries[i].generated_col,
            i < map->count - 1 ? "," : "");
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
}

void sourcemap_free(SourceMap* map) {
    if (!map) return;
    free(map->entries);
    free(map->source_filename);
    free(map->generated_filename);
    free(map);
}

int sourcemap_lookup(SourceMap* map, int generated_line) {
    if (!map || map->count == 0) return -1;
    // Binary search for the closest mapping
    int lo = 0, hi = map->count - 1;
    int best = -1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (map->entries[mid].generated_line <= generated_line) {
            best = map->entries[mid].source_line;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return best;
}
