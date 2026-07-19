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

static inline CnextString os_exec_output(CnextString command) {
    FILE* pipe = popen(command.data, "r");
    if (!pipe) return (CnextString){NULL, 0};
    size_t cap = 4096;
    size_t len = 0;
    char* buffer = (char*)malloc(cap);
    if (!buffer) { pclose(pipe); return (CnextString){NULL, 0}; }
    size_t n;
    while ((n = fread(buffer + len, 1, cap - len - 1, pipe)) > 0) {
        len += n;
        if (len + 1 >= cap) {
            cap *= 2;
            char* nb = (char*)realloc(buffer, cap);
            if (!nb) { free(buffer); pclose(pipe); return (CnextString){NULL, 0}; }
            buffer = nb;
        }
    }
    buffer[len] = '\0';
    pclose(pipe);
    _cnext_track(buffer);
    return (CnextString){buffer, len};
}

static inline int os_exec_status(CnextString command) {
    return system(command.data);
}

static inline CnextString os_arch(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return (CnextString){(char*)"x86_64", 6};
#elif defined(__aarch64__) || defined(_M_ARM64)
    return (CnextString){(char*)"aarch64", 7};
#elif defined(__i386__) || defined(_M_IX86)
    return (CnextString){(char*)"x86", 3};
#elif defined(__arm__) || defined(_M_ARM)
    return (CnextString){(char*)"arm", 3};
#else
    return (CnextString){(char*)"unknown", 7};
#endif
}

static inline bool os_file_exists(CnextString path) {
#ifdef _WIN32
    return GetFileAttributesA(path.data) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path.data, &st) == 0;
#endif
}

static inline bool os_is_dir(CnextString path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path.data);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path.data, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

static inline long os_file_size(CnextString path) {
    FILE* f = fopen(path.data, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return size;
}

static inline bool os_rename(CnextString old_path, CnextString new_path) {
    return rename(old_path.data, new_path.data) == 0;
}

static inline bool os_remove(CnextString path) {
    return remove(path.data) == 0;
}

static inline CnextString os_read_dir(CnextString path) {
#ifdef _WIN32
    char pattern[4096];
    snprintf(pattern, sizeof(pattern), "%s\\*", path.data);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return (CnextString){NULL, 0};
    size_t cap = 4096;
    char* buffer = (char*)malloc(cap);
    if (!buffer) { FindClose(hFind); return (CnextString){NULL, 0}; }
    buffer[0] = '\0';
    size_t len = 0;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        size_t nlen = strlen(fd.cFileName);
        if (len + nlen + 2 > cap) { cap *= 2; char* nb = (char*)realloc(buffer, cap); if (!nb) break; buffer = nb; }
        if (len > 0) { buffer[len++] = '\n'; }
        memcpy(buffer + len, fd.cFileName, nlen);
        len += nlen;
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
    buffer[len] = '\0';
    _cnext_track(buffer);
    return (CnextString){buffer, len};
#else
    DIR* dir = opendir(path.data);
    if (!dir) return (CnextString){NULL, 0};
    size_t cap = 4096;
    char* buffer = (char*)malloc(cap);
    if (!buffer) { closedir(dir); return (CnextString){NULL, 0}; }
    buffer[0] = '\0';
    size_t len = 0;
    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        size_t nlen = strlen(ent->d_name);
        if (len + nlen + 2 > cap) { cap *= 2; char* nb = (char*)realloc(buffer, cap); if (!nb) break; buffer = nb; }
        if (len > 0) { buffer[len++] = '\n'; }
        memcpy(buffer + len, ent->d_name, nlen);
        len += nlen;
    }
    closedir(dir);
    buffer[len] = '\0';
    _cnext_track(buffer);
    return (CnextString){buffer, len};
#endif
}

static inline bool os_mkdir_p(CnextString path) {
#ifdef _WIN32
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path.data);
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '\\') {
            char c = *p;
            *p = '\0';
            _mkdir(tmp);
            *p = c;
        }
    }
    return _mkdir(tmp) == 0 || errno == EEXIST;
#else
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path.data);
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            char c = *p;
            *p = '\0';
            mkdir(tmp, 0755);
            *p = c;
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
#endif
}

#endif
