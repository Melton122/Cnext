#ifndef CNEXT_UUID_H
#define CNEXT_UUID_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

static inline CnextString uuid_v4(void) {
    uint8_t bytes[16];
#ifdef _WIN32
    HCRYPTPROV hProv;
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hProv, 16, bytes);
        CryptReleaseContext(hProv, 0);
    } else {
        srand((unsigned)time(NULL));
        for (int i = 0; i < 16; i++) bytes[i] = (uint8_t)(rand() & 0xFF);
    }
#else
    FILE* f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t nread = fread(bytes, 1, 16, f);
        (void)nread;
        fclose(f);
    } else {
        srand((unsigned)time(NULL));
        for (int i = 0; i < 16; i++) bytes[i] = (uint8_t)(rand() & 0xFF);
    }
#endif

    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    bytes[8] = (bytes[8] & 0x3F) | 0x80;

    static const char hex[] = "0123456789abcdef";
    char* buf = (char*)malloc(37);
    if (!buf) return (CnextString){NULL, 0};

    int pos = 0;
    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) buf[pos++] = '-';
        buf[pos++] = hex[(bytes[i] >> 4) & 0xF];
        buf[pos++] = hex[bytes[i] & 0xF];
    }
    buf[36] = '\0';

    _cnext_track(buf);
    return (CnextString){buf, 36};
}

static inline bool uuid_is_valid(CnextString uuid) {
    if (uuid.length != 36) return false;
    for (int i = 0; i < 36; i++) {
        char c = uuid.data[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') return false;
        } else {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
                return false;
        }
    }
    return true;
}

static inline CnextString uuid_v4_stripped(void) {
    CnextString full = uuid_v4();
    if (!full.data) return full;
    char* buf = (char*)malloc(33);
    if (!buf) { cnext_free(full); return (CnextString){NULL, 0}; }
    int pos = 0;
    for (int i = 0; i < 36; i++) {
        if (full.data[i] != '-') buf[pos++] = full.data[i];
    }
    buf[32] = '\0';
    cnext_free(full);
    _cnext_track(buf);
    return (CnextString){buf, 32};
}

#endif
