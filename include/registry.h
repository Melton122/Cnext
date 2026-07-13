#ifndef REGISTRY_H
#define REGISTRY_H

#include "semver.h"
#include <stdbool.h>

/* Registry configuration:
 * Set CNEXT_REGISTRY_URL environment variable to enable remote registry.
 * Example: export CNEXT_REGISTRY_URL=https://registry.example.com
 * Without this, only local package cache is used. */

typedef struct {
    char name[256];
    char version[64];
    char description[512];
    char author[128];
    char url[512];
    char hash[128];
} RegistryPackage;

typedef struct {
    RegistryPackage* versions;
    int count;
} RegistryResult;

bool registry_search(const char* query, RegistryResult* result);
bool registry_info(const char* package_name, RegistryResult* result);
bool registry_resolve(const char* package_name, const VersionSpec* spec, RegistryPackage* out);
bool registry_download(const char* url, const char* dest_path);
bool registry_publish(const char* package_dir, const char* token);
void registry_free(RegistryResult* result);

#endif // REGISTRY_H
