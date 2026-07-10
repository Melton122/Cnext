#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#define CNEXT_PATH_SEP '\\'
#define CNEXT_DEFAULT_EXE "out.exe"
int _mkdir(const char* path);
#else
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define CNEXT_PATH_SEP '/'
#define CNEXT_DEFAULT_EXE "out"
#endif

#include "codegen.h"
#include "parser.h"
#include "semantics.h"
#include <ctype.h>

#define CNEXT_VERSION "3.0"
#define CNEXT_TEMP_C "temp_out.c"
#define CNEXT_PATH_MAX 4096

static char* read_file(const char* path) {
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

static bool copy_file(const char* source_path, const char* destination_path) {
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

// --- Compilation ---

static char* build_source_with_packages(const char* source, const char* project_dir);

static int compile_file(const char* input_path, const char* output_c_path, bool test_mode) {
    char* raw_source = read_file(input_path);
    if (!raw_source) return 74;

    char* source = build_source_with_packages(raw_source, ".");
    if (!source) {
        free(raw_source);
        return 74;
    }
    free(raw_source);

    ASTNode* program = parse_program(source);
    if (!program) {
        free(source);
        return 65;
    }

    int status = 0;
    if (!analyze_semantics(program, !test_mode)) {
        fprintf(stderr, "Semantic analysis failed.\n");
        status = 65;
    } else if (!generate_c_code(program, output_c_path, test_mode)) {
        status = 74;
    }

    free_ast(program);
    free(source);
    return status;
}

static bool has_path_separator(const char* path) {
    return strchr(path, '/') != NULL || strchr(path, '\\') != NULL;
}

static bool dirname_from_path(const char* path, char* buffer, size_t buffer_size) {
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

static bool build_include_path(const char* argv0, char* include_path, size_t include_path_size) {
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

static int run_process(const char* program, char* const args[]) {
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

static const char* executable_command(const char* output_exe, char* buffer, size_t buffer_size) {
    if (has_path_separator(output_exe)) return output_exe;

#ifdef _WIN32
    int written = snprintf(buffer, buffer_size, ".\\%s", output_exe);
#else
    int written = snprintf(buffer, buffer_size, "./%s", output_exe);
#endif
    if (written < 0 || (size_t)written >= buffer_size) return output_exe;
    return buffer;
}

static int make_dir(const char* path) {
#ifdef _WIN32
    if (_mkdir(path) == 0 || errno == EEXIST) return 0;
#else
    if (mkdir(path, 0755) == 0 || errno == EEXIST) return 0;
#endif
    fprintf(stderr, "Could not create directory \"%s\": %s\n", path, strerror(errno));
    return 1;
}

static int join_path(char* out, size_t out_size, const char* base, const char* name) {
    int written = snprintf(out, out_size, "%s%c%s", base, CNEXT_PATH_SEP, name);
    if (written < 0 || (size_t)written >= out_size) {
        fprintf(stderr, "Path is too long: %s/%s\n", base, name);
        return 1;
    }
    return 0;
}

// --- Package Manager ---

static bool is_standard_module(const char* name) {
    static const char* std_modules[] = {"io", "file", "net", "json", "math", "os", "string_utils", "time", "regex", "collections", "crypto", "path", "encoding", "process", "random", NULL};
    for (int i = 0; std_modules[i]; i++) {
        if (strcmp(name, std_modules[i]) == 0) return true;
    }
    return false;
}

static bool find_registry_path(const char* argv0, char* registry_path, size_t registry_path_size) {
    char base[CNEXT_PATH_MAX];
    if (dirname_from_path(argv0, base, sizeof(base))) {
        int written = snprintf(registry_path, registry_path_size, "%s%c%s", base, CNEXT_PATH_SEP, "registry.txt");
        if (written >= 0 && (size_t)written < registry_path_size) {
            FILE* f = fopen(registry_path, "r");
            if (f) { fclose(f); return true; }
        }
    }
    // Try current directory
    int written = snprintf(registry_path, registry_path_size, "registry.txt");
    if (written >= 0 && (size_t)written < registry_path_size) {
        FILE* f = fopen(registry_path, "r");
        if (f) { fclose(f); return true; }
    }
    return false;
}

static bool resolve_package_url(const char* name, const char* registry_path, char* url, size_t url_size) {
    FILE* f = fopen(registry_path, "r");
    if (!f) return false;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        // Skip comments and blank lines
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\0' || *p == '\r' || *p == '\n') continue;

        // Parse name = url
        char entry_name[256] = {0};
        char entry_url[768] = {0};
        if (sscanf(p, "%255[^= ] = %767s", entry_name, entry_url) == 2) {
            if (strcmp(entry_name, name) == 0) {
                int written = snprintf(url, url_size, "%s", entry_url);
                fclose(f);
                return (written >= 0 && (size_t)written < url_size);
            }
        }
    }
    fclose(f);
    return false;
}

static bool download_package(const char* name, const char* url) {
    make_dir("packages");
    char out_path[CNEXT_PATH_MAX];
    int w = snprintf(out_path, sizeof(out_path), "packages%c%s.cn", CNEXT_PATH_SEP, name);
    if (w < 0 || (size_t)w >= sizeof(out_path)) return false;

    // Check if it's a local file path
    if (strncmp(url, "file://", 7) == 0) {
        const char* local_path = url + 7;
        // On Windows, file://C:/path → C:/path, file:///C:/path → C:/path
#ifdef _WIN32
        if (local_path[0] == '/' && isalpha(local_path[1]) && local_path[2] == ':') {
            local_path++;
        }
#endif
        printf("Copying %s to %s...\n", local_path, out_path);
        return copy_file(local_path, out_path);
    }

    // Try curl first (cross-platform)
    char command[CNEXT_PATH_MAX * 3 + 1024];
    int written = snprintf(command, sizeof(command), "curl -fsSL \"%s\" -o \"%s\"", url, out_path);
    if (written < 0 || (size_t)written >= sizeof(command)) {
        fprintf(stderr, "Download command is too long for package '%s'.\n", name);
        return false;
    }
    printf("Downloading %s...\n", name);
    int res = system(command);
    if (res == 0) return true;

    // Fallback: wget
    written = snprintf(command, sizeof(command), "wget -q \"%s\" -O \"%s\"", url, out_path);
    if (written < 0 || (size_t)written >= sizeof(command)) {
        fprintf(stderr, "Download command is too long for package '%s'.\n", name);
        return false;
    }
    res = system(command);
    if (res == 0) return true;

#ifdef _WIN32
    // Fallback: PowerShell
    written = snprintf(command, sizeof(command),
        "powershell -Command \"try { Invoke-WebRequest -Uri '%s' -OutFile '%s' } catch { exit 1 }\"",
        url, out_path);
    if (written < 0 || (size_t)written >= sizeof(command)) {
        fprintf(stderr, "Download command is too long for package '%s'.\n", name);
        return false;
    }
    res = system(command);
    if (res == 0) return true;
#endif

    fprintf(stderr, "Failed to download %s from %s\n", name, url);
    return false;
}

static bool install_single_package(const char* name, const char* argv0) {
    char registry_path[CNEXT_PATH_MAX];
    if (!find_registry_path(argv0, registry_path, sizeof(registry_path))) {
        fprintf(stderr, "No registry found. Cannot resolve package '%s'.\n", name);
        return false;
    }

    char url[1024];
    if (!resolve_package_url(name, registry_path, url, sizeof(url))) {
        fprintf(stderr, "Package '%s' not found in registry.\n", name);
        return false;
    }

    return download_package(name, url);
}

static void install_packages_from_toml(const char* argv0) {
    FILE* f = fopen("cnext.toml", "r");
    if (!f) {
        fprintf(stderr, "No cnext.toml found in current directory.\n");
        return;
    }

    char registry_path[CNEXT_PATH_MAX];
    bool have_registry = find_registry_path(argv0, registry_path, sizeof(registry_path));

    char line[512];
    bool in_deps = false;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;

        if (line[0] == '[') {
            if (strncmp(line, "[dependencies]", 14) == 0) in_deps = true;
            else in_deps = false;
            continue;
        }

        if (in_deps && strchr(line, '=')) {
            char name[256] = {0};
            char value[768] = {0};
            if (sscanf(line, " %255[^= ] = \"%767[^\"]\"", name, value) == 2) {
                // If value looks like a URL, download directly
                if (strncmp(value, "http", 4) == 0 || strncmp(value, "file://", 7) == 0) {
                    download_package(name, value);
                } else if (have_registry) {
                    // Look up in registry
                    char url[1024];
                    if (resolve_package_url(name, registry_path, url, sizeof(url))) {
                        download_package(name, url);
                    } else {
                        fprintf(stderr, "Package '%s' not found in registry.\n", name);
                    }
                } else {
                    fprintf(stderr, "No registry found. Cannot resolve package '%s'.\n", name);
                }
            }
        }
    }
    fclose(f);
}

// --- Source combining for packages ---

static char* build_source_with_packages(const char* source, const char* project_dir) {
    // Find all import statements and prepend non-standard package sources
    // Use a simple visited tracker to avoid duplicates and infinite loops
    char visited[32][64] = {{0}};
    int visited_count = 0;

    char* combined = strdup("");
    if (!combined) return NULL;

    const char* p = source;
    while (*p) {
        // Skip whitespace and comments to find "import"
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p == '/' && *(p+1) == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }
        if (*p == '/' && *(p+1) == '*') {
            p += 2;
            while (*p && !(*p == '*' && *(p+1) == '/')) p++;
            if (*p) p += 2;
            continue;
        }

        if (strncmp(p, "import", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
            p += 6;
            while (*p == ' ' || *p == '\t') p++;
            const char* name_start = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
            int name_len = (int)(p - name_start);
            if (name_len > 0 && name_len < 64) {
                char name[64] = {0};
                memcpy(name, name_start, name_len);

                if (!is_standard_module(name)) {
                    // Check if already visited
                    bool already_loaded = false;
                    for (int i = 0; i < visited_count; i++) {
                        if (strcmp(visited[i], name) == 0) { already_loaded = true; break; }
                    }

                    if (!already_loaded && visited_count < 32) {
                        strcpy(visited[visited_count++], name);

                        char pkg_path[CNEXT_PATH_MAX];
                        int w = snprintf(pkg_path, sizeof(pkg_path), "packages%c%s.cn", CNEXT_PATH_SEP, name);
                        if (w > 0 && (size_t)w < sizeof(pkg_path)) {
                            char* pkg_source = read_file(pkg_path);
                            if (pkg_source) {
                                // Recursively load this package's imports too
                                char* pkg_combined = build_source_with_packages(pkg_source, project_dir);
                                if (pkg_combined) {
                                    size_t new_len = strlen(pkg_combined) + strlen(combined) + 1;
                                    char* new_combined = (char*)malloc(new_len);
                                    if (new_combined) {
                                        new_combined[0] = '\0';
                                        strcat(new_combined, pkg_combined);
                                        strcat(new_combined, combined);
                                        free(combined);
                                        combined = new_combined;
                                    }
                                    free(pkg_combined);
                                }
                                free(pkg_source);
                            } else {
                                fprintf(stderr, "Warning: Package '%s' not found at %s\n", name, pkg_path);
                            }
                        }
                    }
                }
            }
        }
        if (*p) p++;
    }

    // Append main source
    size_t total_len = strlen(combined) + strlen(source) + 1;
    char* result = (char*)malloc(total_len);
    if (!result) {
        free(combined);
        return NULL;
    }
    strcpy(result, combined);
    strcat(result, source);
    free(combined);
    return result;
}

static int create_project(const char* project_name) {
    if (!project_name || project_name[0] == '\0') {
        fprintf(stderr, "Project name cannot be empty.\n");
        return 64;
    }

    char src[CNEXT_PATH_MAX];
    char packages[CNEXT_PATH_MAX];
    char build[CNEXT_PATH_MAX];

    if (join_path(src, sizeof(src), project_name, "src") ||
        join_path(packages, sizeof(packages), project_name, "packages") ||
        join_path(build, sizeof(build), project_name, "build")) {
        return 74;
    }

    printf("Creating new project '%s'...\n", project_name);
    if (make_dir(project_name) || make_dir(src) || make_dir(packages) || make_dir(build)) {
        return 74;
    }

    char toml_path[CNEXT_PATH_MAX];
    if (join_path(toml_path, sizeof(toml_path), project_name, "cnext.toml") == 0) {
        FILE* f = fopen(toml_path, "w");
        if (f) {
            fprintf(f, "[package]\nname = \"%s\"\nversion = \"0.1.0\"\n\n[dependencies]\n", project_name);
            fclose(f);
        }
    }

    char main_path[CNEXT_PATH_MAX];
    if (join_path(main_path, sizeof(main_path), src, "main.cn") == 0) {
        FILE* f = fopen(main_path, "w");
        if (f) {
            fprintf(f, "main {\n    printin(\"Hello, %s!\")\n}\n", project_name);
            fclose(f);
        }
    }

    printf("Project '%s' created.\n", project_name);
    return 0;
}

static void print_help(void) {
    printf("Cnext Compiler v%s\n", CNEXT_VERSION);
    printf("Usage: cnext <command> [arguments]\n\n");
    printf("Commands:\n");
    printf("  new <ProjectName>          Create a new Cnext project.\n");
    printf("  run <file.cn> [-o <exe>]   Compile and run a Cnext file.\n");
    printf("  build <file.cn> [-o <exe>] Compile a Cnext file to an executable.\n");
    printf("  version                    Show compiler version.\n");
    printf("  install [package]          Install all dependencies from cnext.toml, or a single package.\n");
}

static int parse_build_args(int argc, char** argv, const char** input_path, const char** output_exe) {
    if (argc < 3) return 64;

    *input_path = argv[2];
    *output_exe = CNEXT_DEFAULT_EXE;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Expected output path after %s.\n", argv[i]);
                return 64;
            }
            *output_exe = argv[++i];
        } else {
            fprintf(stderr, "Unknown option for %s: %s\n", argv[1], argv[i]);
            return 64;
        }
    }

    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_help();
        return 0;
    }

    const char* command = argv[1];

    if (strcmp(command, "version") == 0) {
        printf("Cnext version %s\n", CNEXT_VERSION);
        return 0;
    }

    if (strcmp(command, "help") == 0 || strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
        print_help();
        return 0;
    }

    if (strcmp(command, "run") == 0 || strcmp(command, "build") == 0) {
        const char* input_path = NULL;
        const char* output_exe = NULL;
        int arg_status = parse_build_args(argc, argv, &input_path, &output_exe);
        if (arg_status != 0) {
            fprintf(stderr, "Usage: cnext %s <file.cn> [-o <executable>]\n", command);
            return arg_status;
        }

        char include_path[CNEXT_PATH_MAX];
        if (!build_include_path(argv[0], include_path, sizeof(include_path))) {
            return 74;
        }

        remove(CNEXT_TEMP_C);
        int compile_status = compile_file(input_path, CNEXT_TEMP_C, false);
        if (compile_status != 0) {
            remove(CNEXT_TEMP_C);
            return compile_status;
        }

        char* gcc_args[] = {
            "gcc",
            "-std=gnu11",
            "-iquote",
            include_path,
            CNEXT_TEMP_C,
            "-o",
            (char*)output_exe,
            "-lwinhttp",
            "-lws2_32",
            NULL
        };

        int gcc_status = run_process("gcc", gcc_args);
        if (gcc_status != 0) {
            fprintf(stderr, "C compilation failed.\n");
            return 65;
        }

        remove(CNEXT_TEMP_C);

        if (strcmp(command, "run") == 0) {
            char executable_buffer[CNEXT_PATH_MAX];
            const char* executable = executable_command(output_exe, executable_buffer, sizeof(executable_buffer));
            char* run_args[] = {(char*)executable, NULL};
            return run_process(executable, run_args);
        }

        return 0;
    }

    if (strcmp(command, "new") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: cnext new <ProjectName>\n");
            return 64;
        }
        return create_project(argv[2]);
    }

    if (strcmp(command, "install") == 0) {
        if (argc >= 3) {
            // Install single package
            return install_single_package(argv[2], argv[0]) ? 0 : 1;
        } else {
            // Install from cnext.toml
            install_packages_from_toml(argv[0]);
            return 0;
        }
    }

    fprintf(stderr, "Unknown command: %s\n", command);
    print_help();
    return 64;
}
