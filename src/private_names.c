#include "private_names.h"
#include "checked_alloc.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A small scanner stands in for the lexer: it only needs to know where
   identifiers, comments, string/character literals and braces are, and it must
   behave identically in the self-hosted compiler (src2/privatenames.cn). */

enum {
    SCAN_IDENT = 1,
    SCAN_PUNCT = 2,
    SCAN_SKIP = 3   /* comment, string, character literal or number */
};

typedef struct {
    int start;
    int end;
    int kind;
} ScanToken;

static bool is_ident_start(char c) {
    unsigned char u = (unsigned char)c;
    return isalpha(u) || c == '_';
}

static bool is_ident_char(char c) {
    unsigned char u = (unsigned char)c;
    return isalnum(u) || c == '_';
}

/* Skips whitespace and returns the end offset of the token that starts at
   `*start`. */
static int scan_token(const char* source, int i, int length, int* kind, int* start) {
    while (i < length && (source[i] == ' ' || source[i] == '\t' ||
                          source[i] == '\r' || source[i] == '\n')) {
        i++;
    }
    *start = i;
    if (i >= length) {
        *kind = SCAN_SKIP;
        return length;
    }

    char c = source[i];

    if (c == '/' && i + 1 < length) {
        if (source[i + 1] == '/') {
            int j = i + 2;
            while (j < length && source[j] != '\n') j++;
            *kind = SCAN_SKIP;
            return j;
        }
        if (source[i + 1] == '*') {
            int j = i + 2;
            while (j + 1 < length && !(source[j] == '*' && source[j + 1] == '/')) j++;
            if (j + 1 < length) j += 2;
            *kind = SCAN_SKIP;
            return j;
        }
    }

    if (c == '"' || c == '\'') {
        char quote = c;
        int j = i + 1;
        while (j < length) {
            if (source[j] == '\\' && j + 1 < length) { j += 2; continue; }
            if (source[j] == quote) { j++; break; }
            /* An unterminated literal stops at the end of the line so one bad
               file cannot swallow the rest of the source. */
            if (source[j] == '\n') break;
            j++;
        }
        *kind = SCAN_SKIP;
        return j;
    }

    if (isdigit((unsigned char)c)) {
        int j = i + 1;
        while (j < length) {
            char d = source[j];
            if (isalnum((unsigned char)d) || d == '.' || d == '_') { j++; continue; }
            break;
        }
        *kind = SCAN_SKIP;
        return j;
    }

    if (is_ident_start(c)) {
        int j = i + 1;
        while (j < length && is_ident_char(source[j])) j++;
        *kind = SCAN_IDENT;
        return j;
    }

    *kind = SCAN_PUNCT;
    return i + 1;
}

static bool token_is(const char* source, ScanToken token, const char* text) {
    size_t length = strlen(text);
    return (size_t)(token.end - token.start) == length &&
           strncmp(source + token.start, text, length) == 0;
}

static bool is_type_keyword(const char* source, ScanToken token) {
    return token_is(source, token, "int") ||
           token_is(source, token, "long") ||
           token_is(source, token, "float") ||
           token_is(source, token, "double") ||
           token_is(source, token, "str") ||
           token_is(source, token, "bool") ||
           token_is(source, token, "char") ||
           token_is(source, token, "byte") ||
           token_is(source, token, "uint") ||
           token_is(source, token, "ulong") ||
           token_is(source, token, "void") ||
           token_is(source, token, "var") ||
           token_is(source, token, "const");
}

/* A declaration may name its type first (`const int LIMIT = 3`), so the type
   token is not the declared name. */
static bool is_builtin_type_name(const char* source, ScanToken token) {
    return token_is(source, token, "int") ||
           token_is(source, token, "long") ||
           token_is(source, token, "float") ||
           token_is(source, token, "double") ||
           token_is(source, token, "str") ||
           token_is(source, token, "bool") ||
           token_is(source, token, "char") ||
           token_is(source, token, "byte") ||
           token_is(source, token, "uint") ||
           token_is(source, token, "ulong");
}

static bool is_declaration_keyword(const char* source, ScanToken token) {
    return token_is(source, token, "func") ||
           token_is(source, token, "class") ||
           token_is(source, token, "struct") ||
           token_is(source, token, "enum") ||
           token_is(source, token, "trait") ||
           token_is(source, token, "interface") ||
           token_is(source, token, "var") ||
           token_is(source, token, "const") ||
           token_is(source, token, "type");
}

typedef struct {
    int start;
    int end;
} Span;

typedef struct {
    Span* spans;
    int count;
    int capacity;
} SpanList;

static void span_list_add(SpanList* list, int start, int end) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity ? list->capacity * 2 : 8;
        list->spans = (Span*)checked_realloc(list->spans, sizeof(Span) * (size_t)list->capacity);
    }
    list->spans[list->count].start = start;
    list->spans[list->count].end = end;
    list->count++;
}

static bool span_list_contains(const SpanList* list, int start, int end) {
    for (int i = 0; i < list->count; i++) {
        if (list->spans[i].start == start && list->spans[i].end == end) return true;
    }
    return false;
}

/* True when `token` spells one of the names in `list` (used for renames: every
   occurrence of a private name in its file is rewritten). */
static bool span_list_has_name(const char* source, Span token, const SpanList* list) {
    for (int i = 0; i < list->count; i++) {
        const Span* span = &list->spans[i];
        if ((span->end - span->start) == (token.end - token.start) &&
            strncmp(source + span->start, source + token.start,
                    (size_t)(token.end - token.start)) == 0) {
            return true;
        }
    }
    return false;
}

/* Member accesses (`.name`) and attribute names (`@name`) keep their spelling:
   a field may legally share a name with a private top-level declaration. */
static bool is_member_or_attribute_name(const char* source, int start) {
    int j = start - 1;
    while (j >= 0) {
        char c = source[j];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { j--; continue; }
        return c == '.' || c == '@';
    }
    return false;
}

/* Collects the spans of `export` keywords and the names of private top-level
   declarations. */
static void scan_declarations(const char* source, int length,
                              SpanList* exports, SpanList* privates,
                              SpanList* shadows) {
    int depth = 0;
    int i = 0;
    bool pending_export = false;
    bool expect_name = false;
    bool expect_value_name = false;

    while (i < length) {
        int kind = 0;
        int start = 0;
        int end = scan_token(source, i, length, &kind, &start);
        ScanToken token = { start, end, kind };
        i = end;

        if (kind == SCAN_SKIP) {
            /* Literals and numbers are not declaration names. */
            expect_name = false;
            expect_value_name = false;
            continue;
        }

        if (kind == SCAN_IDENT && expect_value_name) {
            if (is_builtin_type_name(source, token)) continue;
            /* A member or method declaration. Member uses are written
               `x.name` and are never renamed, so the declaration must keep its
               spelling too. */
            span_list_add(shadows, token.start, token.end);
            expect_value_name = false;
            expect_name = false;
            pending_export = false;
            continue;
        }

        if (kind == SCAN_IDENT) {
            if (depth == 0 && token_is(source, token, "export")) {
                span_list_add(exports, token.start, token.end);
                pending_export = true;
                expect_name = false;
                continue;
            }
            if (depth == 0 && is_declaration_keyword(source, token)) {
                expect_name = true;
                continue;
            }
            if (depth == 1 && is_type_keyword(source, token)) {
                expect_value_name = true;
                expect_name = false;
                pending_export = false;
                continue;
            }
            if (depth == 1 && token_is(source, token, "func")) {
                /* Method declaration: calls are written `x.name()`. */
                expect_value_name = true;
                expect_name = false;
                pending_export = false;
                continue;
            }
            if (depth == 1 && (token_is(source, token, "var") || token_is(source, token, "const"))) {
                expect_value_name = true;
                expect_name = false;
                pending_export = false;
                continue;
            }
            if (expect_name) {
                if (is_builtin_type_name(source, token)) continue;
                if (!pending_export) span_list_add(privates, token.start, token.end);
                expect_name = false;
                pending_export = false;
                continue;
            }
            pending_export = false;
            continue;
        }

        /* Punctuation. */
        if (source[token.start] == '{') {
            depth++;
        } else if (source[token.start] == '}') {
            if (depth > 0) depth--;
        }
        expect_name = false;
        expect_value_name = false;
        pending_export = false;
    }
}

int source_has_local_import(const char* source) {
    if (!source) return 0;
    int length = (int)strlen(source);
    int i = 0;

    while (i < length) {
        int kind = 0;
        int start = 0;
        int end = scan_token(source, i, length, &kind, &start);
        ScanToken token = { start, end, kind };
        i = end;

        if (kind != SCAN_IDENT || !token_is(source, token, "import")) continue;

        /* The scanner would report the spec as a literal, so look at the raw
           characters that follow the keyword. */
        int j = token.end;
        while (j < length && (source[j] == ' ' || source[j] == '\t')) j++;
        if (j < length && source[j] == '"') return 1;
    }
    return 0;
}

static size_t append_mangled_name(char* out, size_t offset, const char* source,
                                  Span span, int module_tag) {
    size_t length = (size_t)(span.end - span.start);
    memcpy(out + offset, source + span.start, length);
    offset += length;
    offset += (size_t)snprintf(out + offset, 16, "__m%d", module_tag);
    return offset;
}

char* module_privatize_source(const char* source, int module_tag) {
    if (!source) return NULL;

    int length = (int)strlen(source);
    SpanList exports = {0};
    SpanList privates = {0};
    SpanList shadows = {0};
    scan_declarations(source, length, &exports, &privates, &shadows);

    if (exports.count == 0 && (module_tag < 0 || privates.count == 0)) {
        free(exports.spans);
        free(privates.spans);
        free(shadows.spans);
        char* copy = checked_strdup(source);
        return copy;
    }

    /* A rename adds at most `__m` plus a few tag digits to a token that is at
       least one byte long, so ten times the source length is always enough. */
    char* out = (char*)checked_malloc((size_t)length * 10 + 64);
    size_t offset = 0;
    int i = 0;

    while (i < length) {
        int kind = 0;
        int start = 0;
        int end = scan_token(source, i, length, &kind, &start);
        Span token = { start, end };

        /* Copy the whitespace between the previous token and this one. */
        size_t gap = (size_t)(token.start - i);
        memcpy(out + offset, source + i, gap);
        offset += gap;

        if (kind == SCAN_IDENT && span_list_contains(&exports, token.start, token.end)) {
            /* `export` becomes whitespace so every later column stays put. */
            memset(out + offset, ' ', (size_t)(token.end - token.start));
            offset += (size_t)(token.end - token.start);
        } else if (kind == SCAN_IDENT && module_tag >= 0 &&
                   !is_member_or_attribute_name(source, token.start) &&
                   span_list_has_name(source, token, &privates) &&
                   !span_list_contains(&shadows, token.start, token.end)) {
            offset = append_mangled_name(out, offset, source, token, module_tag);
        } else {
            memcpy(out + offset, source + i + gap, (size_t)(end - token.start));
            offset += (size_t)(end - token.start);
        }
        i = end;
    }
    out[offset] = '\0';

    free(exports.spans);
    free(privates.spans);
    free(shadows.spans);
    return out;
}
