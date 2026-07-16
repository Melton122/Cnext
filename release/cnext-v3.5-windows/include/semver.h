#ifndef SEMVER_H
#define SEMVER_H

#include <stdbool.h>

typedef struct {
    int major;
    int minor;
    int patch;
    char prerelease[64]; // e.g. "alpha.1", ""
    char build[64];      // e.g. "build.123", ""
} SemVer;

typedef enum {
    COMP_EXACT,      // 1.2.3
    COMP_CARET,      // ^1.2.3  (>=1.2.3 <2.0.0)
    COMP_TILDE,      // ~1.2.3  (>=1.2.3 <1.3.0)
    COMP_GTE,        // >=1.2.3
    COMP_GT,         // >1.2.3
    COMP_LTE,        // <=1.2.3
    COMP_LT,         // <1.2.3
    COMP_ANY,        // * or ""
} VersionConstraint;

typedef struct {
    VersionConstraint op;
    SemVer version;
} VersionSpec;

bool semver_parse(SemVer* out, const char* str);
bool semver_parse_spec(VersionSpec* out, const char* str);
int semver_compare(const SemVer* a, const SemVer* b);
bool semver_satisfies(const SemVer* version, const VersionSpec* spec);
char* semver_to_string(const SemVer* ver, char* buf, int buf_size);

#endif // SEMVER_H
