#include <stdio.h>
#include <stdlib.h>

typedef struct { int* data; size_t length; } IntArray;

static inline size_t _cnext_array_check(size_t length, size_t index) {
    if (index >= length) {
        fprintf(stderr, "Bounds error\n");
        exit(1);
    }
    return index;
}

#define CNEXT_ARRAY_IDX(arr, idx) (*({ \
    __auto_type _a = (arr); \
    size_t _i = (idx); \
    _cnext_array_check(_a.length, _i); \
    &_a.data[_i]; \
}))

int main() {
    int* d = malloc(sizeof(int)*3);
    d[0] = 10; d[1] = 20; d[2] = 30;
    IntArray arr = { d, 3 };

    CNEXT_ARRAY_IDX(arr, 1) = 99;
    printf("%d\n", CNEXT_ARRAY_IDX(arr, 1));
    return 0;
}
