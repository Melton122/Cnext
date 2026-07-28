#include "diagnostics.h"

#ifdef _WIN32
static HANDLE _diag_hConsole = NULL;
static int _diag_color_support = -1;
#endif

int _diag_error_count = 0;
int _diag_warning_count = 0;

void init_diagnostics(void) {
#ifdef _WIN32
    if (_diag_color_support == -1) {
        _diag_hConsole = GetStdHandle(STD_ERROR_HANDLE);
        DWORD mode = 0;
        _diag_color_support = (GetConsoleMode(_diag_hConsole, &mode)) ? 1 : 0;
    }
#endif
}

int diag_get_error_count(void) { return _diag_error_count; }
int diag_get_warning_count(void) { return _diag_warning_count; }
void diag_reset_counts(void) { _diag_error_count = 0; _diag_warning_count = 0; }

const char* error_code_string(ErrorCode code) {
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

void print_source_context(const char* source, int line) {
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

void print_caret_at(const char* source, int line, int col, int length) {
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

void diag_emit(DiagLevel level, ErrorCode code, int line, int col,
               const char* message, const char* hint, const char* note)
{
    init_diagnostics();
    if (level == DIAG_ERROR) _diag_error_count++;
    if (level == DIAG_WARNING) _diag_warning_count++;
    const char* color = diag_color(level);
    const char* label = diag_label(level);
    const char* code_str = error_code_string(code);
    if (line > 0 && col > 0) {
        fprintf(stderr, "%s%s%s %s[%s]%s %s[%d:%d]%s: %s\n",
            color, COLOR_BOLD, label, COLOR_MAGENTA, code_str, COLOR_RESET,
            COLOR_DIM, line, col, COLOR_RESET, message);
    } else if (line > 0) {
        fprintf(stderr, "%s%s%s %s[%s]%s %s[%d]%s: %s\n",
            color, COLOR_BOLD, label, COLOR_MAGENTA, code_str, COLOR_RESET,
            COLOR_DIM, line, COLOR_RESET, message);
    } else {
        fprintf(stderr, "%s%s%s %s[%s]%s: %s\n",
            color, COLOR_BOLD, label, COLOR_MAGENTA, code_str, COLOR_RESET, message);
    }
    if (hint) {
        fprintf(stderr, "%s  = hint: %s%s\n", COLOR_GREEN, hint, COLOR_RESET);
    }
    if (note) {
        fprintf(stderr, "%s  = note: %s%s\n", COLOR_CYAN, note, COLOR_RESET);
    }
}

void diag_error(ErrorCode code, int line, const char* message, const char* hint) {
    diag_emit(DIAG_ERROR, code, line, 0, message, hint, NULL);
}

void diag_warning(ErrorCode code, int line, const char* message, const char* hint) {
    diag_emit(DIAG_WARNING, code, line, 0, message, hint, NULL);
}

void diag_note(int line, const char* message) {
    diag_emit(DIAG_NOTE, ERR_NONE, line, 0, message, NULL, NULL);
}

void diag_hint_msg(const char* message) {
    diag_emit(DIAG_HINT, ERR_NONE, 0, 0, message, NULL, NULL);
}

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
    {ERR_MISSING_METHOD,   "Implement the missing method in the class body."},
    {ERR_SEM_UNDECLARED,   "Declare the variable first: var name = value"},
    {ERR_SEM_TYPE_MISMATCH,"Types don't match. Use 'as' for explicit casting."},
    {ERR_SEM_CONST_MUTATE, "Cannot modify a const variable."},
    {ERR_SEM_DUPLICATE_DECL,"This name is already declared in this scope."},
    {ERR_SEM_INVALID_ASSIGN,"Cannot assign to this expression."},
    {ERR_SEM_BREAK_OUTSIDE,"'break' can only be used inside loops or switch."},
    {ERR_SEM_CONTINUE_OUTSIDE,"'continue' can only be used inside loops."},
    {ERR_SEM_YIELD_OUTSIDE,"'yield' can only be used inside a generator function."},
    {ERR_SEM_NO_MAIN,      "Add a 'main' block to your program."},
    {ERR_SEM_CANT_EXTEND,  "Cannot extend a final class."},
    {ERR_SEM_MISSING_INIT, "Variables declared with 'var' must have an initializer."},
    {ERR_SEM_FIELD_INIT,   "Class fields cannot have initializers."},
    {ERR_SEM_INVALID_POSTFIX,"Invalid target for ++ or -- operator."},
    {ERR_SEM_UNKNOWN_TYPE,  "Check spelling or declare this type."},
    {ERR_SEM_CANT_CALL,     "Only functions can be called."},
    {ERR_SEM_ABSTRACT_INST, "Cannot instantiate an abstract class."},
    {ERR_SEM_UNKNOWN_CLASS, "Check spelling or import the module."},
    {ERR_SEM_MISSING_METHOD, "Implement the missing method in the class body."},
    {ERR_NONE, NULL}
};

const char* get_hint_for_error(ErrorCode code) {
    for (int i = 0; error_hints[i].hint; i++) {
        if (error_hints[i].code == code) return error_hints[i].hint;
    }
    return NULL;
}
