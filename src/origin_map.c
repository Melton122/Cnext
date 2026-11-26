#include "origin_map.h"
#include "checked_alloc.h"

#include <stdlib.h>
#include <string.h>

OriginMap* origin_map_create(void) {
    return (OriginMap*)checked_calloc(1, sizeof(OriginMap));
}

void origin_map_add(OriginMap* map, int merged_line, int line_count,
                    const char* file, int file_line) {
    if (!map || !file || merged_line < 1 || line_count < 1) return;
    if (map->count == map->capacity) {
        map->capacity = map->capacity ? map->capacity * 2 : 8;
        map->entries = (OriginEntry*)checked_realloc(map->entries,
            sizeof(OriginEntry) * (size_t)map->capacity);
    }
    OriginEntry* entry = &map->entries[map->count++];
    entry->merged_line = merged_line;
    entry->line_count = line_count;
    entry->file_line = file_line > 0 ? file_line : 1;
    entry->file = checked_strdup(file);
}

void origin_map_shift(OriginMap* map, int delta) {
    if (!map || delta == 0) return;
    for (int i = 0; i < map->count; i++) {
        map->entries[i].merged_line += delta;
    }
}

bool origin_map_lookup(const OriginMap* map, int merged_line,
                       const char** file, int* file_line) {
    if (!map || map->count == 0 || merged_line < 1) return false;
    const OriginEntry* found = NULL;
    for (int i = 0; i < map->count; i++) {
        if (map->entries[i].merged_line > merged_line) break;
        found = &map->entries[i];
    }
    if (!found) return false;
    int offset = merged_line - found->merged_line;
    /* Past the end of the block (e.g. the blank line separating two pasted
       files): keep the block's file but stop pushing the line number out. */
    if (offset >= found->line_count) offset = found->line_count - 1;
    if (file) *file = found->file;
    if (file_line) *file_line = found->file_line + offset;
    return true;
}

void origin_map_free(OriginMap* map) {
    if (!map) return;
    for (int i = 0; i < map->count; i++) free((void*)map->entries[i].file);
    free(map->entries);
    free(map);
}

int origin_map_count_lines(const char* text) {
    if (!text || !text[0]) return 0;
    int lines = 1;
    for (const char* p = text; *p; p++) {
        if (*p == '\n') lines++;
    }
    /* A trailing newline starts no extra line. */
    if (text[strlen(text) - 1] == '\n') lines--;
    return lines;
}
