#include "main_internal.h"

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

    char* buffer = (char*)malloc((size_t)file_size + 1);
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
    const char* sep = last_slash > last_backslash ? last_slash : last_backslash;
    if (!sep) return false;

    size_t length = (size_t)(sep - path);
    if (length == 0 || length + 1 > buffer_size) return false;
    memcpy(buffer, path, length);
    buffer[length] = '\0';
    return true;
}

bool build_include_path(const char* argv0, char* include_path, size_t include_path_size) {
    char base[CNEXT_PATH_MAX];
    const char* include_dir = "include";
    char resolved_include_path[CNEXT_PATH_MAX];

    if (dirname_from_path(argv0, base, sizeof(base))) {
        size_t base_len = strlen(base);
        const char* include_leaf = "include";
        size_t include_len = strlen(include_leaf);
        if (base_len + 1 + include_len + 1 > sizeof(resolved_include_path)) {
            fprintf(stderr, "Compiler path is too long.\n");
            return false;
        }
        memcpy(resolved_include_path, base, base_len);
        resolved_include_path[base_len] = CNEXT_PATH_SEP;
        memcpy(resolved_include_path + base_len + 1, include_leaf, include_len + 1);
        include_dir = resolved_include_path;
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
