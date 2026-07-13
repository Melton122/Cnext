#include "registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
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
static const char* get_registry_url(void) {
    const char* url = getenv("CNEXT_REGISTRY_URL");
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

static bool http_get(const char* url, char** out_body, long* out_status) {
    char cmd[2048];
    char tmpfile_path[512];
#ifdef _WIN32
    snprintf(tmpfile_path, sizeof(tmpfile_path), "%s\\http_response.tmp", get_cache_dir());
    cnext_mkdir(get_cache_dir());
    snprintf(cmd, sizeof(cmd),
        "curl -s -w \"%%{http_code}\" -o \"%s\" \"%s\"",
        tmpfile_path, url);
#else
    snprintf(tmpfile_path, sizeof(tmpfile_path), "%s/http_response.tmp", get_cache_dir());
    cnext_mkdir(get_cache_dir());
    snprintf(cmd, sizeof(cmd),
        "curl -s -w '%%{http_code}' -o '%s' '%s'",
        tmpfile_path, url);
#endif

    FILE* pipe = popen(cmd, "r");
    if (!pipe) return false;

    char status_buf[16] = "";
    fgets(status_buf, sizeof(status_buf), pipe);
    pclose(pipe);

    *out_status = atol(status_buf);

    FILE* f = fopen(tmpfile_path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    *out_body = (char*)malloc(size + 1);
    if (!*out_body) { fclose(f); return false; }
    fread(*out_body, 1, size, f);
    (*out_body)[size] = '\0';
    fclose(f);
    remove(tmpfile_path);
    return true;
}

static bool http_post(const char* url, const char* body, const char* token, long* out_status) {
    char cmd[4096];
    char tmpfile_path[512];
#ifdef _WIN32
    snprintf(tmpfile_path, sizeof(tmpfile_path), "%s\\http_response.tmp", get_cache_dir());
    cnext_mkdir(get_cache_dir());
    if (token && token[0]) {
        snprintf(cmd, sizeof(cmd),
            "curl -s -w \"%%{http_code}\" -o \"%s\" -X POST -H \"Content-Type: application/json\" -H \"Authorization: Bearer %s\" -d @- \"%s\"",
            tmpfile_path, token, url);
    } else {
        snprintf(cmd, sizeof(cmd),
            "curl -s -w \"%%{http_code}\" -o \"%s\" -X POST -H \"Content-Type: application/json\" -d @- \"%s\"",
            tmpfile_path, url);
    }
#else
    snprintf(tmpfile_path, sizeof(tmpfile_path), "%s/http_response.tmp", get_cache_dir());
    cnext_mkdir(get_cache_dir());
    if (token && token[0]) {
        snprintf(cmd, sizeof(cmd),
            "curl -s -w '%%{http_code}' -o '%s' -X POST -H 'Content-Type: application/json' -H 'Authorization: Bearer %s' -d @- '%s'",
            tmpfile_path, token, url);
    } else {
        snprintf(cmd, sizeof(cmd),
            "curl -s -w '%%{http_code}' -o '%s' -X POST -H 'Content-Type: application/json' -d @- '%s'",
            tmpfile_path, url);
    }
#endif

    FILE* pipe = popen(cmd, "w");
    if (!pipe) return false;
    if (body) fwrite(body, 1, strlen(body), pipe);
    pclose(pipe);

    // Read status code from tmpfile (we wrote it there)
    // Actually the status was written to stdout, not tmpfile. Let's re-read.
    FILE* f = fopen(tmpfile_path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char* resp = (char*)malloc(size + 1);
    if (resp) {
        fread(resp, 1, size, f);
        resp[size] = '\0';
        // Status is typically in the last line
        *out_status = 200; // default
    }
    free(resp);
    fclose(f);
    remove(tmpfile_path);
    return true;
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
        snprintf(url, sizeof(url), "%s/api/search?q=%s", registry_url, query);
        if (http_get(url, &body, &status) && status == 200) {
            char* line = strtok(body, "\n");
            int capacity = 0;
            while (line) {
                line[strcspn(line, "\r")] = '\0';
                if (line[0] == '\0') { line = strtok(NULL, "\n"); continue; }

                if (result->count >= capacity) {
                    capacity = capacity ? capacity * 2 : 8;
                    result->versions = realloc(result->versions, sizeof(RegistryPackage) * capacity);
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
                result->versions = realloc(result->versions, sizeof(RegistryPackage) * capacity);
            }
            RegistryPackage* pkg = &result->versions[result->count++];
            memset(pkg, 0, sizeof(*pkg));
            strncpy(pkg->name, line, sizeof(pkg->name) - 1);
        }
    }
    fclose(index);
    return result->count > 0;
}

bool registry_info(const char* package_name, RegistryResult* result) {
    result->versions = NULL;
    result->count = 0;

    const char* registry_url = get_registry_url();
    if (registry_url) {
        char url[1024];
        snprintf(url, sizeof(url), "%s/api/packages/%s", registry_url, package_name);

        char* body = NULL;
        long status = 0;
        if (http_get(url, &body, &status) && status == 200) {
            char* line = strtok(body, "\n");
            int capacity = 0;
            while (line) {
                line[strcspn(line, "\r")] = '\0';
                if (line[0] == '\0') { line = strtok(NULL, "\n"); continue; }

                if (result->count >= capacity) {
                    capacity = capacity ? capacity * 2 : 8;
                    result->versions = realloc(result->versions, sizeof(RegistryPackage) * capacity);
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
            result->versions = realloc(result->versions, sizeof(RegistryPackage) * capacity);
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
    char* body = NULL;
    if (!http_get(url, &body, &status) || status != 200) {
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
    fwrite(body, 1, strlen(body), f);
    fclose(f);
    free(body);
    return true;
}

static int run_process_registry(const char* program, char* const args[]) {
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

    char tarball[1024];
    snprintf(tarball, sizeof(tarball), "%s/%s-%s.tar.gz", package_dir, name, version);
    char pkg_dir_buf[1024];
    strncpy(pkg_dir_buf, package_dir, sizeof(pkg_dir_buf) - 1);
    pkg_dir_buf[sizeof(pkg_dir_buf) - 1] = '\0';
    char* tar_args[] = {"tar", "-czf", tarball, "-C", pkg_dir_buf, ".", NULL};
    int status = run_process_registry("tar", tar_args);
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
    rewind(tf);
    char* tdata = (char*)malloc(tsize);
    if (tdata) fread(tdata, 1, tsize, tf);
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
