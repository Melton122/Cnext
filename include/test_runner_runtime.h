#ifndef CNEXT_TEST_RUNNER_RUNTIME_H
#define CNEXT_TEST_RUNNER_RUNTIME_H

#ifndef _WIN32
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>
#endif
#include "cnext_process.h"

static bool _cnext_test_append_name(char** text, size_t* length, size_t* capacity, const char* name) {
    if (!strcmp(name, ".") || !strcmp(name, "..")) return true;
    size_t count = strlen(name);
    if (strchr(name, '\n') || *length > SIZE_MAX - 2 || count > SIZE_MAX - *length - 2) return false;
    size_t required = *length + count + 2;
    if (required > *capacity) {
        size_t next = *capacity;
        while (next < required) {
            if (next > SIZE_MAX / 2) { next = required; break; }
            next *= 2;
        }
        char* grown = (char*)realloc(*text, next);
        if (!grown) return false;
        *text = grown; *capacity = next;
    }
    if (*length) (*text)[(*length)++] = '\n';
    memcpy(*text + *length, name, count);
    *length += count;
    (*text)[*length] = 0;
    return true;
}

/* Unlike os_read_dir on Windows, distinguish an empty directory from a failed
 * enumeration. An unreadable subtree must not silently disappear from a suite. */
static CnextString cnext_test_list_directory(CnextString path) {
    if (!path.data || !path.length || path.length > SIZE_MAX - 3 || memchr(path.data, 0, path.length)) return (CnextString){NULL, 0};
    char* filename = _cnext_spawn_dup(path);
    size_t capacity = 256, length = 0;
    char* text = (char*)malloc(capacity);
    if (!filename || !text) { free(filename); free(text); return (CnextString){NULL, 0}; }
    text[0] = 0;
    bool ok = true;
#ifdef _WIN32
    char* pattern = (char*)malloc(path.length + 3);
    if (!pattern) { free(filename); free(text); return (CnextString){NULL, 0}; }
    memcpy(pattern, filename, path.length);
    pattern[path.length] = '\\'; pattern[path.length + 1] = '*'; pattern[path.length + 2] = 0;
    WIN32_FIND_DATAA entry;
    HANDLE directory = FindFirstFileA(pattern, &entry);
    free(pattern);
    if (directory == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        DWORD attributes = GetFileAttributesA(filename);
        ok = error == ERROR_FILE_NOT_FOUND && attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
    } else {
        do {
            if (!_cnext_test_append_name(&text, &length, &capacity, entry.cFileName)) { ok = false; break; }
        } while (FindNextFileA(directory, &entry));
        if (ok && GetLastError() != ERROR_NO_MORE_FILES) ok = false;
        FindClose(directory);
    }
#else
    DIR* directory = opendir(filename);
    if (!directory) ok = false;
    else {
        struct dirent* entry;
        errno = 0;
        while ((entry = readdir(directory)) != NULL) {
            if (!_cnext_test_append_name(&text, &length, &capacity, entry->d_name)) { ok = false; break; }
            errno = 0;
        }
        if (errno) ok = false;
        closedir(directory);
    }
#endif
    free(filename);
    if (!ok) { free(text); return (CnextString){NULL, 0}; }
    _cnext_track(text);
    return (CnextString){text, length};
}

/* Internal self-hosted runner bridge. Redirect files directly without a shell.
 * The output must be new: callers cannot accidentally overwrite an existing
 * file. Empty stdin selects the null device so unattended tests do not block
 * waiting for input inherited from the compiler's terminal. */
static int cnext_test_execute(CnextArrayString argv, CnextString input, CnextString output) {
    if (!argv.data || !argv.length || argv.length > 4096 || !output.data || !output.length ||
        memchr(output.data, 0, output.length) || (input.length && (!input.data || memchr(input.data, 0, input.length)))) return -1;
    char** vec = (char**)calloc(argv.length + 1, sizeof(char*));
    if (!vec) return -1;
    for (size_t i = 0; i < argv.length; i++) {
        if (!argv.data[i].data || memchr(argv.data[i].data, 0, argv.data[i].length)) {
            _cnext_spawn_free(vec, i); return -1;
        }
        vec[i] = _cnext_spawn_dup(argv.data[i]);
        if (!vec[i]) { _cnext_spawn_free(vec, i); return -1; }
    }
    char* output_path = _cnext_spawn_dup(output);
    char* input_path = input.length ? _cnext_spawn_dup(input) : NULL;
    if (!output_path || (input.length && !input_path)) {
        free(output_path); free(input_path); _cnext_spawn_free(vec, argv.length); return -1;
    }
    int result = -1;
#ifdef _WIN32
    SECURITY_ATTRIBUTES security = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    HANDLE in = CreateFileA(input_path ? input_path : "NUL", GENERIC_READ,
                            FILE_SHARE_READ, &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE out = INVALID_HANDLE_VALUE;
    if (in != INVALID_HANDLE_VALUE)
        out = CreateFileA(output_path, GENERIC_WRITE, FILE_SHARE_READ, &security,
                          CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (out != INVALID_HANDLE_VALUE) {
        size_t cap = 1;
        bool fits = true;
        for (size_t i = 0; i < argv.length; i++) {
            size_t length = strlen(vec[i]);
            if (cap > SIZE_MAX - 3 || length > (SIZE_MAX - cap - 3) / 2) { fits = false; break; }
            cap += length * 2 + 3;
        }
        char* command = fits ? (char*)malloc(cap) : NULL;
        if (command) {
            size_t position = 0;
            for (size_t i = 0; i < argv.length; i++) {
                if (i) command[position++] = ' ';
                position = _cnext_spawn_quote(command, cap, position, vec[i]);
            }
            command[position] = 0;
            STARTUPINFOA startup = {0};
            startup.cb = sizeof(startup);
            startup.dwFlags = STARTF_USESTDHANDLES;
            startup.hStdInput = in; startup.hStdOutput = out; startup.hStdError = out;
            PROCESS_INFORMATION process = {0};
            if (CreateProcessA(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &startup, &process)) {
                DWORD status = 127;
                if (WaitForSingleObject(process.hProcess, INFINITE) == WAIT_OBJECT_0)
                    GetExitCodeProcess(process.hProcess, &status);
                result = (int)status;
                CloseHandle(process.hProcess); CloseHandle(process.hThread);
            } else result = 127;
            free(command);
        }
    }
    if (in != INVALID_HANDLE_VALUE) CloseHandle(in);
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
#else
    int in = open(input_path ? input_path : "/dev/null", O_RDONLY);
    int out = in < 0 ? -1 : open(output_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (out >= 0) {
        pid_t child = fork();
        if (child == 0) {
            if (dup2(in, STDIN_FILENO) < 0 || dup2(out, STDOUT_FILENO) < 0 || dup2(out, STDERR_FILENO) < 0) _exit(127);
            if (in > STDERR_FILENO) close(in);
            if (out > STDERR_FILENO) close(out);
            execvp(vec[0], vec);
            _exit(127);
        }
        if (child > 0) {
            int status = 0;
            pid_t waited;
            do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
            if (waited == child) result = WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 127;
        } else result = 127;
    }
    if (in >= 0) close(in);
    if (out >= 0) close(out);
#endif
    free(output_path); free(input_path); _cnext_spawn_free(vec, argv.length);
    return result;
}

#endif
