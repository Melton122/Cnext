#ifndef CNEXT_PROCESS_H
#define CNEXT_PROCESS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

static int process_run_status(CnextString command) {
    if (!command.data) return -1;
    char* cmd = strndup(command.data, command.length);
    if (!cmd) return -1;
    int rc = system(cmd);
    free(cmd);
#ifdef _WIN32
    return rc;
#else
    if (WIFEXITED(rc)) return WEXITSTATUS(rc);
    return -1;
#endif
}

static CnextString process_run(CnextString command) {
    if (!command.data) return (CnextString){NULL, 0};
    char* cmd = strndup(command.data, command.length);
    if (!cmd) return (CnextString){NULL, 0};
#ifdef _WIN32
    HANDLE rd, wr;
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, 1};
    if (!CreatePipe(&rd, &wr, &sa, 0)) { free(cmd); return (CnextString){NULL, 0}; }
    char* full = (char*)malloc(strlen(cmd) + 32);
    if (!full) { CloseHandle(rd); CloseHandle(wr); free(cmd); return (CnextString){NULL, 0}; }
    snprintf(full, strlen(cmd) + 32, "cmd.exe /c %s", cmd);
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError = wr;
    PROCESS_INFORMATION pi;
    int ok = CreateProcessA(NULL, full, NULL, NULL, 1, 0, NULL, NULL, &si, &pi);
    free(full);
    CloseHandle(wr);
    if (!ok) { CloseHandle(rd); free(cmd); return (CnextString){NULL, 0}; }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    DWORD avail = 0;
    PeekNamedPipe(rd, NULL, 0, NULL, &avail, NULL);
    if (avail == 0) { CloseHandle(rd); free(cmd); return (CnextString){NULL, 0}; }
    char* out = (char*)malloc(avail + 1);
    if (!out) { CloseHandle(rd); free(cmd); return (CnextString){NULL, 0}; }
    DWORD read_bytes = 0;
    ReadFile(rd, out, avail, &read_bytes, NULL);
    out[read_bytes] = '\0';
    while (read_bytes > 0 && (out[read_bytes-1] == '\n' || out[read_bytes-1] == '\r')) read_bytes--;
    out[read_bytes] = '\0';
    CloseHandle(rd);
    free(cmd);
    _cnext_track(out);
    return (CnextString){out, (size_t)read_bytes};
#else
    char* full = (char*)malloc(strlen(cmd) + 16);
    if (!full) { free(cmd); return (CnextString){NULL, 0}; }
    snprintf(full, strlen(cmd) + 16, "%s 2>&1", cmd);
    FILE* fp = popen(full, "r");
    free(full);
    if (!fp) { free(cmd); return (CnextString){NULL, 0}; }
    size_t cap = 256, len = 0;
    char* out = (char*)malloc(cap);
    if (!out) { pclose(fp); free(cmd); return (CnextString){NULL, 0}; }
    while (fgets(out + len, (int)(cap - len), fp)) {
        len += strlen(out + len);
        if (len + 1 >= cap) {
            size_t new_cap = cap * 2;
            char* new_out = (char*)realloc(out, new_cap);
            if (!new_out) break;
            out = new_out;
            cap = new_cap;
        }
    }
    pclose(fp);
    while (len > 0 && (out[len-1] == '\n' || out[len-1] == '\r')) len--;
    out[len] = '\0';
    free(cmd);
    _cnext_track(out);
    return (CnextString){out, len};
#endif
}

/* Copy one argv element into a NUL-terminated buffer. Cnext strings may be
   slices, so the length (not a NUL) decides where the text ends. */
static char* _cnext_spawn_dup(CnextString value) {
    if (!value.data) return NULL;
    char* copy = (char*)malloc(value.length + 1);
    if (!copy) return NULL;
    memcpy(copy, value.data, value.length);
    copy[value.length] = '\0';
    return copy;
}

static void _cnext_spawn_free(char** vec, size_t count) {
    for (size_t i = 0; i < count; i++) free(vec[i]);
    free(vec);
}

#ifdef _WIN32
/* Append arg to a CommandLineToArgvW-compatible command line. Each token is
   wrapped in quotes; runs of backslashes are doubled, and a run immediately
   before a literal '"' gains one extra so the quote stays data instead of
   closing the token. */
static size_t _cnext_spawn_quote(char* dst, size_t cap, size_t pos, const char* arg) {
    if (pos < cap - 1) dst[pos++] = '"';
    int slashes = 0;
    for (const char* p = arg; *p; p++) {
        if (*p == '\\') {
            slashes++;
            continue;
        }
        int n = (*p == '"') ? slashes * 2 + 1 : slashes;
        for (int s = 0; s < n; s++)
            if (pos < cap - 1) dst[pos++] = '\\';
        slashes = 0;
        if (pos < cap - 1) dst[pos++] = *p;
    }
    for (int s = 0; s < slashes * 2; s++)
        if (pos < cap - 1) dst[pos++] = '\\';
    if (pos < cap - 1) dst[pos++] = '"';
    return pos;
}
#endif

/* Run a program straight from an argv vector: no shell, no re-parsing, no
   globbing and no metacharacter interpretation. argv[0] is the program, the
   remaining elements are passed verbatim, so untrusted arguments cannot turn
   into extra commands. Returns the child's exit status, 127 when the program
   could not be started, or -1 when argv is empty or unusable. Use the
   process_run* functions only when shell syntax is genuinely wanted. */
static int process_spawn(CnextArrayString argv) {
    if (!argv.data || argv.length == 0) return -1;
    char** vec = (char**)calloc(argv.length + 1, sizeof(char*));
    if (!vec) return -1;
    for (size_t i = 0; i < argv.length; i++) {
        vec[i] = _cnext_spawn_dup(argv.data[i]);
        if (!vec[i]) { _cnext_spawn_free(vec, i); return -1; }
    }
#ifdef _WIN32
    size_t cap = 1;
    for (size_t i = 0; i < argv.length; i++) cap += 2 * strlen(vec[i]) + 3;
    char* cmdline = (char*)malloc(cap);
    if (!cmdline) { _cnext_spawn_free(vec, argv.length); return -1; }
    size_t pos = 0;
    for (size_t i = 0; i < argv.length; i++) {
        if (i > 0) cmdline[pos++] = ' ';
        pos = _cnext_spawn_quote(cmdline, cap, pos, vec[i]);
    }
    cmdline[pos] = '\0';
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    int started = CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    free(cmdline);
    _cnext_spawn_free(vec, argv.length);
    if (!started) return 127;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 127;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
#else
    pid_t pid = fork();
    if (pid < 0) { _cnext_spawn_free(vec, argv.length); return 127; }
    if (pid == 0) {
        execvp(vec[0], vec);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        _cnext_spawn_free(vec, argv.length);
        return 127;
    }
    _cnext_spawn_free(vec, argv.length);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 127;
#endif
}

#ifndef _WIN32
static int process_kill(int pid) {
    return kill((pid_t)pid, 9) == 0;
}
static int process_exists(int pid) {
    return kill((pid_t)pid, 0) == 0;
}
#else
static int process_kill(int pid) {
    HANDLE h = OpenProcess(PROCESS_TERMINATE, 0, (DWORD)pid);
    if (!h) return 0;
    int r = TerminateProcess(h, 1) ? 1 : 0;
    CloseHandle(h);
    return r;
}
static int process_exists(int pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, (DWORD)pid);
    if (!h) return 0;
    DWORD exit_code;
    int r = GetExitCodeProcess(h, &exit_code) && exit_code == STILL_ACTIVE ? 1 : 0;
    CloseHandle(h);
    return r;
}
#endif

#endif
