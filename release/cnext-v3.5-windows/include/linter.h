#ifndef LINTER_H
#define LINTER_H

#include <stdbool.h>

typedef struct {
    int line;
    int column;
    char* message;
    char severity; // 'E', 'W', 'I'
    char* fix;     // optional auto-fix suggestion
} LintIssue;

typedef struct {
    LintIssue* issues;
    int count;
    int capacity;
} LintResult;

void lint_init(LintResult* result);
void lint_free(LintResult* result);
bool lint_file(const char* file_path, LintResult* result);
bool lint_source(const char* source, LintResult* result);
void lint_print_issues(const LintResult* result);

#endif // LINTER_H
