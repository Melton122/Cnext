#ifndef CNEXT_CRYPTO_H
#define CNEXT_CRYPTO_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

static void _md5(const uint8_t* init, uint64_t len, uint8_t out[16]) {
    uint32_t h[4] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476};
    uint32_t k[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
    };
    uint32_t r[64] = {7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
    uint64_t blen = ((len + 8) / 64 + 1) * 64;
    uint8_t* buf = (uint8_t*)calloc(blen, 1);
    if (!buf) { memset(out, 0, 16); return; }
    memcpy(buf, init, len);
    buf[len] = 0x80;
    for (int i = 0; i < 8; i++) buf[blen - 8 + i] = (uint8_t)((len * 8) >> (i * 8));
    for (uint64_t off = 0; off < blen; off += 64) {
        uint32_t w[16], a = h[0], b = h[1], c = h[2], d = h[3];
        for (int i = 0; i < 16; i++) w[i] = (uint32_t)buf[off+i*4]|(uint32_t)buf[off+i*4+1]<<8|(uint32_t)buf[off+i*4+2]<<16|(uint32_t)buf[off+i*4+3]<<24;
        for (int i = 0; i < 64; i++) {
            uint32_t f, g;
            if (i < 16) { f = (b & c) | (~b & d); g = i; }
            else if (i < 32) { f = (d & b) | (~d & c); g = (5*i+1)%16; }
            else if (i < 48) { f = b ^ c ^ d; g = (3*i+5)%16; }
            else { f = c ^ (b | ~d); g = (7*i)%16; }
            uint32_t temp = d; d = c; c = b; b = b + ((a + f + k[i] + w[g]) << r[i] | (a + f + k[i] + w[g]) >> (32 - r[i])); a = temp;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    }
    for (int i = 0; i < 4; i++) { out[i*4] = h[i] & 0xFF; out[i*4+1] = (h[i] >> 8) & 0xFF; out[i*4+2] = (h[i] >> 16) & 0xFF; out[i*4+3] = (h[i] >> 24) & 0xFF; }
    free(buf);
}

static void _sha256(const uint8_t* data, uint64_t len, uint8_t out[32]) {
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint64_t blen = ((len + 9) / 64 + 1) * 64;
    uint8_t* buf = (uint8_t*)calloc(blen, 1);
    memcpy(buf, data, len);
    buf[len] = 0x80;
    for (int i = 0; i < 8; i++) buf[blen - 8 + i] = (uint8_t)((len * 8) >> (56 - i * 8));
    for (uint64_t off = 0; off < blen; off += 64) {
        uint32_t w[64], a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 16; i++) w[i] = (uint32_t)buf[off+i*4]<<24|(uint32_t)buf[off+i*4+1]<<16|(uint32_t)buf[off+i*4+2]<<8|(uint32_t)buf[off+i*4+3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = ((w[i-15]>>7)|(w[i-15]<<25)) ^ ((w[i-15]>>18)|(w[i-15]<<14)) ^ (w[i-15]>>3);
            uint32_t s1 = ((w[i-2]>>17)|(w[i-2]<<15)) ^ ((w[i-2]>>19)|(w[i-2]<<13)) ^ (w[i-2]>>10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = ((e>>6)|(e<<26)) ^ ((e>>11)|(e<<21)) ^ ((e>>25)|(e<<7));
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t temp1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = ((a>>2)|(a<<30)) ^ ((a>>13)|(a<<19)) ^ ((a>>22)|(a<<10));
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;
            hh = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    for (int i = 0; i < 8; i++) { out[i*4] = (h[i]>>24)&0xFF; out[i*4+1] = (h[i]>>16)&0xFF; out[i*4+2] = (h[i]>>8)&0xFF; out[i*4+3] = h[i]&0xFF; }
    free(buf);
}

static const char _b64c[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static signed char _b64d[256];
static int _b64_init = 0;
static void _b64_tab(void) {
    if (_b64_init) return; _b64_init = 1;
    memset(_b64d, -1, 256);
    for (int i = 0; i < 64; i++) _b64d[(unsigned char)_b64c[i]] = i;
}

static CnextString _hex_str(const uint8_t* hash, int len, bool upper) {
    static const char hex[] = "0123456789abcdef";
    static const char HEX[] = "0123456789ABCDEF";
    const char* tbl = upper ? HEX : hex;
    char* buf = (char*)malloc(len * 2 + 1);
    if (!buf) return (CnextString){NULL, 0};
    for (int i = 0; i < len; i++) {
        buf[i*2] = tbl[(hash[i] >> 4) & 0xF];
        buf[i*2+1] = tbl[hash[i] & 0xF];
    }
    buf[len*2] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, (size_t)(len*2)};
}

static CnextString crypto_md5(CnextString input) {
    if (!input.data) return (CnextString){NULL, 0};
    uint8_t hash[16];
    _md5((const uint8_t*)input.data, input.length, hash);
    return _hex_str(hash, 16, 0);
}

static CnextString crypto_sha256(CnextString input) {
    if (!input.data) return (CnextString){NULL, 0};
    uint8_t hash[32];
    _sha256((const uint8_t*)input.data, input.length, hash);
    return _hex_str(hash, 32, 0);
}

static CnextString crypto_base64_encode(CnextString input) {
    if (!input.data) return (CnextString){NULL, 0};
    size_t len = input.length;
    size_t olen = ((len + 2) / 3) * 4;
    char* buf = (char*)malloc(olen + 1);
    if (!buf) return (CnextString){NULL, 0};
    const uint8_t* in = (const uint8_t*)input.data;
    for (size_t i = 0, o = 0; i < len; i += 3) {
        int b = (int)in[i] << 16 | (i+1 < len ? (int)in[i+1] << 8 : 0) | (i+2 < len ? in[i+2] : 0);
        buf[o++] = _b64c[(b >> 18) & 0x3F];
        buf[o++] = _b64c[(b >> 12) & 0x3F];
        buf[o++] = (i+1 < len) ? _b64c[(b >> 6) & 0x3F] : '=';
        buf[o++] = (i+2 < len) ? _b64c[b & 0x3F] : '=';
    }
    buf[olen] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, olen};
}

static CnextString crypto_base64_decode(CnextString input) {
    if (!input.data) return (CnextString){NULL, 0};
    _b64_tab();
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
            int v = _b64d[in[i+j]];
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

#endif
