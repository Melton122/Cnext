#include "main_internal.h"
#include <stdint.h>

bool is_standard_module(const char* name) {
    static const char* std_modules[] = {"io", "file", "net", "json", "math", "os", "string_utils", "time", "regex", "collections", "crypto", "path", "encoding", "process", "random", "thread", "http", "log", "args", "fmt", NULL};
    for (int i = 0; std_modules[i]; i++) {
        if (strcmp(name, std_modules[i]) == 0) return true;
    }
    return false;
}

static bool is_safe_url(const char* url) {
    if (!url || url[0] == '\0') return false;
    for (const char* p = url; *p; p++) {
        if (*p == '"' || *p == '\'' || *p == '`' || *p == '$' || *p == '\\')
            return false;
    }
    return true;
}

bool find_registry_path(const char* argv0, char* registry_path, size_t registry_path_size) {
    char base[CNEXT_PATH_MAX];
    if (dirname_from_path(argv0, base, sizeof(base))) {
        int written = snprintf(registry_path, registry_path_size, "%s%c%s", base, CNEXT_PATH_SEP, "registry.txt");
        if (written >= 0 && (size_t)written < registry_path_size) {
            FILE* f = fopen(registry_path, "r");
            if (f) { fclose(f); return true; }
        }
    }
    int written = snprintf(registry_path, registry_path_size, "registry.txt");
    if (written >= 0 && (size_t)written < registry_path_size) {
        FILE* f = fopen(registry_path, "r");
        if (f) { fclose(f); return true; }
    }
    return false;
}

bool resolve_package_url(const char* name, const char* registry_path, char* url, size_t url_size) {
    FILE* f = fopen(registry_path, "r");
    if (!f) return false;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\0' || *p == '\r' || *p == '\n') continue;

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

bool download_package(const char* name, const char* url) {
    make_dir("packages");
    char out_path[CNEXT_PATH_MAX];
    int w = snprintf(out_path, sizeof(out_path), "packages%c%s.cn", CNEXT_PATH_SEP, name);
    if (w < 0 || (size_t)w >= sizeof(out_path)) return false;

    if (strncmp(url, "file://", 7) == 0) {
        const char* local_path = url + 7;
#ifdef _WIN32
        if (local_path[0] == '/' && isalpha(local_path[1]) && local_path[2] == ':') {
            local_path++;
        }
#endif
        printf("Copying %s to %s...\n", local_path, out_path);
        return copy_file(local_path, out_path);
    }

    if (!is_safe_url(url)) {
        fprintf(stderr, "URL contains unsafe characters for package '%s'.\n", name);
        return false;
    }

    char command[CNEXT_PATH_MAX * 3 + 1024];
    int written = snprintf(command, sizeof(command), "curl -fsSL \"%s\" -o \"%s\"", url, out_path);
    if (written < 0 || (size_t)written >= sizeof(command)) {
        fprintf(stderr, "Download command is too long for package '%s'.\n", name);
        return false;
    }
    printf("Downloading %s...\n", name);
    int res = system(command);
    if (res == 0) return true;

    written = snprintf(command, sizeof(command), "wget -q \"%s\" -O \"%s\"", url, out_path);
    if (written < 0 || (size_t)written >= sizeof(command)) {
        fprintf(stderr, "Download command is too long for package '%s'.\n", name);
        return false;
    }
    res = system(command);
    if (res == 0) return true;

#ifdef _WIN32
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

bool install_single_package(const char* name, const char* argv0) {
    if (is_standard_module(name)) {
        printf("Package '%s' is a standard module (no install needed).\n", name);
        return true;
    }

    char registry_path[CNEXT_PATH_MAX];
    if (!find_registry_path(argv0, registry_path, sizeof(registry_path))) {
        fprintf(stderr, "Error: No registry found. Cannot resolve package '%s'.\n", name);
        fprintf(stderr, "Create a registry.txt file or set CNEXT_REGISTRY_URL.\n");
        return false;
    }

    char url[1024];
    if (!resolve_package_url(name, registry_path, url, sizeof(url))) {
        fprintf(stderr, "Error: Package '%s' not found in registry.\n", name);
        fprintf(stderr, "Available packages can be found with: cnext search %s\n", name);
        return false;
    }

    printf("Installing package '%s'...\n", name);
    bool ok = download_package(name, url);
    if (ok) {
        printf("Package '%s' installed successfully.\n", name);
    } else {
        fprintf(stderr, "Failed to install package '%s'.\n", name);
    }
    return ok;
}

void install_packages_from_toml(const char* argv0) {
    FILE* f = fopen("cnext.toml", "r");
    if (!f) {
        fprintf(stderr, "Error: No cnext.toml found in the current directory.\n");
        fprintf(stderr, "Run 'cnext init' to create one, or 'cnext install <package>' for a single package.\n");
        return;
    }

    char registry_path[CNEXT_PATH_MAX];
    bool have_registry = find_registry_path(argv0, registry_path, sizeof(registry_path));

    int installed = 0;
    int failed = 0;
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
                if (is_standard_module(name)) {
                    printf("Package '%s' is a standard module (no install needed).\n", name);
                    installed++;
                } else if (strncmp(value, "http", 4) == 0 || strncmp(value, "file://", 7) == 0) {
                    if (download_package(name, value)) {
                        installed++;
                    } else {
                        failed++;
                    }
                } else if (have_registry) {
                    char url[1024];
                    if (resolve_package_url(name, registry_path, url, sizeof(url))) {
                        if (download_package(name, url)) {
                            installed++;
                        } else {
                            failed++;
                        }
                    } else {
                        fprintf(stderr, "Error: Package '%s' not found in registry.\n", name);
                        failed++;
                    }
                } else {
                    fprintf(stderr, "Error: No registry found. Cannot resolve package '%s'.\n", name);
                    failed++;
                }
            }
        }
    }
    fclose(f);

    if (installed > 0 || failed > 0) {
        printf("\nInstall complete: %d installed, %d failed.\n", installed, failed);
    } else {
        printf("No dependencies found in cnext.toml.\n");
    }
}

char* build_source_with_packages(const char* source, const char* project_dir) {
    char visited[32][64] = {{0}};
    int visited_count = 0;

    char* combined = strdup("");
    if (!combined) return NULL;

    const char* p = source;
    while (*p) {
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
                                char* pkg_combined = build_source_with_packages(pkg_source, project_dir);
                                if (pkg_combined) {
                                    size_t len1 = strlen(pkg_combined);
                                    size_t len2 = strlen(combined);
                                    if (len1 > SIZE_MAX - len2 - 1) {
                                        free(pkg_combined);
                                        free(pkg_source);
                                        free(combined);
                                        return NULL;
                                    }
                                    size_t new_len = len1 + len2 + 1;
                                    char* new_combined = (char*)malloc(new_len);
                                    if (new_combined) {
                                        memcpy(new_combined, pkg_combined, len1);
                                        memcpy(new_combined + len1, combined, len2 + 1);
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

    size_t len_combined = strlen(combined);
    size_t len_source = strlen(source);
    if (len_combined > SIZE_MAX - len_source - 1) {
        free(combined);
        return NULL;
    }
    size_t total_len = len_combined + len_source + 1;
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
