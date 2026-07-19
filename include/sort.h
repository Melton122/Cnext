#ifndef CNEXT_SORT_H
#define CNEXT_SORT_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static inline void sort_ints(int* data, int len) {
    if (!data || len <= 1) return;
    for (int i = 1; i < len; i++) {
        int key = data[i];
        int j = i - 1;
        while (j >= 0 && data[j] > key) {
            data[j + 1] = data[j];
            j--;
        }
        data[j + 1] = key;
    }
}

static inline void sort_ints_desc(int* data, int len) {
    if (!data || len <= 1) return;
    for (int i = 1; i < len; i++) {
        int key = data[i];
        int j = i - 1;
        while (j >= 0 && data[j] < key) {
            data[j + 1] = data[j];
            j--;
        }
        data[j + 1] = key;
    }
}

static inline void sort_floats(float* data, int len) {
    if (!data || len <= 1) return;
    for (int i = 1; i < len; i++) {
        float key = data[i];
        int j = i - 1;
        while (j >= 0 && data[j] > key) {
            data[j + 1] = data[j];
            j--;
        }
        data[j + 1] = key;
    }
}

static inline void sort_strings(CnextString* data, int len) {
    if (!data || len <= 1) return;
    for (int i = 1; i < len; i++) {
        CnextString key = data[i];
        int j = i - 1;
        while (j >= 0 && (data[j].length > key.length ||
               (data[j].length == key.length && memcmp(data[j].data, key.data, key.length) > 0))) {
            data[j + 1] = data[j];
            j--;
        }
        data[j + 1] = key;
    }
}

static inline void sort_strings_by_length(CnextString* data, int len) {
    if (!data || len <= 1) return;
    for (int i = 1; i < len; i++) {
        CnextString key = data[i];
        int j = i - 1;
        while (j >= 0 && data[j].length > key.length) {
            data[j + 1] = data[j];
            j--;
        }
        data[j + 1] = key;
    }
}

static inline int search_int(int* data, int len, int target) {
    if (!data || len <= 0) return -1;
    for (int i = 0; i < len; i++) {
        if (data[i] == target) return i;
    }
    return -1;
}

static inline int search_string(CnextString* data, int len, CnextString target) {
    if (!data || len <= 0) return -1;
    for (int i = 0; i < len; i++) {
        if (data[i].length == target.length && memcmp(data[i].data, target.data, target.length) == 0) return i;
    }
    return -1;
}

static inline bool contains_int(int* data, int len, int target) {
    return search_int(data, len, target) >= 0;
}

static inline bool contains_string(CnextString* data, int len, CnextString target) {
    return search_string(data, len, target) >= 0;
}

static inline void reverse_ints(int* data, int len) {
    if (!data || len <= 1) return;
    for (int i = 0, j = len - 1; i < j; i++, j--) {
        int tmp = data[i];
        data[i] = data[j];
        data[j] = tmp;
    }
}

static inline void reverse_strings(CnextString* data, int len) {
    if (!data || len <= 1) return;
    for (int i = 0, j = len - 1; i < j; i++, j--) {
        CnextString tmp = data[i];
        data[i] = data[j];
        data[j] = tmp;
    }
}

static inline int min_int(int* data, int len) {
    if (!data || len <= 0) return 0;
    int m = data[0];
    for (int i = 1; i < len; i++) {
        if (data[i] < m) m = data[i];
    }
    return m;
}

static inline int max_int(int* data, int len) {
    if (!data || len <= 0) return 0;
    int m = data[0];
    for (int i = 1; i < len; i++) {
        if (data[i] > m) m = data[i];
    }
    return m;
}

static inline long sum_ints(int* data, int len) {
    long s = 0;
    for (int i = 0; i < len; i++) s += data[i];
    return s;
}

static inline double avg_ints(int* data, int len) {
    if (len <= 0) return 0.0;
    return (double)sum_ints(data, len) / (double)len;
}

#endif
