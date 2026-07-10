#ifndef CNEXT_ENCODING_H
#define CNEXT_ENCODING_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

static const char _e_b64c[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static signed char _e_b64d[256];
static int _e_b64_init = 0;
static void _e_b64_tab(void) {
    if (_e_b64_init) return; _e_b64_init = 1;
    memset(_e_b64d, -1, 256);
    for (int i = 0; i < 64; i++) _e_b64d[(unsigned char)_e_b64c[i]] = i;
}

static CnextString base64_encode(CnextString input) {
    if (!input.data) return (CnextString){NULL, 0};
    size_t len = input.length;
    size_t olen = ((len + 2) / 3) * 4;
    char* buf = (char*)malloc(olen + 1);
    if (!buf) return (CnextString){NULL, 0};
    const uint8_t* in = (const uint8_t*)input.data;
    for (size_t i = 0, o = 0; i < len; i += 3) {
        int b = (int)in[i] << 16 | (i+1 < len ? (int)in[i+1] << 8 : 0) | (i+2 < len ? in[i+2] : 0);
        buf[o++] = _e_b64c[(b >> 18) & 0x3F];
        buf[o++] = _e_b64c[(b >> 12) & 0x3F];
        buf[o++] = (i+1 < len) ? _e_b64c[(b >> 6) & 0x3F] : '=';
        buf[o++] = (i+2 < len) ? _e_b64c[b & 0x3F] : '=';
    }
    buf[olen] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, olen};
}

static CnextString base64_decode(CnextString input) {
    if (!input.data) return (CnextString){NULL, 0};
    _e_b64_tab();
    size_t len = input.length;
    while (len > 0 && input.data[len-1] == '=') len--;
    size_t olen = (len / 4) * 3 + ((len % 4) * 3 / 4);
    char* buf = (char*)malloc(olen + 1);
    if (!buf) return (CnextString){NULL, 0};
    const uint8_t* in = (const uint8_t*)input.data;
    size_t o = 0;
    for (size_t i = 0; i < len; i += 4) {
        int c = 0, b = 0;
        for (int j = 0; j < 4 && i+j < len; j++) {
            int v = _e_b64d[in[i+j]];
            if (v < 0) continue;
            b = (b << 6) | v; c++;
        }
        if (c == 4) { buf[o++] = (b >> 16) & 0xFF; buf[o++] = (b >> 8) & 0xFF; buf[o++] = b & 0xFF; }
        else if (c == 3) { buf[o++] = (b >> 10) & 0xFF; buf[o++] = (b >> 2) & 0xFF; }
        else if (c == 2) { buf[o++] = (b >> 4) & 0xFF; }
    }
    buf[o] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, o};
}

static CnextString hex_encode(CnextString input) {
    if (!input.data) return (CnextString){NULL, 0};
    size_t len = input.length;
    char* buf = (char*)malloc(len * 2 + 1);
    if (!buf) return (CnextString){NULL, 0};
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        buf[i*2] = hex[(input.data[i] >> 4) & 0xF];
        buf[i*2+1] = hex[input.data[i] & 0xF];
    }
    buf[len*2] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, len * 2};
}

static CnextString hex_decode(CnextString input) {
    if (!input.data || input.length % 2 != 0) return (CnextString){NULL, 0};
    size_t len = input.length / 2;
    char* buf = (char*)malloc(len + 1);
    if (!buf) return (CnextString){NULL, 0};
    for (size_t i = 0; i < len; i++) {
        int hi = 0, lo = 0;
        char c = input.data[i*2];
        if (c >= '0' && c <= '9') hi = c - '0';
        else if (c >= 'a' && c <= 'f') hi = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') hi = c - 'A' + 10;
        c = input.data[i*2+1];
        if (c >= '0' && c <= '9') lo = c - '0';
        else if (c >= 'a' && c <= 'f') lo = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') lo = c - 'A' + 10;
        buf[i] = (char)((hi << 4) | lo);
    }
    buf[len] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, len};
}

static CnextString url_encode(CnextString input) {
    if (!input.data) return (CnextString){NULL, 0};
    size_t len = input.length;
    size_t cap = len * 3 + 1;
    char* buf = (char*)malloc(cap);
    if (!buf) return (CnextString){NULL, 0};
    size_t w = 0;
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)input.data[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            buf[w++] = (char)c;
        } else if (c == ' ') {
            buf[w++] = '+';
        } else {
            buf[w++] = '%';
            buf[w++] = hex[(c >> 4) & 0xF];
            buf[w++] = hex[c & 0xF];
        }
    }
    buf[w] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, w};
}

static CnextString url_decode(CnextString input) {
    if (!input.data) return (CnextString){NULL, 0};
    size_t len = input.length;
    char* buf = (char*)malloc(len + 1);
    if (!buf) return (CnextString){NULL, 0};
    size_t w = 0;
    for (size_t i = 0; i < len; i++) {
        char c = input.data[i];
        if (c == '%' && i + 2 < len) {
            int hi = 0, lo = 0;
            char h = input.data[i+1];
            if (h >= '0' && h <= '9') hi = h - '0';
            else if (h >= 'a' && h <= 'f') hi = h - 'a' + 10;
            else if (h >= 'A' && h <= 'F') hi = h - 'A' + 10;
            char l = input.data[i+2];
            if (l >= '0' && l <= '9') lo = l - '0';
            else if (l >= 'a' && l <= 'f') lo = l - 'a' + 10;
            else if (l >= 'A' && l <= 'F') lo = l - 'A' + 10;
            buf[w++] = (char)((hi << 4) | lo);
            i += 2;
        } else if (c == '+') {
            buf[w++] = ' ';
        } else {
            buf[w++] = c;
        }
    }
    buf[w] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, w};
}

#endif
