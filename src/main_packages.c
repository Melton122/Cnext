#include "main_internal.h"
#include "private_names.h"
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
    if (url[0] == '-') return false;
    for (const char* p = url; *p; p++) {
        if ((unsigned char)*p < 0x20 || (unsigned char)*p == 0x7f) return false;
        if (*p == '"' || *p == '\'' || *p == '`' || *p == '$' || *p == '\\')
            return false;
    }
    return true;
}

static bool is_allowed_dependency_url(const char* url) {
    return url && (strncmp(url, "https://", 8) == 0 || strncmp(url, "file://", 7) == 0);
}

static bool is_safe_local_file_path(const char* path) {
    if (!path || path[0] == '\0' || strstr(path, "..")) return false;
    for (const char* p = path; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x20 || c == 0x7f || c == '\\') return false;
    }
    return true;
}

static bool make_package_parent(const char* name) {
    const char* sep = strchr(name, '/');
    if (!sep) return true;
    char dir[CNEXT_PATH_MAX];
    int len = (int)(sep - name);
    if (len <= 0 || len >= (int)sizeof(dir)) return false;
    int written = snprintf(dir, sizeof(dir), "packages%c%.*s", CNEXT_PATH_SEP, len, name);
    if (written < 0 || (size_t)written >= sizeof(dir)) return false;
    if (make_dir(dir) == 0) return true;
    return errno == EEXIST;
}

static bool package_temp_path(const char* out_path, char* tmp_path, size_t tmp_path_size) {
#ifdef _WIN32
    unsigned long pid = (unsigned long)_getpid();
#else
    unsigned long pid = (unsigned long)getpid();
#endif
    int written = snprintf(tmp_path, tmp_path_size, "%s.%lu.tmp", out_path, pid);
    return written >= 0 && (size_t)written < tmp_path_size;
}

static bool finalize_package_file(const char* tmp_path, const char* out_path) {
    remove(out_path);
    if (rename(tmp_path, out_path) == 0) return true;
    remove(tmp_path);
    return false;
}

/* The package name is embedded in a filesystem path, so it must stay inside the
   packages/ directory: no absolute paths, no traversal, no colon/backslash
   tricks, and no characters that could terminate a quoting context in the
   PowerShell fallback below. A single '/' is allowed so scoped names such as
   "vendor/lib" keep working. */
static bool is_safe_package_name(const char* name) {
    if (!name || name[0] == '\0') return false;
    if (name[0] == '-' || name[0] == '.' || name[0] == '/') return false;
    size_t len = strlen(name);
    if (name[len - 1] == '/') return false;
    if (strstr(name, "..") || strstr(name, "//")) return false;
    for (const char* p = name; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x20 || c == 0x7f) return false;
        if (c == '\\' || c == ':') return false;
        if (c == '"' || c == '\'' || c == '`' || c == '$' || c == ';' || c == '&' ||
            c == '|' || c == '<' || c == '>' || c == '(' || c == ')' || c == '{' ||
            c == '}' || c == '*' || c == '?' || c == '!') return false;
    }
    return true;
}

/* Copy src into dst as a PowerShell single-quoted string literal. Inside such a
   literal the only escape is a doubled quote, so every ' becomes ''. */
static void ps_quote(char* dst, size_t cap, const char* src) {
    size_t o = 0;
    if (o < cap - 1) dst[o++] = '\'';
    for (const char* p = src; *p; p++) {
        if (*p == '\'') {
            if (o < cap - 2) dst[o++] = '\'';
            if (o < cap - 1) dst[o++] = '\'';
        } else if (o < cap - 1) {
            dst[o++] = *p;
        }
    }
    if (o < cap - 1) dst[o++] = '\'';
    dst[o] = '\0';
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
    if (!is_safe_package_name(name)) {
        fprintf(stderr, "Invalid package name '%s'.\n", name ? name : "(null)");
        return false;
    }
    if (!is_safe_url(url)) {
        fprintf(stderr, "URL contains unsafe characters for package '%s'.\n", name);
        return false;
    }
    if (make_dir("packages") != 0 && errno != EEXIST) {
        fprintf(stderr, "Could not create packages directory.\n");
        return false;
    }
    if (!make_package_parent(name)) {
        fprintf(stderr, "Invalid package scope for '%s'.\n", name);
        return false;
    }
    char out_path[CNEXT_PATH_MAX];
    int w = snprintf(out_path, sizeof(out_path), "packages%c%s.cn", CNEXT_PATH_SEP, name);
    if (w < 0 || (size_t)w >= sizeof(out_path)) return false;
    char tmp_path[CNEXT_PATH_MAX];
    if (!package_temp_path(out_path, tmp_path, sizeof(tmp_path))) return false;

    if (strncmp(url, "file://", 7) == 0) {
        const char* local_path = url + 7;
#ifdef _WIN32
        if (local_path[0] == '/' && isalpha(local_path[1]) && local_path[2] == ':') {
            local_path++;
        }
#endif
        if (!is_safe_local_file_path(local_path)) {
            fprintf(stderr, "Unsafe local package path for '%s'.\n", name);
            return false;
        }
        printf("Copying %s to %s...\n", local_path, out_path);
        bool copied = copy_file(local_path, tmp_path);
        if (!copied) {
            remove(tmp_path);
            return false;
        }
        return finalize_package_file(tmp_path, out_path);
    }

    if (strncmp(url, "https://", 8) != 0) {
        fprintf(stderr, "Refusing insecure package URL for '%s'.\n", name);
        return false;
    }

    printf("Downloading %s...\n", name);

    // Try curl with safe argument array (no shell injection)
    {
        const char* const curl_args[] = {"curl", "-fsSL", "-o", tmp_path, "--", url, NULL};
        int res = run_process("curl", curl_args);
        if (res == 0 && finalize_package_file(tmp_path, out_path)) return true;
        remove(tmp_path);
    }

    // Try wget
    {
        const char* const wget_args[] = {"wget", "-q", "-O", tmp_path, "--", url, NULL};
        int res = run_process("wget", wget_args);
        if (res == 0 && finalize_package_file(tmp_path, out_path)) return true;
        remove(tmp_path);
    }

#ifdef _WIN32
    // Try PowerShell Invoke-WebRequest. -Command takes the remaining arguments
    // and evaluates them as one script, so both operands are emitted as
    // quoted string literals (quotes doubled) rather than bare text.
    {
        char q_uri[CNEXT_PATH_MAX * 2 + 64];
        char q_out[CNEXT_PATH_MAX * 2 + 64];
        char ps_uri[CNEXT_PATH_MAX * 2 + 128];
        char ps_out[CNEXT_PATH_MAX * 2 + 128];
        ps_quote(q_uri, sizeof(q_uri), url);
        ps_quote(q_out, sizeof(q_out), tmp_path);
        snprintf(ps_uri, sizeof(ps_uri), "Invoke-WebRequest -Uri %s", q_uri);
        snprintf(ps_out, sizeof(ps_out), "-OutFile %s", q_out);
        const char* const ps_args[] = {"powershell", "-NoProfile", "-Command", ps_uri, ps_out, NULL};
        int res = run_process("powershell", ps_args);
        if (res == 0 && finalize_package_file(tmp_path, out_path)) return true;
        remove(tmp_path);
    }
#endif

    fprintf(stderr, "Failed to download %s from %s\n", name, url);
    return false;
}

static bool install_registry_version(const char* name, const char* constraint) {
    VersionSpec spec;
    RegistryPackage pkg;
    if (!semver_parse_spec(&spec, constraint) || !registry_resolve(name, &spec, &pkg) ||
        strncmp(pkg.url, "https://", 8) != 0) {
        fprintf(stderr, "No downloadable version of %s matching %s.\n", name, constraint);
        return false;
    }
    return download_package(name, pkg.url);
}

bool install_single_package(const char* name, const char* argv0) {
    if (is_standard_module(name)) {
        printf("Package '%s' is a standard module (no install needed).\n", name);
        return true;
    }

    const char* remote = getenv("CNEXT_REGISTRY_URL");
    if (remote && *remote) return install_registry_version(name, "*");
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

bool install_packages_from_toml(const char* argv0) {
    FILE* f = fopen("cnext.toml", "r");
    if (!f) {
        fprintf(stderr, "Error: No cnext.toml found in the current directory.\n");
        fprintf(stderr, "Run 'cnext init' to create one, or 'cnext install <package>' for a single package.\n");
        return false;
    }

    char registry_path[CNEXT_PATH_MAX];
    bool have_registry = find_registry_path(argv0, registry_path, sizeof(registry_path));

    const char* remote = getenv("CNEXT_REGISTRY_URL");
    int installed = 0;
    int failed = 0;
    char line[2048];
    bool in_deps = false;
    int line_number = 0;
    while (fgets(line, sizeof(line), f)) {
        line_number++;
        line[strcspn(line, "\r\n")] = 0;
        char* entry = line;
        while (*entry == ' ' || *entry == '\t') entry++;
        if (!*entry || *entry == '#') continue;

        if (entry[0] == '[') {
            if (strncmp(entry, "[dependencies]", 14) == 0) in_deps = true;
            else in_deps = false;
            continue;
        }

        if (in_deps) {
            char name[256] = {0};
            char value[768] = {0};
            int consumed = 0;
            int matched = sscanf(entry, " %255[^= \t] = \"%767[^\"]\" %n", name, value, &consumed);
            if (matched != 2 || consumed == 0 || (entry[consumed] && entry[consumed] != '#')) {
                fprintf(stderr, "Error: Invalid dependency declaration at cnext.toml:%d.\n", line_number);
                failed++;
                continue;
            }
            {
                if (is_standard_module(name)) {
                    printf("Package '%s' is a standard module (no install needed).\n", name);
                    installed++;
                } else if (is_allowed_dependency_url(value)) {
                    if (download_package(name, value)) {
                        installed++;
                    } else {
                        failed++;
                    }
                } else if (remote && *remote) {
                    if (install_registry_version(name, value)) installed++;
                    else failed++;
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
    if (ferror(f)) {
        fprintf(stderr, "Error: Could not read cnext.toml.\n");
        failed++;
    }
    fclose(f);

    if (installed > 0 || failed > 0) {
        printf("\nInstall complete: %d installed, %d failed.\n", installed, failed);
    } else {
        printf("No dependencies found in cnext.toml.\n");
    }
    return failed == 0;
}

/* --- Local file imports ------------------------------------------------
   `import "./util.cn"` (or `import "util"`) merges another source file into
   this translation unit. Imported sources are placed BEFORE the importing
   file, like C headers: codegen writes aggregate definitions where they are
   declared, so a class or struct defined in a module must be complete before
   the importing file's code that uses it by value or with sizeof. Every file
   is expanded at most once, which makes diamond dependencies and import
   cycles harmless.

   Like the package expander (which has always prepended), this moves the
   importing file's line numbers down by the size of the imported code, so the
   expander also fills in an OriginMap: diagnostics then report the real file
   and line instead of a position in the merged text.
*/

#define CNEXT_MAX_IMPORT_FILES 64

typedef struct {
    char paths[CNEXT_MAX_IMPORT_FILES][CNEXT_PATH_MAX];
    int count;
    bool failed;
    bool privatize;      /* rename non-exported names (a local import exists) */
    int next_module_tag; /* unique per expanded file, for private-name suffixes */
} ImportState;

static bool path_is_absolute(const char* path) {
    if (!path || !path[0]) return false;
    if (path[0] == '/' || path[0] == '\\') return true;
#ifdef _WIN32
    if (isalpha((unsigned char)path[0]) && path[1] == ':') return true;
#endif
    return false;
}

/* Collapse `.` components and repeated separators so two spellings of the
   same file share one visited entry. Purely textual: the file need not exist. */
static void normalize_import_path(const char* in, char* out, size_t out_size) {
    char stack[CNEXT_PATH_MAX];
    size_t o = 0;
    const char* p = in;

    bool absolute = path_is_absolute(in);
    if (absolute) {
        stack[o++] = CNEXT_PATH_SEP;
        while (*p == '/' || *p == '\\') p++;
    }

    while (*p) {
        const char* start = p;
        size_t len = 0;
        while (p[len] && p[len] != '/' && p[len] != '\\') len++;
        if (len == 1 && start[0] == '.') {
            /* current directory: drop it */
        } else if (len > 0) {
            bool needs_sep = o > 0 && stack[o - 1] != CNEXT_PATH_SEP;
            if (needs_sep && o + 1 < sizeof(stack)) stack[o++] = CNEXT_PATH_SEP;
            if (o + len + 1 < sizeof(stack)) {
                memcpy(stack + o, start, len);
                o += len;
            }
        }
        p += len;
        while (*p == '/' || *p == '\\') p++;
    }
    stack[o] = '\0';
    snprintf(out, out_size, "%s", stack);
}

static bool resolve_import_path(const char* source_dir, const char* spec,
                                char* out, size_t out_size) {
    if (!spec || !spec[0]) return false;

    char name[CNEXT_PATH_MAX];
    snprintf(name, sizeof(name), "%s", spec);

    size_t name_len = strlen(name);
    if (!(name_len > 3 && strcmp(name + name_len - 3, ".cn") == 0)) {
        const char* base = name;
        for (const char* q = name; *q; q++) {
            if (*q == '/' || *q == '\\') base = q + 1;
        }
        if (!strchr(base, '.')) {
            size_t used = strlen(name);
            if (used + 4 > sizeof(name)) return false;
            memcpy(name + used, ".cn", 4);
        }
    }

    int written;
    if (path_is_absolute(name)) {
        written = snprintf(out, out_size, "%s", name);
    } else {
        const char* dir = (source_dir && source_dir[0]) ? source_dir : ".";
        written = snprintf(out, out_size, "%s%c%s", dir, CNEXT_PATH_SEP, name);
    }
    return written > 0 && (size_t)written < out_size;
}

static void append_text(char** buffer, size_t* length, size_t* capacity, const char* text) {
    size_t add = strlen(text);
    if (*length + add + 1 > *capacity) {
        size_t cap = *capacity ? *capacity : 4096;
        while (cap < *length + add + 1) cap *= 2;
        *buffer = checked_realloc(*buffer, cap);
        *capacity = cap;
    }
    memcpy(*buffer + *length, text, add + 1);
    *length += add;
}

static int count_newlines(const char* text) {
    int newlines = 0;
    for (const char* p = text; p && *p; p++) {
        if (*p == '\n') newlines++;
    }
    return newlines;
}

static void copy_map_entries(OriginMap* into, const OriginMap* from, int line_offset) {
    if (!into || !from) return;
    for (int i = 0; i < from->count; i++) {
        const OriginEntry* entry = &from->entries[i];
        origin_map_add(into, entry->merged_line + line_offset, entry->line_count,
                       entry->file, entry->file_line);
    }
}

static char* expand_local_imports_into(const char* source, const char* source_dir,
                                       ImportState* state, const char* file_path,
                                       OriginMap* map);

static char* expand_local_imports_body(const char* source, const char* source_dir,
                                       ImportState* state, const char* file_path,
                                       OriginMap* map) {
    size_t length = 0;
    size_t capacity = 0;
    char* preamble = NULL;
    int preamble_lines = 0;

    size_t i = 0;
    bool at_line_start = true;
    bool in_block_comment = false;

    while (source[i]) {
        char c = source[i];

        if (in_block_comment) {
            if (c == '*' && source[i + 1] == '/') { in_block_comment = false; i += 2; }
            else i++;
            continue;
        }
        if (c == '\n') { at_line_start = true; i++; continue; }
        if (c == ' ' || c == '\t' || c == '\r') { i++; continue; }
        if (c == '/' && source[i + 1] == '/') {
            while (source[i] && source[i] != '\n') i++;
            continue;
        }
        if (c == '/' && source[i + 1] == '*') { in_block_comment = true; i += 2; continue; }

        if (at_line_start && strncmp(source + i, "import", 6) == 0 &&
            (source[i + 6] == ' ' || source[i + 6] == '\t')) {
            const char* p = source + i + 6;
            while (*p == ' ' || *p == '\t') p++;

            if (*p == '"') {
                p++;
                const char* spec_start = p;
                while (*p && *p != '"' && *p != '\n') {
                    if (*p == '\\' && p[1]) p++;
                    p++;
                }
                if (*p == '"') {
                    size_t spec_len = (size_t)(p - spec_start);
                    char spec[CNEXT_PATH_MAX];
                    if (spec_len == 0 || spec_len >= sizeof(spec)) {
                        fprintf(stderr, "error: invalid import path %.*s\n",
                                (int)spec_len, spec_start);
                        state->failed = true;
                        break;
                    }
                    memcpy(spec, spec_start, spec_len);
                    spec[spec_len] = '\0';

                    char resolved[CNEXT_PATH_MAX];
                    char normalized[CNEXT_PATH_MAX];
                    if (!resolve_import_path(source_dir, spec, resolved, sizeof(resolved))) {
                        fprintf(stderr, "error: import path is too long: \"%s\"\n", spec);
                        state->failed = true;
                        break;
                    }
                    normalize_import_path(resolved, normalized, sizeof(normalized));

                    bool already_loaded = false;
                    for (int v = 0; v < state->count; v++) {
                        if (strcmp(state->paths[v], normalized) == 0) {
                            already_loaded = true;
                            break;
                        }
                    }

                    if (!already_loaded) {
                        if (state->count >= CNEXT_MAX_IMPORT_FILES) {
                            fprintf(stderr, "error: too many imported files (limit %d)\n",
                                    CNEXT_MAX_IMPORT_FILES);
                            state->failed = true;
                            break;
                        }
                        snprintf(state->paths[state->count], CNEXT_PATH_MAX, "%s", normalized);
                        state->count++;

                        struct stat module_info;
                        if (stat(resolved, &module_info) != 0 || !S_ISREG(module_info.st_mode)) {
                            fprintf(stderr, "error: cannot open imported file \"%s\" (looked for \"%s\")\n",
                                    spec, normalized);
                            state->failed = true;
                            break;
                        }

                        char* module_source = read_file(resolved);
                        if (!module_source) {
                            fprintf(stderr, "error: cannot read imported file \"%s\" (looked for \"%s\")\n",
                                    spec, normalized);
                            state->failed = true;
                            break;
                        }

                        char module_dir[CNEXT_PATH_MAX];
                        const char* next_dir = dirname_from_path(resolved, module_dir, sizeof(module_dir))
                            ? module_dir : source_dir;
                        OriginMap* module_map = origin_map_create();
                        char* expanded = expand_local_imports_into(module_source, next_dir, state,
                                                                   normalized, module_map);
                        free(module_source);
                        if (!expanded || state->failed) {
                            free(expanded);
                            origin_map_free(module_map);
                            state->failed = true;
                            break;
                        }

                        append_text(&preamble, &length, &capacity, "\n");
                        preamble_lines += 1;
                        int module_start_line = preamble_lines + 1;
                        append_text(&preamble, &length, &capacity, expanded);
                        preamble_lines += count_newlines(expanded);
                        append_text(&preamble, &length, &capacity, "\n");
                        preamble_lines += 1;

                        if (module_map->count == 0) {
                            /* No imports of its own: the whole module is one block. */
                            origin_map_add(map, module_start_line,
                                           origin_map_count_lines(expanded), normalized, 1);
                        } else {
                            copy_map_entries(map, module_map, module_start_line - 1);
                        }
                        origin_map_free(module_map);
                        free(expanded);
                    }

                    i = (size_t)(p - source) + 1;
                    at_line_start = false;
                    continue;
                }
            }

            /* Not a local import: leave it for the package expander. */
            while (source[i] && source[i] != '\n') i++;
            at_line_start = true;
            continue;
        }

        at_line_start = false;
        if (c == '"' || c == '\'') {
            char quote = c;
            i++;
            while (source[i] && source[i] != quote) {
                if (source[i] == '\\' && source[i + 1]) i++;
                i++;
            }
            if (source[i]) i++;
            continue;
        }
        i++;
    }

    if (state->failed) {
        free(preamble);
        return NULL;
    }

    if (!preamble) return checked_strdup(source);

    size_t total = length + strlen(source) + 2;
    char* result = (char*)checked_malloc(total);
    snprintf(result, total, "%s\n%s", preamble, source);
    free(preamble);

    /* Imported code is pasted first, so the importing file's first line sits
       after the preamble and the blank separator. */
    origin_map_add(map, preamble_lines + 2, origin_map_count_lines(source), file_path, 1);
    return result;
}

/* Each file is rewritten before it is merged: `export` disappears, and when
   the program has local imports the names a file does not export get a suffix
   that is unique to that file (per-file visibility, no cross-file clashes). */
static char* expand_local_imports_into(const char* source, const char* source_dir,
                                       ImportState* state, const char* file_path,
                                       OriginMap* map) {
    int module_tag = state->next_module_tag++;
    char* privatized = module_privatize_source(source, state->privatize ? module_tag : -1);
    if (!privatized) return NULL;
    char* result = expand_local_imports_body(privatized, source_dir, state, file_path, map);
    free(privatized);
    return result;
}

char* build_source_with_local_imports(const char* source, const char* source_dir,
                                      const char* file_path, OriginMap* map) {
    if (!source) return NULL;
    ImportState* state = (ImportState*)checked_calloc(1, sizeof(ImportState));
    state->privatize = source_has_local_import(source) != 0;
    char* result = expand_local_imports_into(source, source_dir, state, file_path, map);
    free(state);
    return result;
}

char* build_source_with_packages(const char* source, const char* project_dir) {
    char visited[32][64] = {{0}};
    int visited_count = 0;

    char* combined = checked_strdup("");
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

                if (name[0] != '"' && !is_standard_module(name) && is_safe_path_component(name)) {
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
                                    char* new_combined = (char*)checked_malloc(new_len);
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
    char* result = (char*)checked_malloc(total_len);
    if (!result) {
        free(combined);
        return NULL;
    }
    strcpy(result, combined);
    strcat(result, source);
    free(combined);
    return result;
}
