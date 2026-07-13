#ifndef MODULERESOLVER_H
#define MODULERESOLVER_H

#include "semver.h"
#include <stdbool.h>

#define MAX_MODULE_PATH 512

typedef struct {
    char namespace[128];  // "std", "org", ""
    char name[128];       // "http", "pkg", "my_module"
    char submodule[128];  // "server", "", ""
    VersionSpec version;
    bool has_version;
} ModulePath;

bool module_path_parse(ModulePath* out, const char* import_str);
bool module_path_resolve(const ModulePath* path, const char* project_dir, char* resolved, int resolved_size);
bool module_is_std(const ModulePath* path);

#endif // MODULERESOLVER_H
