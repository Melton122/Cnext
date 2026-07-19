#ifndef CNEXT_BUFFER_H
#define CNEXT_BUFFER_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} Buffer;

static inline Buffer buf_new(size_t initial_cap) {
    Buffer b;
    b.capacity = initial_cap > 0 ? initial_cap : 256;
    b.data = (char*)malloc(b.capacity);
    b.length = 0;
    if (b.data) b.data[0] = '\0';
    return b;
}

static inline void buf_free(Buffer* b) {
    if (b->data) free(b->data);
    b->data = NULL;
    b->length = 0;
    b->capacity = 0;
}

static inline bool _buf_grow(Buffer* b, size_t needed) {
    if (needed <= b->capacity) return true;
    size_t new_cap = b->capacity;
    while (new_cap < needed) new_cap *= 2;
    char* nd = (char*)realloc(b->data, new_cap);
    if (!nd) return false;
    b->data = nd;
    b->capacity = new_cap;
    return true;
}

static inline void buf_append(Buffer* b, CnextString s) {
    if (!s.data) return;
    if (!_buf_grow(b, b->length + s.length + 1)) return;
    memcpy(b->data + b->length, s.data, s.length);
    b->length += s.length;
    b->data[b->length] = '\0';
}

static inline void buf_append_cstr(Buffer* b, const char* s) {
    if (!s) return;
    size_t len = strlen(s);
    if (!_buf_grow(b, b->length + len + 1)) return;
    memcpy(b->data + b->length, s, len);
    b->length += len;
    b->data[b->length] = '\0';
}

static inline void buf_append_char(Buffer* b, char c) {
    if (!_buf_grow(b, b->length + 2)) return;
    b->data[b->length++] = c;
    b->data[b->length] = '\0';
}

static inline void buf_append_int(Buffer* b, int value) {
    char tmp[32];
    int len = snprintf(tmp, sizeof(tmp), "%d", value);
    buf_append_cstr(b, tmp);
}

static inline void buf_append_fmt(Buffer* b, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (len <= 0) return;
    if (!_buf_grow(b, b->length + (size_t)len + 1)) return;
    va_start(args, fmt);
    vsnprintf(b->data + b->length, (size_t)len + 1, fmt, args);
    va_end(args);
    b->length += (size_t)len;
}

static inline void buf_insert(Buffer* b, size_t pos, CnextString s) {
    if (!s.data || pos > b->length) return;
    if (!_buf_grow(b, b->length + s.length + 1)) return;
    memmove(b->data + pos + s.length, b->data + pos, b->length - pos);
    memcpy(b->data + pos, s.data, s.length);
    b->length += s.length;
    b->data[b->length] = '\0';
}

static inline void buf_remove(Buffer* b, size_t pos, size_t count) {
    if (pos >= b->length || count == 0) return;
    if (pos + count > b->length) count = b->length - pos;
    memmove(b->data + pos, b->data + pos + count, b->length - pos - count);
    b->length -= count;
    b->data[b->length] = '\0';
}

static inline void buf_clear(Buffer* b) {
    b->length = 0;
    if (b->data) b->data[0] = '\0';
}

static inline CnextString buf_to_string(Buffer* b) {
    if (!b->data || b->length == 0) return (CnextString){NULL, 0};
    char* result = (char*)malloc(b->length + 1);
    if (!result) return (CnextString){NULL, 0};
    memcpy(result, b->data, b->length + 1);
    _cnext_track(result);
    return (CnextString){result, b->length};
}

static inline CnextString buf_to_string_no_copy(Buffer* b) {
    if (!b->data) return (CnextString){NULL, 0};
    _cnext_track(b->data);
    CnextString s = {b->data, b->length};
    b->data = NULL;
    b->length = 0;
    b->capacity = 0;
    return s;
}

#endif
