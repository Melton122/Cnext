#ifndef CNEXT_ARGS_H
#define CNEXT_ARGS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int _cnext_argc = 0;
static char** _cnext_argv = NULL;

static inline void args_init(int argc, char** argv) {
    _cnext_argc = argc;
    _cnext_argv = argv;
}

static inline int args_count(void) {
    return _cnext_argc;
}

static inline CnextString args_get(int index) {
    if (index < 0 || index >= _cnext_argc) return (CnextString){NULL, 0};
    char* s = _cnext_argv[index];
    return (CnextString){s, strlen(s)};
}

static inline CnextString args_program_name(void) {
    return args_get(0);
}

static inline bool args_has(CnextString name) {
    for (int i = 1; i < _cnext_argc; i++) {
        char* arg = _cnext_argv[i];
        if (arg[0] == '-' && arg[1] == '-') {
            if (strcmp(arg + 2, name.data) == 0) return true;
        } else if (arg[0] == '-' && arg[1] != '-') {
            if (strlen(arg) == 2 && arg[1] == name.data[0]) return true;
        }
    }
    return false;
}

static inline CnextString args_value(CnextString name) {
    for (int i = 1; i < _cnext_argc - 1; i++) {
        char* arg = _cnext_argv[i];
        if (arg[0] == '-' && arg[1] == '-') {
            if (strcmp(arg + 2, name.data) == 0) {
                char* val = _cnext_argv[i + 1];
                return (CnextString){val, strlen(val)};
            }
        } else if (arg[0] == '-' && arg[1] != '-') {
            if (strlen(arg) == 2 && arg[1] == name.data[0]) {
                char* val = _cnext_argv[i + 1];
                return (CnextString){val, strlen(val)};
            }
        }
    }
    return (CnextString){NULL, 0};
}

static inline int args_value_int(CnextString name) {
    CnextString val = args_value(name);
    if (!val.data) return 0;
    return atoi(val.data);
}

static inline bool args_value_bool(CnextString name) {
    return args_has(name);
}

static inline CnextString args_value_default(CnextString name, CnextString default_val) {
    CnextString val = args_value(name);
    return val.data ? val : default_val;
}

static inline void args_print_usage(CnextString program, CnextString description) {
    printf("%.*s - %.*s\n\n", (int)program.length, program.data, (int)description.length, description.data);
    printf("Usage: %.*s [options] [arguments]\n\n", (int)program.length, program.data);
    printf("Options:\n");
    printf("  --help, -h       Show this help message\n");
    printf("  --version, -v    Show version\n");
}

#endif
