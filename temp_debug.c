#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

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

void cnext_free(char* s) {
    if (!s) return;
    _cnext_untrack(s);
    free(s);
}

static inline void _cnext_printin(const char* fmt, ...) { va_list args; va_start(args, fmt); vprintf(fmt, args); printf("\n"); va_end(args); }

#define printin(X) do { __auto_type _x = (X); _Generic((_x), \
    int: _cnext_printin("%d", _x), \
    unsigned int: _cnext_printin("%u", _x), \
    long: _cnext_printin("%ld", _x), \
    unsigned long: _cnext_printin("%lu", _x), \
    long long: _cnext_printin("%lld", _x), \
    unsigned long long: _cnext_printin("%llu", _x), \
    float: _cnext_printin("%f", _x), \
    double: _cnext_printin("%f", _x), \
    char*: _cnext_printin("%s", _x), \
    const char*: _cnext_printin("%s", _x), \
    bool: _cnext_printin("%s", _x ? "true" : "false"), \
    default: _cnext_printin("%p", (void*)(size_t)_x) \
); } while(0)

char* cnext_input(const char* prompt) {
    printf("%s", prompt);
    char* buffer = malloc(256);
    if (!buffer) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    if (fgets(buffer, 256, stdin)) {
        buffer[strcspn(buffer, "\n")] = 0;
    } else {
        buffer[0] = '\0';
    }
    _cnext_track(buffer);
    return buffer;
}

char* cnext_concat(const char* s1, const char* s2) {
    char* result = malloc(strlen(s1) + strlen(s2) + 1);
    if (!result) { fprintf(stderr, "Cnext runtime: out of memory.\n"); exit(70); }
    strcpy(result, s1);
    strcat(result, s2);
    _cnext_track(result);
    return result;
}

char* cnext_to_string_int(int x) { char* b = malloc(32); snprintf(b, 32, "%d", x); _cnext_track(b); return b; }
char* cnext_to_string_uint(unsigned int x) { char* b = malloc(32); snprintf(b, 32, "%u", x); _cnext_track(b); return b; }
char* cnext_to_string_long(long x) { char* b = malloc(32); snprintf(b, 32, "%ld", x); _cnext_track(b); return b; }
char* cnext_to_string_ulong(unsigned long x) { char* b = malloc(32); snprintf(b, 32, "%lu", x); _cnext_track(b); return b; }
char* cnext_to_string_llong(long long x) { char* b = malloc(32); snprintf(b, 32, "%lld", x); _cnext_track(b); return b; }
char* cnext_to_string_ullong(unsigned long long x) { char* b = malloc(32); snprintf(b, 32, "%llu", x); _cnext_track(b); return b; }
char* cnext_to_string_size_t(size_t x) { char* b = malloc(32); snprintf(b, 32, "%zu", x); _cnext_track(b); return b; }
char* cnext_to_string_float(float x) { char* b = malloc(64); snprintf(b, 64, "%f", x); _cnext_track(b); return b; }
char* cnext_to_string_double(double x) { char* b = malloc(64); snprintf(b, 64, "%f", x); _cnext_track(b); return b; }
char* cnext_to_string_str(char* x) { return x; }
char* cnext_to_string_cstr(const char* x) { return (char*)x; }
char* cnext_to_string_bool(bool x) { return x ? "true" : "false"; }
char* cnext_to_string_ptr(void* x) { char* b = malloc(32); snprintf(b, 32, "%p", x); _cnext_track(b); return b; }
#define cnext_to_string(X) _Generic((X), \
    int: cnext_to_string_int, \
    unsigned int: cnext_to_string_uint, \
    long: cnext_to_string_long, \
    unsigned long: cnext_to_string_ulong, \
    long long: cnext_to_string_llong, \
    unsigned long long: cnext_to_string_ullong, \
    float: cnext_to_string_float, \
    double: cnext_to_string_double, \
    char*: cnext_to_string_str, \
    const char*: cnext_to_string_cstr, \
    bool: cnext_to_string_bool, \
    default: cnext_to_string_ptr)(X)

int main(void) {
    printin(42);
    _cnext_free_all();
    return 0;
}

