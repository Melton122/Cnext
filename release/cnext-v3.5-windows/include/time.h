#ifndef CNEXT_TIME_H
#define CNEXT_TIME_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/time.h>
#endif

static inline long time_now(void) {
    return (long)time(NULL);
}

static inline long time_now_ms(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER ui;
    ui.LowPart = ft.dwLowDateTime;
    ui.HighPart = ft.dwHighDateTime;
    return (long)((ui.QuadPart - 116444736000000000ULL) / 10000);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

static inline void time_sleep(int ms) {
    if (ms <= 0) return;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)ms * 1000);
#endif
}

static inline CnextString time_format(long timestamp, CnextString fmt) {
    if (!fmt.data) return (CnextString){NULL, 0};
    time_t ts = (time_t)timestamp;
    struct tm* t = localtime(&ts);
    if (!t) return (CnextString){NULL, 0};
    size_t bufsize = fmt.length * 4 + 256;
    char* buffer = (char*)malloc(bufsize);
    if (!buffer) return (CnextString){NULL, 0};
    size_t written = strftime(buffer, bufsize, fmt.data, t);
    if (written == 0) { free(buffer); return (CnextString){NULL, 0}; }
    _cnext_track(buffer);
    return (CnextString){buffer, written};
}

#ifdef _WIN32
static inline const char* strptime_impl(const char* s, const char* fmt, struct tm* tm) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", s);
    int y = 0, M = 0, d = 0, h = 0, m = 0, sec = 0;
    const char* p = fmt;
    const char* src = s;
    memset(tm, 0, sizeof(*tm));
    while (*p && *src) {
        if (*p != '%') { if (*p != *src) return NULL; p++; src++; continue; }
        p++;
        switch (*p) {
            case 'Y': { char* end; y = (int)strtol(src, &end, 10); if (end == src) return NULL; src = end; break; }
            case 'y': { char* end; y = (int)strtol(src, &end, 10); if (end == src) return NULL; src = end; y += 2000; break; }
            case 'm': { char* end; M = (int)strtol(src, &end, 10); if (end == src) return NULL; src = end; break; }
            case 'd': { char* end; d = (int)strtol(src, &end, 10); if (end == src) return NULL; src = end; break; }
            case 'H': { char* end; h = (int)strtol(src, &end, 10); if (end == src) return NULL; src = end; break; }
            case 'M': { char* end; m = (int)strtol(src, &end, 10); if (end == src) return NULL; src = end; break; }
            case 'S': { char* end; sec = (int)strtol(src, &end, 10); if (end == src) return NULL; src = end; break; }
            case 'b':
            case 'B': {
                static const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec",NULL};
                for (int i = 0; months[i]; i++) {
                    size_t l = strlen(months[i]);
                    if (strncasecmp(src, months[i], l) == 0) { M = i + 1; src += l; break; }
                }
                break;
            }
            default: return NULL;
        }
        p++;
    }
    tm->tm_year = y - 1900;
    tm->tm_mon = M - 1;
    tm->tm_mday = d;
    tm->tm_hour = h;
    tm->tm_min = m;
    tm->tm_sec = sec;
    return src;
}
#define strptime strptime_impl
#endif

static inline long time_parse(CnextString formatted, CnextString fmt) {
    if (!formatted.data || !fmt.data) return -1;
    struct tm t;
    memset(&t, 0, sizeof(t));
    const char* result = strptime(formatted.data, fmt.data, &t);
    if (!result) return -1;
    time_t ts = mktime(&t);
    return (long)ts;
}

static inline int time_year(long timestamp) {
    time_t ts = (time_t)timestamp;
    struct tm* t = localtime(&ts);
    if (!t) return 0;
    return t->tm_year + 1900;
}

static inline int time_month(long timestamp) {
    time_t ts = (time_t)timestamp;
    struct tm* t = localtime(&ts);
    if (!t) return 0;
    return t->tm_mon + 1;
}

static inline int time_day(long timestamp) {
    time_t ts = (time_t)timestamp;
    struct tm* t = localtime(&ts);
    if (!t) return 0;
    return t->tm_mday;
}

static inline int time_hour(long timestamp) {
    time_t ts = (time_t)timestamp;
    struct tm* t = localtime(&ts);
    if (!t) return 0;
    return t->tm_hour;
}

static inline int time_minute(long timestamp) {
    time_t ts = (time_t)timestamp;
    struct tm* t = localtime(&ts);
    if (!t) return 0;
    return t->tm_min;
}

static inline int time_second(long timestamp) {
    time_t ts = (time_t)timestamp;
    struct tm* t = localtime(&ts);
    if (!t) return 0;
    return t->tm_sec;
}

#endif
