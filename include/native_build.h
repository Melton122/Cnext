#ifndef CNEXT_NATIVE_BUILD_H
#define CNEXT_NATIVE_BUILD_H

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CNEXT_NATIVE_MAX_TOKENS 512
#define CNEXT_NATIVE_MAX_ARGS 128
#define CNEXT_NATIVE_MAX_VALUE 4096
#define CNEXT_NATIVE_MAX_FILE (1024 * 1024)

typedef struct {
    char* args[CNEXT_NATIVE_MAX_TOKENS];
    int count;
    char error[512];
} CnextNativeConfig;

typedef struct {
    const char* path;
    const char* cursor;
    int line;
    CnextNativeConfig* result;
    CnextNativeConfig common;
    CnextNativeConfig target;
    int native_cost;
    int library_cost;
} CnextNativeParser;

static void cnext_native_dispose(CnextNativeConfig* config) {
    for (int i = 0; i < config->count; i++) free(config->args[i]);
    config->count = 0;
}

static bool cnext_native_error(CnextNativeParser* parser, const char* message) {
    if (!parser->result->error[0]) {
        snprintf(parser->result->error, sizeof(parser->result->error),
                 "%s:%d: %s", parser->path, parser->line, message);
    }
    return false;
}

static const char* cnext_native_platform(void) {
#ifdef _WIN32
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}

static void cnext_native_skip(CnextNativeParser* parser, bool lines) {
    for (;;) {
        char c = *parser->cursor;
        if (c == ' ' || c == '\t' || c == '\r') parser->cursor++;
        else if (lines && c == '\n') { parser->cursor++; parser->line++; }
        else if (lines && c == '#') {
            while (*parser->cursor && *parser->cursor != '\n') parser->cursor++;
        } else break;
    }
}

static bool cnext_native_finish_line(CnextNativeParser* parser) {
    cnext_native_skip(parser, false);
    if (*parser->cursor == '#') {
        while (*parser->cursor && *parser->cursor != '\n') parser->cursor++;
    }
    if (*parser->cursor && *parser->cursor != '\n')
        return cnext_native_error(parser, "Unexpected text after native setting.");
    return true;
}

static bool cnext_native_string(CnextNativeParser* parser, char* value) {
    char quote = *parser->cursor++;
    if (quote != '"' && quote != '\'') return cnext_native_error(parser, "Native arrays require quoted strings.");
    size_t length = 0;
    while (*parser->cursor && *parser->cursor != quote) {
        unsigned char c = (unsigned char)*parser->cursor++;
        if (c == '\\' && quote == '"') {
            c = (unsigned char)*parser->cursor++;
            if (c != '\\' && c != '"')
                return cnext_native_error(parser, "Native strings support only escaped quotes and backslashes; use single quotes for literal paths.");
        }
        if (c < 32 || c == 127) return cnext_native_error(parser, "Control characters are not allowed in native values.");
        if (length + 1 >= CNEXT_NATIVE_MAX_VALUE) return cnext_native_error(parser, "Native value is too long.");
        value[length++] = (char)c;
    }
    if (*parser->cursor != quote) return cnext_native_error(parser, "Unterminated native string.");
    parser->cursor++;
    value[length] = '\0';
    if (!length) return cnext_native_error(parser, "Native values must not be empty.");
    return true;
}

static bool cnext_native_append(CnextNativeParser* parser, CnextNativeConfig* list, const char* text) {
    if (list->count >= CNEXT_NATIVE_MAX_TOKENS) return cnext_native_error(parser, "Too many native settings.");
    size_t length = strlen(text);
    char* copy = (char*)malloc(length + 1);
    if (!copy) return cnext_native_error(parser, "Out of memory reading native settings.");
    memcpy(copy, text, length + 1);
    list->args[list->count++] = copy;
    return true;
}

static bool cnext_native_value(CnextNativeParser* parser, int section, int key, bool active, const char* value) {
    static const char* options[] = {"--c-source", "--object", "--include-dir", "--library-dir", "--link-lib"};
    char resolved[CNEXT_NATIVE_MAX_VALUE * 2];
    const char* argument = value;
    if (key == 4) {
        if (value[0] == '-') return cnext_native_error(parser, "Library names must not start with '-'.");
        for (const unsigned char* p = (const unsigned char*)value; *p; p++) {
            if (!isalnum(*p) && *p != '_' && *p != '-' && *p != '.' && *p != '+')
                return cnext_native_error(parser, "Invalid native library name.");
        }
    } else {
        bool drive = isalpha((unsigned char)value[0]) && value[1] == ':';
        if (drive && value[2] != '/' && value[2] != '\\')
            return cnext_native_error(parser, "Drive-relative native paths are not supported.");
        if (value[0] != '/' && value[0] != '\\' && !drive) {
            const char* slash = strrchr(parser->path, '/');
            const char* backslash = strrchr(parser->path, '\\');
            if (backslash && (!slash || backslash > slash)) slash = backslash;
            bool absolute_manifest = parser->path[0] == '/' || parser->path[0] == '\\' ||
                (isalpha((unsigned char)parser->path[0]) && parser->path[1] == ':');
            int written = slash ? snprintf(resolved, sizeof(resolved), "%s%.*s%s", absolute_manifest ? "" : "./",
                                            (int)(slash - parser->path + 1), parser->path, value)
                                : snprintf(resolved, sizeof(resolved), "./%s", value);
            if (written < 0 || (size_t)written >= sizeof(resolved)) return cnext_native_error(parser, "Resolved native path is too long.");
            argument = resolved;
        }
    }
    if (!active) return true;
    int cost = key == 0 || key == 1 ? 1 : 2;
    int* group_cost = key == 4 ? &parser->library_cost : &parser->native_cost;
    if (*group_cost + cost > CNEXT_NATIVE_MAX_ARGS) return cnext_native_error(parser, "Too many native build arguments (maximum 128 per group).");
    *group_cost += cost;
    CnextNativeConfig* list = section == 1 ? &parser->common : &parser->target;
    return cnext_native_append(parser, list, options[key]) && cnext_native_append(parser, list, argument);
}

static bool cnext_native_array(CnextNativeParser* parser, int section, int key, bool active) {
    cnext_native_skip(parser, false);
    if (*parser->cursor++ != '[') return cnext_native_error(parser, "Native settings must be arrays of quoted strings.");
    cnext_native_skip(parser, true);
    if (*parser->cursor == ']') { parser->cursor++; return cnext_native_finish_line(parser); }
    for (;;) {
        char value[CNEXT_NATIVE_MAX_VALUE];
        if (!cnext_native_string(parser, value) || !cnext_native_value(parser, section, key, active, value)) return false;
        cnext_native_skip(parser, true);
        if (*parser->cursor == ']') { parser->cursor++; return cnext_native_finish_line(parser); }
        if (*parser->cursor++ != ',') return cnext_native_error(parser, "Expected ',' or ']' in native array.");
        cnext_native_skip(parser, true);
        if (*parser->cursor == ']') { parser->cursor++; return cnext_native_finish_line(parser); }
    }
}

static bool cnext_native_parse(CnextNativeParser* parser) {
    static const char* sections[] = {"", "native", "native.windows", "native.linux", "native.macos"};
    static const char* keys[] = {"c_sources", "objects", "include_dirs", "library_dirs", "libraries"};
    unsigned seen_sections = 0;
    unsigned seen_keys[5] = {0};
    int section = 0;
    bool active = false;
    while (*parser->cursor) {
        cnext_native_skip(parser, true);
        if (!*parser->cursor) break;
        if (*parser->cursor == '[') {
            const char* start = ++parser->cursor;
            while (*parser->cursor && *parser->cursor != ']' && *parser->cursor != '\n') parser->cursor++;
            const char* end = parser->cursor;
            while (start < end && (*start == ' ' || *start == '\t')) start++;
            while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
            if (end - start >= 8 && (*start == '\'' || *start == '"') && !strncmp(start + 1, "native", 6))
                return cnext_native_error(parser, "Quoted native table names are not supported.");
            section = 0;
            for (int i = 1; i < 5; i++) {
                if ((size_t)(end - start) == strlen(sections[i]) && !strncmp(start, sections[i], (size_t)(end - start))) section = i;
            }
            if (!section && end - start >= 7 && !strncmp(start, "native.", 7))
                return cnext_native_error(parser, "Unsupported native table; use native, native.windows, native.linux, or native.macos.");
            if (*parser->cursor != ']') {
                if (section) return cnext_native_error(parser, "Unterminated native table.");
            } else parser->cursor++;
            if (section) {
                if (seen_sections & (1u << section)) return cnext_native_error(parser, "Duplicate native table.");
                seen_sections |= 1u << section;
                if (!cnext_native_finish_line(parser)) return false;
            }
            active = section == 1 || (section > 1 && !strcmp(sections[section] + 7, cnext_native_platform()));
        } else if (section) {
            const char* start = parser->cursor;
            while (isalnum((unsigned char)*parser->cursor) || *parser->cursor == '_') parser->cursor++;
            size_t length = (size_t)(parser->cursor - start);
            int key = -1;
            for (int i = 0; i < 5; i++) if (length == strlen(keys[i]) && !strncmp(start, keys[i], length)) key = i;
            if (key < 0) return cnext_native_error(parser, "Unknown native setting.");
            if (seen_keys[section] & (1u << key)) return cnext_native_error(parser, "Duplicate native setting.");
            seen_keys[section] |= 1u << key;
            cnext_native_skip(parser, false);
            if (*parser->cursor++ != '=') return cnext_native_error(parser, "Expected '=' after native key.");
            if (!cnext_native_array(parser, section, key, active)) return false;
        }
        while (*parser->cursor && *parser->cursor != '\n') parser->cursor++;
    }
    // Base settings always precede platform settings, independent of table order.
    CnextNativeConfig* lists[] = {&parser->common, &parser->target};
    for (int library = 0; library < 2; library++) {
        for (int group = 0; group < 2; group++) {
            CnextNativeConfig* list = lists[group];
            for (int i = 0; i < list->count; i += 2) {
                bool is_library = !strcmp(list->args[i], "--link-lib");
                if (is_library != (library != 0)) continue;
                if (!cnext_native_append(parser, parser->result, list->args[i]) ||
                    !cnext_native_append(parser, parser->result, list->args[i + 1])) return false;
            }
        }
    }
    return true;
}

static bool cnext_native_load(const char* path, bool required, CnextNativeConfig* result) {
    memset(result, 0, sizeof(*result));
    if (!path || !path[0] || strlen(path) >= CNEXT_NATIVE_MAX_VALUE) {
        snprintf(result->error, sizeof(result->error), "Invalid native manifest path.");
        return false;
    }
    for (const unsigned char* p = (const unsigned char*)path; *p; p++) {
        if (*p < 32 || *p == 127) {
            snprintf(result->error, sizeof(result->error), "Invalid native manifest path: control characters are not allowed.");
            return false;
        }
    }
    if (isalpha((unsigned char)path[0]) && path[1] == ':' && path[2] != '/' && path[2] != '\\') {
        snprintf(result->error, sizeof(result->error), "Invalid native manifest path: drive-relative paths are not supported.");
        return false;
    }
    FILE* file = fopen(path, "rb");
    if (!file) {
        if (!required && errno == ENOENT) return true;
        snprintf(result->error, sizeof(result->error), "%s: Could not read native manifest.", path);
        return false;
    }
    char* content = (char*)malloc(CNEXT_NATIVE_MAX_FILE + 1);
    if (!content) { fclose(file); snprintf(result->error, sizeof(result->error), "Out of memory reading native manifest."); return false; }
    size_t length = fread(content, 1, CNEXT_NATIVE_MAX_FILE, file);
    bool readable = !ferror(file) && (length < CNEXT_NATIVE_MAX_FILE || fgetc(file) == EOF);
    fclose(file);
    content[length] = '\0';
    if (!readable || memchr(content, 0, length)) {
        free(content);
        snprintf(result->error, sizeof(result->error), "%s: Native manifest is too large, unreadable, or contains zero bytes.", path);
        return false;
    }
    CnextNativeParser parser = {0};
    parser.path = path;
    parser.cursor = content;
    if (length >= 3 && (unsigned char)content[0] == 0xef && (unsigned char)content[1] == 0xbb && (unsigned char)content[2] == 0xbf)
        parser.cursor += 3;
    parser.line = 1;
    parser.result = result;
    bool ok = cnext_native_parse(&parser);
    cnext_native_dispose(&parser.common);
    cnext_native_dispose(&parser.target);
    free(content);
    if (!ok) cnext_native_dispose(result);
    return ok;
}

#endif
