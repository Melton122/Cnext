#include "main_internal.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#endif

static bool append_captured_output(char* output_buf, size_t output_buf_size, size_t* total, const char* chunk, size_t chunk_len) {
    if (output_buf_size == 0) return chunk_len > 0;
    size_t limit = output_buf_size - 1;
    if (*total > limit) *total = limit;
    size_t room = limit - *total;
    size_t copy = chunk_len < room ? chunk_len : room;
    if (copy > 0) memcpy(output_buf + *total, chunk, copy);
    *total += copy;
    return copy < chunk_len;
}

static void report_captured_truncation(bool truncated) {
    if (truncated) fprintf(stderr, "Warning: captured output truncated.\n");
}

static bool path_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

bool remove_tree(const char* path) {
    if (!path_exists(path)) return true;
#ifdef _WIN32
    char pattern[CNEXT_PATH_MAX];
    int written = snprintf(pattern, sizeof(pattern), "%s\\*", path);
    if (written < 0 || (size_t)written >= sizeof(pattern)) return false;
    WIN32_FIND_DATAA data;
    HANDLE handle = FindFirstFileA(pattern, &data);
    if (handle == INVALID_HANDLE_VALUE) return false;
    do {
        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) continue;
        char child[CNEXT_PATH_MAX];
        if (join_path(child, sizeof(child), path, data.cFileName) != 0) {
            FindClose(handle);
            return false;
        }
        bool ok = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            ? remove_tree(child)
            : DeleteFileA(child) != 0;
        if (!ok) {
            FindClose(handle);
            return false;
        }
    } while (FindNextFileA(handle, &data));
    FindClose(handle);
    return RemoveDirectoryA(path) != 0;
#else
    DIR* dir = opendir(path);
    if (!dir) return false;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char child[CNEXT_PATH_MAX];
        if (join_path(child, sizeof(child), path, entry->d_name) != 0) {
            closedir(dir);
            return false;
        }
        struct stat st;
        bool ok = stat(child, &st) != 0 || S_ISDIR(st.st_mode)
            ? remove_tree(child)
            : unlink(child) == 0;
        if (!ok) {
            closedir(dir);
            return false;
        }
    }
    closedir(dir);
    return rmdir(path) == 0;
#endif
}

int count_tree_entries(const char* path) {
#ifdef _WIN32
    char pattern[CNEXT_PATH_MAX];
    int written = snprintf(pattern, sizeof(pattern), "%s\\*", path);
    if (written < 0 || (size_t)written >= sizeof(pattern)) return -1;
    WIN32_FIND_DATAA data;
    HANDLE handle = FindFirstFileA(pattern, &data);
    if (handle == INVALID_HANDLE_VALUE) return -1;
    int count = 0;
    do {
        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) continue;
        count++;
    } while (FindNextFileA(handle, &data));
    FindClose(handle);
    return count;
#else
    DIR* dir = opendir(path);
    if (!dir) return -1;
    int count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        count++;
    }
    closedir(dir);
    return count;
#endif
}

char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Could not open file \"%s\": %s\n", path, strerror(errno));
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        fprintf(stderr, "Could not seek file \"%s\".\n", path);
        fclose(file);
        return NULL;
    }

    long file_size = ftell(file);
    if (file_size < 0) {
        fprintf(stderr, "Could not determine size of \"%s\".\n", path);
        fclose(file);
        return NULL;
    }
    rewind(file);

    char* buffer = (char*)checked_malloc((size_t)file_size + 1);
    if (!buffer) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(buffer, sizeof(char), (size_t)file_size, file);
    if (bytes_read < (size_t)file_size && ferror(file)) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[bytes_read] = '\0';
    fclose(file);
    return buffer;
}

bool copy_file(const char* source_path, const char* destination_path) {
    FILE* source = fopen(source_path, "rb");
    if (!source) {
        fprintf(stderr, "Could not open package source \"%s\": %s\n", source_path, strerror(errno));
        return false;
    }

    FILE* destination = fopen(destination_path, "wb");
    if (!destination) {
        fprintf(stderr, "Could not create package file \"%s\": %s\n", destination_path, strerror(errno));
        fclose(source);
        return false;
    }

    char buffer[8192];
    bool ok = true;
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        if (fwrite(buffer, 1, bytes_read, destination) != bytes_read) {
            fprintf(stderr, "Could not write package file \"%s\".\n", destination_path);
            ok = false;
            break;
        }
    }
    if (ferror(source)) {
        fprintf(stderr, "Could not read package source \"%s\".\n", source_path);
        ok = false;
    }

    if (fclose(destination) != 0) ok = false;
    fclose(source);
    if (!ok) remove(destination_path);
    return ok;
}

bool has_path_separator(const char* path) {
    return strchr(path, '/') != NULL || strchr(path, '\\') != NULL;
}

#if defined(_WIN32)
static bool resolve_executable_path(char* out, size_t size) {
    DWORD n = GetModuleFileNameA(NULL, out, (DWORD)size);
    return n > 0 && n < (DWORD)size;
}
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
static bool resolve_executable_path(char* out, size_t size) {
    uint32_t sz = (uint32_t)size;
    return _NSGetExecutablePath(out, &sz) == 0;
}
#else
#include <limits.h>
#include <unistd.h>
static bool resolve_executable_path(char* out, size_t size) {
    ssize_t n = readlink("/proc/self/exe", out, size - 1);
    if (n <= 0) return false;
    out[n] = '\0';
    return true;
}
#endif

bool dirname_from_path(const char* path, char* buffer, size_t buffer_size) {
    const char* last_slash = strrchr(path, '/');
    const char* last_backslash = strrchr(path, '\\');
    const char* sep = NULL;
    if (last_slash && last_backslash) {
        sep = last_slash > last_backslash ? last_slash : last_backslash;
    } else if (last_slash) {
        sep = last_slash;
    } else {
        sep = last_backslash;
    }
    if (!sep) return false;

    size_t length = (size_t)(sep - path);
    if (length == 0 || length + 1 > buffer_size) return false;
    memcpy(buffer, path, length);
    buffer[length] = '\0';
    return true;
}

static bool probe_include_path(const char* dir_path) {
    char probe[CNEXT_PATH_MAX];
    int written = snprintf(probe, sizeof(probe), "%s%cruntime.h", dir_path, CNEXT_PATH_SEP);
    if (written < 0 || (size_t)written >= sizeof(probe)) return false;
    FILE* f = fopen(probe, "r");
    if (f) { fclose(f); return true; }
    return false;
}

bool build_include_path(const char* argv0, char* include_path, size_t include_path_size) {
    char base[CNEXT_PATH_MAX];
    char exe_resolved[CNEXT_PATH_MAX];
    const char* include_dir = "include";
    char resolved_include_path[CNEXT_PATH_MAX];

    // `cnext` invoked via PATH yields a bare argv[0] with no directory —
    // resolve the real executable location so the installed-header probe works
    if (!has_path_separator(argv0) && resolve_executable_path(exe_resolved, sizeof(exe_resolved))) {
        argv0 = exe_resolved;
    }

    if (dirname_from_path(argv0, base, sizeof(base))) {
        size_t base_len = strlen(base);
        const char* include_leaf = "include";
        size_t include_len = strlen(include_leaf);

        /* Try <exe_dir>/include first (works for dev builds) */
        if (base_len + 1 + include_len + 1 <= sizeof(resolved_include_path)) {
            memcpy(resolved_include_path, base, base_len);
            resolved_include_path[base_len] = CNEXT_PATH_SEP;
            memcpy(resolved_include_path + base_len + 1, include_leaf, include_len + 1);

            if (probe_include_path(resolved_include_path)) {
                include_dir = resolved_include_path;
            } else {
                /* Try <exe_dir>/../include (works for installed builds with bin/ prefix) */
                const char* last_sep = strrchr(base, CNEXT_PATH_SEP);
                if (last_sep && last_sep > base) {
                    size_t parent_len = (size_t)(last_sep - base);
                    size_t leaf_len = strlen(include_leaf);
                    if (parent_len + 1 + leaf_len + 1 <= sizeof(resolved_include_path)) {
                        memcpy(resolved_include_path, base, parent_len);
                        resolved_include_path[parent_len] = CNEXT_PATH_SEP;
                        memcpy(resolved_include_path + parent_len + 1, include_leaf, leaf_len + 1);
                        if (probe_include_path(resolved_include_path)) {
                            include_dir = resolved_include_path;
                        } else {
                            /* Try <exe_dir>/../share/cnext/include (POSIX install layout) */
                            const char* share_leaf = "share";
                            const char* cnext_leaf = "cnext";
                            const char* share_include_leaf = "include";
                            if (parent_len + 1 + strlen(share_leaf) + 1 +
                                strlen(cnext_leaf) + 1 + strlen(share_include_leaf) + 1 <=
                                sizeof(resolved_include_path)) {
                                size_t pos = 0;
                                memcpy(resolved_include_path + pos, base, parent_len);
                                pos += parent_len;
                                resolved_include_path[pos++] = CNEXT_PATH_SEP;
                                memcpy(resolved_include_path + pos, share_leaf, strlen(share_leaf));
                                pos += strlen(share_leaf);
                                resolved_include_path[pos++] = CNEXT_PATH_SEP;
                                memcpy(resolved_include_path + pos, cnext_leaf, strlen(cnext_leaf));
                                pos += strlen(cnext_leaf);
                                resolved_include_path[pos++] = CNEXT_PATH_SEP;
                                memcpy(resolved_include_path + pos, share_include_leaf, strlen(share_include_leaf));
                                pos += strlen(share_include_leaf);
                                resolved_include_path[pos] = '\0';
                                if (probe_include_path(resolved_include_path)) {
                                    include_dir = resolved_include_path;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    int written = snprintf(include_path, include_path_size, "%s", include_dir);
    if (written < 0 || (size_t)written >= include_path_size) {
        fprintf(stderr, "Include path is too long.\n");
        return false;
    }
    return true;
}

#ifdef _WIN32
static size_t append_quoted_arg(char* dst, size_t cap, size_t pos, const char* arg);
#endif

int run_process(const char* program, const char* const args[]) {
#ifdef _WIN32
    // _spawnvp joins argv with spaces without quoting it. Preserve each token
    // using the same Windows quoting rules as the captured-process helpers.
    size_t count = 0;
    while (args[count]) count++;
    char** quoted = calloc(count + 1, sizeof(*quoted));
    for (size_t i = 0; i < count; i++) {
        size_t capacity = 2 * strlen(args[i]) + 3;
        quoted[i] = malloc(capacity);
        size_t end = append_quoted_arg(quoted[i], capacity, 0, args[i]);
        quoted[i][end] = '\0';
    }
    intptr_t result = _spawnvp(_P_WAIT, program, (const char* const*)quoted);
    int saved_errno = errno;
    for (size_t i = 0; i < count; i++) free(quoted[i]);
    free(quoted);
    if (result == -1) {
        fprintf(stderr, "Could not start %s: %s\n", program, strerror(saved_errno));
        return 127;
    }
    return (int)result;
#else
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Could not start %s: %s\n", program, strerror(errno));
        return 127;
    }
    if (pid == 0) {
        execvp(program, (char* const*)args);
        fprintf(stderr, "Could not start %s: %s\n", program, strerror(errno));
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "Could not wait for %s: %s\n", program, strerror(errno));
        return 127;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 127;
#endif
}

const char* executable_command(const char* output_exe, char* buffer, size_t buffer_size) {
    if (has_path_separator(output_exe)) return output_exe;

#ifdef _WIN32
    int written = snprintf(buffer, buffer_size, ".\\%s", output_exe);
#else
    int written = snprintf(buffer, buffer_size, "./%s", output_exe);
#endif
    if (written < 0 || (size_t)written >= buffer_size) return output_exe;
    return buffer;
}

int make_dir(const char* path) {
#ifdef _WIN32
    if (_mkdir(path) == 0 || errno == EEXIST) return 0;
#else
    if (mkdir(path, 0755) == 0 || errno == EEXIST) return 0;
#endif
    fprintf(stderr, "Could not create directory \"%s\": %s\n", path, strerror(errno));
    return 1;
}

int join_path(char* out, size_t out_size, const char* base, const char* name) {
    int written = snprintf(out, out_size, "%s%c%s", base, CNEXT_PATH_SEP, name);
    if (written < 0 || (size_t)written >= out_size) {
        fprintf(stderr, "Path is too long: %s/%s\n", base, name);
        return 1;
    }
    return 0;
}

int parse_expect_lines(const char* source_path, ExpectedLine* out, int max) {
    FILE* f = fopen(source_path, "r");
    if (!f) return 0;

    int count = 0;
    char line[4096];
    while (count < max && fgets(line, sizeof(line), f)) {
        const char* prefix = "// EXPECT: ";
        size_t prefix_len = strlen(prefix);
        if (strncmp(line, prefix, prefix_len) == 0) {
            const char* text = line + prefix_len;
            size_t len = strlen(text);
            while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) len--;
            if (len < EXPECT_LINE_MAX) {
                memcpy(out[count].text, text, len);
                out[count].text[len] = '\0';
                count++;
            }
        }
    }
    fclose(f);
    return count;
}

/* Collect the `// STDIN: ...` lines of a test file. Each line becomes one
   line of input for the program under test, so a test that reads typed input
   can say what it reads. */
size_t parse_stdin_lines(const char* source_path, char* out, size_t out_size) {
    if (out_size == 0) return 0;
    out[0] = '\0';
    FILE* f = fopen(source_path, "r");
    if (!f) return 0;

    size_t total = 0;
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        const char* prefix = "// STDIN:";
        size_t prefix_len = strlen(prefix);
        if (strncmp(line, prefix, prefix_len) == 0) {
            const char* text = line + prefix_len;
            /* One optional space separates the marker from the text, so an
               empty marker line is an empty line of input. */
            if (*text == ' ') text++;
            size_t len = strlen(text);
            while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) len--;
            if (total + len + 1 >= out_size) break;
            memcpy(out + total, text, len);
            total += len;
            out[total++] = '\n';
            out[total] = '\0';
        }
    }
    fclose(f);
    return total;
}

/* Append arg to dst as a single CommandLineToArgvW-quoted token, starting the
   token with a leading double quote (argv entries are space separated) and
   returning the new write position. Quoting rule: runs of backslashes are
   doubled, and a run immediately before a literal '"' gains one extra so the
   quote survives as data instead of closing the token. */
static size_t append_quoted_arg(char* dst, size_t cap, size_t pos, const char* arg) {
    if (pos < cap - 1) dst[pos++] = '"';
    if (!*arg) {
        if (pos < cap - 1) dst[pos++] = '"';
        return pos;
    }
    int slashes = 0;
    for (const char* p = arg; *p; p++) {
        if (*p == '\\') {
            slashes++;
            continue;
        }
        /* A run of backslashes is doubled only when it precedes a quote (the
           extra one escapes the quote). Before an ordinary character the run
           is emitted verbatim. */
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

int run_process_quiet(const char* program, const char* const args[]) {
    if (!program || program[0] == '\0') return 127;
#ifdef _WIN32
    HANDLE devnull = CreateFileA("nul", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (devnull == INVALID_HANDLE_VALUE) return 127;
    char cmdline[4096];
    size_t pos = 0;
    cmdline[pos++] = '"';
    for (const char* p = program; *p; p++)
        if (pos < sizeof(cmdline) - 1) cmdline[pos++] = *p;
    if (pos < sizeof(cmdline) - 1) cmdline[pos++] = '"';
    for (int i = 1; args[i]; i++) {
        if (pos >= sizeof(cmdline) - 256) break;
        cmdline[pos++] = ' ';
        pos = append_quoted_arg(cmdline, sizeof(cmdline), pos, args[i]);
    }
    cmdline[pos] = '\0';
    char prog_copy[4096];
    strncpy(prog_copy, cmdline, sizeof(prog_copy) - 1);
    prog_copy[sizeof(prog_copy) - 1] = '\0';
    PROCESS_INFORMATION pi = {0};
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.hStdOutput = devnull;
    si.hStdError = devnull;
    si.dwFlags = STARTF_USESTDHANDLES;
    BOOL ok = CreateProcessA(NULL, prog_copy, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    CloseHandle(devnull);
    if (!ok) return 127;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exit_code;
#else
    pid_t pid = fork();
    if (pid < 0) return 127;
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO) close(devnull);
        }
        execvp(program, (char* const*)args);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return 127;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 127;
#endif
}

int run_process_captured(const char* program, const char* const args[], char* output_buf, size_t output_buf_size) {
    output_buf[0] = '\0';

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return run_process(program, args);
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    /* Build the command line using the CommandLineToArgvW quoting rules:
       wrap each argument in double quotes and backslash-escape any embedded
       quote, doubling every run of backslashes that precedes one. Without
       this an argument containing '"' or a trailing backslash can terminate
       the quoting early and inject further argv entries. */
    char cmdline[4096];
    size_t pos = 0;
    cmdline[pos++] = '"';
    for (const char* p = program; *p; p++)
        if (pos < sizeof(cmdline) - 1) cmdline[pos++] = *p;
    if (pos < sizeof(cmdline) - 1) cmdline[pos++] = '"';
    for (int i = 1; args[i]; i++) {
        if (pos >= sizeof(cmdline) - 256) break;
        cmdline[pos++] = ' ';
        pos = append_quoted_arg(cmdline, sizeof(cmdline), pos, args[i]);
    }
    cmdline[pos] = '\0';

    PROCESS_INFORMATION pi = {0};
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.dwFlags = STARTF_USESTDHANDLES;

    char prog_copy[4096];
    strncpy(prog_copy, cmdline, sizeof(prog_copy) - 1);
    prog_copy[sizeof(prog_copy) - 1] = '\0';

    BOOL ok = CreateProcessA(NULL, prog_copy, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    CloseHandle(hWritePipe);

    if (!ok) {
        CloseHandle(hReadPipe);
        return run_process(program, args);
    }

    size_t total = 0;
    bool truncated = false;
    DWORD n;
    char tmpbuf[4096];
    while (ReadFile(hReadPipe, tmpbuf, sizeof(tmpbuf), &n, NULL) && n > 0) {
        truncated = append_captured_output(output_buf, output_buf_size, &total, tmpbuf, (size_t)n) || truncated;
    }
    if (output_buf_size > 0) output_buf[total] = '\0';
    report_captured_truncation(truncated);
    CloseHandle(hReadPipe);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exit_code;
#else
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        return run_process(program, args);
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return run_process(program, args);
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execvp(program, (char* const*)args);
        _exit(127);
    }

    close(pipefd[1]);
    size_t total = 0;
    bool truncated = false;
    ssize_t n;
    char tmpbuf[4096];
    while ((n = read(pipefd[0], tmpbuf, sizeof(tmpbuf))) > 0) {
        truncated = append_captured_output(output_buf, output_buf_size, &total, tmpbuf, (size_t)n) || truncated;
    }
    if (output_buf_size > 0) output_buf[total] = '\0';
    report_captured_truncation(truncated);
    close(pipefd[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return 127;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 127;
#endif
}

bool is_safe_path_component(const char* name) {
    if (!name || name[0] == '\0') return false;
    for (const char* p = name; *p; p++) {
        if (*p == '.' && *(p + 1) == '.') return false;  // no ".."
        if (*p == '/' || *p == '\\') return false;         // no path separators
    }
    return true;
}

#ifdef _WIN32
typedef struct {
    HANDLE pipe;
    char* buffer;
    size_t size;
    size_t total;
    bool truncated;
} CaptureContext;

static unsigned __stdcall capture_pipe_thread(void* param) {
    CaptureContext* context = (CaptureContext*)param;
    DWORD n;
    char tmpbuf[4096];
    while (ReadFile(context->pipe, tmpbuf, sizeof(tmpbuf), &n, NULL) && n > 0) {
        context->truncated = append_captured_output(context->buffer, context->size, &context->total,
                                                    tmpbuf, (size_t)n) || context->truncated;
    }
    if (context->size > 0) context->buffer[context->total] = '\0';
    _endthreadex(0);
    return 0;
}
#endif

int run_process_with_stdin(const char* program, const char* const args[], char* output_buf, size_t output_buf_size, const char* stdin_data) {
    output_buf[0] = '\0';
    if (!stdin_data) stdin_data = "";

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hStdinRead, hStdinWrite;
    HANDLE hStdoutRead, hStdoutWrite;
    if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0) ||
        !CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
        return run_process_captured(program, args, output_buf, output_buf_size);
    }
    /* The parent keeps the write end of stdin and the read end of stdout; the
       child must not inherit them, or it would hold stdin open and never see
       the end of the input. */
    SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    char cmdline[4096];
    size_t pos = 0;
    cmdline[pos++] = '"';
    for (const char* p = program; *p; p++)
        if (pos < sizeof(cmdline) - 1) cmdline[pos++] = *p;
    if (pos < sizeof(cmdline) - 1) cmdline[pos++] = '"';
    for (int i = 1; args[i]; i++) {
        if (pos >= sizeof(cmdline) - 256) break;
        cmdline[pos++] = ' ';
        cmdline[pos] = '\0';
        pos = append_quoted_arg(cmdline, sizeof(cmdline), pos, args[i]);
    }
    cmdline[pos] = '\0';

    PROCESS_INFORMATION pi = {0};
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.hStdInput = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite;
    si.dwFlags = STARTF_USESTDHANDLES;

    BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);

    if (!ok) {
        CloseHandle(hStdinWrite);
        CloseHandle(hStdoutRead);
        return run_process_captured(program, args, output_buf, output_buf_size);
    }

    CaptureContext context = {0};
    context.pipe = hStdoutRead;
    context.buffer = output_buf;
    context.size = output_buf_size;
    HANDLE reader = (HANDLE)_beginthreadex(NULL, 0, capture_pipe_thread, &context, 0, NULL);

    size_t input_len = strlen(stdin_data);
    size_t written_total = 0;
    while (reader != NULL && written_total < input_len) {
        DWORD chunk = input_len - written_total > 4096 ? 4096 : (DWORD)(input_len - written_total);
        DWORD written = 0;
        if (!WriteFile(hStdinWrite, stdin_data + written_total, chunk, &written, NULL) || written == 0) break;
        written_total += written;
    }
    CloseHandle(hStdinWrite);

    if (reader == NULL) {
        size_t total = 0;
        bool truncated = false;
        DWORD n;
        char tmpbuf[4096];
        while (ReadFile(hStdoutRead, tmpbuf, sizeof(tmpbuf), &n, NULL) && n > 0) {
            truncated = append_captured_output(output_buf, output_buf_size, &total, tmpbuf, (size_t)n) || truncated;
        }
        if (output_buf_size > 0) output_buf[total] = '\0';
        report_captured_truncation(truncated);
        CloseHandle(hStdoutRead);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    if (reader != NULL) {
        WaitForSingleObject(reader, INFINITE);
        CloseHandle(reader);
        CloseHandle(hStdoutRead);
        report_captured_truncation(context.truncated);
    }
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exit_code;
#else
    int in_pipe[2];
    int out_pipe[2];
    if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0) {
        return run_process_captured(program, args, output_buf, output_buf_size);
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        return run_process_captured(program, args, output_buf, output_buf_size);
    }

    if (pid == 0) {
        close(in_pipe[1]);
        close(out_pipe[0]);
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(out_pipe[1], STDERR_FILENO);
        close(in_pipe[0]);
        close(out_pipe[1]);
        signal(SIGPIPE, SIG_DFL);
        execvp(program, (char* const*)args);
        _exit(127);
    }

    close(in_pipe[0]);
    close(out_pipe[1]);

    void (*old_sigpipe)(int) = signal(SIGPIPE, SIG_IGN);
    size_t input_len = strlen(stdin_data);
    size_t written_total = 0;
    bool in_open = input_len > 0;
    bool out_open = true;
    if (!in_open) close(in_pipe[1]);
    size_t total = 0;
    bool truncated = false;
    char tmpbuf[4096];

    while (in_open || out_open) {
        if (in_open && written_total >= input_len) {
            close(in_pipe[1]);
            in_open = false;
        }

        struct pollfd fds[2];
        int in_index = -1;
        int out_index = -1;
        nfds_t nfds = 0;
        if (in_open) {
            in_index = (int)nfds;
            fds[nfds].fd = in_pipe[1];
            fds[nfds].events = POLLOUT;
            fds[nfds].revents = 0;
            nfds++;
        }
        if (out_open) {
            out_index = (int)nfds;
            fds[nfds].fd = out_pipe[0];
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            nfds++;
        }

        int ready = poll(fds, nfds, -1);
        if (ready < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (in_index >= 0 && fds[in_index].revents & (POLLOUT | POLLERR | POLLHUP | POLLNVAL)) {
            if (fds[in_index].revents & POLLOUT) {
                size_t remaining = input_len - written_total;
                size_t chunk = remaining > sizeof(tmpbuf) ? sizeof(tmpbuf) : remaining;
                ssize_t w = write(in_pipe[1], stdin_data + written_total, chunk);
                if (w > 0) {
                    written_total += (size_t)w;
                } else if (w < 0 && errno != EINTR) {
                    close(in_pipe[1]);
                    in_open = false;
                }
            } else {
                close(in_pipe[1]);
                in_open = false;
            }
        }

        if (out_index >= 0 && fds[out_index].revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL)) {
            ssize_t n = read(out_pipe[0], tmpbuf, sizeof(tmpbuf));
            if (n > 0) {
                truncated = append_captured_output(output_buf, output_buf_size, &total, tmpbuf, (size_t)n) || truncated;
            } else if (n == 0 || (n < 0 && errno != EINTR)) {
                close(out_pipe[0]);
                out_open = false;
            }
        }
    }

    if (in_open) close(in_pipe[1]);
    if (out_open) close(out_pipe[0]);
    signal(SIGPIPE, old_sigpipe);
    if (output_buf_size > 0) output_buf[total] = '\0';
    report_captured_truncation(truncated);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return 127;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 127;
#endif
}
