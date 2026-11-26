#include "registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>

extern int run_process(const char* program, const char* const args[]);
extern int run_process_captured(const char* program, const char* const args[], char* output_buf, size_t output_buf_size);

#ifdef _WIN32
#define CNEXT_PATH_SEP '\\'
#else
#define CNEXT_PATH_SEP '/'
#endif

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#define cnext_mkdir(path) _mkdir(path)
#define popen _popen
#define pclose _pclose
#else
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define cnext_mkdir(path) mkdir(path, 0755)
#endif

/* Registry URL - set CNEXT_REGISTRY_URL environment variable to override.
 * Default: local-only mode (no remote registry). */
static bool is_https_url(const char* url) {
    if (!url) return false;
    return strncmp(url, "https://", 8) == 0;
}

static const char* get_registry_url(void) {
    const char* url = getenv("CNEXT_REGISTRY_URL");
    if (url && !is_https_url(url)) {
        fprintf(stderr, "Warning: Registry URL must use HTTPS. Refusing insecure connection to: %s\n", url);
        return NULL;
    }
    return url ? url : NULL;
}

void registry_free(RegistryResult* result) {
    free(result->versions);
    result->versions = NULL;
    result->count = 0;
}

static char registry_cache_dir[512] = "";

static const char* get_cache_dir(void) {
    if (registry_cache_dir[0]) return registry_cache_dir;
#ifdef _WIN32
    const char* home = getenv("USERPROFILE");
    snprintf(registry_cache_dir, sizeof(registry_cache_dir), "%s\\.cnext\\registry", home ? home : ".");
#else
    const char* home = getenv("HOME");
    snprintf(registry_cache_dir, sizeof(registry_cache_dir), "%s/.cnext/registry", home ? home : ".");
#endif
    return registry_cache_dir;
}

static unsigned long cache_temp_counter = 0;

static bool is_safe_registry_name(const char* name) {
    if (!name || name[0] == '\0' || name[0] == '/' || name[strlen(name) - 1] == '/') return false;
    if (strstr(name, "..") || strstr(name, "//")) return false;
    for (const char* p = name; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!(isalnum(c) || c == '.' || c == '_' || c == '-' || c == '/')) return false;
    }
    return true;
}

static char* encode_query(const char* query) {
    if (!query) return NULL;
    size_t len = strlen(query);
    if (len > SIZE_MAX / 3 - 1) return NULL;
    char* encoded = (char*)checked_malloc(len * 3 + 1);
    if (!encoded) return NULL;
    size_t out = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)query[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded[out++] = (char)c;
        } else {
            static const char hex[] = "0123456789ABCDEF";
            encoded[out++] = '%';
            encoded[out++] = hex[c >> 4];
            encoded[out++] = hex[c & 0x0f];
        }
    }
    encoded[out] = '\0';
    return encoded;
}

static bool parse_http_status(const char* text, long* status) {
    if (!status || !text || text[0] == '\0') return false;
    long value = 0;
    for (const char* p = text; *p; p++) {
        if (*p < '0' || *p > '9') return false;
        value = value * 10 + (*p - '0');
        if (value > 999) return false;
    }
    *status = value;
    return true;
}

static bool ensure_cache_dirs(void) {
    char parent[512];
    snprintf(parent, sizeof(parent), "%s", get_cache_dir());
    char* sep = strrchr(parent, CNEXT_PATH_SEP);
    if (!sep || sep == parent) return false;
    *sep = '\0';
    if (cnext_mkdir(parent) != 0 && errno != EEXIST) return false;
    if (cnext_mkdir(get_cache_dir()) != 0 && errno != EEXIST) return false;
    return true;
}

static bool make_cache_temp_path(char* dst, size_t dst_size, const char* prefix) {
#ifdef _WIN32
    unsigned long pid = (unsigned long)_getpid();
#else
    unsigned long pid = (unsigned long)getpid();
#endif
    cache_temp_counter++;
    int written = snprintf(dst, dst_size, "%s%c%s_%lu_%lu.tmp", get_cache_dir(), CNEXT_PATH_SEP,
                           prefix, pid, cache_temp_counter);
    return written >= 0 && (size_t)written < dst_size;
}

static bool http_get_ex(const char* url, const char* token, char** out_body, long* out_status, long* out_size) {
    *out_status = 0;
    *out_body = NULL;
    if (out_size) *out_size = 0;
    if (!url || url[0] == '\0') return false;
    if (!is_https_url(url)) {
        fprintf(stderr, "Warning: Refusing non-HTTPS request. Use HTTPS instead.\n");
        return false;
    }
    if (token && strpbrk(token, "\r\n")) {
        fprintf(stderr, "Invalid token.\n");
        return false;
    }
    if (!ensure_cache_dirs()) return false;
    char tmpfile_path[1024];
    if (!make_cache_temp_path(tmpfile_path, sizeof(tmpfile_path), "http_response")) return false;

    char* auth_header = NULL;
    const char* curl_args[12];
    int argc = 0;
    curl_args[argc++] = "curl";
    curl_args[argc++] = "-s";
    curl_args[argc++] = "-w";
    curl_args[argc++] = "%{http_code}";
    curl_args[argc++] = "-o";
    curl_args[argc++] = tmpfile_path;
    if (token && token[0]) {
        size_t cap = strlen("Authorization: Bearer ") + strlen(token) + 1;
        auth_header = (char*)checked_malloc(cap);
        if (!auth_header) {
            remove(tmpfile_path);
            return false;
        }
        snprintf(auth_header, cap, "Authorization: Bearer %s", token);
        curl_args[argc++] = "-H";
        curl_args[argc++] = auth_header;
    }
    curl_args[argc++] = "--";
    curl_args[argc++] = url;
    curl_args[argc] = NULL;

    char stdout_buf[64] = "";
    int curl_result = run_process_captured("curl", curl_args, stdout_buf, sizeof(stdout_buf));
    free(auth_header);
    long status = 0;
    if (curl_result != 0 || !parse_http_status(stdout_buf, &status)) {
        remove(tmpfile_path);
        return false;
    }
    *out_status = status;

    FILE* f = fopen(tmpfile_path, "rb");
    if (!f) { remove(tmpfile_path); return false; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0 || size > 64L * 1024L * 1024L) {
        fclose(f);
        remove(tmpfile_path);
        *out_status = 0;
        return false;
    }
    rewind(f);
    *out_body = (char*)checked_malloc((size_t)size + 1);
    if (!*out_body) {
        fclose(f);
        remove(tmpfile_path);
        *out_status = 0;
        return false;
    }
    size_t bytes_read = size > 0 ? fread(*out_body, 1, (size_t)size, f) : 0;
    fclose(f);
    remove(tmpfile_path);
    if (size > 0 && bytes_read != (size_t)size) {
        free(*out_body);
        *out_body = NULL;
        *out_status = 0;
        return false;
    }
    (*out_body)[size] = '\0';
    if (out_size) *out_size = size;
    return true;
}

static bool http_get(const char* url, char** out_body, long* out_status, long* out_size) {
    return http_get_ex(url, NULL, out_body, out_status, out_size);
}

static bool http_post(const char* url, const char* body, const char* token, long* out_status) {
    *out_status = 0;
    if (!url || url[0] == '\0') return false;
    if (!is_https_url(url)) {
        fprintf(stderr, "Warning: Refusing non-HTTPS request. Use HTTPS instead.\n");
        return false;
    }
    if (!ensure_cache_dirs()) return false;
    char tmpfile_path[1024];
    char body_tmpfile[1024];
    if (!make_cache_temp_path(tmpfile_path, sizeof(tmpfile_path), "http_response")) return false;
    if (!make_cache_temp_path(body_tmpfile, sizeof(body_tmpfile), "http_body")) return false;

    // Write body to temp file (avoids piping via shell)
    if (body) {
        FILE* bf = fopen(body_tmpfile, "wb");
        if (!bf) { remove(tmpfile_path); return false; }
        size_t body_len = strlen(body);
        size_t written = fwrite(body, 1, body_len, bf);
        fclose(bf);
        if (written != body_len) {
            remove(body_tmpfile);
            remove(tmpfile_path);
            return false;
        }
    }
    if (token && strpbrk(token, "\r\n")) {
        remove(body_tmpfile);
        remove(tmpfile_path);
        return false;
    }

    // Build curl args array (no shell interpretation)
    const char* curl_args[17];
    int argc = 0;
    curl_args[argc++] = "curl";
    curl_args[argc++] = "-s";
    curl_args[argc++] = "-w";
    curl_args[argc++] = "%{http_code}";
    curl_args[argc++] = "-o";
    curl_args[argc++] = tmpfile_path;
    curl_args[argc++] = "-X";
    curl_args[argc++] = "POST";
    curl_args[argc++] = "-H";
    curl_args[argc++] = "Content-Type: application/json";
    char* auth_header = NULL;
    char* data_arg = NULL;
    if (token && token[0]) {
        size_t cap = strlen("Authorization: Bearer ") + strlen(token) + 1;
        auth_header = (char*)checked_malloc(cap);
        if (!auth_header) {
            remove(body_tmpfile);
            remove(tmpfile_path);
            return false;
        }
        snprintf(auth_header, cap, "Authorization: Bearer %s", token);
        curl_args[argc++] = "-H";
        curl_args[argc++] = auth_header;
    }
    if (body) {
        size_t cap = strlen("-d@") + strlen(body_tmpfile) + 1;
        data_arg = (char*)checked_malloc(cap);
        if (!data_arg) {
            free(auth_header);
            remove(body_tmpfile);
            remove(tmpfile_path);
            return false;
        }
        snprintf(data_arg, cap, "-d@%s", body_tmpfile);
        curl_args[argc++] = data_arg;
    }
    curl_args[argc++] = "--";
    curl_args[argc++] = url;
    curl_args[argc] = NULL;

    char stdout_buf[64] = "";
    int curl_result = run_process_captured("curl", curl_args, stdout_buf, sizeof(stdout_buf));
    long status = 0;
    bool ok = curl_result == 0 && parse_http_status(stdout_buf, &status);
    if (ok) *out_status = status;

    free(auth_header);
    free(data_arg);
    remove(body_tmpfile);
    remove(tmpfile_path);
    return ok;
}

bool registry_search(const char* query, RegistryResult* result) {
    result->versions = NULL;
    result->count = 0;

    const char* registry_url = get_registry_url();
    if (!registry_url) {
        fprintf(stderr, "No registry configured. Set CNEXT_REGISTRY_URL to use remote registry.\n");
        fprintf(stderr, "Searching local cache only...\n");
    }

    char url[1024];
    char* body = NULL;
    long status = 0;

    if (registry_url) {
        char* encoded_query = encode_query(query);
        if (!encoded_query) return false;
        int written = snprintf(url, sizeof(url), "%s/api/search?q=%s", registry_url, encoded_query);
        free(encoded_query);
        if (written < 0 || (size_t)written >= sizeof(url)) return false;
        if (http_get(url, &body, &status, NULL) && status == 200) {
            char* line = strtok(body, "\n");
            int capacity = 0;
            while (line) {
                line[strcspn(line, "\r")] = '\0';
                if (line[0] == '\0') { line = strtok(NULL, "\n"); continue; }

                if (result->count >= capacity) {
                    capacity = capacity ? capacity * 2 : 8;
                    RegistryPackage* nv = checked_realloc(result->versions, sizeof(RegistryPackage) * capacity);
                    if (!nv) break;
                    result->versions = nv;
                }
                RegistryPackage* pkg = &result->versions[result->count++];
                memset(pkg, 0, sizeof(*pkg));

                char* tok = strtok(line, "|");
                if (tok) strncpy(pkg->name, tok, sizeof(pkg->name) - 1);
                tok = strtok(NULL, "|");
                if (tok) strncpy(pkg->version, tok, sizeof(pkg->version) - 1);
                tok = strtok(NULL, "|");
                if (tok) strncpy(pkg->description, tok, sizeof(pkg->description) - 1);

                line = strtok(NULL, "\n");
            }
            free(body);
            return result->count > 0;
        }
        free(body);
    }

    char path[1024];
    snprintf(path, sizeof(path), "%s/index.txt", get_cache_dir());
    FILE* index = fopen(path, "rb");
    if (!index) {
        fprintf(stderr, "No packages found.\n");
        return false;
    }
    char line[1024];
    int capacity = 0;
    while (fgets(line, sizeof(line), index)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strstr(line, query)) {
            if (result->count >= capacity) {
                capacity = capacity ? capacity * 2 : 8;
                RegistryPackage* nv = checked_realloc(result->versions, sizeof(RegistryPackage) * capacity);
                if (!nv) break;
                result->versions = nv;
            }
            RegistryPackage* pkg = &result->versions[result->count++];
            memset(pkg, 0, sizeof(*pkg));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(pkg->name, sizeof(pkg->name), "%s", line);
#pragma GCC diagnostic pop
        }
    }
    fclose(index);
    return result->count > 0;
}

bool registry_info(const char* package_name, RegistryResult* result) {
    result->versions = NULL;
    result->count = 0;
    if (!is_safe_registry_name(package_name)) {
        fprintf(stderr, "Invalid package name \"%s\".\n", package_name ? package_name : "(null)");
        return false;
    }

    const char* registry_url = get_registry_url();
    if (registry_url) {
        char url[1024];
        snprintf(url, sizeof(url), "%s/api/packages/%s", registry_url, package_name);

        char* body = NULL;
        long status = 0;
        if (http_get(url, &body, &status, NULL) && status == 200) {
            char* line = strtok(body, "\n");
            int capacity = 0;
            while (line) {
                line[strcspn(line, "\r")] = '\0';
                if (line[0] == '\0') { line = strtok(NULL, "\n"); continue; }

                if (result->count >= capacity) {
                    capacity = capacity ? capacity * 2 : 8;
                    RegistryPackage* nv = checked_realloc(result->versions, sizeof(RegistryPackage) * capacity);
                    if (!nv) break;
                    result->versions = nv;
                }
                RegistryPackage* pkg = &result->versions[result->count++];
                memset(pkg, 0, sizeof(*pkg));

                char* tok = strtok(line, "|");
                if (tok) strncpy(pkg->version, tok, sizeof(pkg->version) - 1);
                tok = strtok(NULL, "|");
                if (tok) strncpy(pkg->description, tok, sizeof(pkg->description) - 1);
                tok = strtok(NULL, "|");
                if (tok) strncpy(pkg->url, tok, sizeof(pkg->url) - 1);
                strncpy(pkg->name, package_name, sizeof(pkg->name) - 1);

                line = strtok(NULL, "\n");
            }
            free(body);
            return result->count > 0;
        }
        free(body);
    }

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s/versions.txt", get_cache_dir(), package_name);
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Package \"%s\" not found.\n", package_name);
        return false;
    }
    char line[1024];
    int capacity = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;
        if (result->count >= capacity) {
            capacity = capacity ? capacity * 2 : 8;
            RegistryPackage* nv = checked_realloc(result->versions, sizeof(RegistryPackage) * capacity);
            if (!nv) break;
            result->versions = nv;
        }
        RegistryPackage* pkg = &result->versions[result->count++];
        memset(pkg, 0, sizeof(*pkg));
        char* tok = strtok(line, "|");
        if (tok) strncpy(pkg->version, tok, sizeof(pkg->version) - 1);
        tok = strtok(NULL, "|");
        if (tok) strncpy(pkg->description, tok, sizeof(pkg->description) - 1);
        strncpy(pkg->name, package_name, sizeof(pkg->name) - 1);
    }
    fclose(f);
    return result->count > 0;
}

bool registry_resolve(const char* package_name, const VersionSpec* spec, RegistryPackage* out) {
    RegistryResult result;
    if (!registry_info(package_name, &result)) return false;

    bool found = false;
    SemVer best = {0, 0, 0, "", ""};

    for (int i = 0; i < result.count; i++) {
        SemVer ver;
        if (semver_parse(&ver, result.versions[i].version)) {
            if (semver_satisfies(&ver, spec)) {
                if (!found || semver_compare(&ver, &best) > 0) {
                    best = ver;
                    *out = result.versions[i];
                    found = true;
                }
            }
        }
    }

    registry_free(&result);
    return found;
}

bool registry_download(const char* url, const char* dest_path) {
    fprintf(stderr, "Downloading %s...\n", url);
    long status = 0;
    long body_size = 0;
    char* body = NULL;
    if (!http_get(url, &body, &status, &body_size) || status != 200) {
        fprintf(stderr, "Download failed (HTTP %ld).\n", status);
        free(body);
        return false;
    }
    FILE* f = fopen(dest_path, "wb");
    if (!f) {
        fprintf(stderr, "Could not write to \"%s\".\n", dest_path);
        free(body);
        return false;
    }
    size_t written = body_size > 0 ? fwrite(body, 1, (size_t)body_size, f) : 0;
    bool ok = body_size == 0 || written == (size_t)body_size;
    if (fclose(f) != 0) ok = false;
    free(body);
    return ok;
}

bool registry_publish(const char* package_dir, const char* token) {
    char toml_path[1024];
    snprintf(toml_path, sizeof(toml_path), "%s/cnext.toml", package_dir);

    FILE* f = fopen(toml_path, "rb");
    if (!f) {
        fprintf(stderr, "Could not find cnext.toml in \"%s\"\n", package_dir);
        return false;
    }

    char name[256] = "";
    char version[64] = "";
    char description[512] = "";

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strstr(line, "name")) {
            char* eq = strchr(line, '=');
            if (eq) { eq++; while (*eq == ' ' || *eq == '"') eq++; char* end = eq + strlen(eq) - 1; if (end > eq && *end == '"') *end = '\0'; strncpy(name, eq, sizeof(name)-1); }
        } else if (strstr(line, "version")) {
            char* eq = strchr(line, '=');
            if (eq) { eq++; while (*eq == ' ' || *eq == '"') eq++; char* end = eq + strlen(eq) - 1; if (end > eq && *end == '"') *end = '\0'; strncpy(version, eq, sizeof(version)-1); }
        } else if (strstr(line, "description")) {
            char* eq = strchr(line, '=');
            if (eq) { eq++; while (*eq == ' ' || *eq == '"') eq++; char* end = eq + strlen(eq) - 1; if (end > eq && *end == '"') *end = '\0'; strncpy(description, eq, sizeof(description)-1); }
        }
    }
    fclose(f);

    if (!name[0] || !version[0]) {
        fprintf(stderr, "cnext.toml must have name and version fields.\n");
        return false;
    }
    SemVer parsed_version;
    if (!is_safe_registry_name(name) || !semver_parse(&parsed_version, version)) {
        fprintf(stderr, "Invalid package name or version in cnext.toml.\n");
        return false;
    }

    char tarball[1024];
    snprintf(tarball, sizeof(tarball), "%s/%s-%s.tar.gz", package_dir, name, version);
    char pkg_dir_buf[1024];
    strncpy(pkg_dir_buf, package_dir, sizeof(pkg_dir_buf) - 1);
    pkg_dir_buf[sizeof(pkg_dir_buf) - 1] = '\0';
    const char* const tar_args[] = {"tar", "-czf", tarball, "-C", pkg_dir_buf, ".", NULL};
    int status = run_process("tar", tar_args);
    if (status != 0) {
        fprintf(stderr, "Failed to create package tarball.\n");
        return false;
    }

    const char* registry_url = get_registry_url();
    if (!registry_url) {
        fprintf(stderr, "Cannot publish: no registry configured.\n");
        fprintf(stderr, "Set CNEXT_REGISTRY_URL environment variable to use remote registry.\n");
        remove(tarball);
        return false;
    }

    fprintf(stderr, "Publishing %s@%s to %s...\n", name, version, registry_url);

    char post_url[1024];
    snprintf(post_url, sizeof(post_url), "%s/api/publish", registry_url);

    // Read tarball as body
    FILE* tf = fopen(tarball, "rb");
    if (!tf) {
        fprintf(stderr, "Could not read tarball.\n");
        remove(tarball);
        return false;
    }
    fseek(tf, 0, SEEK_END);
    long tsize = ftell(tf);
    if (tsize < 0) { fclose(tf); remove(tarball); return false; }
    rewind(tf);
    char* tdata = (char*)checked_malloc(tsize);
    if (tdata && fread(tdata, 1, tsize, tf)) { /* read ok */ }
    fclose(tf);

    long http_status = 0;
    bool ok = http_post(post_url, tdata, token, &http_status);
    free(tdata);
    remove(tarball);

    if (!ok) {
        fprintf(stderr, "Upload failed.\n");
        return false;
    }

    if (http_status == 200 || http_status == 201) {
        fprintf(stderr, "Published %s@%s successfully.\n", name, version);
        return true;
    } else {
        fprintf(stderr, "Upload failed (HTTP %ld).\n", http_status);
        return false;
    }
}

// --- Auth Token Management ---

static char auth_token_path[512] = "";

static const char* get_auth_token_path(void) {
    if (auth_token_path[0]) return auth_token_path;
#ifdef _WIN32
    const char* home = getenv("USERPROFILE");
    snprintf(auth_token_path, sizeof(auth_token_path), "%s\\.cnext\\token", home ? home : ".");
#else
    const char* home = getenv("HOME");
    snprintf(auth_token_path, sizeof(auth_token_path), "%s/.cnext/token", home ? home : ".");
#endif
    return auth_token_path;
}

const char* registry_get_token(void) {
    static char token[512] = "";
    if (token[0]) return token;
    FILE* f = fopen(get_auth_token_path(), "r");
    if (!f) return NULL;
    if (fgets(token, sizeof(token), f)) {
        token[strcspn(token, "\r\n")] = '\0';
    }
    fclose(f);
    return token[0] ? token : NULL;
}

static void save_token(const char* token) {
    const char* path = get_auth_token_path();
    if (!token || strpbrk(token, "\r\n")) {
        fprintf(stderr, "Invalid token.\n");
        return;
    }
    if (!ensure_cache_dirs()) {
        fprintf(stderr, "Could not save token to %s\n", path);
        return;
    }
    char tmp_path[1024];
    if (!make_cache_temp_path(tmp_path, sizeof(tmp_path), "token")) {
        fprintf(stderr, "Could not save token to %s\n", path);
        return;
    }
    FILE* f = fopen(tmp_path, "w");
    if (!f) {
        fprintf(stderr, "Could not save token to %s\n", path);
        return;
    }
    size_t len = strlen(token);
    bool ok = fwrite(token, 1, len, f) == len && fputc('\n', f) != EOF;
    if (fclose(f) != 0) ok = false;
#ifndef _WIN32
    if (ok && chmod(tmp_path, 0600) != 0) ok = false;
#endif
    if (!ok) {
        remove(tmp_path);
        fprintf(stderr, "Could not save token to %s\n", path);
        return;
    }
    remove(path);
    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        fprintf(stderr, "Could not save token to %s\n", path);
        return;
    }
    fprintf(stderr, "Token saved.\n");
}

static void remove_token(void) {
    const char* path = get_auth_token_path();
    if (remove(path) == 0) {
        fprintf(stderr, "Logged out. Token removed.\n");
    } else {
        fprintf(stderr, "No token found.\n");
    }
}

bool registry_login(const char* registry_url) {
    if (registry_url && !is_https_url(registry_url)) {
        fprintf(stderr, "Registry URL must use HTTPS.\n");
        return false;
    }
    fprintf(stderr, "Login to %s\n", registry_url);
    fprintf(stderr, "Enter your API token: ");

    char token[512];
    if (!fgets(token, sizeof(token), stdin)) {
        fprintf(stderr, "Login cancelled.\n");
        return false;
    }
    token[strcspn(token, "\r\n")] = '\0';

    if (token[0] == '\0') {
        fprintf(stderr, "No token provided.\n");
        return false;
    }

    // Verify token by making a test request
    if (registry_url) {
        char url[1024];
        snprintf(url, sizeof(url), "%s/api/me", registry_url);
        char* body = NULL;
        long status = 0;
        if (!http_get_ex(url, token, &body, &status, NULL)) {
            free(body);
            fprintf(stderr, "Token verification failed.\n");
            return false;
        }
        free(body);
        if (status != 200) {
            fprintf(stderr, "Invalid token.\n");
            return false;
        }
    }

    save_token(token);
    return true;
}

bool registry_logout(void) {
    remove_token();
    return true;
}

// --- Update ---

// --- Remove ---

bool registry_remove(const char* package_name, const char* project_dir) {
    // Path traversal guard: reject names with ".." or path separators
    if (!package_name || package_name[0] == '\0') return false;
    for (const char* p = package_name; *p; p++) {
        if ((*p == '.' && *(p + 1) == '.') || *p == '/' || *p == '\\') {
            fprintf(stderr, "Invalid package name: contains path separators.\n");
            return false;
        }
    }

    char toml_path[1024];
    snprintf(toml_path, sizeof(toml_path), "%s/cnext.toml", project_dir);

    FILE* f = fopen(toml_path, "r");
    if (!f) {
        fprintf(stderr, "No cnext.toml found.\n");
        return false;
    }

    // Read entire file
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) { fclose(f); return false; }
    rewind(f);
    char* content = (char*)checked_malloc(size + 1);
    if (!content) { fclose(f); return false; }
    if (fread(content, 1, size, f)) { /* read ok */ }
    content[size] = '\0';
    fclose(f);

    // Find and remove the dependency line
    char* dep_start = strstr(content, package_name);
    if (!dep_start) {
        fprintf(stderr, "Package '%s' not found in cnext.toml.\n", package_name);
        free(content);
        return false;
    }

    // Find the start of the line
    char* line_start = dep_start;
    while (line_start > content && *(line_start - 1) != '\n') line_start--;

    // Find the end of the line
    char* line_end = dep_start;
    while (*line_end && *line_end != '\n') line_end++;
    if (*line_end == '\n') line_end++;

    // Write back without that line
    FILE* out = fopen(toml_path, "w");
    if (!out) { free(content); return false; }
    fwrite(content, 1, line_start - content, out);
    fwrite(line_end, 1, strlen(line_end), out);
    fclose(out);
    free(content);

    // Remove cached package
    char pkg_dir[1024];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/packages/%s", project_dir, package_name);
    // rmdir recursively (simplified)
    remove(pkg_dir);

    fprintf(stderr, "Removed package '%s'.\n", package_name);
    return true;
}
