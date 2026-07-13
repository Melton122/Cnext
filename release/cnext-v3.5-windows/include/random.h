#ifndef CNEXT_RANDOM_H
#define CNEXT_RANDOM_H
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

static uint64_t random_state[2];

static inline void random_seed(int seed) {
    random_state[0] = (uint64_t)seed;
    random_state[1] = (uint64_t)(seed * 6364136223846793005ULL + 1442695040888963407ULL);
}

static inline uint64_t random_next(void) {
    uint64_t s1 = random_state[0];
    uint64_t s0 = random_state[1];
    random_state[0] = s0;
    s1 ^= s1 << 23;
    random_state[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
    return random_state[1] + s0;
}

static inline int random_int(int min, int max) {
    if (min >= max) return min;
    uint64_t range = (uint64_t)(max - min) + 1;
    uint64_t r = random_next();
    return min + (int)(r % range);
}

static inline float random_float(void) {
    return (float)(random_next() >> 11) * (1.0f / 9007199254740992.0f);
}

static inline float random_uniform(float min, float max) {
    return min + random_float() * (max - min);
}

static inline float random_gaussian(void) {
    float u1 = random_float();
    float u2 = random_float();
    return sqrtf(-2.0f * logf(u1 > 0.0f ? u1 : 0.0001f)) * cosf(6.2831853f * u2);
}

#endif
