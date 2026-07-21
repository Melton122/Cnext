#include "main_internal.h"
#ifdef _WIN32
#include <windows.h>
#endif

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
    const char* include_dir = "include";
    char resolved_include_path[CNEXT_PATH_MAX];

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

int run_process(const char* program, char* const args[]) {
#ifdef _WIN32
    intptr_t result = _spawnvp(_P_WAIT, program, (const char* const*)args);
    if (result == -1) {
        fprintf(stderr, "Could not start %s: %s\n", program, strerror(errno));
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
        execvp(program, args);
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

int run_process_captured(const char* program, char* const args[], char* output_buf, size_t output_buf_size) {
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

    /* Build command line: "program" "arg1" "arg2" ... */
    char cmdline[4096];
    int pos = 0;
    pos += snprintf(cmdline + pos, sizeof(cmdline) - pos, "\"%s\"", program);
    for (int i = 1; args[i]; i++) {
        pos += snprintf(cmdline + pos, sizeof(cmdline) - pos, " \"%s\"", args[i]);
        if (pos >= (int)sizeof(cmdline) - 128) break;
    }

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
    DWORD n;
    char tmpbuf[4096];
    while (ReadFile(hReadPipe, tmpbuf, sizeof(tmpbuf), &n, NULL) && n > 0) {
        if (total + n < output_buf_size - 1) {
            memcpy(output_buf + total, tmpbuf, n);
            total += n;
        }
    }
    output_buf[total] = '\0';
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
        execvp(program, args);
        _exit(127);
    }

    close(pipefd[1]);
    size_t total = 0;
    ssize_t n;
    while (total < output_buf_size - 1 && (n = read(pipefd[0], output_buf + total, output_buf_size - 1 - total)) > 0) {
        total += n;
    }
    output_buf[total] = '\0';
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
