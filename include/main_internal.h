#ifndef MAIN_INTERNAL_H
#define MAIN_INTERNAL_H

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#define CNEXT_PATH_SEP '\\'
#define CNEXT_DEFAULT_EXE "out.exe"
int _mkdir(const char* path);
#else
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define CNEXT_PATH_SEP '/'
#define CNEXT_DEFAULT_EXE "out"
#define cnext_mkdir(path) mkdir(path, 0755)
#endif

#include "ast.h"
#include "codegen.h"
#include "parser.h"
#include "semantics.h"

#define CNEXT_VERSION "6.0"
#define CNEXT_TEMP_C "temp_out.c"
#define CNEXT_PATH_MAX 4096

/* --- File Utilities (main_utils.c) --- */
char* read_file(const char* path);
bool copy_file(const char* source_path, const char* destination_path);
bool has_path_separator(const char* path);
bool dirname_from_path(const char* path, char* buffer, size_t buffer_size);
bool build_include_path(const char* argv0, char* include_path, size_t include_path_size);
int run_process(const char* program, char* const args[]);
const char* executable_command(const char* output_exe, char* buffer, size_t buffer_size);
int make_dir(const char* path);
int join_path(char* out, size_t out_size, const char* base, const char* name);

/* --- Package Manager (main_packages.c) --- */
bool is_standard_module(const char* name);
bool find_registry_path(const char* argv0, char* registry_path, size_t registry_path_size);
bool resolve_package_url(const char* name, const char* registry_path, char* url, size_t url_size);
bool download_package(const char* name, const char* url);
bool install_single_package(const char* name, const char* argv0);
void install_packages_from_toml(const char* argv0);
char* build_source_with_packages(const char* source, const char* project_dir);

/* --- Compilation (main_compiler.c) --- */
int compile_file(const char* input_path, const char* output_c_path, bool test_mode);

#endif /* MAIN_INTERNAL_H */
