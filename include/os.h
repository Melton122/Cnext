#ifndef CNEXT_OS_H
#define CNEXT_OS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <process.h>
#define getcwd _getcwd
#define chdir _chdir
#else
#include <unistd.h>
#include <sys/utsname.h>
#endif

static inline CnextString os_getenv(CnextString name) {
#ifdef _WIN32
    DWORD len = GetEnvironmentVariableA(name.data, NULL, 0);
    if (len == 0) return (CnextString){NULL, 0};
    char* buffer = (char*)malloc(len);
    if (!buffer) return (CnextString){NULL, 0};
    GetEnvironmentVariableA(name.data, buffer, len);
    _cnext_track(buffer);
    return (CnextString){buffer, len - 1};
#else
    char* val = getenv(name.data);
    if (!val) return (CnextString){NULL, 0};
    size_t len = strlen(val);
    char* buffer = (char*)malloc(len + 1);
    if (!buffer) return (CnextString){NULL, 0};
    memcpy(buffer, val, len + 1);
    _cnext_track(buffer);
    return (CnextString){buffer, len};
#endif
}

static inline bool os_setenv(CnextString name, CnextString value) {
#ifdef _WIN32
    return SetEnvironmentVariableA(name.data, value.data) != 0;
#else
    return setenv(name.data, value.data, 1) == 0;
#endif
}

static inline bool os_unsetenv(CnextString name) {
#ifdef _WIN32
    return SetEnvironmentVariableA(name.data, NULL) != 0;
#else
    return unsetenv(name.data) == 0;
#endif
}

static inline int os_exec(CnextString command) {
    return system(command.data);
}

static inline CnextString os_get_cwd(void) {
#ifdef _WIN32
    DWORD len = GetCurrentDirectoryA(0, NULL);
    if (len == 0) return (CnextString){NULL, 0};
    char* buffer = (char*)malloc(len);
    if (!buffer) return (CnextString){NULL, 0};
    GetCurrentDirectoryA(len, buffer);
    _cnext_track(buffer);
    return (CnextString){buffer, strlen(buffer)};
#else
    long len = pathconf(".", _PC_PATH_MAX);
    if (len <= 0) len = 4096;
    char* buffer = (char*)malloc((size_t)len);
    if (!buffer) return (CnextString){NULL, 0};
    if (!getcwd(buffer, (size_t)len)) { free(buffer); return (CnextString){NULL, 0}; }
    _cnext_track(buffer);
    return (CnextString){buffer, strlen(buffer)};
#endif
}

static inline bool os_set_cwd(CnextString path) {
    return chdir(path.data) == 0;
}

static inline CnextString os_temp_dir(void) {
#ifdef _WIN32
    DWORD len = GetTempPathA(0, NULL);
    if (len == 0) return (CnextString){NULL, 0};
    char* buffer = (char*)malloc(len);
    if (!buffer) return (CnextString){NULL, 0};
    GetTempPathA(len, buffer);
    size_t slen = strlen(buffer);
    if (slen > 0 && buffer[slen - 1] == '\\') buffer[slen - 1] = '\0';
    _cnext_track(buffer);
    return (CnextString){buffer, strlen(buffer)};
#else
    const char* tmp = getenv("TMPDIR");
    if (!tmp) tmp = "/tmp";
    size_t len = strlen(tmp);
    char* buffer = (char*)malloc(len + 1);
    if (!buffer) return (CnextString){NULL, 0};
    memcpy(buffer, tmp, len + 1);
    _cnext_track(buffer);
    return (CnextString){buffer, len};
#endif
}

static inline CnextString os_home_dir(void) {
    const char* home = getenv("HOME");
#ifdef _WIN32
    if (!home) home = getenv("USERPROFILE");
#endif
    if (!home) return (CnextString){NULL, 0};
    size_t len = strlen(home);
    char* buffer = (char*)malloc(len + 1);
    if (!buffer) return (CnextString){NULL, 0};
    memcpy(buffer, home, len + 1);
    _cnext_track(buffer);
    return (CnextString){buffer, len};
}

static inline bool os_mkdir(CnextString path) {
#ifdef _WIN32
    return _mkdir(path.data) == 0 || errno == EEXIST;
#else
    return mkdir(path.data, 0755) == 0 || errno == EEXIST;
#endif
}

static inline bool os_rmdir(CnextString path) {
#ifdef _WIN32
    return _rmdir(path.data) == 0;
#else
    return rmdir(path.data) == 0;
#endif
}

static inline CnextString os_os_name(void) {
#ifdef _WIN32
    return (CnextString){(char*)"Windows", 7};
#elif __APPLE__
    return (CnextString){(char*)"macOS", 5};
#elif __linux__
    return (CnextString){(char*)"Linux", 5};
#else
    return (CnextString){(char*)"Unknown", 7};
#endif
}

static inline CnextString os_hostname(void) {
#ifdef _WIN32
    DWORD len = MAX_COMPUTERNAME_LENGTH + 1;
    char* buffer = (char*)malloc(len);
    if (!buffer) return (CnextString){NULL, 0};
    if (!GetComputerNameA(buffer, &len)) { free(buffer); return (CnextString){NULL, 0}; }
    _cnext_track(buffer);
    return (CnextString){buffer, strlen(buffer)};
#else
    char* buffer = (char*)malloc(256);
    if (!buffer) return (CnextString){NULL, 0};
    if (gethostname(buffer, 256) != 0) { free(buffer); return (CnextString){NULL, 0}; }
    _cnext_track(buffer);
    return (CnextString){buffer, strlen(buffer)};
#endif
}

static inline int os_pid(void) {
#ifdef _WIN32
    return (int)GetCurrentProcessId();
#else
    return (int)getpid();
#endif
}

#endif
