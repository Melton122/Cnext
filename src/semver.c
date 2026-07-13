#include "semver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int parse_int(const char** s) {
    int val = 0;
    while (**s && isdigit(**s)) {
        val = val * 10 + (**s - '0');
        (*s)++;
    }
    return val;
}

bool semver_parse(SemVer* out, const char* str) {
    memset(out, 0, sizeof(*out));
    if (!str || !*str) return false;

    const char* p = str;

    // major
    if (!isdigit(*p)) return false;
    out->major = parse_int(&p);

    // minor
    if (*p == '.') { p++; } else { return false; }
    if (!isdigit(*p)) return false;
    out->minor = parse_int(&p);

    // patch
    if (*p == '.') { p++; } else { return false; }
    if (!isdigit(*p)) return false;
    out->patch = parse_int(&p);

    // prerelease
    if (*p == '-') {
        p++;
        int i = 0;
        while (*p && *p != '+' && i < (int)sizeof(out->prerelease) - 1) {
            out->prerelease[i++] = *p++;
        }
        out->prerelease[i] = '\0';
    }

    // build
    if (*p == '+') {
        p++;
        int i = 0;
        while (*p && i < (int)sizeof(out->build) - 1) {
            out->build[i++] = *p++;
        }
        out->build[i] = '\0';
    }

    return true;
}

bool semver_parse_spec(VersionSpec* out, const char* str) {
    memset(out, 0, sizeof(*out));
    if (!str || !*str) {
        out->op = COMP_ANY;
        return true;
    }

    const char* p = str;
    VersionConstraint op = COMP_EXACT;

    if (p[0] == '^' && p[1] != '=') { op = COMP_CARET; p++; }
    else if (p[0] == '~' && p[1] != '=') { op = COMP_TILDE; p++; }
    else if (p[0] == '>' && p[1] == '=') { op = COMP_GTE; p += 2; }
    else if (p[0] == '>' && p[1] != '=') { op = COMP_GT; p++; }
    else if (p[0] == '<' && p[1] == '=') { op = COMP_LTE; p += 2; }
    else if (p[0] == '<' && p[1] != '=') { op = COMP_LT; p++; }
    else if (p[0] == '=' && p[1] == '=') { op = COMP_EXACT; p += 2; }
    else if (*p == '*') { out->op = COMP_ANY; return true; }

    out->op = op;
    return semver_parse(&out->version, p);
}

int semver_compare(const SemVer* a, const SemVer* b) {
    if (a->major != b->major) return a->major - b->major;
    if (a->minor != b->minor) return a->minor - b->minor;
    if (a->patch != b->patch) return a->patch - b->patch;

    // prerelease versions have lower precedence
    bool a_pre = a->prerelease[0] != '\0';
    bool b_pre = b->prerelease[0] != '\0';
    if (a_pre && !b_pre) return -1;
    if (!a_pre && b_pre) return 1;
    if (!a_pre && !b_pre) return 0;

    return strcmp(a->prerelease, b->prerelease);
}

bool semver_satisfies(const SemVer* version, const VersionSpec* spec) {
    switch (spec->op) {
        case COMP_ANY: return true;
        case COMP_EXACT: return semver_compare(version, &spec->version) == 0;
        case COMP_GTE: return semver_compare(version, &spec->version) >= 0;
        case COMP_GT: return semver_compare(version, &spec->version) > 0;
        case COMP_LTE: return semver_compare(version, &spec->version) <= 0;
        case COMP_LT: return semver_compare(version, &spec->version) < 0;
        case COMP_CARET: {
            int cmp = semver_compare(version, &spec->version);
            if (cmp < 0) return false;
            // ^1.2.3 means >=1.2.3 <2.0.0
            // ^0.2.3 means >=0.2.3 <0.3.0
            // ^0.0.3 means >=0.0.3 <0.0.4
            if (spec->version.major > 0) {
                return version->major == spec->version.major;
            } else if (spec->version.minor > 0) {
                return version->major == spec->version.major &&
                       version->minor == spec->version.minor;
            } else {
                return version->major == spec->version.major &&
                       version->minor == spec->version.minor &&
                       version->patch == spec->version.patch;
            }
        }
        case COMP_TILDE: {
            int cmp = semver_compare(version, &spec->version);
            if (cmp < 0) return false;
            // ~1.2.3 means >=1.2.3 <1.3.0
            return version->major == spec->version.major &&
                   version->minor == spec->version.minor;
        }
    }
    return false;
}

char* semver_to_string(const SemVer* ver, char* buf, int buf_size) {
    if (ver->prerelease[0] && ver->build[0]) {
        snprintf(buf, buf_size, "%d.%d.%d-%s+%s",
            ver->major, ver->minor, ver->patch,
            ver->prerelease, ver->build);
    } else if (ver->prerelease[0]) {
        snprintf(buf, buf_size, "%d.%d.%d-%s",
            ver->major, ver->minor, ver->patch,
            ver->prerelease);
    } else if (ver->build[0]) {
        snprintf(buf, buf_size, "%d.%d.%d+%s",
            ver->major, ver->minor, ver->patch,
            ver->build);
    } else {
        snprintf(buf, buf_size, "%d.%d.%d",
            ver->major, ver->minor, ver->patch);
    }
    return buf;
}
