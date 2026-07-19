#ifndef CNEXT_LOG_H
#define CNEXT_LOG_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>

typedef enum {
    LOG_TRACE = 0,
    LOG_DEBUG = 1,
    LOG_INFO  = 2,
    LOG_WARN  = 3,
    LOG_ERROR = 4,
    LOG_FATAL = 5,
    LOG_OFF   = 6
} LogLevel;

static LogLevel _cnext_log_level = LOG_INFO;
static bool _cnext_log_color = true;
static bool _cnext_log_time = true;
static FILE* _cnext_log_file = NULL;

static inline void log_set_level(LogLevel level) { _cnext_log_level = level; }
static inline void log_set_color(bool enabled) { _cnext_log_color = enabled; }
static inline void log_set_timestamps(bool enabled) { _cnext_log_time = enabled; }
static inline void log_set_file(const char* path) {
    if (_cnext_log_file && _cnext_log_file != stderr) fclose(_cnext_log_file);
    _cnext_log_file = path ? fopen(path, "a") : NULL;
}

static inline const char* _log_level_str(LogLevel level) {
    switch (level) {
        case LOG_TRACE: return "TRACE";
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO ";
        case LOG_WARN:  return "WARN ";
        case LOG_ERROR: return "ERROR";
        case LOG_FATAL: return "FATAL";
        default:        return "?????";
    }
}

static inline const char* _log_level_color(LogLevel level) {
    if (!_cnext_log_color) return "";
    switch (level) {
        case LOG_TRACE: return "\033[90m";
        case LOG_DEBUG: return "\033[36m";
        case LOG_INFO:  return "\033[32m";
        case LOG_WARN:  return "\033[33m";
        case LOG_ERROR: return "\033[31m";
        case LOG_FATAL: return "\033[35;1m";
        default:        return "";
    }
}

static inline void _log_write(LogLevel level, const char* fmt, va_list args) {
    if (level < _cnext_log_level) return;

    FILE* out = _cnext_log_file ? _cnext_log_file : stderr;

    if (_cnext_log_time) {
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        char timebuf[32];
        strftime(timebuf, sizeof(timebuf), "%H:%M:%S", t);
        fprintf(out, "%s%s ", _log_level_color(level), timebuf);
    }

    fprintf(out, "%s%s%s ", _log_level_color(level), _log_level_str(level),
        _cnext_log_color ? "\033[0m" : "");

    vfprintf(out, fmt, args);
    fprintf(out, "\n");
    fflush(out);
}

static inline void log_trace(const char* fmt, ...) { va_list a; va_start(a, fmt); _log_write(LOG_TRACE, fmt, a); va_end(a); }
static inline void log_debug(const char* fmt, ...) { va_list a; va_start(a, fmt); _log_write(LOG_DEBUG, fmt, a); va_end(a); }
static inline void log_info(const char* fmt, ...)  { va_list a; va_start(a, fmt); _log_write(LOG_INFO, fmt, a);  va_end(a); }
static inline void log_warn(const char* fmt, ...)  { va_list a; va_start(a, fmt); _log_write(LOG_WARN, fmt, a);  va_end(a); }
static inline void log_error(const char* fmt, ...) { va_list a; va_start(a, fmt); _log_write(LOG_ERROR, fmt, a); va_end(a); }
static inline void log_fatal(const char* fmt, ...) { va_list a; va_start(a, fmt); _log_write(LOG_FATAL, fmt, a); va_end(a); }

static inline void log_close(void) {
    if (_cnext_log_file && _cnext_log_file != stderr) {
        fclose(_cnext_log_file);
        _cnext_log_file = NULL;
    }
}

#endif
