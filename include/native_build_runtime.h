#ifndef CNEXT_NATIVE_BUILD_RUNTIME_H
#define CNEXT_NATIVE_BUILD_RUNTIME_H

#include "native_build.h"

/* Internal bridge for the self-hosted build driver. Values cannot contain
 * newlines, so a newline-delimited argv preserves every accepted byte. */
static CnextString cnext_native_manifest_args(CnextString path, bool required) {
    if (!path.length || path.length >= CNEXT_NATIVE_MAX_VALUE) {
        fprintf(stderr, "Invalid native manifest path.\n"); exit(64);
    }
    char* filename = (char*)malloc(path.length + 1);
    if (!filename) { fprintf(stderr, "Out of memory reading native manifest.\n"); exit(74); }
    if (!path.data || memchr(path.data, 0, path.length)) {
        free(filename); fprintf(stderr, "Invalid native manifest path.\n"); exit(64);
    }
    memcpy(filename, path.data, path.length);
    filename[path.length] = '\0';
    CnextNativeConfig config;
    bool ok = cnext_native_load(filename, required, &config);
    free(filename);
    if (!ok) { fprintf(stderr, "%s\n", config.error); exit(64); }
    size_t length = 0;
    for (int i = 0; i < config.count; i++) length += strlen(config.args[i]) + 1;
    char* text = (char*)malloc(length + 1);
    if (!text) { cnext_native_dispose(&config); fprintf(stderr, "Out of memory reading native manifest.\n"); exit(74); }
    size_t offset = 0;
    for (int i = 0; i < config.count; i++) {
        size_t count = strlen(config.args[i]);
        memcpy(text + offset, config.args[i], count);
        offset += count;
        text[offset++] = '\n';
    }
    if (offset) offset--;
    text[offset] = '\0';
    cnext_native_dispose(&config);
    _cnext_track(text);
    return (CnextString){text, offset};
}

#endif
