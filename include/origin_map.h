#ifndef ORIGIN_MAP_H
#define ORIGIN_MAP_H

#include <stdbool.h>

/* Local imports paste imported sources ahead of the importing file, so every
   line of the importing file moves. An OriginMap records which file each line
   of the merged translation unit really came from, so diagnostics can report
   `path:line:col` instead of a shifted line number in a nameless blob.

   Entries are contiguous blocks in ascending merged-line order. A lookup uses
   the nearest block that starts at or before the requested line. */
typedef struct {
    int merged_line;    /* first line of the block in the merged text */
    int line_count;     /* how many lines the block covers */
    int file_line;      /* line in `file` that `merged_line` corresponds to */
    const char* file;   /* owned copy */
} OriginEntry;

typedef struct OriginMap {
    OriginEntry* entries;
    int count;
    int capacity;
} OriginMap;

OriginMap* origin_map_create(void);
void origin_map_add(OriginMap* map, int merged_line, int line_count,
                    const char* file, int file_line);
/* Move every block down by `delta` lines (used when another expander, such as
   the package loader, prepends text in front of the merged program). */
void origin_map_shift(OriginMap* map, int delta);
bool origin_map_lookup(const OriginMap* map, int merged_line,
                       const char** file, int* file_line);
void origin_map_free(OriginMap* map);

/* Number of lines in `text`, counting a final unterminated line. */
int origin_map_count_lines(const char* text);

#endif /* ORIGIN_MAP_H */
