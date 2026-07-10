#ifndef CNEXT_RUNTIME_H
#define CNEXT_RUNTIME_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>

/* ========================================================================
 * Cnext Runtime Library
 * Automatically included by all generated C code from the Cnext compiler.
 * ======================================================================== */

/* --- String Type --- */
typedef struct {
    char* data;
    size_t length;
} CnextString;

/* --- Memory Tracking --- */
typedef struct _AllocNode {
    void* ptr;
    struct _AllocNode* next;
} _AllocNode;

static _AllocNode* _cnext_allocs = NULL;

static void _cnext_track(void* ptr) {
    if (!ptr) return;
    _AllocNode* node = (_AllocNode*)malloc(sizeof(_AllocNode));
    if (!node) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    node->ptr = ptr;
    node->next = _cnext_allocs;
    _cnext_allocs = node;
}

static void _cnext_untrack(void* ptr) {
    if (!ptr) return;
    _AllocNode** curr = &_cnext_allocs;
    while (*curr) {
        if ((*curr)->ptr == ptr) {
            _AllocNode* to_remove = *curr;
            *curr = (*curr)->next;
            free(to_remove);
            return;
        }
        curr = &(*curr)->next;
    }
}

static void _cnext_free_all() {
    while (_cnext_allocs) {
        _AllocNode* node = _cnext_allocs;
        _cnext_allocs = node->next;
        free(node->ptr);
        free(node);
    }
}

static inline void cnext_free(CnextString s) {
    if (!s.data) return;
    _cnext_untrack(s.data);
    free(s.data);
}

/* --- Error Handling (try/catch/throw via setjmp/longjmp) --- */

#define CNEXT_MAX_TRY_DEPTH 64

typedef struct {
    jmp_buf buf;
    bool active;
} _CnextTryFrame;

static _CnextTryFrame _cnext_try_stack[CNEXT_MAX_TRY_DEPTH];
static int _cnext_try_depth = 0;
static CnextString _cnext_error_message = {NULL, 0};

static inline void _cnext_push_try(void) {
    if (_cnext_try_depth >= CNEXT_MAX_TRY_DEPTH) {
        fprintf(stderr, "Cnext runtime: try/catch nesting too deep.\n");
        exit(70);
    }
    _cnext_try_stack[_cnext_try_depth].active = true;
    _cnext_try_depth++;
}

static inline void _cnext_pop_try(void) {
    if (_cnext_try_depth > 0) {
        _cnext_try_depth--;
        _cnext_try_stack[_cnext_try_depth].active = false;
    }
}

static inline _Noreturn void cnext_throw(CnextString message) {
    _cnext_error_message = message;
    if (_cnext_try_depth > 0) {
        _cnext_try_depth--;
        longjmp(_cnext_try_stack[_cnext_try_depth].buf, 1);
    } else {
        fprintf(stderr, "Unhandled error: %s\n", message.data ? message.data : "(null)");
        _cnext_free_all();
        exit(1);
    }
}

static inline void cnext_throw_cstr(const char* message) {
    cnext_throw((CnextString){(char*)message, strlen(message)});
}

/* --- Print Functions --- */

static inline void _cnext_printin(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

static inline void printin_int(int x) { _cnext_printin("%d", x); }
static inline void printin_uint(unsigned int x) { _cnext_printin("%u", x); }
static inline void printin_long(long x) { _cnext_printin("%ld", x); }
static inline void printin_ulong(unsigned long x) { _cnext_printin("%lu", x); }
static inline void printin_llong(long long x) { _cnext_printin("%lld", x); }
static inline void printin_ullong(unsigned long long x) { _cnext_printin("%llu", x); }
static inline void printin_float(float x) { _cnext_printin("%f", x); }
static inline void printin_double(double x) { _cnext_printin("%f", x); }
static inline void printin_str(CnextString x) { _cnext_printin("%s", x.data); }
static inline void printin_cstr(const char* x) { _cnext_printin("%s", x); }
static inline void printin_bool(bool x) { _cnext_printin("%s", x ? "true" : "false"); }
static inline void printin_ptr(void* x) { _cnext_printin("%p", x); }
static inline void printin_char(char x) { _cnext_printin("%c", x); }

#define printin(...) do { __auto_type _x = (__VA_ARGS__); _Generic((_x), \
    int: printin_int, \
    unsigned int: printin_uint, \
    long: printin_long, \
    unsigned long: printin_ulong, \
    long long: printin_llong, \
    unsigned long long: printin_ullong, \
    float: printin_float, \
    double: printin_double, \
    CnextString: printin_str, \
    char*: printin_cstr, \
    const char*: printin_cstr, \
    char: printin_char, \
    bool: printin_bool, \
    default: printin_ptr)(_x); } while(0)

/* --- Input --- */

static inline CnextString cnext_input(CnextString prompt) {
    printf("%s", prompt.data);
    char* buffer = (char*)malloc(256);
    if (!buffer) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    if (fgets(buffer, 256, stdin)) {
        buffer[strcspn(buffer, "\n")] = 0;
    } else {
        buffer[0] = '\0';
    }
    _cnext_track(buffer);
    return (CnextString){buffer, strlen(buffer)};
}

/* --- String Concatenation --- */

static inline CnextString cnext_concat(CnextString s1, CnextString s2) {
    char* result = (char*)malloc(s1.length + s2.length + 1);
    if (!result) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    memcpy(result, s1.data, s1.length);
    memcpy(result + s1.length, s2.data, s2.length);
    result[s1.length + s2.length] = '\0';
    _cnext_track(result);
    return (CnextString){result, s1.length + s2.length};
}

/* --- To-String Conversion --- */

static inline CnextString cnext_to_string_int(int x) { char* b = (char*)malloc(32); snprintf(b, 32, "%d", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_uint(unsigned int x) { char* b = (char*)malloc(32); snprintf(b, 32, "%u", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_long(long x) { char* b = (char*)malloc(32); snprintf(b, 32, "%ld", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_ulong(unsigned long x) { char* b = (char*)malloc(32); snprintf(b, 32, "%lu", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_llong(long long x) { char* b = (char*)malloc(32); snprintf(b, 32, "%lld", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_ullong(unsigned long long x) { char* b = (char*)malloc(32); snprintf(b, 32, "%llu", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_size_t(size_t x) { char* b = (char*)malloc(32); snprintf(b, 32, "%zu", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_float(float x) { char* b = (char*)malloc(64); snprintf(b, 64, "%f", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_double(double x) { char* b = (char*)malloc(64); snprintf(b, 64, "%f", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_str(CnextString x) { return x; }
static inline CnextString cnext_to_string_cstr(const char* x) { return (CnextString){(char*)x, strlen(x)}; }
static inline CnextString cnext_to_string_bool(bool x) { return x ? (CnextString){(char*)"true", 4} : (CnextString){(char*)"false", 5}; }
static inline CnextString cnext_to_string_ptr(void* x) { char* b = (char*)malloc(32); snprintf(b, 32, "%p", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_char(char x) { char* b = (char*)malloc(2); b[0] = x; b[1] = '\0'; _cnext_track(b); return (CnextString){b, 1}; }

#define cnext_to_string(...) _Generic((__VA_ARGS__), \
    int: cnext_to_string_int, \
    unsigned int: cnext_to_string_uint, \
    long: cnext_to_string_long, \
    unsigned long: cnext_to_string_ulong, \
    long long: cnext_to_string_llong, \
    unsigned long long: cnext_to_string_ullong, \
    float: cnext_to_string_float, \
    double: cnext_to_string_double, \
    CnextString: cnext_to_string_str, \
    char*: cnext_to_string_cstr, \
    const char*: cnext_to_string_cstr, \
    char: cnext_to_string_char, \
    bool: cnext_to_string_bool, \
    default: cnext_to_string_ptr)(__VA_ARGS__)

/* --- String Equality --- */

static inline bool cnext_str_eq(CnextString a, CnextString b) {
    if (a.length != b.length) return false;
    if (a.data == NULL && b.data == NULL) return true;
    if (a.data == NULL || b.data == NULL) return false;
    return memcmp(a.data, b.data, a.length) == 0;
}

/* --- Array Bounds Checking --- */

static inline size_t _cnext_array_check(size_t length, size_t index) {
    if (index >= length) {
        fprintf(stderr, "Array bounds out of range: index %zu >= length %zu\n", index, length);
        exit(139);
    }
    return index;
}

#define CNEXT_ARRAY_IDX(arr, idx) (*({ __auto_type _a = (arr); size_t _i = (idx); _cnext_array_check(_a.length, _i); &_a.data[_i]; }))

#define CNEXT_SLICE(arr, start, end) ({ \
    __auto_type _a = (arr); \
    size_t _s = (size_t)(start); \
    size_t _e = (size_t)(end); \
    if (_s > _e || _e > _a.length) { \
        fprintf(stderr, "Slice bounds out of range: [%zu:%zu] length %zu\n", _s, _e, _a.length); \
        exit(139); \
    } \
    typeof(_a) _r; \
    _r.data = _a.data + _s; \
    _r.length = _e - _s; \
    _r; \
})

#define CNEXT_SLICE_TO(dst, src, start, end) do { \
    __auto_type _d = &(dst); \
    __auto_type _s = (src); \
    size_t _st = (size_t)(start); \
    size_t _en = (size_t)(end); \
    if (_st > _en || _en > _s.length) { \
        fprintf(stderr, "Slice bounds out of range: [%zu:%zu] length %zu\n", _st, _en, _s.length); \
        exit(139); \
    } \
    _d->data = _s.data + _st; \
    _d->length = _en - _st; \
} while(0)

#endif /* CNEXT_RUNTIME_H */
