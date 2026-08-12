#ifndef CNEXT_RUNTIME_H
#define CNEXT_RUNTIME_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include <math.h>
#include <time.h>
#include <stdint.h>

/* ========================================================================
 * Cnext Runtime Library
 * Automatically included by all generated C code from the Cnext compiler.
 *
 * Design note: All functions are static inline because the compiler generates
 * a single .c file that is compiled directly to an executable. This avoids
 * the need for a separate runtime library link step. For multi-file projects,
 * consider extracting these into a runtime.c file and linking separately.
 *
 * Thread safety: The global tracking list, arena, and pool are process-wide
 * statics guarded by an internal lock, so generated multithreaded programs may
 * allocate, track, and release memory from any thread safely.
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

/* Internal lock protecting the tracking list, global arena, and global pool.
 * C11 atomic spinlock: works on GCC/Clang everywhere without pulling in
 * platform headers (which would leak winapi/pthread identifiers into the
 * namespaces of generated programs, e.g. GDI's "Rectangle"). */
#include <stdatomic.h>
static atomic_flag _cnext_mem_lock = ATOMIC_FLAG_INIT;
static void _cnext_mem_lock_acquire(void) {
    while (atomic_flag_test_and_set_explicit(&_cnext_mem_lock, memory_order_acquire)) {
    }
}
static void _cnext_mem_lock_release(void) {
    atomic_flag_clear_explicit(&_cnext_mem_lock, memory_order_release);
}

static void _cnext_track(void* ptr) {
    if (!ptr) return;
    _AllocNode* node = (_AllocNode*)malloc(sizeof(_AllocNode));
    if (!node) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    node->ptr = ptr;
    _cnext_mem_lock_acquire();
    node->next = _cnext_allocs;
    _cnext_allocs = node;
    _cnext_mem_lock_release();
}

static void _cnext_untrack(void* ptr) {
    if (!ptr) return;
    _cnext_mem_lock_acquire();
    _AllocNode** curr = &_cnext_allocs;
    while (*curr) {
        if ((*curr)->ptr == ptr) {
            _AllocNode* to_remove = *curr;
            *curr = (*curr)->next;
            free(to_remove);
            _cnext_mem_lock_release();
            return;
        }
        curr = &(*curr)->next;
    }
    _cnext_mem_lock_release();
}

static void _cnext_free_all() {
    _cnext_mem_lock_acquire();
    while (_cnext_allocs) {
        _AllocNode* node = _cnext_allocs;
        _cnext_allocs = node->next;
        free(node->ptr);
        free(node);
    }
    _cnext_mem_lock_release();
}

#include "json.h"

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
        _cnext_mem_lock_acquire();
        arena->total_allocated += size;
        _cnext_mem_lock_release();
        return ptr;
    }

    _cnext_mem_lock_acquire();
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
    _cnext_mem_lock_release();
    return ptr;
}

static void cnext_arena_free_all(CnextArena* arena) {
    _cnext_mem_lock_acquire();
    _ArenaBlock* block = arena->current;
    while (block) {
        _ArenaBlock* next = block->next;
        free(block);
        block = next;
    }
    arena->current = NULL;
    arena->total_freed = arena->total_allocated;
    _cnext_mem_lock_release();
}

/* --- Arena API (exposed to the language) --- */
// Heap-allocated arenas give deterministic reclamation during program run:
// every allocation made from an arena is freed in one call, unlike the
// process-wide tracked allocations that live until exit. Idiomatic use:
//   var a = mem_arena_create(); defer mem_arena_destroy(a);
static CnextArena* cnext_mem_arena_create(void) {
    CnextArena* arena = (CnextArena*)malloc(sizeof(CnextArena));
    if (!arena) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    arena->current = NULL;
    arena->total_allocated = 0;
    arena->total_freed = 0;
    return arena;
}

static void* cnext_mem_arena_alloc(CnextArena* arena, size_t size) {
    return cnext_arena_alloc(arena, size);
}

static void cnext_mem_arena_free(CnextArena* arena) {
    if (!arena) return;
    cnext_arena_free_all(arena);
}

static void cnext_mem_arena_destroy(CnextArena* arena) {
    if (!arena) return;
    cnext_arena_free_all(arena);
    free(arena);
}

static size_t cnext_mem_arena_usage(CnextArena* arena) {
    if (!arena) return 0;
    return arena->total_allocated > arena->total_freed
        ? arena->total_allocated - arena->total_freed
        : 0;
}

/* --- Scoped Memory (automatic, thread-local) --- */
// Inside mem_scope_begin()/mem_scope_end(), every `new` allocates from the
// thread's innermost scope arena instead of the tracked heap, so entire object
// graphs die deterministically when the scope ends. Scoped objects have no
// destructor call and must not outlive their scope.
typedef struct _CnextScopeEntry {
    struct _CnextScopeEntry* prev;
    CnextArena* arena;
} _CnextScopeEntry;

static _Thread_local _CnextScopeEntry* _cnext_scope_head = NULL;
static _Thread_local CnextArena* _cnext_cur_arena = NULL;

static void cnext_mem_scope_begin(void) {
    _CnextScopeEntry* e = (_CnextScopeEntry*)malloc(sizeof(_CnextScopeEntry));
    if (!e) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    e->arena = cnext_mem_arena_create();
    e->prev = _cnext_scope_head;
    _cnext_scope_head = e;
    _cnext_cur_arena = e->arena;
}

static void cnext_mem_scope_end(void) {
    if (!_cnext_scope_head) return;
    _CnextScopeEntry* e = _cnext_scope_head;
    _cnext_scope_head = e->prev;
    _cnext_cur_arena = _cnext_scope_head ? _cnext_scope_head->arena : NULL;
    cnext_mem_arena_destroy(e->arena);
    free(e);
}

static size_t cnext_mem_scope_usage(void) {
    return _cnext_cur_arena ? cnext_mem_arena_usage(_cnext_cur_arena) : 0;
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

    _cnext_mem_lock_acquire();
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
    _cnext_mem_lock_release();
    return ptr;
}

static void cnext_pool_free_all(CnextPool* pool) {
    _cnext_mem_lock_acquire();
    _PoolBlock* block = pool->blocks;
    while (block) {
        _PoolBlock* next = block->next;
        free(block);
        block = next;
    }
    pool->blocks = NULL;
    _cnext_mem_lock_release();
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

/* --- Reference handles (exposed to the language) --- */
// Heap-allocated refcount handles let `new` objects die deterministically when
// the last reference is released (instead of at exit). Both the handle and the
// wrapped object are tracked, so forgetting to release cannot leak past exit
// (and the object is freed exactly once, never double-freed). Once the count
// reaches zero the handle is freed and becomes invalid — using it further is
// undefined. Per-handle retain/release must not race across threads without a
// user mutex.
static void* cnext_ref_new(void* obj) {
    if (!obj) return NULL;
    CnextRef* h = (CnextRef*)malloc(sizeof(CnextRef));
    if (!h) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    h->ptr = obj;
    h->refcount = 1;
    h->destructor = NULL;
    _cnext_track(h);
    return h;
}

static void cnext_ref_handle_retain(void* handle) {
    CnextRef* h = (CnextRef*)handle;
    if (!h) return;
    h->refcount++;
}

static void cnext_ref_handle_release(void* handle) {
    CnextRef* h = (CnextRef*)handle;
    if (!h || h->refcount <= 0) return;
    h->refcount--;
    if (h->refcount == 0) {
        _cnext_untrack(h->ptr);
        free(h->ptr);
        _cnext_untrack(h);
        free(h);
    }
}

static int cnext_ref_handle_count(void* handle) {
    CnextRef* h = (CnextRef*)handle;
    return h ? h->refcount : 0;
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
            if (count >= cap) { cap *= 2; _cnext_untrack(items); items = (CnextString*)realloc(items, sizeof(CnextString) * cap); _cnext_track(items); }
            items[count++] = (CnextString){s.data + pos, (size_t)found};
            pos += found + delim.length;
        } else {
            if (count >= cap) { cap *= 2; _cnext_untrack(items); items = (CnextString*)realloc(items, sizeof(CnextString) * cap); _cnext_track(items); }
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

/* ========================================================================
 * New Built-in Functions (Global, no import required)
 * ======================================================================== */

/* --- String Builtins --- */

static inline CnextString cnext_str_capitalize(CnextString s) {
    if (!s.data || s.length == 0) return s;
    char* result = (char*)malloc(s.length + 1);
    if (!result) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    memcpy(result, s.data, s.length);
    if (result[0] >= 'a' && result[0] <= 'z') result[0] -= 32;
    result[s.length] = '\0';
    _cnext_track(result);
    return (CnextString){result, s.length};
}

static inline CnextString cnext_str_title(CnextString s) {
    if (!s.data || s.length == 0) return s;
    char* result = (char*)malloc(s.length + 1);
    if (!result) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    memcpy(result, s.data, s.length);
    bool new_word = true;
    for (size_t i = 0; i < s.length; i++) {
        if (s.data[i] == ' ' || s.data[i] == '\t' || s.data[i] == '\n') {
            new_word = true;
        } else if (new_word) {
            if (result[i] >= 'a' && result[i] <= 'z') result[i] -= 32;
            new_word = false;
        }
    }
    result[s.length] = '\0';
    _cnext_track(result);
    return (CnextString){result, s.length};
}

static inline CnextString cnext_str_pad_left(CnextString s, int width, char pad_char) {
    if (!s.data || (int)s.length >= width) return s;
    int pad = width - (int)s.length;
    char* result = (char*)malloc((size_t)width + 1);
    if (!result) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    for (int i = 0; i < pad; i++) result[i] = pad_char;
    memcpy(result + pad, s.data, s.length);
    result[width] = '\0';
    _cnext_track(result);
    return (CnextString){result, (size_t)width};
}

static inline CnextString cnext_str_pad_right(CnextString s, int width, char pad_char) {
    if (!s.data || (int)s.length >= width) return s;
    int pad = width - (int)s.length;
    char* result = (char*)malloc((size_t)width + 1);
    if (!result) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    memcpy(result, s.data, s.length);
    for (int i = 0; i < pad; i++) result[s.length + i] = pad_char;
    result[width] = '\0';
    _cnext_track(result);
    return (CnextString){result, (size_t)width};
}

static inline int cnext_str_last_index_of(CnextString s, CnextString sub) {
    if (!s.data || !sub.data || sub.length == 0 || sub.length > s.length) return -1;
    for (int i = (int)(s.length - sub.length); i >= 0; i--) {
        if (memcmp(s.data + i, sub.data, sub.length) == 0) return i;
    }
    return -1;
}

static inline CnextString cnext_str_remove(CnextString s, CnextString sub) {
    if (!s.data || !sub.data || sub.length == 0) return s;
    int pos = cnext_str_find(s, sub);
    if (pos < 0) return s;
    size_t new_len = s.length - sub.length;
    char* result = (char*)malloc(new_len + 1);
    if (!result) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    memcpy(result, s.data, pos);
    memcpy(result + pos, s.data + pos + sub.length, s.length - pos - sub.length);
    result[new_len] = '\0';
    _cnext_track(result);
    return (CnextString){result, new_len};
}

static inline CnextString cnext_str_insert(CnextString s, int pos, CnextString sub) {
    if (!s.data || !sub.data || pos < 0 || (size_t)pos > s.length) return s;
    size_t new_len = s.length + sub.length;
    char* result = (char*)malloc(new_len + 1);
    if (!result) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    memcpy(result, s.data, pos);
    memcpy(result + pos, sub.data, sub.length);
    memcpy(result + pos + sub.length, s.data + pos, s.length - pos);
    result[new_len] = '\0';
    _cnext_track(result);
    return (CnextString){result, new_len};
}

/* --- Type Conversion Builtins --- */

static inline int cnext_to_int_val(int x) { return x; }
static inline int cnext_to_int_from_float(float x) { return (int)x; }
static inline int cnext_to_int_from_double(double x) { return (int)x; }
static inline int cnext_to_int_from_str(CnextString s) { return cnext_str_to_int(s); }
static inline int cnext_to_int_from_bool(bool x) { return x ? 1 : 0; }
static inline int cnext_to_int_from_char(char x) { return (int)(unsigned char)x; }

#define cnext_to_int(...) _Generic((__VA_ARGS__), \
    int: cnext_to_int_val, \
    float: cnext_to_int_from_float, \
    double: cnext_to_int_from_double, \
    CnextString: cnext_to_int_from_str, \
    bool: cnext_to_int_from_bool, \
    char: cnext_to_int_from_char, \
    default: cnext_to_int_val)(__VA_ARGS__)

static inline float cnext_to_float_from_int(int x) { return (float)x; }
static inline float cnext_to_float_val(float x) { return x; }
static inline float cnext_to_float_from_double(double x) { return (float)x; }
static inline float cnext_to_float_from_str(CnextString s) { return cnext_str_parse_float(s); }

#define cnext_to_float(...) _Generic((__VA_ARGS__), \
    int: cnext_to_float_from_int, \
    float: cnext_to_float_val, \
    double: cnext_to_float_from_double, \
    CnextString: cnext_to_float_from_str, \
    default: cnext_to_float_from_int)(__VA_ARGS__)

static inline bool cnext_to_bool_from_int(int x) { return x != 0; }
static inline bool cnext_to_bool_val(bool x) { return x; }
static inline bool cnext_to_bool_from_str(CnextString s) { return s.data != NULL && s.length > 0; }
static inline bool cnext_to_bool_from_ptr(void* x) { return x != NULL; }

#define cnext_to_bool(...) _Generic((__VA_ARGS__), \
    int: cnext_to_bool_from_int, \
    bool: cnext_to_bool_val, \
    CnextString: cnext_to_bool_from_str, \
    default: cnext_to_bool_from_ptr)(__VA_ARGS__)

static inline char cnext_to_char_from_int(int x) { return (char)x; }
static inline char cnext_to_char_val(char x) { return x; }

#define cnext_to_char(...) _Generic((__VA_ARGS__), \
    int: cnext_to_char_from_int, \
    char: cnext_to_char_val, \
    default: cnext_to_char_from_int)(__VA_ARGS__)

static inline int cnext_parse_int(CnextString s) { return cnext_str_to_int(s); }
static inline float cnext_parse_float(CnextString s) { return cnext_str_parse_float(s); }
static inline int cnext_char_fn(char x) { return (int)(unsigned char)x; }
static inline int cnext_ord(CnextString s) {
    if (!s.data || s.length == 0) return -1;
    return (int)(unsigned char)s.data[0];
}
static inline CnextString cnext_bytes(CnextString s) { return s; }

/* --- Core Builtins --- */

#define cnext_swap(a, b) do { __typeof__(a) _tmp = (a); (a) = (b); (b) = _tmp; } while(0)

#define cnext_clone(x) _Generic((x), \
    int: cnext_clone_int, \
    float: cnext_clone_float, \
    CnextString: cnext_clone_str, \
    default: cnext_clone_ptr)(x)

static inline int cnext_clone_int(int x) { return x; }
static inline float cnext_clone_float(float x) { return x; }
static inline CnextString cnext_clone_str(CnextString s) {
    if (!s.data) return s;
    char* buf = (char*)malloc(s.length + 1);
    if (!buf) return s;
    memcpy(buf, s.data, s.length);
    buf[s.length] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, s.length};
}
static inline void* cnext_clone_ptr(void* x) { return x; }

/* --- Math Builtins --- */

static inline int cnext_abs(int x) { return x < 0 ? -x : x; }
static inline float cnext_abs_f(float x) { return x < 0.0f ? -x : x; }
static inline int cnext_min(int a, int b) { return a < b ? a : b; }
static inline float cnext_min_f(float a, float b) { return a < b ? a : b; }
static inline int cnext_max(int a, int b) { return a > b ? a : b; }
static inline float cnext_max_f(float a, float b) { return a > b ? a : b; }
static inline int cnext_clamp(int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline float cnext_clamp_f(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline int cnext_round(float x) { return (int)(x >= 0 ? x + 0.5f : x - 0.5f); }
static inline int cnext_floor(float x) { int i = (int)x; return (x < 0 && x != i) ? i - 1 : i; }
static inline int cnext_ceil(float x) { int i = (int)x; return (x > 0 && x != i) ? i + 1 : i; }
static inline float cnext_sqrt(float x) { return (float)sqrt((double)x); }
static inline float cnext_pow(float base, float exp) { return (float)pow((double)base, (double)exp); }
static inline float cnext_log(float x) { return (float)log((double)x); }
static inline float cnext_exp(float x) { return (float)exp((double)x); }
static inline float cnext_sin(float x) { return (float)sin((double)x); }
static inline float cnext_cos(float x) { return (float)cos((double)x); }
static inline float cnext_tan(float x) { return (float)tan((double)x); }
static inline float cnext_random(void) { return (float)rand() / (float)RAND_MAX; }
static inline int cnext_random_range(int lo, int hi) { return lo + rand() % (hi - lo + 1); }

static inline int cnext_gcd(int a, int b) {
    a = a < 0 ? -a : a; b = b < 0 ? -b : b;
    while (b) { int t = b; b = a % b; a = t; }
    return a;
}

static inline int cnext_lcm(int a, int b) {
    if (a == 0 || b == 0) return 0;
    return (a / cnext_gcd(a, b)) * b;
}

static inline long long cnext_factorial(int n) {
    if (n < 0) { fprintf(stderr, "factorial of negative number\n"); exit(1); }
    long long r = 1;
    for (int i = 2; i <= n; i++) r *= i;
    return r;
}

static inline int cnext_fibonacci(int n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    int a = 0, b = 1;
    for (int i = 2; i <= n; i++) { int t = a + b; a = b; b = t; }
    return b;
}

/* --- Time Builtins --- */

#ifdef _WIN32
#include <windows.h>
static inline long long cnext_timestamp(void) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    // FILETIME is 100ns intervals since 1601-01-01; convert to Unix seconds (since 1970-01-01).
    return (long long)((uli.QuadPart - 116444736000000000ULL) / 10000000ULL);
}
static inline void cnext_time_sleep(int ms) { Sleep((DWORD)ms); }
#else
#include <time.h>
#include <unistd.h>
static inline long long cnext_timestamp(void) { return (long long)time(NULL); }
static inline void cnext_time_sleep(int ms) { usleep((useconds_t)ms * 1000); }
#endif

static inline CnextString cnext_date(void) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char* buf = (char*)malloc(32);
    if (!buf) return (CnextString){NULL, 0};
    strftime(buf, 32, "%Y-%m-%d", t);
    _cnext_track(buf);
    return (CnextString){buf, strlen(buf)};
}

static inline CnextString cnext_time_str(void) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char* buf = (char*)malloc(32);
    if (!buf) return (CnextString){NULL, 0};
    strftime(buf, 32, "%H:%M:%S", t);
    _cnext_track(buf);
    return (CnextString){buf, strlen(buf)};
}

static inline CnextString cnext_format_time(CnextString fmt) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char* buf = (char*)malloc(128);
    if (!buf) return (CnextString){NULL, 0};
    char fbuf[128];
    size_t flen = fmt.length < 127 ? fmt.length : 127;
    memcpy(fbuf, fmt.data, flen);
    fbuf[flen] = '\0';
    strftime(buf, 128, fbuf, t);
    _cnext_track(buf);
    return (CnextString){buf, strlen(buf)};
}

static long long _cnext_stopwatch_start_time = 0;
static inline void cnext_stopwatch_start(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    _cnext_stopwatch_start_time = ((long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    _cnext_stopwatch_start_time = (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
#endif
}
static inline long long cnext_stopwatch_stop(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    long long end = ((long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return (end - _cnext_stopwatch_start_time) / 10;  // Convert to microseconds
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long long end = (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
    return (end - _cnext_stopwatch_start_time) / 1000;  // Convert to microseconds
#endif
}

/* --- File Builtins --- */

static inline CnextString cnext_read_file(CnextString path) {
    char buf[1024];
    size_t plen = path.length < 1023 ? path.length : 1023;
    memcpy(buf, path.data, plen);
    buf[plen] = '\0';
    FILE* f = fopen(buf, "rb");
    if (!f) return (CnextString){NULL, 0};
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return (CnextString){NULL, 0}; }
    char* data = (char*)malloc((size_t)size + 1);
    if (!data) { fclose(f); return (CnextString){NULL, 0}; }
    size_t read = fread(data, 1, (size_t)size, f);
    data[read] = '\0';
    fclose(f);
    _cnext_track(data);
    return (CnextString){data, read};
}

static inline void cnext_write_file(CnextString path, CnextString content) {
    char buf[1024];
    size_t plen = path.length < 1023 ? path.length : 1023;
    memcpy(buf, path.data, plen);
    buf[plen] = '\0';
    FILE* f = fopen(buf, "wb");
    if (!f) { fprintf(stderr, "Cannot write to file: %.*s\n", (int)path.length, path.data); return; }
    fwrite(content.data, 1, content.length, f);
    fclose(f);
}

static inline void cnext_append_file(CnextString path, CnextString content) {
    char buf[1024];
    size_t plen = path.length < 1023 ? path.length : 1023;
    memcpy(buf, path.data, plen);
    buf[plen] = '\0';
    FILE* f = fopen(buf, "ab");
    if (!f) { fprintf(stderr, "Cannot append to file: %.*s\n", (int)path.length, path.data); return; }
    fwrite(content.data, 1, content.length, f);
    fclose(f);
}

static inline void cnext_delete_file(CnextString path) {
    char buf[1024];
    size_t plen = path.length < 1023 ? path.length : 1023;
    memcpy(buf, path.data, plen);
    buf[plen] = '\0';
    remove(buf);
}

static inline void cnext_copy_file(CnextString src, CnextString dst) {
    CnextString content = cnext_read_file(src);
    if (content.data) cnext_write_file(dst, content);
}

static inline void cnext_move_file(CnextString src, CnextString dst) {
    char sbuf[1024], dbuf[1024];
    size_t slen = src.length < 1023 ? src.length : 1023;
    size_t dlen = dst.length < 1023 ? dst.length : 1023;
    memcpy(sbuf, src.data, slen); sbuf[slen] = '\0';
    memcpy(dbuf, dst.data, dlen); dbuf[dlen] = '\0';
    rename(sbuf, dbuf);
}

static inline bool cnext_file_exists(CnextString path) {
    char buf[1024];
    size_t plen = path.length < 1023 ? path.length : 1023;
    memcpy(buf, path.data, plen);
    buf[plen] = '\0';
    FILE* f = fopen(buf, "rb");
    if (f) { fclose(f); return true; }
    return false;
}

static inline long cnext_file_size(CnextString path) {
    char buf[1024];
    size_t plen = path.length < 1023 ? path.length : 1023;
    memcpy(buf, path.data, plen);
    buf[plen] = '\0';
    FILE* f = fopen(buf, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return size;
}

/* --- System Builtins --- */

static inline CnextString cnext_cwd(void) {
    char* buf = (char*)malloc(4096);
    if (!buf) return (CnextString){NULL, 0};
#ifdef _WIN32
    DWORD len = GetCurrentDirectoryA(4096, buf);
    if (len == 0) { free(buf); return (CnextString){NULL, 0}; }
    _cnext_track(buf);
    return (CnextString){buf, len};
#else
    if (!getcwd(buf, 4096)) { free(buf); return (CnextString){NULL, 0}; }
    size_t len = strlen(buf);
    _cnext_track(buf);
    return (CnextString){buf, len};
#endif
}

static inline void cnext_chdir(CnextString path) {
    char buf[1024];
    size_t plen = path.length < 1023 ? path.length : 1023;
    memcpy(buf, path.data, plen);
    buf[plen] = '\0';
#ifdef _WIN32
    SetCurrentDirectoryA(buf);
#else
    chdir(buf);
#endif
}

static inline CnextString cnext_platform(void) {
#ifdef _WIN32
    return (CnextString){(char*)"windows", 7};
#elif defined(__APPLE__)
    return (CnextString){(char*)"macos", 5};
#elif defined(__linux__)
    return (CnextString){(char*)"linux", 5};
#else
    return (CnextString){(char*)"unknown", 7};
#endif
}

/* --- CLI args support --- */
static int _cnext_argc = 0;
static char** _cnext_argv = NULL;

static inline void cnext_init_args(int argc, char** argv) {
    _cnext_argc = argc;
    _cnext_argv = argv;
}

static inline CnextString cnext_args_str(void) {
    if (_cnext_argc < 2) return (CnextString){(char*)"", 0};
    // Join args from index 1 with space separator
    size_t total = 0;
    for (int i = 1; i < _cnext_argc; i++) {
        total += strlen(_cnext_argv[i]);
        if (i > 1) total += 1; // space separator
    }
    char* buf = (char*)malloc(total + 1);
    size_t pos = 0;
    for (int i = 1; i < _cnext_argc; i++) {
        if (i > 1) { buf[pos++] = ' '; }
        size_t len = strlen(_cnext_argv[i]);
        memcpy(buf + pos, _cnext_argv[i], len);
        pos += len;
    }
    buf[pos] = '\0';
    CnextString result = {buf, pos};
    _cnext_track(buf);
    return result;
}

static inline CnextString cnext_arg_at(int index) {
    if (index < 0 || index >= _cnext_argc) return (CnextString){(char*)"", 0};
    char* s = _cnext_argv[index];
    return (CnextString){s, strlen(s)};
}

static inline int cnext_arg_count(void) {
    return _cnext_argc;
}

static inline CnextString cnext_getenv_str(CnextString name) {
    char buf[256];
    size_t nlen = name.length < 255 ? name.length : 255;
    memcpy(buf, name.data, nlen);
    buf[nlen] = '\0';
    const char* val = getenv(buf);
    if (!val) return (CnextString){NULL, 0};
    size_t vlen = strlen(val);
    char* result = (char*)malloc(vlen + 1);
    if (!result) return (CnextString){NULL, 0};
    memcpy(result, val, vlen + 1);
    _cnext_track(result);
    return (CnextString){result, vlen};
}

static inline void cnext_setenv_str(CnextString name, CnextString value) {
    char nbuf[256], vbuf[1024];
    size_t nlen = name.length < 255 ? name.length : 255;
    size_t vlen = value.length < 1023 ? value.length : 1023;
    memcpy(nbuf, name.data, nlen); nbuf[nlen] = '\0';
    memcpy(vbuf, value.data, vlen); vbuf[vlen] = '\0';
#ifdef _WIN32
    _putenv_s(nbuf, vbuf);
#else
    setenv(nbuf, vbuf, 1);
#endif
}

static inline CnextString cnext_hostname_str(void) {
    char* buf = (char*)malloc(256);
    if (!buf) return (CnextString){NULL, 0};
#ifdef _WIN32
    DWORD size = 256;
    if (!GetComputerNameA(buf, &size)) { free(buf); return (CnextString){(char*)"", 0}; }
    _cnext_track(buf);
    return (CnextString){buf, size};
#else
    if (gethostname(buf, 256) != 0) { free(buf); return (CnextString){(char*)"", 0}; }
    size_t len = strlen(buf);
    _cnext_track(buf);
    return (CnextString){buf, len};
#endif
}

static inline CnextString cnext_username_str(void) {
    char* buf = (char*)malloc(256);
    if (!buf) return (CnextString){NULL, 0};
#ifdef _WIN32
    DWORD size = 256;
    if (!GetUserNameA(buf, &size)) { free(buf); return (CnextString){(char*)"", 0}; }
    _cnext_track(buf);
    return (CnextString){buf, size};
#else
    if (getlogin_r(buf, 256) != 0) { free(buf); return (CnextString){(char*)"", 0}; }
    size_t len = strlen(buf);
    _cnext_track(buf);
    return (CnextString){buf, len};
#endif
}

static inline int cnext_cpu_count(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
#else
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

static inline CnextString cnext_temp_dir(void) {
#ifdef _WIN32
    char* buf = (char*)malloc(MAX_PATH + 1);
    if (!buf) return (CnextString){NULL, 0};
    DWORD len = GetTempPathA(MAX_PATH + 1, buf);
    _cnext_track(buf);
    return (CnextString){buf, len};
#else
    return (CnextString){(char*)"/tmp", 4};
#endif
}

static inline CnextString cnext_home_dir(void) {
#ifdef _WIN32
    const char* home = getenv("USERPROFILE");
    if (!home) home = getenv("HOMEDRIVE");
#else
    const char* home = getenv("HOME");
#endif
    if (!home) return (CnextString){NULL, 0};
    size_t len = strlen(home);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return (CnextString){NULL, 0};
    memcpy(buf, home, len + 1);
    _cnext_track(buf);
    return (CnextString){buf, len};
}

static inline void cnext_exec(CnextString cmd) {
    char buf[4096];
    size_t clen = cmd.length < 4095 ? cmd.length : 4095;
    memcpy(buf, cmd.data, clen);
    buf[clen] = '\0';
    system(buf);
}

static inline CnextString cnext_shell(CnextString cmd) {
    char buf[4096];
    size_t clen = cmd.length < 4095 ? cmd.length : 4095;
    memcpy(buf, cmd.data, clen);
    buf[clen] = '\0';
    FILE* pipe = popen(buf, "r");
    if (!pipe) return (CnextString){NULL, 0};
    char* result = (char*)malloc(65536);
    if (!result) { pclose(pipe); return (CnextString){NULL, 0}; }
    size_t total = 0;
    size_t n;
    while ((n = fread(result + total, 1, 65535 - total, pipe)) > 0) total += n;
    result[total] = '\0';
    pclose(pipe);
    _cnext_track(result);
    return (CnextString){result, total};
}

/* --- JSON Builtins --- */

static inline CnextString cnext_json_parse(CnextString s) {
    if (!s.data) return (CnextString){NULL, 0};
    void* root = json_parse(s);
    if (!root) return (CnextString){NULL, 0};
    CnextString result = json_stringify(root);
    json_free(root);
    return result;
}

static inline CnextString cnext_json_stringify(CnextString s) {
    if (!s.data) return (CnextString){NULL, 0};
    void* root = json_parse(s);
    if (!root) return s;
    CnextString result = json_stringify(root);
    json_free(root);
    return result;
}

/* --- Encoding Builtins (placeholder) --- */

static inline CnextString cnext_base64_encode(CnextString s) {
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t len = s.length;
    size_t olen = 4 * ((len + 2) / 3);
    char* out = (char*)malloc(olen + 1);
    if (!out) return (CnextString){NULL, 0};
    size_t i, j;
    for (i = 0, j = 0; i < len;) {
        size_t start = i;
        unsigned int a = i < len ? (unsigned char)s.data[i++] : 0;
        unsigned int b = i < len ? (unsigned char)s.data[i++] : 0;
        unsigned int c = i < len ? (unsigned char)s.data[i++] : 0;
        int bytes_read = (int)(i - start);
        unsigned int triple = (a << 16) | (b << 8) | c;
        out[j++] = tbl[(triple >> 18) & 0x3F];
        out[j++] = tbl[(triple >> 12) & 0x3F];
        out[j++] = (bytes_read < 2) ? '=' : tbl[(triple >> 6) & 0x3F];
        out[j++] = (bytes_read < 3) ? '=' : tbl[triple & 0x3F];
    }
    out[j] = '\0';
    _cnext_track(out);
    return (CnextString){out, j};
}

static inline CnextString cnext_base64_decode(CnextString s) {
    static const unsigned char dtable[256] = {
        ['A']=0,['B']=1,['C']=2,['D']=3,['E']=4,['F']=5,['G']=6,['H']=7,
        ['I']=8,['J']=9,['K']=10,['L']=11,['M']=12,['N']=13,['O']=14,['P']=15,
        ['Q']=16,['R']=17,['S']=18,['T']=19,['U']=20,['V']=21,['W']=22,['X']=23,
        ['Y']=24,['Z']=25,['a']=26,['b']=27,['c']=28,['d']=29,['e']=30,['f']=31,
        ['g']=32,['h']=33,['i']=34,['j']=35,['k']=36,['l']=37,['m']=38,['n']=39,
        ['o']=40,['p']=41,['q']=42,['r']=43,['s']=44,['t']=45,['u']=46,['v']=47,
        ['w']=48,['x']=49,        ['y']=50,['z']=51,['0']=52,['1']=53,['2']=54,['3']=55,
        ['4']=56,['5']=57,['6']=58,['7']=59,['8']=60,['9']=61,['+']=62,['/']=63,
        ['=']=64
    };
    size_t len = s.length;
    if (len == 0) return (CnextString){NULL, 0};
    char* out = (char*)malloc(len);
    if (!out) return (CnextString){NULL, 0};
    size_t j = 0;
    for (size_t i = 0; i < len;) {
        unsigned int a = dtable[(unsigned char)s.data[i++]];
        unsigned int b = i < len ? dtable[(unsigned char)s.data[i++]] : 0;
        unsigned int c = i < len ? dtable[(unsigned char)s.data[i++]] : 0;
        unsigned int d = i < len ? dtable[(unsigned char)s.data[i++]] : 0;
        unsigned int triple = (a << 18) | (b << 12) | (c << 6) | d;
        out[j++] = (char)((triple >> 16) & 0xFF);
        if (c != 64) out[j++] = (char)((triple >> 8) & 0xFF);
        if (d != 64) out[j++] = (char)(triple & 0xFF);
    }
    out[j] = '\0';
    _cnext_track(out);
    return (CnextString){out, j};
}

/* --- Crypto Builtins (simple hash) --- */

static inline CnextString cnext_hash_md5_str(CnextString s) {
    unsigned int h = 5381;
    for (size_t i = 0; i < s.length; i++) h = ((h << 5) + h) + (unsigned char)s.data[i];
    char* buf = (char*)malloc(9);
    if (!buf) return (CnextString){NULL, 0};
    snprintf(buf, 9, "%08x", h);
    _cnext_track(buf);
    return (CnextString){buf, 8};
}

static inline CnextString cnext_hash_sha1_str(CnextString s) {
    unsigned int h = 0x67452301;
    for (size_t i = 0; i < s.length; i++) {
        h = (h << 5) + h + (unsigned char)s.data[i];
        h ^= h >> 16;
    }
    char* buf = (char*)malloc(9);
    if (!buf) return (CnextString){NULL, 0};
    snprintf(buf, 9, "%08x", h);
    _cnext_track(buf);
    return (CnextString){buf, 8};
}

static inline CnextString cnext_hash_sha256_str(CnextString s) {
    unsigned int h = 0x6a09e667;
    for (size_t i = 0; i < s.length; i++) {
        h = (h << 5) + h + (unsigned char)s.data[i];
        h ^= h >> 16;
        h *= 0x85ebca6b;
    }
    char* buf = (char*)malloc(9);
    if (!buf) return (CnextString){NULL, 0};
    snprintf(buf, 9, "%08x", h);
    _cnext_track(buf);
    return (CnextString){buf, 8};
}

static inline CnextString cnext_uuid(void) {
    char* buf = (char*)malloc(37);
    if (!buf) return (CnextString){NULL, 0};
    srand((unsigned int)time(NULL));
    snprintf(buf, 37, "%08x-%04x-%04x-%04x-%04x%08x",
        rand(), rand() & 0xFFFF, rand() & 0xFFFF,
        rand() & 0xFFFF, rand() & 0xFFFF, rand());
    _cnext_track(buf);
    return (CnextString){buf, 36};
}

/* --- Collections Builtins (array operations) --- */

/* Note: Array operations work on CnextSlice<T> which is { T* data; int length; } */
/* The code generator will emit type-specific versions */

/* --- Utility Builtins --- */

static inline void cnext_debug_int(int x) { fprintf(stderr, "[debug] %d\n", x); }
static inline void cnext_debug_float(float x) { fprintf(stderr, "[debug] %f\n", x); }
static inline void cnext_debug_str(CnextString x) { fprintf(stderr, "[debug] %.*s\n", (int)x.length, x.data ? x.data : "(null)"); }
static inline void cnext_debug_bool(bool x) { fprintf(stderr, "[debug] %s\n", x ? "true" : "false"); }
static inline void cnext_debug_ptr(void* x) { fprintf(stderr, "[debug] %p\n", x); }

#define cnext_debug(...) _Generic((__VA_ARGS__), \
    int: cnext_debug_int, \
    float: cnext_debug_float, \
    CnextString: cnext_debug_str, \
    bool: cnext_debug_bool, \
    default: cnext_debug_ptr)(__VA_ARGS__)

static inline void cnext_gc(void) { _cnext_free_all(); }

static inline long long cnext_benchmark(int iterations) {
#ifdef _WIN32
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    (void)iterations;
    QueryPerformanceCounter(&end);
    return (end.QuadPart - start.QuadPart) * 1000000LL / freq.QuadPart;
#else
    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    (void)iterations;
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    return (ts_end.tv_sec - ts_start.tv_sec) * 1000000LL + (ts_end.tv_nsec - ts_start.tv_nsec) / 1000;
#endif
}

/* --- typeof builtin --- */

static inline CnextString cnext_typeof_int(int x) { (void)x; return (CnextString){(char*)"int", 3}; }
static inline CnextString cnext_typeof_float(float x) { (void)x; return (CnextString){(char*)"float", 5}; }
static inline CnextString cnext_typeof_str(CnextString x) { (void)x; return (CnextString){(char*)"str", 3}; }
static inline CnextString cnext_typeof_bool(bool x) { (void)x; return (CnextString){(char*)"bool", 4}; }
static inline CnextString cnext_typeof_ptr(void* x) { (void)x; return (CnextString){(char*)"void", 4}; }

#define cnext_typeof(...) _Generic((__VA_ARGS__), \
    int: cnext_typeof_int, \
    float: cnext_typeof_float, \
    CnextString: cnext_typeof_str, \
    bool: cnext_typeof_bool, \
    default: cnext_typeof_ptr)(__VA_ARGS__)

/* --- assert builtin --- */

static inline void cnext_assert_fn(bool condition, CnextString msg) {
    if (!condition) {
        fprintf(stderr, "Assertion failed: %.*s\n", (int)msg.length, msg.data ? msg.data : "");
        exit(1);
    }
}

/* --- panic builtin --- */

static inline _Noreturn void cnext_panic(CnextString msg) {
    fprintf(stderr, "panic: %.*s\n", (int)msg.length, msg.data ? msg.data : "");
    exit(1);
}

/* --- exit builtin --- */

static inline _Noreturn void cnext_exit_fn(int code) { exit(code); }

/* --- typeof for class types --- */

static inline CnextString cnext_typeof_class(void* x, const char* name) {
    (void)x;
    return (CnextString){(char*)name, (size_t)strlen(name)};
}

/* --- Tagged Union / Variant support --- */
/* For union types: int | str | bool etc.
   We use a generic 64-byte payload to avoid heap allocation for small types.
   Larger types (strings, arrays, objects) are stored by pointer. */

#define CNEXT_VARIANT_MAX_PAYLOAD 64

typedef struct {
    int tag;
    size_t size;
    _Alignas(16) char payload[CNEXT_VARIANT_MAX_PAYLOAD];
} CnextVariant;

static inline CnextVariant cnext_variant_make_int(int tag, int64_t val) {
    CnextVariant v = {tag, sizeof(int64_t), {0}};
    memcpy(v.payload, &val, sizeof(int64_t));
    return v;
}

static inline CnextVariant cnext_variant_make_float(int tag, double val) {
    CnextVariant v = {tag, sizeof(double), {0}};
    memcpy(v.payload, &val, sizeof(double));
    return v;
}

static inline CnextVariant cnext_variant_make_str(int tag, CnextString val) {
    CnextVariant v = {tag, sizeof(CnextString), {0}};
    memcpy(v.payload, &val, sizeof(CnextString));
    return v;
}

static inline CnextVariant cnext_variant_make_bool(int tag, bool val) {
    CnextVariant v = {tag, sizeof(bool), {0}};
    memcpy(v.payload, &val, sizeof(bool));
    return v;
}

static inline CnextVariant cnext_variant_make_ptr(int tag, void* val) {
    CnextVariant v = {tag, sizeof(void*), {0}};
    memcpy(v.payload, &val, sizeof(void*));
    return v;
}

static inline int64_t cnext_variant_as_int(CnextVariant v) {
    int64_t val = 0;
    memcpy(&val, v.payload, sizeof(int64_t));
    return val;
}

static inline double cnext_variant_as_float(CnextVariant v) {
    double val = 0;
    memcpy(&val, v.payload, sizeof(double));
    return val;
}

static inline CnextString cnext_variant_as_str(CnextVariant v) {
    CnextString val = {NULL, 0};
    memcpy(&val, v.payload, sizeof(CnextString));
    return val;
}

static inline bool cnext_variant_as_bool(CnextVariant v) {
    bool val = false;
    memcpy(&val, v.payload, sizeof(bool));
    return val;
}

static inline void* cnext_variant_as_ptr(CnextVariant v) {
    void* val = NULL;
    memcpy(&val, v.payload, sizeof(void*));
    return val;
}

static inline bool cnext_variant_is(CnextVariant v, int tag) {
    return v.tag == tag;
}

#endif /* CNEXT_RUNTIME_H */
