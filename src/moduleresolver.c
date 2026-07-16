#include "moduleresolver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

bool module_path_parse(ModulePath* out, const char* import_str) {
    memset(out, 0, sizeof(*out));
    if (!import_str || !*import_str) return false;

    const char* p = import_str;

    // Check for version constraint: std/http@^1.2.3
    char version_part[128] = "";
    const char* at = strchr(p, '@');
    if (at && at > p) {
        int len = (int)(at - p);
        if (len >= (int)strlen(import_str)) len = (int)strlen(import_str) - 1;
        strncpy(version_part, at + 1, sizeof(version_part) - 1);
        // We'll parse the path first, then come back to version
    }

    // Count slashes to determine structure
    int slash_count = 0;
    for (const char* s = p; *s; s++) {
        if (*s == '/' && *(s+1) && *(s+1) != '@') slash_count++;
    }

    char name_buf[256] = "";
    const char* end = at ? at : (p + strlen(p));
    int name_len = (int)(end - p);
    if (name_len >= (int)sizeof(name_buf)) name_len = (int)sizeof(name_buf) - 1;
    strncpy(name_buf, p, name_len);
    name_buf[name_len] = '\0';

    if (slash_count == 0) {
        // Simple: "http" or "my_module"
        snprintf(out->name, sizeof(out->name), "%s", name_buf);
        snprintf(out->namespace, sizeof(out->namespace), "std");
    } else if (slash_count == 1) {
        // Namespaced: "std/http" or "org/pkg"
        char* slash = strchr(name_buf, '/');
        if (slash) {
            *slash = '\0';
            snprintf(out->namespace, sizeof(out->namespace), "%s", name_buf);
            snprintf(out->name, sizeof(out->name), "%s", slash + 1);
        }
    } else if (slash_count == 2) {
        // Submodule: "std/http/server"
        char* first_slash = strchr(name_buf, '/');
        if (first_slash) {
            *first_slash = '\0';
            snprintf(out->namespace, sizeof(out->namespace), "%s", name_buf);
            char* rest = first_slash + 1;
            char* second_slash = strchr(rest, '/');
            if (second_slash) {
                *second_slash = '\0';
                snprintf(out->name, sizeof(out->name), "%s", rest);
                snprintf(out->submodule, sizeof(out->submodule), "%s", second_slash + 1);
            }
        }
    }

    // Parse version if present
    if (version_part[0]) {
        if (semver_parse_spec(&out->version, version_part)) {
            out->has_version = true;
        }
    }

    return true;
}

bool module_path_resolve(const ModulePath* path, const char* project_dir, char* resolved, int resolved_size) {
    if (!path->namespace[0] && !path->name[0]) return false;

    // Check for standard library modules
    if (strcmp(path->namespace, "std") == 0) {
        snprintf(resolved, resolved_size, "lib/std/%s.cn", path->name);
        return true;
    }

    // Namespaced modules: org/pkg -> packages/org-pkg/
    if (path->namespace[0] && strcmp(path->namespace, "std") != 0) {
        snprintf(resolved, resolved_size, "%s/packages/%s-%s/%s",
            project_dir, path->namespace, path->name,
            path->submodule[0] ? path->submodule : path->name);
        return true;
    }

    // Local modules
    snprintf(resolved, resolved_size, "%s/%s", project_dir, path->name);
    return true;
}

bool module_is_std(const ModulePath* path) {
    return strcmp(path->namespace, "std") == 0;
}
