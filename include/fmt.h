#ifndef CNEXT_FMT_H
#define CNEXT_FMT_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

static inline CnextString fmt_pad_left(CnextString s, int total_width, char pad_char) {
    if ((int)s.length >= total_width) return s;
    int pad = total_width - (int)s.length;
    char* buf = (char*)malloc(total_width + 1);
    if (!buf) return (CnextString){NULL, 0};
    memset(buf, pad_char, pad);
    memcpy(buf + pad, s.data, s.length);
    buf[total_width] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, (size_t)total_width};
}

static inline CnextString fmt_pad_right(CnextString s, int total_width, char pad_char) {
    if ((int)s.length >= total_width) return s;
    int pad = total_width - (int)s.length;
    char* buf = (char*)malloc(total_width + 1);
    if (!buf) return (CnextString){NULL, 0};
    memcpy(buf, s.data, s.length);
    memset(buf + s.length, pad_char, pad);
    buf[total_width] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, (size_t)total_width};
}

static inline CnextString fmt_pad_center(CnextString s, int total_width, char pad_char) {
    if ((int)s.length >= total_width) return s;
    int pad = total_width - (int)s.length;
    int left = pad / 2;
    int right = pad - left;
    char* buf = (char*)malloc(total_width + 1);
    if (!buf) return (CnextString){NULL, 0};
    memset(buf, pad_char, left);
    memcpy(buf + left, s.data, s.length);
    memset(buf + left + s.length, pad_char, right);
    buf[total_width] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, (size_t)total_width};
}

static inline CnextString fmt_join(CnextString* parts, int count, CnextString separator) {
    if (count <= 0) return (CnextString){(char*)"", 0};
    size_t total = 0;
    for (int i = 0; i < count; i++) total += parts[i].length;
    total += separator.length * (size_t)(count - 1);
    char* buf = (char*)malloc(total + 1);
    if (!buf) return (CnextString){NULL, 0};
    size_t pos = 0;
    for (int i = 0; i < count; i++) {
        if (i > 0 && separator.data) {
            memcpy(buf + pos, separator.data, separator.length);
            pos += separator.length;
        }
        memcpy(buf + pos, parts[i].data, parts[i].length);
        pos += parts[i].length;
    }
    buf[pos] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, pos};
}

static inline CnextString fmt_repeat(CnextString s, int count) {
    if (count <= 0 || !s.data) return (CnextString){(char*)"", 0};
    size_t total = s.length * (size_t)count;
    char* buf = (char*)malloc(total + 1);
    if (!buf) return (CnextString){NULL, 0};
    for (int i = 0; i < count; i++) {
        memcpy(buf + (size_t)i * s.length, s.data, s.length);
    }
    buf[total] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, total};
}

static inline CnextString fmt_int(int value) {
    char* buf = (char*)malloc(32);
    if (!buf) return (CnextString){NULL, 0};
    int len = snprintf(buf, 32, "%d", value);
    _cnext_track(buf);
    return (CnextString){buf, (size_t)len};
}

static inline CnextString fmt_float(double value, int decimals) {
    char* buf = (char*)malloc(64);
    if (!buf) return (CnextString){NULL, 0};
    char fmt[32];
    snprintf(fmt, sizeof(fmt), "%%.%df", decimals);
    int len = snprintf(buf, 64, fmt, value);
    _cnext_track(buf);
    return (CnextString){buf, (size_t)len};
}

static inline CnextString fmt_hex(int value) {
    char* buf = (char*)malloc(32);
    if (!buf) return (CnextString){NULL, 0};
    int len = snprintf(buf, 32, "0x%x", (unsigned)value);
    _cnext_track(buf);
    return (CnextString){buf, (size_t)len};
}

static inline CnextString fmt_hex_upper(int value) {
    char* buf = (char*)malloc(32);
    if (!buf) return (CnextString){NULL, 0};
    int len = snprintf(buf, 32, "0x%X", (unsigned)value);
    _cnext_track(buf);
    return (CnextString){buf, (size_t)len};
}

static inline CnextString fmt_binary(int value) {
    char* buf = (char*)malloc(64);
    if (!buf) return (CnextString){NULL, 0};
    unsigned v = (unsigned)value;
    if (v == 0) { buf[0] = '0'; buf[1] = '\0'; _cnext_track(buf); return (CnextString){buf, 1}; }
    int i = 0;
    char tmp[32];
    while (v > 0) { tmp[i++] = '0' + (v & 1); v >>= 1; }
    for (int j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
    buf[i] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, (size_t)i};
}

static inline CnextString fmt_filesize(long bytes) {
    char* buf = (char*)malloc(64);
    if (!buf) return (CnextString){NULL, 0};
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double size = (double)bytes;
    int unit = 0;
    while (size >= 1024.0 && unit < 4) { size /= 1024.0; unit++; }
    int len;
    if (unit == 0) len = snprintf(buf, 64, "%d B", (int)bytes);
    else len = snprintf(buf, 64, "%.1f %s", size, units[unit]);
    _cnext_track(buf);
    return (CnextString){buf, (size_t)len};
}

static inline CnextString fmt_plural(int count, CnextString singular, CnextString plural) {
    return count == 1 ? singular : plural;
}

static inline CnextString fmt_slice(CnextString s, int start, int end) {
    if (!s.data || start < 0) start = 0;
    if (end > (int)s.length) end = (int)s.length;
    if (start >= end) return (CnextString){NULL, 0};
    return (CnextString){s.data + start, (size_t)(end - start)};
}

#endif
