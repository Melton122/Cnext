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
 *
 * Design note: All functions are static inline because the compiler generates
 * a single .c file that is compiled directly to an executable. This avoids
 * the need for a separate runtime library link step. For multi-file projects,
 * consider extracting these into a runtime.c file and linking separately.
 *
 * Thread safety: The global arena/pool/ref tracking are process-wide statics.
 * Generated programs using threads should use per-thread arenas or protect
 * shared allocators with mutexes. This is acceptable for v1.0.
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

/* --- Arena Allocator --- */
// Fast bulk allocation: allocate from a pre-allocated block, free all at once.
#define CNEXT_ARENA_BLOCK_SIZE (64 * 1024)  // 64KB blocks

typedef struct _ArenaBlock {
    struct _ArenaBlock* next;
    size_t used;
    char data[CNEXT_ARENA_BLOCK_SIZE];
} _ArenaBlock;

typedef struct {
    _ArenaBlock* current;
    size_t total_allocated;
    size_t total_freed;
} CnextArena;

static CnextArena _cnext_global_arena = {NULL, 0, 0};

static void* cnext_arena_alloc(CnextArena* arena, size_t size) {
    // Align to 8 bytes
    size = (size + 7) & ~(size_t)7;

    if (size > CNEXT_ARENA_BLOCK_SIZE) {
        // Too large for arena, use regular malloc
        void* ptr = malloc(size);
        if (!ptr) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
        arena->total_allocated += size;
        return ptr;
    }

    if (!arena->current || arena->current->used + size > CNEXT_ARENA_BLOCK_SIZE) {
        _ArenaBlock* block = (_ArenaBlock*)malloc(sizeof(_ArenaBlock));
        if (!block) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
        block->next = arena->current;
        block->used = 0;
        arena->current = block;
    }

    void* ptr = arena->current->data + arena->current->used;
    arena->current->used += size;
    arena->total_allocated += size;
    return ptr;
}

static void cnext_arena_free_all(CnextArena* arena) {
    _ArenaBlock* block = arena->current;
    while (block) {
        _ArenaBlock* next = block->next;
        free(block);
        block = next;
    }
    arena->current = NULL;
    arena->total_freed = arena->total_allocated;
}

// Convenience macro for using the global arena
#define ARENA_ALLOC(size) cnext_arena_alloc(&_cnext_global_arena, size)
#define ARENA_FREE_ALL() cnext_arena_free_all(&_cnext_global_arena)

/* --- Memory Pool --- */
// Fixed-size block allocator for efficient allocation of many small objects.
#define CNEXT_POOL_BLOCK_SIZE 1024

typedef struct _PoolBlock {
    struct _PoolBlock* next;
    char data[CNEXT_POOL_BLOCK_SIZE];
    int used;
} _PoolBlock;

typedef struct {
    _PoolBlock* blocks;
    size_t total_allocated;
} CnextPool;

static CnextPool _cnext_global_pool = {NULL, 0};

static void* cnext_pool_alloc(CnextPool* pool, size_t size) {
    if (size > CNEXT_POOL_BLOCK_SIZE / 4) {
        // Too large for pool, use regular malloc
        return malloc(size);
    }

    // Align to 8 bytes
    size = (size + 7) & ~(size_t)7;

    if (!pool->blocks || pool->blocks->used + (int)size > CNEXT_POOL_BLOCK_SIZE) {
        _PoolBlock* block = (_PoolBlock*)malloc(sizeof(_PoolBlock));
        if (!block) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
        block->next = pool->blocks;
        block->used = 0;
        pool->blocks = block;
    }

    void* ptr = pool->blocks->data + pool->blocks->used;
    pool->blocks->used += size;
    pool->total_allocated += size;
    return ptr;
}

static void cnext_pool_free_all(CnextPool* pool) {
    _PoolBlock* block = pool->blocks;
    while (block) {
        _PoolBlock* next = block->next;
        free(block);
        block = next;
    }
    pool->blocks = NULL;
}

// Convenience macros
#define POOL_ALLOC(size) cnext_pool_alloc(&_cnext_global_pool, size)
#define POOL_FREE_ALL() cnext_pool_free_all(&_cnext_global_pool)

/* --- Reference Counting --- */
// Simple reference-counted pointer for shared ownership.
typedef struct {
    void* ptr;
    int refcount;
    void (*destructor)(void*);
} CnextRef;

static inline CnextRef cnext_ref_create(void* ptr, void (*destructor)(void*)) {
    CnextRef ref;
    ref.ptr = ptr;
    ref.refcount = 1;
    ref.destructor = destructor;
    _cnext_track(ptr);
    return ref;
}

static inline CnextRef cnext_ref_retain(CnextRef ref) {
    ref.refcount++;
    return ref;
}

static inline void cnext_ref_release(CnextRef* ref) {
    if (!ref || !ref->ptr) return;
    ref->refcount--;
    if (ref->refcount <= 0) {
        if (ref->destructor) ref->destructor(ref->ptr);
        else free(ref->ptr);
        _cnext_untrack(ref->ptr);
        ref->ptr = NULL;
        ref->refcount = 0;
    }
}

/* --- Memory Profiling --- */
static size_t _cnext_alloc_count = 0;
static size_t _cnext_free_count = 0;
static size_t _cnext_peak_bytes = 0;
static size_t _cnext_current_bytes = 0;

static void _cnext_profile_report(void) {
    fprintf(stderr, "\n=== Cnext Memory Profile ===\n");
    fprintf(stderr, "  Allocations:  %zu\n", _cnext_alloc_count);
    fprintf(stderr, "  Frees:        %zu\n", _cnext_free_count);
    fprintf(stderr, "  Peak usage:   %zu bytes\n", _cnext_peak_bytes);
    fprintf(stderr, "  Current:      %zu bytes\n", _cnext_current_bytes);
    fprintf(stderr, "  Arena total:  %zu bytes\n", _cnext_global_arena.total_allocated);
    if (_cnext_alloc_count > _cnext_free_count) {
        fprintf(stderr, "  WARNING: %zu possible leak(s)\n",
                _cnext_alloc_count - _cnext_free_count);
    }
    fprintf(stderr, "============================\n\n");
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
        fprintf(stderr, "\033[31m\033[1mUnhandled error:\033[0m %s\n",
                message.data ? message.data : "(null)");
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
static inline void printin_str(CnextString x) { _cnext_printin("%s", x.data ? x.data : "(null)"); }
static inline void printin_cstr(const char* x) { _cnext_printin("%s", x ? x : "(null)"); }
static inline void printin_bool(bool x) { _cnext_printin("%s", x ? "true" : "false"); }
static inline void printin_ptr(void* x) { _cnext_printin("%p", x); }
static inline void printin_char(char x) { _cnext_printin("%c", x); }

/* --- Closure Type --- */
typedef struct {
    void* fn;
    void* env;
    int refcount;
} CnextClosure;

static inline void cnext_closure_decref(CnextClosure* c) {
    if (!c) return;
    if (--c->refcount <= 0) {
        if (c->env) {
            free(c->env);
            c->env = NULL;
        }
        c->fn = NULL;
    }
}

static inline void printin_closure(CnextClosure x) { (void)x; _cnext_printin("<closure>"); }

/* --- Iterator Type (Generators) --- */
typedef struct {
    int _pc;
    bool done;
} CnextIterBase;

static inline void printin_iter(CnextIterBase x) { (void)x; _cnext_printin("<iterator>"); }

/* --- Tuple Type --- */
typedef struct {
    int count;
    CnextString repr;
} CnextTuple;

static inline void printin_tuple(CnextTuple x) { printf("%s\n", x.repr.data ? x.repr.data : "(null)"); }

/* --- print (no newline) variants --- */
static inline void _cnext_print(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
static inline void print_int(int x) { _cnext_print("%d", x); }
static inline void print_uint(unsigned int x) { _cnext_print("%u", x); }
static inline void print_long(long x) { _cnext_print("%ld", x); }
static inline void print_ulong(unsigned long x) { _cnext_print("%lu", x); }
static inline void print_llong(long long x) { _cnext_print("%lld", x); }
static inline void print_ullong(unsigned long long x) { _cnext_print("%llu", x); }
static inline void print_float(float x) { _cnext_print("%f", x); }
static inline void print_double(double x) { _cnext_print("%f", x); }
static inline void print_str(CnextString x) { _cnext_print("%s", x.data ? x.data : "(null)"); }
static inline void print_cstr(const char* x) { _cnext_print("%s", x ? x : "(null)"); }
static inline void print_bool(bool x) { _cnext_print("%s", x ? "true" : "false"); }
static inline void print_ptr(void* x) { _cnext_print("%p", x); }
static inline void print_char(char x) { _cnext_print("%c", x); }
static inline void print_closure(CnextClosure x) { (void)x; _cnext_print("<closure>"); }
static inline void print_iter(CnextIterBase x) { (void)x; _cnext_print("<iterator>"); }
static inline void print_tuple(CnextTuple x) { printf("%s", x.repr.data ? x.repr.data : "(null)"); }

#define print_raw(...) do { __auto_type _x = (__VA_ARGS__); _Generic((_x), \
    int: print_int, \
    unsigned int: print_uint, \
    short: print_int, \
    unsigned short: print_uint, \
    long: print_long, \
    unsigned long: print_ulong, \
    long long: print_llong, \
    unsigned long long: print_ullong, \
    float: print_float, \
    double: print_double, \
    CnextString: print_str, \
    CnextTuple: print_tuple, \
    char*: print_cstr, \
    const char*: print_cstr, \
    char: print_char, \
    unsigned char: print_uint, \
    bool: print_bool, \
    CnextClosure: print_closure, \
    CnextIterBase: print_iter, \
    default: print_ptr)(_x); } while(0)

#define printin(...) do { __auto_type _x = (__VA_ARGS__); _Generic((_x), \
    int: printin_int, \
    unsigned int: printin_uint, \
    short: printin_int, \
    unsigned short: printin_uint, \
    long: printin_long, \
    unsigned long: printin_ulong, \
    long long: printin_llong, \
    unsigned long long: printin_ullong, \
    float: printin_float, \
    double: printin_double, \
    CnextString: printin_str, \
    CnextTuple: printin_tuple, \
    char*: printin_cstr, \
    const char*: printin_cstr, \
    char: printin_char, \
    unsigned char: printin_uint, \
    bool: printin_bool, \
    CnextClosure: printin_closure, \
    CnextIterBase: printin_iter, \
    default: printin_ptr)(_x); } while(0)

/* --- Input --- */

static inline CnextString cnext_input(CnextString prompt) {
    if (prompt.data) printf("%s", prompt.data);
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
    if (!s1.data || !s2.data) {
        if (!s1.data && !s2.data) return (CnextString){NULL, 0};
        if (!s1.data) return s2;
        return s1;
    }
    size_t total = s1.length + s2.length;
    if (total < s1.length || total < s2.length) {
        fprintf(stderr, "Cnext runtime: string concatenation overflow.\n");
        exit(70);
    }
    char* result = (char*)malloc(total + 1);
    if (!result) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    memcpy(result, s1.data, s1.length);
    memcpy(result + s1.length, s2.data, s2.length);
    result[total] = '\0';
    _cnext_track(result);
    return (CnextString){result, total};
}

/* --- Null Safety --- */

static inline bool cnext_is_null_ptr(void* p) { return p == NULL; }
static inline bool cnext_is_null_cstr(const char* p) { return p == NULL; }
static inline bool cnext_is_null_str(CnextString s) { return s.data == NULL; }

static inline void* cnext_unwrap_ptr(void* p, const char* msg) {
    if (!p) { fprintf(stderr, "Unwrap failed: %s\n", msg ? msg : "value is null"); exit(1); }
    return p;
}

static inline CnextString cnext_unwrap_str(CnextString s, const char* msg) {
    if (!s.data) { fprintf(stderr, "Unwrap failed: %s\n", msg ? msg : "value is null"); exit(1); }
    return s;
}

static inline CnextString cnext_expect_str(CnextString s, const char* msg) {
    if (!s.data) { cnext_throw(msg ? (CnextString){(char*)msg, strlen(msg)} : (CnextString){(char*)"expect failed: value is null", 29}); }
    return s;
}

/* --- To-String Conversion --- */

static inline CnextString cnext_to_string_int(int x) { char* b = (char*)malloc(32); if (!b) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); } snprintf(b, 32, "%d", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_uint(unsigned int x) { char* b = (char*)malloc(32); if (!b) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); } snprintf(b, 32, "%u", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_long(long x) { char* b = (char*)malloc(32); if (!b) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); } snprintf(b, 32, "%ld", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_ulong(unsigned long x) { char* b = (char*)malloc(32); if (!b) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); } snprintf(b, 32, "%lu", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_llong(long long x) { char* b = (char*)malloc(32); if (!b) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); } snprintf(b, 32, "%lld", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_ullong(unsigned long long x) { char* b = (char*)malloc(32); if (!b) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); } snprintf(b, 32, "%llu", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_size_t(size_t x) { char* b = (char*)malloc(32); if (!b) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); } snprintf(b, 32, "%zu", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_float(float x) { char* b = (char*)malloc(64); if (!b) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); } snprintf(b, 64, "%f", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_double(double x) { char* b = (char*)malloc(64); if (!b) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); } snprintf(b, 64, "%f", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_str(CnextString x) { return x; }
static inline CnextString cnext_to_string_cstr(const char* x) { if (!x) return (CnextString){(char*)"", 0}; return (CnextString){(char*)x, strlen(x)}; }
static inline CnextString cnext_to_string_bool(bool x) { return x ? (CnextString){(char*)"true", 4} : (CnextString){(char*)"false", 5}; }
static inline CnextString cnext_to_string_ptr(void* x) { char* b = (char*)malloc(32); if (!b) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); } snprintf(b, 32, "%p", x); _cnext_track(b); return (CnextString){b, strlen(b)}; }
static inline CnextString cnext_to_string_char(char x) { char* b = (char*)malloc(2); if (!b) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); } b[0] = x; b[1] = '\0'; _cnext_track(b); return (CnextString){b, 1}; }

#define cnext_to_string(...) _Generic((__VA_ARGS__), \
    int: cnext_to_string_int, \
    unsigned int: cnext_to_string_uint, \
    short: cnext_to_string_int, \
    unsigned short: cnext_to_string_uint, \
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
    unsigned char: cnext_to_string_uint, \
    bool: cnext_to_string_bool, \
    default: cnext_to_string_ptr)(__VA_ARGS__)

/* --- String Equality --- */

static inline bool cnext_str_eq(CnextString a, CnextString b) {
    if (a.length != b.length) return false;
    if (a.data == NULL && b.data == NULL) return true;
    if (a.data == NULL || b.data == NULL) return false;
    return memcmp(a.data, b.data, a.length) == 0;
}

/* --- String Utility Functions --- */

static inline bool cnext_str_contains(CnextString s, CnextString sub) {
    if (sub.length > s.length) return false;
    if (sub.length == 0) return true;
    for (size_t i = 0; i <= s.length - sub.length; i++) {
        if (memcmp(s.data + i, sub.data, sub.length) == 0) return true;
    }
    return false;
}

static inline bool cnext_str_starts_with(CnextString s, CnextString prefix) {
    if (prefix.length > s.length) return false;
    return memcmp(s.data, prefix.data, prefix.length) == 0;
}

static inline bool cnext_str_ends_with(CnextString s, CnextString suffix) {
    if (suffix.length > s.length) return false;
    return memcmp(s.data + s.length - suffix.length, suffix.data, suffix.length) == 0;
}

static inline CnextString cnext_str_to_upper(CnextString s) {
    char* result = (char*)malloc(s.length + 1);
    if (!result) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    for (size_t i = 0; i < s.length; i++) {
        result[i] = (s.data[i] >= 'a' && s.data[i] <= 'z') ? s.data[i] - 32 : s.data[i];
    }
    result[s.length] = '\0';
    _cnext_track(result);
    return (CnextString){result, s.length};
}

static inline CnextString cnext_str_to_lower(CnextString s) {
    char* result = (char*)malloc(s.length + 1);
    if (!result) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    for (size_t i = 0; i < s.length; i++) {
        result[i] = (s.data[i] >= 'A' && s.data[i] <= 'Z') ? s.data[i] + 32 : s.data[i];
    }
    result[s.length] = '\0';
    _cnext_track(result);
    return (CnextString){result, s.length};
}

static inline CnextString cnext_str_trim(CnextString s) {
    size_t start = 0;
    while (start < s.length && (s.data[start] == ' ' || s.data[start] == '\t' || s.data[start] == '\n' || s.data[start] == '\r')) {
        start++;
    }
    size_t end = s.length;
    while (end > start && (s.data[end - 1] == ' ' || s.data[end - 1] == '\t' || s.data[end - 1] == '\n' || s.data[end - 1] == '\r')) {
        end--;
    }
    return (CnextString){s.data + start, end - start};
}

static inline CnextString cnext_str_sub(CnextString s, int start, int end) {
    if (start < 0) start = 0;
    if (end > (int)s.length) end = (int)s.length;
    if (start > end) return (CnextString){NULL, 0};
    return (CnextString){s.data + start, (size_t)(end - start)};
}

static inline int cnext_str_find(CnextString s, CnextString sub) {
    if (sub.length > s.length) return -1;
    if (sub.length == 0) return 0;
    for (size_t i = 0; i <= s.length - sub.length; i++) {
        if (memcmp(s.data + i, sub.data, sub.length) == 0) return (int)i;
    }
    return -1;
}

static inline CnextString cnext_str_replace(CnextString s, CnextString from, CnextString to) {
    if (!from.data || from.length == 0) return s;
    int pos = cnext_str_find(s, from);
    if (pos < 0) return s;

    size_t new_len = s.length - from.length + to.length;
    if (to.length > from.length && new_len < to.length) {
        fprintf(stderr, "Cnext runtime: string replace overflow.\n");
        exit(70);
    }
    char* result = (char*)malloc(new_len + 1);
    if (!result) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }

    memcpy(result, s.data, pos);
    if (to.data && to.length > 0) {
        memcpy(result + pos, to.data, to.length);
    }
    memcpy(result + pos + to.length, s.data + pos + from.length, s.length - pos - from.length);
    result[new_len] = '\0';
    _cnext_track(result);
    return (CnextString){result, new_len};
}

static inline int cnext_str_count(CnextString s, CnextString sub) {
    int count = 0;
    size_t i = 0;
    while (i <= s.length && sub.length > 0) {
        int pos = cnext_str_find((CnextString){s.data + i, s.length - i}, sub);
        if (pos < 0) break;
        count++;
        i += pos + sub.length;
    }
    return count;
}

static inline CnextString cnext_str_repeat(CnextString s, int count) {
    if (count <= 0 || !s.data) return (CnextString){(char*)"", 0};
    size_t total = (size_t)s.length * (size_t)count;
    if (s.length > 0 && total / (size_t)s.length != (size_t)count) {
        fprintf(stderr, "Cnext runtime: string repeat overflow.\n");
        exit(70);
    }
    char* result = (char*)malloc(total + 1);
    if (!result) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    for (int i = 0; i < count; i++) {
        memcpy(result + (size_t)i * s.length, s.data, s.length);
    }
    result[total] = '\0';
    _cnext_track(result);
    return (CnextString){result, total};
}

static inline bool cnext_str_is_empty(CnextString s) {
    return s.length == 0 || s.data == NULL;
}

/* --- String Split/Join --- */

typedef struct {
    CnextString* items;
    int length;
} CnextStringArray;

static inline CnextStringArray cnext_str_split(CnextString s, CnextString delim) {
    if (!s.data || s.length == 0) { CnextStringArray r = {NULL, 0}; return r; }
    if (!delim.data || delim.length == 0) {
        CnextStringArray r = (CnextStringArray){(CnextString*)malloc(sizeof(CnextString) * s.length), (int)s.length};
        for (size_t i = 0; i < s.length; i++) {
            r.items[i] = (CnextString){s.data + i, 1};
        }
        return r;
    }
    int cap = 8;
    int count = 0;
    CnextString* items = (CnextString*)malloc(sizeof(CnextString) * cap);
    size_t pos = 0;
    while (pos <= s.length) {
        int found = -1;
        if (pos + delim.length <= s.length) {
            for (size_t j = 0; j <= s.length - delim.length - pos; j++) {
                if (memcmp(s.data + pos + j, delim.data, delim.length) == 0) { found = (int)j; break; }
            }
        }
        if (found >= 0) {
            if (count >= cap) { cap *= 2; items = (CnextString*)realloc(items, sizeof(CnextString) * cap); }
            items[count++] = (CnextString){s.data + pos, (size_t)found};
            pos += found + delim.length;
        } else {
            if (count >= cap) { cap *= 2; items = (CnextString*)realloc(items, sizeof(CnextString) * cap); }
            items[count++] = (CnextString){s.data + pos, s.length - pos};
            break;
        }
    }
    _cnext_track(items);
    CnextStringArray r = {items, count};
    return r;
}

static inline CnextString cnext_str_join(CnextStringArray parts, CnextString delim) {
    if (parts.length == 0) return (CnextString){NULL, 0};
    if (parts.length == 1) return parts.items[0];
    size_t total = 0;
    for (int i = 0; i < parts.length; i++) { total += parts.items[i].length; }
    if (delim.length > 0) total += (size_t)(parts.length - 1) * delim.length;
    char* buf = (char*)malloc(total + 1);
    if (!buf) return (CnextString){NULL, 0};
    size_t w = 0;
    for (int i = 0; i < parts.length; i++) {
        if (i > 0 && delim.data) { memcpy(buf + w, delim.data, delim.length); w += delim.length; }
        if (parts.items[i].data) { memcpy(buf + w, parts.items[i].data, parts.items[i].length); w += parts.items[i].length; }
    }
    buf[w] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, w};
}

static inline CnextString cnext_str_reverse(CnextString s) {
    char* result = (char*)malloc(s.length + 1);
    if (!result) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    for (size_t i = 0; i < s.length; i++) {
        result[i] = s.data[s.length - 1 - i];
    }
    result[s.length] = '\0';
    _cnext_track(result);
    return (CnextString){result, s.length};
}

static inline int cnext_str_to_int(CnextString s) {
    if (!s.data || s.length == 0) return 0;
    char buf[64];
    size_t len = s.length < 63 ? s.length : 63;
    memcpy(buf, s.data, len);
    buf[len] = '\0';
    return atoi(buf);
}

static inline double cnext_str_to_double(CnextString s) {
    if (!s.data || s.length == 0) return 0.0;
    char buf[64];
    size_t len = s.length < 63 ? s.length : 63;
    memcpy(buf, s.data, len);
    buf[len] = '\0';
    return strtod(buf, NULL);
}

static inline float cnext_str_parse_float(CnextString s) {
    return (float)cnext_str_to_double(s);
}

static inline int cnext_str_char_at(CnextString s, int index) {
    if (!s.data || index < 0 || (size_t)index >= s.length) return -1;
    return (unsigned char)s.data[index];
}

static inline int cnext_str_index_of_char(CnextString s, char c) {
    if (!s.data) return -1;
    for (size_t i = 0; i < s.length; i++) {
        if (s.data[i] == c) return (int)i;
    }
    return -1;
}

static inline CnextString cnext_str_char_to_string(char c) {
    char* buf = (char*)malloc(2);
    if (!buf) return (CnextString){NULL, 0};
    buf[0] = c;
    buf[1] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, 1};
}

static inline CnextString cnext_int_to_string(int x) {
    char* buf = (char*)malloc(32);
    if (!buf) return (CnextString){NULL, 0};
    snprintf(buf, 32, "%d", x);
    _cnext_track(buf);
    return (CnextString){buf, strlen(buf)};
}

static inline CnextString cnext_float_to_string(float x) {
    char* buf = (char*)malloc(64);
    if (!buf) return (CnextString){NULL, 0};
    snprintf(buf, 64, "%f", x);
    _cnext_track(buf);
    return (CnextString){buf, strlen(buf)};
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

/* --- Threading --- */

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOGDI
#define NOGDI
#endif
#include <windows.h>
#ifdef ERROR
#undef ERROR
#endif
#ifdef WARNING
#undef WARNING
#endif
#ifdef OK
#undef OK
#endif
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#ifdef small
#undef small
#endif

typedef HANDLE CnextThread;
typedef CRITICAL_SECTION CnextMutex;

typedef struct {
    CnextString* buffer;
    int capacity;
    int head;
    int tail;
    int count;
    CRITICAL_SECTION mutex;
    HANDLE not_empty;
    HANDLE not_full;
} CnextChannel;

static inline CnextThread cnext_thread_spawn(void* (*func)(void*), void* arg) {
    HANDLE thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL);
    if (!thread) { fprintf(stderr, "Cnext: failed to create thread\n"); exit(1); }
    return thread;
}

static inline void cnext_thread_join(CnextThread t) { WaitForSingleObject(t, INFINITE); CloseHandle(t); }

static inline CnextMutex cnext_mutex_new(void) {
    CnextMutex m; InitializeCriticalSection(&m); return m;
}
static inline void cnext_mutex_lock(CnextMutex* m) { EnterCriticalSection(m); }
static inline void cnext_mutex_unlock(CnextMutex* m) { LeaveCriticalSection(m); }
static inline void cnext_mutex_free(CnextMutex* m) { DeleteCriticalSection(m); }

static inline CnextChannel cnext_channel_new(int capacity) {
    CnextChannel ch;
    ch.capacity = capacity > 0 ? capacity : 16;
    ch.buffer = (CnextString*)malloc(sizeof(CnextString) * ch.capacity);
    if (!ch.buffer) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    ch.head = 0; ch.tail = 0; ch.count = 0;
    InitializeCriticalSection(&ch.mutex);
    ch.not_empty = CreateEvent(NULL, FALSE, FALSE, NULL);
    ch.not_full = CreateEvent(NULL, FALSE, FALSE, NULL);
    return ch;
}

static inline void cnext_channel_send(CnextChannel* ch, CnextString msg) {
    EnterCriticalSection(&ch->mutex);
    while (ch->count >= ch->capacity) {
        LeaveCriticalSection(&ch->mutex);
        WaitForSingleObject(ch->not_full, INFINITE);
        EnterCriticalSection(&ch->mutex);
    }
    ch->buffer[ch->tail] = msg;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    SetEvent(ch->not_empty);
    LeaveCriticalSection(&ch->mutex);
}

static inline CnextString cnext_channel_recv(CnextChannel* ch) {
    EnterCriticalSection(&ch->mutex);
    while (ch->count <= 0) {
        LeaveCriticalSection(&ch->mutex);
        WaitForSingleObject(ch->not_empty, INFINITE);
        EnterCriticalSection(&ch->mutex);
    }
    CnextString msg = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    SetEvent(ch->not_full);
    LeaveCriticalSection(&ch->mutex);
    return msg;
}

static inline void cnext_channel_free(CnextChannel* ch) {
    free(ch->buffer);
    DeleteCriticalSection(&ch->mutex);
    CloseHandle(ch->not_empty);
    CloseHandle(ch->not_full);
}

#else /* POSIX */
#include <pthread.h>

typedef pthread_t CnextThread;
typedef pthread_mutex_t CnextMutex;

typedef struct {
    CnextString* buffer;
    int capacity;
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} CnextChannel;

static inline CnextThread cnext_thread_spawn(void* (*func)(void*), void* arg) {
    CnextThread t;
    if (pthread_create(&t, NULL, func, arg) != 0) {
        fprintf(stderr, "Cnext: failed to create thread\n"); exit(1);
    }
    return t;
}

static inline void cnext_thread_join(CnextThread t) { pthread_join(t, NULL); }

static inline CnextMutex cnext_mutex_new(void) {
    CnextMutex m; pthread_mutex_init(&m, NULL); return m;
}
static inline void cnext_mutex_lock(CnextMutex* m) { pthread_mutex_lock(m); }
static inline void cnext_mutex_unlock(CnextMutex* m) { pthread_mutex_unlock(m); }
static inline void cnext_mutex_free(CnextMutex* m) { pthread_mutex_destroy(m); }

static inline CnextChannel cnext_channel_new(int capacity) {
    CnextChannel ch;
    ch.capacity = capacity > 0 ? capacity : 16;
    ch.buffer = (CnextString*)malloc(sizeof(CnextString) * ch.capacity);
    if (!ch.buffer) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    ch.head = 0; ch.tail = 0; ch.count = 0;
    pthread_mutex_init(&ch.mutex, NULL);
    pthread_cond_init(&ch.not_empty, NULL);
    pthread_cond_init(&ch.not_full, NULL);
    return ch;
}

static inline void cnext_channel_send(CnextChannel* ch, CnextString msg) {
    pthread_mutex_lock(&ch->mutex);
    while (ch->count >= ch->capacity)
        pthread_cond_wait(&ch->not_full, &ch->mutex);
    ch->buffer[ch->tail] = msg;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->not_empty);
    pthread_mutex_unlock(&ch->mutex);
}

static inline CnextString cnext_channel_recv(CnextChannel* ch) {
    pthread_mutex_lock(&ch->mutex);
    while (ch->count <= 0)
        pthread_cond_wait(&ch->not_empty, &ch->mutex);
    CnextString msg = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->not_full);
    pthread_mutex_unlock(&ch->mutex);
    return msg;
}

static inline void cnext_channel_free(CnextChannel* ch) {
    free(ch->buffer);
    pthread_mutex_destroy(&ch->mutex);
    pthread_cond_destroy(&ch->not_empty);
    pthread_cond_destroy(&ch->not_full);
}

#endif /* _WIN32 */

#endif /* CNEXT_RUNTIME_H */
