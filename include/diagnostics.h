#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdio.h>

// Terminal color support
#ifdef _WIN32
#include <windows.h>
static HANDLE _diag_hConsole = NULL;
static int _diag_color_support = -1; // -1=unknown, 0=no, 1=yes
#endif

static void init_diagnostics(void) {
#ifdef _WIN32
    if (_diag_color_support == -1) {
        _diag_hConsole = GetStdHandle(STD_ERROR_HANDLE);
        DWORD mode = 0;
        _diag_color_support = (GetConsoleMode(_diag_hConsole, &mode)) ? 1 : 0;
    }
#else
    // Assume ANSI support on Unix/macOS
#endif
}

// Color codes for terminal
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"

// Diagnostic message types
typedef enum {
    DIAG_ERROR,
    DIAG_WARNING,
    DIAG_NOTE,
    DIAG_HINT
} DiagLevel;

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

// Suggestion mapping for common errors
typedef struct {
    const char* pattern;
    const char* suggestion;
} ErrorSuggestion;

static const ErrorSuggestion suggestions[] = {
    {"Expect '(' after",       "Did you forget parentheses? Try: func_name(args)"},
    {"Expect ')' after",       "Missing closing parenthesis. Check for unmatched '('."},
    {"Expect ';' after",       "Missing semicolon. Add ';' at end of statement."},
    {"Expect '{' before",      "Missing opening brace. Check block structure."},
    {"Expect '}' after",       "Missing closing brace. Check block structure."},
    {"Undeclared identifier",  "Variable not declared. Use 'var x: type' or 'var x = val'."},
    {"Expect expression",      "Invalid syntax here. Check operator usage."},
    {"Type mismatch",          "Types don't match. Use 'as' for explicit casting."},
    {"Cannot return value",    "Function returns void. Remove return value or add return type."},
    {"Too many arguments",     "Check function signature for correct parameter count."},
    {"Too few arguments",      "Missing required arguments. Check function signature."},
    {NULL, NULL}
};

static const char* find_suggestion(const char* error_msg) {
    for (int i = 0; suggestions[i].pattern; i++) {
        if (strstr(error_msg, suggestions[i].pattern)) {
            return suggestions[i].suggestion;
        }
    }
    return NULL;
}

static void diag_emit(DiagLevel level, int line, const char* message, const char* source_line, const char* hint) {
    init_diagnostics();
    const char* color = diag_color(level);
    const char* label = diag_label(level);

    if (line > 0) {
        fprintf(stderr, "%s%s%s [%d]: %s%s\n", color, COLOR_BOLD, label, line, message, COLOR_RESET);
    } else {
        fprintf(stderr, "%s%s%s: %s%s\n", color, COLOR_BOLD, label, message, COLOR_RESET);
    }

    if (source_line && source_line[0]) {
        fprintf(stderr, "%s  | %s%s\n", COLOR_DIM, source_line, COLOR_RESET);
    }

    const char* suggestion = find_suggestion(message);
    if (suggestion && level == DIAG_ERROR) {
        fprintf(stderr, "%s  --> %s%s\n", COLOR_GREEN, suggestion, COLOR_RESET);
    }

    if (hint) {
        fprintf(stderr, "%s  = %s%s\n", COLOR_DIM, hint, COLOR_RESET);
    }
}

#endif // DIAGNOSTICS_H
