#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdio.h>
#include <stdbool.h>

// Terminal color support
#ifdef _WIN32
#include <windows.h>
static HANDLE _diag_hConsole = NULL;
static int _diag_color_support = -1;
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
    ERR_MAX
} ErrorCode;

static void init_diagnostics(void) {
#ifdef _WIN32
    if (_diag_color_support == -1) {
        _diag_hConsole = GetStdHandle(STD_ERROR_HANDLE);
        DWORD mode = 0;
        _diag_color_support = (GetConsoleMode(_diag_hConsole, &mode)) ? 1 : 0;
    }
#endif
}

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
static int _diag_error_count = 0;
static int _diag_warning_count = 0;

static inline int diag_get_error_count(void) { return _diag_error_count; }
static inline int diag_get_warning_count(void) { return _diag_warning_count; }
static inline void diag_reset_counts(void) { _diag_error_count = 0; _diag_warning_count = 0; }

static const char* error_code_string(ErrorCode code) {
    static char buf[8];
    if (code == ERR_NONE) return "----";
    snprintf(buf, sizeof(buf), "E%03d", (int)code);
    return buf;
}

static const char* diag_color(DiagLevel level) {
    switch (level) {
        case DIAG_ERROR:   return COLOR_RED;
        case DIAG_WARNING: return COLOR_YELLOW;
        case DIAG_NOTE:    return COLOR_CYAN;
        case DIAG_HINT:    return COLOR_DIM;
        default:           return COLOR_RESET;
    }
}

static const char* diag_label(DiagLevel level) {
    switch (level) {
        case DIAG_ERROR:   return "error";
        case DIAG_WARNING: return "warning";
        case DIAG_NOTE:    return "note";
        case DIAG_HINT:    return "hint";
        default:           return "";
    }
}

// Source context for error display
static void print_source_context(const char* source, int line) {
    if (!source || line < 1) return;
    const char* p = source;
    int current_line = 1;
    while (*p && current_line < line) {
        if (*p == '\n') current_line++;
        p++;
    }
    if (!*p) return;
    const char* line_start = p;
    while (*p && *p != '\n') p++;
    size_t line_len = (size_t)(p - line_start);
    if (line_len > 0) {
        fprintf(stderr, "%s  | %.*s%s\n", COLOR_DIM, (int)line_len, line_start, COLOR_RESET);
    }
}

static void print_caret_at(const char* source, int line, int col, int length) {
    if (!source || line < 1) return;
    const char* p = source;
    int current_line = 1;
    while (*p && current_line < line) {
        if (*p == '\n') current_line++;
        p++;
    }
    if (!*p) return;
    fprintf(stderr, "%s  | ", COLOR_DIM);
    for (int i = 1; i < col; i++) fprintf(stderr, " ");
    fprintf(stderr, "%s^", COLOR_GREEN);
    for (int i = 1; i < length && i < 40; i++) fprintf(stderr, "~");
    fprintf(stderr, "%s\n", COLOR_RESET);
}

// Main diagnostic emission function
static void diag_emit(DiagLevel level, ErrorCode code, int line, int col,
                      const char* message, const char* hint, const char* note) {
    init_diagnostics();

    if (level == DIAG_ERROR) _diag_error_count++;
    if (level == DIAG_WARNING) _diag_warning_count++;

    const char* color = diag_color(level);
    const char* label = diag_label(level);
    const char* code_str = error_code_string(code);

    // Header line: error [E001] [line]: message
    if (line > 0) {
        fprintf(stderr, "%s%s%s %s[%s]%s %s[%d]%s: %s\n",
            color, COLOR_BOLD, label, COLOR_MAGENTA, code_str, COLOR_RESET,
            COLOR_DIM, line, COLOR_RESET, message);
    } else {
        fprintf(stderr, "%s%s%s %s[%s]%s: %s\n",
            color, COLOR_BOLD, label, COLOR_MAGENTA, code_str, COLOR_RESET, message);
    }

    // Hint line
    if (hint) {
        fprintf(stderr, "%s  = hint: %s%s\n", COLOR_GREEN, hint, COLOR_RESET);
    }

    // Note line
    if (note) {
        fprintf(stderr, "%s  = note: %s%s\n", COLOR_CYAN, note, COLOR_RESET);
    }
}

// Convenience functions
static inline void diag_error(ErrorCode code, int line, const char* message, const char* hint) {
    diag_emit(DIAG_ERROR, code, line, 0, message, hint, NULL);
}

static inline void diag_warning(ErrorCode code, int line, const char* message, const char* hint) {
    diag_emit(DIAG_WARNING, code, line, 0, message, hint, NULL);
}

static inline void diag_note(int line, const char* message) {
    diag_emit(DIAG_NOTE, ERR_NONE, line, 0, message, NULL, NULL);
}

static inline void diag_hint_msg(const char* message) {
    diag_emit(DIAG_HINT, ERR_NONE, 0, 0, message, NULL, NULL);
}

// Suggestion mapping for common errors
typedef struct {
    ErrorCode code;
    const char* hint;
} ErrorHint;

static const ErrorHint error_hints[] = {
    {ERR_EXPECT_EXPR,      "Check operator usage or variable names."},
    {ERR_EXPECT_TOKEN,     "Add the missing token shown above."},
    {ERR_UNDECLARED,       "Declare the variable first: var name = value"},
    {ERR_TYPE_MISMATCH,    "Types don't match. Use 'as' for explicit casting."},
    {ERR_UNEXPECTED_TOKEN, "Unexpected token. Check syntax near this location."},
    {ERR_MISSING_BRACE,    "Check block structure for unmatched braces."},
    {ERR_MISSING_PAREN,    "Check for unmatched parentheses."},
    {ERR_MISSING_SEMICOLON,"Add a semicolon at the end of the statement."},
    {ERR_DUPLICATE_DECL,   "This name is already declared in this scope."},
    {ERR_INVALID_ASSIGNMENT,"Cannot assign to this expression."},
    {ERR_TOO_MANY_ARGS,    "Check function signature for correct parameter count."},
    {ERR_TOO_FEW_ARGS,     "Missing required arguments. Check function signature."},
    {ERR_RETURN_TYPE,      "Function returns void. Remove return value or add return type."},
    {ERR_BREAK_OUTSIDE,    "'break' can only be used inside loops or switch."},
    {ERR_CONTINUE_OUTSIDE, "'continue' can only be used inside loops."},
    {ERR_DIVISION_ZERO,    "Division by zero is undefined."},
    {ERR_OVERFLOW,         "Numeric literal is too large for this type."},
    {ERR_NONE, NULL}
};

static const char* get_hint_for_error(ErrorCode code) {
    for (int i = 0; error_hints[i].hint; i++) {
        if (error_hints[i].code == code) return error_hints[i].hint;
    }
    return NULL;
}

#endif // DIAGNOSTICS_H
