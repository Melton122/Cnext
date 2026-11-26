#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

static void* live_pointers[8192];
static size_t live_count;
static size_t fail_after = SIZE_MAX;
static int expected_live_on_exit = -1;
static size_t force_realloc_copy;
static void* expected_index_on_exit;

static void remember(void* pointer) {
    if (!pointer) return;
    if (live_count == sizeof(live_pointers) / sizeof(live_pointers[0])) exit(90);
    live_pointers[live_count++] = pointer;
}

static size_t find_pointer(void* pointer) {
    for (size_t i = 0; i < live_count; i++) if (live_pointers[i] == pointer) return i;
    fprintf(stderr, "allocator contract: untracked free or realloc\n");
    exit(91);
}

static bool allocation_allowed(void) {
    if (!fail_after) return false;
    if (fail_after != SIZE_MAX) fail_after--;
    return true;
}

static void* harness_malloc(size_t size) {
    if (!allocation_allowed()) return NULL;
    void* pointer = malloc(size);
    remember(pointer);
    return pointer;
}

static void* harness_calloc(size_t count, size_t size) {
    if (!allocation_allowed()) return NULL;
    void* pointer = calloc(count, size);
    remember(pointer);
    return pointer;
}

static void* harness_realloc(void* previous, size_t size) {
    if (!previous) return harness_malloc(size);
    size_t index = find_pointer(previous);
    if (!size) { live_pointers[index] = live_pointers[--live_count]; free(previous); return NULL; }
    if (!allocation_allowed()) return NULL;
    if (force_realloc_copy) {
        void* pointer = malloc(size);
        if (pointer) {
            memcpy(pointer, previous, force_realloc_copy < size ? force_realloc_copy : size);
            free(previous);
            live_pointers[index] = pointer;
        }
        return pointer;
    }
    void* pointer = realloc(previous, size);
    if (pointer) live_pointers[index] = pointer;
    return pointer;
}

static void harness_free(void* pointer) {
    if (!pointer) return;
    size_t index = find_pointer(pointer);
    live_pointers[index] = live_pointers[--live_count];
    free(pointer);
}

#define malloc harness_malloc
#define calloc harness_calloc
#define realloc harness_realloc
#define free harness_free
#include "runtime.h"
#undef malloc
#undef calloc
#undef realloc
#undef free

#define REQUIRE(condition) do { if (!(condition)) { fprintf(stderr, "allocator contract failed at line %d\n", __LINE__); return 1; } } while (0)

static void verify_error_cleanup(void) {
    if (atomic_flag_test_and_set_explicit(&_cnext_mem_lock, memory_order_acquire)) _Exit(92);
    atomic_flag_clear_explicit(&_cnext_mem_lock, memory_order_release);
    if (expected_live_on_exit >= 0 && live_count != (size_t)expected_live_on_exit) _Exit(93);
    if (expected_index_on_exit) {
        _AllocNode* entry = _cnext_alloc_index[_cnext_alloc_bucket(expected_index_on_exit)];
        while (entry && entry->ptr != expected_index_on_exit) entry = entry->index_next;
        if (!entry) _Exit(95);
    }
}

int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "normal";
    if (!strcmp(mode, "split-growth")) {
        char text[1024];
        size_t counts[] = {1, 8, 9, 16, 17, 64, 129};
        for (size_t round = 0; round < 50; round++) {
            for (size_t k = 0; k < sizeof(counts) / sizeof(counts[0]); k++) {
                size_t count = counts[k];
                for (size_t i = 0; i < count; i++) { text[i * 2] = 'x'; text[i * 2 + 1] = ','; }
                CnextStringArray parts = cnext_str_split((CnextString){text, count * 2 - 1}, (CnextString){",", 1});
                REQUIRE(parts.length == count);
                REQUIRE(_cnext_allocs && _cnext_allocs->ptr == parts.data && !_cnext_allocs->next);
                for (size_t i = 0; i < count; i++) REQUIRE(parts.data[i].length == 1 && parts.data[i].data[0] == 'x');
                force_realloc_copy = parts.length * sizeof(*parts.data);
                parts = CNEXT_ARRAY_PUSH(parts, ((CnextString){"y", 1}));
                force_realloc_copy = 0;
                REQUIRE(parts.length == count + 1 && parts.data[count].data[0] == 'y');
                REQUIRE(_cnext_allocs && _cnext_allocs->ptr == parts.data && !_cnext_allocs->next);
                force_realloc_copy = parts.length * sizeof(*parts.data);
                parts = CNEXT_ARRAY_PUSH(parts, ((CnextString){"z", 1}));
                force_realloc_copy = 0;
                REQUIRE(parts.length == count + 2 && parts.data[count + 1].data[0] == 'z');
                REQUIRE(_cnext_allocs && _cnext_allocs->ptr == parts.data && !_cnext_allocs->next);
                _cnext_free_all();
                REQUIRE(live_count == 0);
            }
        }
        puts("split growth contracts passed");
        return 0;
    }
    if (strcmp(mode, "normal")) {
        atexit(verify_error_cleanup);
        if (!strcmp(mode, "array-overflow")) { expected_live_on_exit = 0; _cnext_array_grow(NULL, SIZE_MAX, sizeof(int)); }
        if (!strcmp(mode, "array-oom")) {
            void* previous = harness_malloc(16);
            _cnext_track(previous);
            expected_live_on_exit = 2;
            expected_index_on_exit = previous;
            fail_after = 0;
            _cnext_array_grow(previous, 4, sizeof(int));
        }
        if (!strcmp(mode, "create-oom")) { fail_after = 0; expected_live_on_exit = 0; cnext_mem_arena_create(); }
        CnextArena* arena = cnext_mem_arena_create();
        expected_live_on_exit = 1;
        if (!strcmp(mode, "arena-overflow")) cnext_mem_arena_alloc(arena, SIZE_MAX);
        if (!strcmp(mode, "arena-accounting")) { arena->total_allocated = SIZE_MAX; cnext_mem_arena_alloc(arena, 1); }
        if (!strcmp(mode, "block-oom")) { fail_after = 0; cnext_mem_arena_alloc(arena, 1); }
        if (!strcmp(mode, "large-oom")) { fail_after = 0; cnext_mem_arena_alloc(arena, 70000); }
        if (!strcmp(mode, "ledger-oom")) { fail_after = 1; cnext_mem_arena_alloc(arena, 70000); }
        if (!strcmp(mode, "null-arena")) cnext_mem_arena_alloc(NULL, 1);
        if (!strcmp(mode, "pool-overflow")) POOL_ALLOC(SIZE_MAX);
        if (!strcmp(mode, "pool-accounting")) { _cnext_global_pool.total_allocated = SIZE_MAX; POOL_ALLOC(1); }
        if (!strcmp(mode, "pool-block-oom")) { fail_after = 0; POOL_ALLOC(1); }
        if (!strcmp(mode, "pool-ledger-oom")) { fail_after = 1; POOL_ALLOC(70000); }
        return 94;
    }
    size_t sizes[] = {0, 1, 3, 7, 8, 9, 15, 16, 17, 70000, 70001, 131072};
    for (int round = 0; round < 200; round++) {
        CnextArena* arena = cnext_mem_arena_create();
        for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
            void* pointer = cnext_mem_arena_alloc(arena, sizes[i]);
            REQUIRE((uintptr_t)pointer % CNEXT_ALLOC_ALIGNMENT == 0);
            memset(pointer, 0xa5, sizes[i]);
        }
        REQUIRE(cnext_mem_arena_usage(arena) >= 70000 + 70001 + 131072);
        cnext_mem_arena_free(arena);
        REQUIRE(live_count == 1 && cnext_mem_arena_usage(arena) == 0);
        REQUIRE(cnext_mem_arena_alloc(arena, 0) != cnext_mem_arena_alloc(arena, 0));
        cnext_mem_arena_destroy(arena);
        REQUIRE(live_count == 0);
        for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
            void* pointer = POOL_ALLOC(sizes[i]);
            REQUIRE((uintptr_t)pointer % CNEXT_ALLOC_ALIGNMENT == 0);
            memset(pointer, 0x5a, sizes[i]);
        }
        REQUIRE(_cnext_global_pool.total_allocated >= 70000 + 70001 + 131072);
        POOL_FREE_ALL();
        POOL_FREE_ALL();
        REQUIRE(live_count == 0 && _cnext_global_pool.total_allocated == 0);
    }
    cnext_mem_scope_begin();
    unsigned char* outer = (unsigned char*)cnext_mem_arena_alloc(_cnext_cur_arena, 70000);
    outer[69999] = 42;
    cnext_mem_scope_begin();
    cnext_mem_arena_alloc(_cnext_cur_arena, 80000);
    cnext_mem_scope_end();
    REQUIRE(outer[69999] == 42 && live_count == 4);
    cnext_mem_scope_end();
    REQUIRE(live_count == 0 && !_cnext_cur_arena && !_cnext_scope_head);
    ARENA_ALLOC(70000);
    ARENA_FREE_ALL();
    ARENA_FREE_ALL();
    REQUIRE(live_count == 0);
    puts("allocator contracts passed");
    return 0;
}
