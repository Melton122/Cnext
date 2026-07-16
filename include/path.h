#ifndef CNEXT_PATH_H
#define CNEXT_PATH_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static CnextString path_join(CnextString a, CnextString b) {
    if (!a.data) return b;
    if (!b.data) return a;
    int need_sep = 1;
    if (a.length > 0 && (a.data[a.length-1] == '/' || a.data[a.length-1] == '\\')) need_sep = 0;
    if (b.length > 0 && (b.data[0] == '/' || b.data[0] == '\\')) need_sep = 0;
    const char* sep = need_sep ? "/" : "";
    size_t slen = need_sep ? 1 : 0;
    size_t total = a.length + slen + b.length;
    char* buf = (char*)malloc(total + 1);
    if (!buf) return (CnextString){NULL, 0};
    memcpy(buf, a.data, a.length);
    if (need_sep) buf[a.length] = '/';
    memcpy(buf + a.length + slen, b.data, b.length);
    buf[total] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, total};
}

static CnextString path_dirname(CnextString path) {
    if (!path.data || path.length == 0) return (CnextString){NULL, 0};
    int end = (int)path.length - 1;
    while (end >= 0 && (path.data[end] == '/' || path.data[end] == '\\')) end--;
    if (end < 0) { char* b = (char*)malloc(2); b[0]='.'; b[1]=0; _cnext_track(b); return (CnextString){b,1}; }
    int slash = -1;
    for (int i = end; i >= 0; i--) {
        if (path.data[i] == '/' || path.data[i] == '\\') { slash = i; break; }
    }
    if (slash < 0) { char* b = (char*)malloc(2); b[0]='.'; b[1]=0; _cnext_track(b); return (CnextString){b,1}; }
    char* buf = (char*)malloc((size_t)slash + 1);
    if (!buf) return (CnextString){NULL, 0};
    memcpy(buf, path.data, (size_t)slash);
    buf[slash] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, (size_t)slash};
}

static CnextString path_basename(CnextString path) {
    if (!path.data || path.length == 0) return (CnextString){NULL, 0};
    int end = (int)path.length - 1;
    while (end >= 0 && (path.data[end] == '/' || path.data[end] == '\\')) end--;
    if (end < 0) { char* b = (char*)malloc(2); b[0]='/'; b[1]=0; _cnext_track(b); return (CnextString){b,1}; }
    int start = end;
    while (start >= 0 && path.data[start] != '/' && path.data[start] != '\\') start--;
    start++;
    size_t len = (size_t)(end - start + 1);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return (CnextString){NULL, 0};
    memcpy(buf, path.data + start, len);
    buf[len] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, len};
}

static CnextString path_extension(CnextString path) {
    if (!path.data || path.length == 0) return (CnextString){NULL, 0};
    int end = (int)path.length - 1;
    while (end >= 0 && (path.data[end] == '/' || path.data[end] == '\\')) end--;
    if (end < 0) return (CnextString){NULL, 0};
    int dot = -1;
    for (int i = end; i >= 0; i--) {
        if (path.data[i] == '.' && (i == 0 || (path.data[i-1] != '/' && path.data[i-1] != '\\'))) { dot = i; break; }
        if (path.data[i] == '/' || path.data[i] == '\\') break;
    }
    if (dot < 0) return (CnextString){NULL, 0};
    size_t len = (size_t)(end - dot + 1);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return (CnextString){NULL, 0};
    memcpy(buf, path.data + dot, len);
    buf[len] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, len};
}

static CnextString path_normalize(CnextString path) {
    if (!path.data || path.length == 0) return (CnextString){NULL, 0};
    size_t len = path.length;
    char* copy = (char*)malloc(len + 1);
    if (!copy) return (CnextString){NULL, 0};
    memcpy(copy, path.data, len);
    copy[len] = '\0';
    for (size_t j = 0; j < len; j++) {
        if (copy[j] == '\\') copy[j] = '/';
    }
    char* buf = (char*)malloc(len + 1);
    if (!buf) { free(copy); return (CnextString){NULL, 0}; }
    size_t w = 0;
    size_t i = 0;
    if (len >= 2 && copy[1] == ':') {
        buf[w++] = copy[0];
        buf[w++] = copy[1];
        i = 2;
    }
    size_t seg_start = w;
    while (i < len) {
        if (copy[i] == '/') { i++; continue; }
        if (copy[i] == '.' && (i+1 >= len || copy[i+1] == '/')) { i++; continue; }
        if (copy[i] == '.' && copy[i+1] == '.' && (i+2 >= len || copy[i+2] == '/')) {
            if (w > seg_start) {
                w--;
                while (w > seg_start && buf[w-1] != '/') w--;
            }
            i += 2;
            continue;
        }
        if (w > 0 && buf[w-1] != '/') buf[w++] = '/';
        while (i < len && copy[i] != '/') buf[w++] = copy[i++];
    }
    if (w == 0) buf[w++] = '.';
    buf[w] = '\0';
    free(copy);
    _cnext_track(buf);
    return (CnextString){buf, w};
}

#endif
