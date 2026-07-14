#include "linter.h"
#include "lexer.h"
#include "checked_alloc.h"
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
        result->issues = checked_realloc(result->issues, sizeof(LintIssue) * result->capacity);
    }
    LintIssue* issue = &result->issues[result->count++];
    issue->line = line;
    issue->column = col;
    issue->message = checked_strdup(msg);
    issue->severity = severity;
    issue->fix = fix ? checked_strdup(fix) : NULL;
}

static char char_buffer[256];

// Track declared identifiers for unused variable/function warnings
#define MAX_DECLS 256
typedef struct {
    char name[128];
    int line;
    bool used;
    bool is_func;
} DeclInfo;

bool lint_source(const char* source, LintResult* result) {
    init_lexer(source);

    Token prev_token = {TOKEN_EOF, "", 0, 0};
    Token prev_prev_token = {TOKEN_EOF, "", 0, 0};
    bool in_func = false;
    bool in_if = false;
    bool in_while = false;
    int paren_depth = 0;
    int brace_depth = 0;
    bool after_return = false;
    bool after_break = false;
    bool after_continue = false;

    // Track declarations for unused warnings
    DeclInfo decls[MAX_DECLS];
    int decl_count = 0;

    for (;;) {
        Token token = next_token();
        if (token.type == TOKEN_EOF) break;
        if (token.type == TOKEN_ERROR) continue;

        // Track brace nesting
        if (token.type == TOKEN_LBRACE) brace_depth++;
        if (token.type == TOKEN_RBRACE) {
            if (brace_depth > 0) brace_depth--;
            after_return = false;
            after_break = false;
            after_continue = false;
        }

        // Track paren nesting
        if (token.type == TOKEN_LPAREN) paren_depth++;
        if (token.type == TOKEN_RPAREN) {
            if (paren_depth > 0) paren_depth--;
            in_if = false;
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

        // Rule: function declaration
        if (token.type == TOKEN_FUNC) {
            in_func = true;
        }

        // Rule: assignment in condition (likely should be ==)
        if (token.type == TOKEN_EQUAL && paren_depth > 0 && prev_token.type != TOKEN_EOF) {
            if (prev_token.type == TOKEN_IDENTIFIER) {
                lint_add(result, token.line, 1, 'W',
                    "Possible assignment in condition; did you mean `==`?", "==");
            }
        }

        // Rule: double semicolons
        if (token.type == TOKEN_SEMICOLON && prev_token.type == TOKEN_SEMICOLON) {
            lint_add(result, token.line, 1, 'W',
                "Extra semicolon", NULL);
        }

        // Rule: unreachable code after return/break/continue
        if (after_return || after_break || after_continue) {
            if (token.type != TOKEN_RBRACE && token.type != TOKEN_SEMICOLON &&
                prev_token.type != TOKEN_SEMICOLON) {
                const char* kind = after_return ? "return" : (after_break ? "break" : "continue");
                snprintf(char_buffer, sizeof(char_buffer),
                    "Unreachable code after `%s`", kind);
                lint_add(result, token.line, 1, 'W', char_buffer, NULL);
                after_return = false;
                after_break = false;
                after_continue = false;
            }
        }

        // Rule: track return/break/continue for unreachable code detection
        if (token.type == TOKEN_RETURN) after_return = true;
        if (token.type == TOKEN_BREAK) after_break = true;
        if (token.type == TOKEN_CONTINUE) after_continue = true;
        if (token.type == TOKEN_SEMICOLON) {
            // Reset after semicolons (end of statement)
        }

        // Rule: empty if/while body
        if (token.type == TOKEN_IF) in_if = true;
        if (token.type == TOKEN_WHILE) in_while = true;

        // Rule: == true / == false
        if (token.type == TOKEN_EQ_EQ && prev_token.type == TOKEN_TRUE) {
            lint_add(result, token.line, 1, 'W',
                "Unnecessary comparison with `true`; use the expression directly", NULL);
        }
        if (token.type == TOKEN_EQ_EQ && prev_token.type == TOKEN_FALSE) {
            lint_add(result, token.line, 1, 'W',
                "Unnecessary comparison with `false`; use `!expression`", NULL);
        }
        if (token.type == TOKEN_BANG_EQ && prev_token.type == TOKEN_TRUE) {
            lint_add(result, token.line, 1, 'W',
                "Unnecessary comparison with `true`; use `!expression`", NULL);
        }
        if (token.type == TOKEN_BANG_EQ && prev_token.type == TOKEN_FALSE) {
            lint_add(result, token.line, 1, 'W',
                "Unnecessary comparison with `false`; use the expression directly", NULL);
        }

        // Rule: single = in if condition (common mistake)
        if (token.type == TOKEN_LPAREN && prev_token.type == TOKEN_IF) {
            // Look ahead for = in condition
        }

        // Rule: TODO/FIXME/HACK comments (by checking for comment-like patterns)
        // This is a simple heuristic

        // Rule: Magic numbers (numbers > 1 that aren't 0 or 1)
        if (token.type == TOKEN_NUMBER && token.length > 1) {
            char num_buf[32];
            int nlen = token.length < 31 ? token.length : 31;
            memcpy(num_buf, token.start, nlen);
            num_buf[nlen] = '\0';
            long val = strtol(num_buf, NULL, 10);
            if (val > 1 && val != 10 && val != 100 && val != 1000) {
                // Could be a magic number - only warn if not in a common pattern
                if (prev_token.type != TOKEN_EQUAL && prev_token.type != TOKEN_PLUS &&
                    prev_token.type != TOKEN_MINUS && prev_token.type != TOKEN_STAR &&
                    prev_token.type != TOKEN_SLASH) {
                    // Don't warn for common cases like array indices
                }
            }
        }

        // Rule: Empty blocks
        if (token.type == TOKEN_LBRACE && prev_token.type != TOKEN_EOF) {
            // Check if next token is } (empty block)
        }

        // Rule: Nested deep (more than 4 levels)
        if (token.type == TOKEN_LBRACE && brace_depth > 4) {
            lint_add(result, token.line, 1, 'I',
                "Deeply nested code (consider refactoring)", NULL);
        }

        // Track variable declarations (var x = ..., type x = ...)
        if ((prev_token.type == TOKEN_VAR || prev_token.type == TOKEN_CONST) &&
            token.type == TOKEN_IDENTIFIER && decl_count < MAX_DECLS) {
            char name[128];
            int nlen = token.length < 127 ? token.length : 127;
            memcpy(name, token.start, nlen);
            name[nlen] = '\0';
            strcpy(decls[decl_count].name, name);
            decls[decl_count].line = token.line;
            decls[decl_count].used = false;
            decls[decl_count].is_func = false;
            decl_count++;
        }

        // Track function declarations
        if (prev_token.type == TOKEN_FUNC && token.type == TOKEN_IDENTIFIER &&
            decl_count < MAX_DECLS) {
            char name[128];
            int nlen = token.length < 127 ? token.length : 127;
            memcpy(name, token.start, nlen);
            name[nlen] = '\0';
            strcpy(decls[decl_count].name, name);
            decls[decl_count].line = token.line;
            decls[decl_count].used = false;
            decls[decl_count].is_func = true;
            decl_count++;
        }

        // Mark identifiers as used when they appear in expressions
        if (token.type == TOKEN_IDENTIFIER) {
            char name[128];
            int nlen = token.length < 127 ? token.length : 127;
            memcpy(name, token.start, nlen);
            name[nlen] = '\0';
            for (int i = 0; i < decl_count; i++) {
                if (strcmp(decls[i].name, name) == 0 && !decls[i].is_func) {
                    decls[i].used = true;
                }
            }
        }

        // Mark functions as used when called
        if (token.type == TOKEN_LPAREN && prev_token.type == TOKEN_IDENTIFIER) {
            char name[128];
            int nlen = prev_token.length < 127 ? prev_token.length : 127;
            memcpy(name, prev_token.start, nlen);
            name[nlen] = '\0';
            for (int i = 0; i < decl_count; i++) {
                if (strcmp(decls[i].name, name) == 0 && decls[i].is_func) {
                    decls[i].used = true;
                }
            }
        }

        prev_prev_token = prev_token;
        prev_token = token;
    }

    // Report unused variables and functions
    for (int i = 0; i < decl_count; i++) {
        if (!decls[i].used && !decls[i].is_func) {
            snprintf(char_buffer, sizeof(char_buffer),
                "Variable '%s' is declared but never used", decls[i].name);
            lint_add(result, decls[i].line, 1, 'W', char_buffer, "Remove or use the variable");
        }
        // Don't warn about unused functions - they may be part of an API
    }

    return true;
}

bool lint_file(const char* file_path, LintResult* result) {
    FILE* f = fopen(file_path, "rb");
    if (!f) {
        fprintf(stderr, "Could not open file \"%s\"\n", file_path);
        return false;
    }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
    long size = ftell(f);
    if (size < 0) { fclose(f); return false; }
    rewind(f);
    char* source = (char*)checked_malloc(size + 1);
    size_t read_size = fread(source, 1, size, f);
    source[read_size] = '\0';
    fclose(f);

    bool ok = lint_source(source, result);
    free(source);
    return ok;
}

void lint_print_issues(const LintResult* result) {
    if (result->count == 0) {
        printf("No issues found.\n");
        return;
    }
    int errors = 0, warnings = 0;
    for (int i = 0; i < result->count; i++) {
        const LintIssue* issue = &result->issues[i];
        const char* color = issue->severity == 'E' ? "\033[31m" : (issue->severity == 'W' ? "\033[33m" : "\033[36m");
        const char* reset = "\033[0m";
        fprintf(stderr, "%s%c%d:%d: %s%s\n", color, issue->severity, issue->line, issue->column, issue->message, reset);
        if (issue->fix) {
            fprintf(stderr, "  fix: %s\n", issue->fix);
        }
        if (issue->severity == 'E') errors++;
        if (issue->severity == 'W') warnings++;
    }
    fprintf(stderr, "\n%d error(s), %d warning(s)\n", errors, warnings);
}
