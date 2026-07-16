#ifndef REGISTRY_H
#define REGISTRY_H

#include "semver.h"
#include <stdbool.h>

/* Registry configuration:
 * Set CNEXT_REGISTRY_URL environment variable to enable remote registry.
 * Example: export CNEXT_REGISTRY_URL=https://packages.cnext.dev
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

// Search for packages
bool registry_search(const char* query, RegistryResult* result);

// Get package info and versions
bool registry_info(const char* package_name, RegistryResult* result);

// Resolve a version constraint to a specific version
bool registry_resolve(const char* package_name, const VersionSpec* spec, RegistryPackage* out);

// Download a package
bool registry_download(const char* url, const char* dest_path);

// Publish a package
bool registry_publish(const char* package_dir, const char* token);

// Login to registry (saves token)
bool registry_login(const char* registry_url);

// Logout from registry (removes saved token)
bool registry_logout(void);

// Get saved auth token
const char* registry_get_token(void);

// Update all dependencies in cnext.toml
bool registry_update(const char* project_dir);

// Remove a package
bool registry_remove(const char* package_name, const char* project_dir);

void registry_free(RegistryResult* result);

#endif // REGISTRY_H
