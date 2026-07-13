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
    sprintf(full, "cmd.exe /c %s", cmd);
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
    sprintf(full, "%s 2>&1", cmd);
    FILE* fp = popen(full, "r");
    free(full);
    if (!fp) { free(cmd); return (CnextString){NULL, 0}; }
    size_t cap = 256, len = 0;
    char* out = (char*)malloc(cap);
    if (!out) { pclose(fp); free(cmd); return (CnextString){NULL, 0}; }
    while (fgets(out + len, (int)(cap - len), fp)) {
        len += strlen(out + len);
        if (len + 1 >= cap) { cap *= 2; out = (char*)realloc(out, cap); }
    }
    pclose(fp);
    while (len > 0 && (out[len-1] == '\n' || out[len-1] == '\r')) len--;
    out[len] = '\0';
    free(cmd);
    _cnext_track(out);
    return (CnextString){out, len};
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
