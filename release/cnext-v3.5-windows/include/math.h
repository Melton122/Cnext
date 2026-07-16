#ifndef CNEXT_MATH_H
#define CNEXT_MATH_H
#include <math.h>
#include <stdlib.h>
#include <time.h>

// --- Basic integer math ---

static inline int math_abs(int x) {
    return (x < 0) ? (x == (-2147483647 - 1) ? x : -x) : x;
}

static inline int math_max(int a, int b) {
    return a > b ? a : b;
}

static inline int math_min(int a, int b) {
    return a < b ? a : b;
}

// --- Float math (wrapping C math.h float variants) ---

static inline float math_sqrt(float x) {
    return sqrtf(x);
}

static inline float math_pow(float base, float exp) {
    return powf(base, exp);
}

static inline float math_sin(float x) {
    return sinf(x);
}

static inline float math_cos(float x) {
    return cosf(x);
}

static inline float math_tan(float x) {
    return tanf(x);
}

static inline float math_log(float x) {
    return logf(x);
}

static inline float math_log10(float x) {
    return log10f(x);
}

static inline float math_exp(float x) {
    return expf(x);
}

static inline float math_floor(float x) {
    return floorf(x);
}

static inline float math_ceil(float x) {
    return ceilf(x);
}

static inline float math_round(float x) {
    return roundf(x);
}

static inline float math_fabs(float x) {
    return fabsf(x);
}

// --- Random ---

static inline void math_random_seed(int seed) {
    srand((unsigned int)seed);
}

static inline int math_random_int(int min, int max) {
    if (min >= max) return min;
    return min + (rand() % (max - min + 1));
}

static inline float math_random_float(void) {
    return (float)rand() / (float)RAND_MAX;
}

// --- Constants ---

#define MATH_PI 3.14159265358979323846f
#define MATH_E  2.71828182845904523536f

#endif
