#ifndef CNEXT_THREAD_H
#define CNEXT_THREAD_H

#include "runtime.h"

/* Thread module functions for Cnext */
/* These are wrappers around the platform-specific runtime functions */

/* Thread functions */
static inline void* thread_spawn(void* (*func)(void*), void* arg) {
    CnextThread t = cnext_thread_spawn(func, arg);
    return (void*)(intptr_t)t;
}

static inline void thread_join(void* thread) {
    cnext_thread_join((CnextThread)(intptr_t)thread);
}

/* Mutex functions */
static inline void* mutex_new(void) {
    CnextMutex* m = (CnextMutex*)malloc(sizeof(CnextMutex));
    if (!m) return NULL;
    *m = cnext_mutex_new();
    _cnext_track(m);
    return m;
}

static inline void mutex_lock(void* mutex) {
    cnext_mutex_lock((CnextMutex*)mutex);
}

static inline void mutex_unlock(void* mutex) {
    cnext_mutex_unlock((CnextMutex*)mutex);
}

static inline void mutex_free(void* mutex) {
    cnext_mutex_free((CnextMutex*)mutex);
    _cnext_untrack(mutex);
    free(mutex);
}

/* Channel functions */
static inline void* channel_new(int capacity) {
    CnextChannel* ch = (CnextChannel*)malloc(sizeof(CnextChannel));
    if (!ch) return NULL;
    *ch = cnext_channel_new(capacity);
    _cnext_track(ch);
    return ch;
}

static inline void channel_send(void* channel, CnextString msg) {
    cnext_channel_send((CnextChannel*)channel, msg);
}

static inline CnextString channel_recv(void* channel) {
    return cnext_channel_recv((CnextChannel*)channel);
}

static inline void channel_free(void* channel) {
    cnext_channel_free((CnextChannel*)channel);
    _cnext_untrack(channel);
    free(channel);
}

#endif /* CNEXT_THREAD_H */
