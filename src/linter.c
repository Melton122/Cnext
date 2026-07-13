#include "linter.h"
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void lint_init(LintResult* result) {
    result->issues = NULL;
    result->count = 0;
    result->capacity = 0;
}

void lint_free(LintResult* result) {
    for (int i = 0; i < result->count; i++) {
        free(result->issues[i].message);
        if (result->issues[i].fix) free(result->issues[i].fix);
    }
    free(result->issues);
    result->issues = NULL;
    result->count = 0;
    result->capacity = 0;
}

static void lint_add(LintResult* result, int line, int col, char severity, const char* msg, const char* fix) {
    if (result->count >= result->capacity) {
        result->capacity = result->capacity ? result->capacity * 2 : 16;
        result->issues = realloc(result->issues, sizeof(LintIssue) * result->capacity);
    }
    LintIssue* issue = &result->issues[result->count++];
    issue->line = line;
    issue->column = col;
    issue->message = strdup(msg);
    issue->severity = severity;
    issue->fix = fix ? strdup(fix) : NULL;
}

static char char_buffer[256];

bool lint_source(const char* source, LintResult* result) {
    init_lexer(source);

    Token prev_token = {TOKEN_EOF, "", 0, 0};
    bool in_func = false;
    int paren_depth = 0;

    for (;;) {
        Token token = next_token();
        if (token.type == TOKEN_EOF) break;
        if (token.type == TOKEN_ERROR) continue;

        // Track brace nesting (simplified)
        if (token.type == TOKEN_LPAREN) paren_depth++;

        // Rule: empty paren expressions (function calls with no args are OK)
        if (token.type == TOKEN_RPAREN) {
            if (paren_depth > 0) paren_depth--;
        }

        // Rule: comparison with true/false literals
        if ((token.type == TOKEN_TRUE || token.type == TOKEN_FALSE) && prev_token.type != TOKEN_EOF) {
            bool is_cmp = false;
            if (prev_token.type == TOKEN_EQ_EQ || prev_token.type == TOKEN_BANG_EQ ||
                prev_token.type == TOKEN_AND_AND || prev_token.type == TOKEN_OR_OR ||
                prev_token.type == TOKEN_LPAREN || prev_token.type == TOKEN_LBRACE) {
                // Not a comparison
            } else {
                is_cmp = true;
            }
            if (is_cmp) {
                snprintf(char_buffer, sizeof(char_buffer),
                    "Comparison with literal `%.*s`; remove comparison",
                    token.length, token.start);
                lint_add(result, token.line, 1, 'W', char_buffer, NULL);
            }
        }

        // Rule: function declaration should start with func keyword
        if (token.type == TOKEN_FUNC) {
            in_func = true;
        }

        // Rule: unused parameter naming convention (parameters should start with underscore)
        if (in_func && paren_depth > 0 && token.type == TOKEN_IDENTIFIER) {
            char c = token.start[0];
            if (c >= 'a' && c <= 'z' && token.length > 1 && token.start[1] == ':') {
                // Single letter param - just a style suggestion
            }
        }

        // Rule: assignment in condition (likely should be ==)
        if (token.type == TOKEN_EQUAL && paren_depth > 0 && prev_token.type != TOKEN_EOF) {
            if (prev_token.type == TOKEN_IDENTIFIER) {
                // Check if this is inside an if condition (simplified: just after LPAREN)
                lint_add(result, token.line, 1, 'W',
                    "Possible assignment in condition; did you mean `==`?", "==");
            }
        }

        // Rule: double semicolons (handled by checking if we get ; after ;)
        if (token.type == TOKEN_SEMICOLON && prev_token.type == TOKEN_SEMICOLON) {
            lint_add(result, token.line, 1, 'W',
                "Extra semicolon", NULL);
        }

        prev_token = token;
    }
    return true;
}

bool lint_file(const char* file_path, LintResult* result) {
    FILE* f = fopen(file_path, "rb");
    if (!f) {
        fprintf(stderr, "Could not open file \"%s\"\n", file_path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char* source = (char*)malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);

    bool ok = lint_source(source, result);
    free(source);
    return ok;
}

void lint_print_issues(const LintResult* result) {
    for (int i = 0; i < result->count; i++) {
        const LintIssue* issue = &result->issues[i];
        char sev = issue->severity == 'E' ? 'E' : (issue->severity == 'W' ? 'W' : 'I');
        fprintf(stderr, "%c%d: %s\n", sev, issue->line, issue->message);
        if (issue->fix) {
            fprintf(stderr, "  fix: %s\n", issue->fix);
        }
    }
}
