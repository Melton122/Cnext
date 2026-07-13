#ifndef CHECKED_ALLOC_H
#define CHECKED_ALLOC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void* checked_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Out of memory.\n");
        exit(70);
    }
    return ptr;
}

static inline void* checked_calloc(size_t count, size_t size) {
    void* ptr = calloc(count, size);
    if (!ptr) {
        fprintf(stderr, "Out of memory.\n");
        exit(70);
    }
    return ptr;
}

static inline void* checked_realloc(void* ptr, size_t size) {
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        fprintf(stderr, "Out of memory.\n");
        exit(70);
    }
    return new_ptr;
}

static inline char* checked_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* copy = (char*)checked_malloc(len + 1);
    memcpy(copy, s, len + 1);
    return copy;
}

static inline char* checked_strndup(const char* s, size_t n) {
    if (!s) return NULL;
    char* copy = (char*)checked_malloc(n + 1);
    memcpy(copy, s, n);
    copy[n] = '\0';
    return copy;
}

#endif /* CHECKED_ALLOC_H */
