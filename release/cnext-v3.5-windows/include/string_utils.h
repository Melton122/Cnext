#ifndef CNEXT_STRING_UTILS_H
#define CNEXT_STRING_UTILS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static inline CnextString str_trim(CnextString s) {
    if (!s.data || s.length == 0) return s;
    const char* start = s.data;
    const char* end = s.data + s.length - 1;
    while (start <= end && isspace((unsigned char)*start)) start++;
    while (end >= start && isspace((unsigned char)*end)) end--;
    size_t new_len = (size_t)(end - start + 1);
    char* buffer = (char*)malloc(new_len + 1);
    if (!buffer) return (CnextString){NULL, 0};
    memcpy(buffer, start, new_len);
    buffer[new_len] = '\0';
    _cnext_track(buffer);
    return (CnextString){buffer, new_len};
}

static inline CnextString str_trim_left(CnextString s) {
    if (!s.data || s.length == 0) return s;
    const char* start = s.data;
    while (*start && isspace((unsigned char)*start)) start++;
    size_t new_len = s.length - (size_t)(start - s.data);
    char* buffer = (char*)malloc(new_len + 1);
    if (!buffer) return (CnextString){NULL, 0};
    memcpy(buffer, start, new_len);
    buffer[new_len] = '\0';
    _cnext_track(buffer);
    return (CnextString){buffer, new_len};
}

static inline CnextString str_trim_right(CnextString s) {
    if (!s.data || s.length == 0) return s;
    const char* end = s.data + s.length - 1;
    while (end >= s.data && isspace((unsigned char)*end)) end--;
    size_t new_len = (size_t)(end - s.data + 1);
    char* buffer = (char*)malloc(new_len + 1);
    if (!buffer) return (CnextString){NULL, 0};
    memcpy(buffer, s.data, new_len);
    buffer[new_len] = '\0';
    _cnext_track(buffer);
    return (CnextString){buffer, new_len};
}

static inline CnextString str_to_upper(CnextString s) {
    if (!s.data) return s;
    char* buffer = (char*)malloc(s.length + 1);
    if (!buffer) return (CnextString){NULL, 0};
    for (size_t i = 0; i < s.length; i++) {
        buffer[i] = (char)toupper((unsigned char)s.data[i]);
    }
    buffer[s.length] = '\0';
    _cnext_track(buffer);
    return (CnextString){buffer, s.length};
}

static inline CnextString str_to_lower(CnextString s) {
    if (!s.data) return s;
    char* buffer = (char*)malloc(s.length + 1);
    if (!buffer) return (CnextString){NULL, 0};
    for (size_t i = 0; i < s.length; i++) {
        buffer[i] = (char)tolower((unsigned char)s.data[i]);
    }
    buffer[s.length] = '\0';
    _cnext_track(buffer);
    return (CnextString){buffer, s.length};
}

static inline bool str_starts_with(CnextString s, CnextString prefix) {
    if (!s.data || !prefix.data) return false;
    if (prefix.length > s.length) return false;
    return strncmp(s.data, prefix.data, prefix.length) == 0;
}

static inline bool str_ends_with(CnextString s, CnextString suffix) {
    if (!s.data || !suffix.data) return false;
    if (suffix.length > s.length) return false;
    return strncmp(s.data + s.length - suffix.length, suffix.data, suffix.length) == 0;
}

static inline bool str_contains(CnextString s, CnextString substr) {
    if (!s.data || !substr.data) return false;
    if (substr.length == 0) return true;
    if (substr.length > s.length) return false;
    for (size_t i = 0; i <= s.length - substr.length; i++) {
        if (strncmp(s.data + i, substr.data, substr.length) == 0) return true;
    }
    return false;
}

static inline int str_index_of(CnextString s, CnextString substr) {
    if (!s.data || !substr.data) return -1;
    if (substr.length == 0) return 0;
    if (substr.length > s.length) return -1;
    for (size_t i = 0; i <= s.length - substr.length; i++) {
        if (strncmp(s.data + i, substr.data, substr.length) == 0) return (int)i;
    }
    return -1;
}

static inline CnextString str_replace(CnextString s, CnextString old, CnextString new_str) {
    if (!s.data || !old.data || !new_str.data || old.length == 0) return s;
    size_t count = 0;
    const char* p = s.data;
    while ((p = strstr(p, old.data)) != NULL) { count++; p += old.length; }
    if (count == 0) {
        char* buffer = (char*)malloc(s.length + 1);
        if (!buffer) return (CnextString){NULL, 0};
        memcpy(buffer, s.data, s.length + 1);
        _cnext_track(buffer);
        return (CnextString){buffer, s.length};
    }
    size_t new_len = s.length + count * (new_str.length - old.length);
    char* buffer = (char*)malloc(new_len + 1);
    if (!buffer) return (CnextString){NULL, 0};
    char* w = buffer;
    const char* r = s.data;
    const char* end = s.data + s.length;
    while (r <= end) {
        const char* found = strstr(r, old.data);
        if (!found || found > end) {
            size_t rem = (size_t)(end - r);
            memcpy(w, r, rem);
            w += rem;
            break;
        }
        size_t copy_len = (size_t)(found - r);
        memcpy(w, r, copy_len);
        w += copy_len;
        memcpy(w, new_str.data, new_str.length);
        w += new_str.length;
        r = found + old.length;
    }
    *w = '\0';
    _cnext_track(buffer);
    return (CnextString){buffer, new_len};
}

static inline CnextString str_substring(CnextString s, int start, int end) {
    if (!s.data || start < 0 || end < 0 || (size_t)end > s.length || start >= end) {
        return (CnextString){NULL, 0};
    }
    size_t new_len = (size_t)(end - start);
    char* buffer = (char*)malloc(new_len + 1);
    if (!buffer) return (CnextString){NULL, 0};
    memcpy(buffer, s.data + start, new_len);
    buffer[new_len] = '\0';
    _cnext_track(buffer);
    return (CnextString){buffer, new_len};
}

static inline CnextString str_repeat(CnextString s, int count) {
    if (!s.data || count <= 0) return (CnextString){NULL, 0};
    size_t new_len = s.length * (size_t)count;
    char* buffer = (char*)malloc(new_len + 1);
    if (!buffer) return (CnextString){NULL, 0};
    for (int i = 0; i < count; i++) {
        memcpy(buffer + i * s.length, s.data, s.length);
    }
    buffer[new_len] = '\0';
    _cnext_track(buffer);
    return (CnextString){buffer, new_len};
}

static inline CnextString str_reverse(CnextString s) {
    if (!s.data) return s;
    char* buffer = (char*)malloc(s.length + 1);
    if (!buffer) return (CnextString){NULL, 0};
    for (size_t i = 0; i < s.length; i++) {
        buffer[i] = s.data[s.length - 1 - i];
    }
    buffer[s.length] = '\0';
    _cnext_track(buffer);
    return (CnextString){buffer, s.length};
}

#endif
