#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdio.h>
#include <stdbool.h>

// Terminal color support
#ifdef _WIN32
#include <windows.h>
#endif

// Error codes
typedef enum {
    ERR_NONE = 0,
    ERR_EXPECT_EXPR,        // E001
    ERR_EXPECT_TOKEN,       // E002
    ERR_UNDECLARED,         // E003
    ERR_TYPE_MISMATCH,      // E004
    ERR_UNEXPECTED_TOKEN,   // E005
    ERR_MISSING_BRACE,      // E006
    ERR_MISSING_PAREN,      // E007
    ERR_MISSING_SEMICOLON,  // E008
    ERR_DUPLICATE_DECL,     // E009
    ERR_INVALID_ASSIGNMENT, // E010
    ERR_TOO_MANY_ARGS,      // E011
    ERR_TOO_FEW_ARGS,       // E012
    ERR_RETURN_TYPE,        // E013
    ERR_BREAK_OUTSIDE,      // E014
    ERR_CONTINUE_OUTSIDE,   // E015
    ERR_DIVISION_ZERO,      // E016
    ERR_OVERFLOW,           // E017
    ERR_MISSING_METHOD,     // E018
    // Semantic analysis errors (E100+)
    ERR_SEM_UNDECLARED,     // E100
    ERR_SEM_TYPE_MISMATCH,  // E101
    ERR_SEM_CONST_MUTATE,   // E102
    ERR_SEM_DUPLICATE_DECL, // E103
    ERR_SEM_INVALID_ASSIGN, // E104
    ERR_SEM_BREAK_OUTSIDE,  // E105
    ERR_SEM_CONTINUE_OUTSIDE, // E106
    ERR_SEM_YIELD_OUTSIDE,  // E107
    ERR_SEM_NO_MAIN,        // E108
    ERR_SEM_CANT_EXTEND,    // E109
    ERR_SEM_MISSING_INIT,   // E110
    ERR_SEM_FIELD_INIT,     // E111
    ERR_SEM_INVALID_POSTFIX, // E112
    ERR_SEM_UNKNOWN_TYPE,   // E113
    ERR_SEM_CANT_CALL,      // E114
    ERR_SEM_ABSTRACT_INST,  // E115
    ERR_SEM_UNKNOWN_CLASS,  // E116
    ERR_SEM_MISSING_METHOD, // E117
    ERR_SEM_GENERIC,     // E118 - generic semantic error
    ERR_MAX
} ErrorCode;

// Color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"
#define COLOR_MAGENTA "\033[35m"

// Diagnostic levels
typedef enum {
    DIAG_ERROR,
    DIAG_WARNING,
    DIAG_NOTE,
    DIAG_HINT
} DiagLevel;

// Error info structure
typedef struct {
    ErrorCode code;
    DiagLevel level;
    int line;
    int column;
    const char* message;
    const char* hint;
    const char* note;
} Diagnostic;

// Global error counter
extern int _diag_error_count;
extern int _diag_warning_count;

int diag_get_error_count(void);
int diag_get_warning_count(void);
void diag_reset_counts(void);

const char* error_code_string(ErrorCode code);

// Main diagnostic emission function
void diag_emit(DiagLevel level, ErrorCode code, int line, int col,
               const char* message, const char* hint, const char* note);

// Convenience functions
void diag_error(ErrorCode code, int line, const char* message, const char* hint);
void diag_warning(ErrorCode code, int line, const char* message, const char* hint);
void diag_note(int line, const char* message);
void diag_hint_msg(const char* message);

// Source context display (used by parser for error highlighting)
void print_source_context(const char* source, int line);
void print_caret_at(const char* source, int line, int col, int length);

// Suggestion mapping for common errors
typedef struct {
    ErrorCode code;
    const char* hint;
} ErrorHint;

const char* get_hint_for_error(ErrorCode code);

#endif // DIAGNOSTICS_H
