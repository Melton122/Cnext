#include "diagnostics.h"

#ifdef _WIN32
#include <windows.h>
#endif

#define DIAG_MAX_CONTEXT_COLUMN 240

static int _diag_color_support = -1;
#ifdef _WIN32
static HANDLE _diag_hConsole = NULL;
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
#else
    _diag_color_support = 1;
#endif
}

int diag_get_error_count(void) { return _diag_error_count; }
int diag_get_warning_count(void) { return _diag_warning_count; }
void diag_reset_counts(void) { _diag_error_count = 0; _diag_warning_count = 0; }

static const DiagnosticInfo diagnostic_info[] = {
    {ERR_NONE, "----", "CN-0000", NULL},
    {ERR_EXPECT_EXPR, "E001", "CN-E001", "Check operator usage or variable names."},
    {ERR_EXPECT_TOKEN, "E002", "CN-E002", "Add the missing token shown above."},
    {ERR_UNDECLARED, "E003", "CN-E003", "Declare the variable first: var name = value"},
    {ERR_TYPE_MISMATCH, "E004", "CN-E004", "Types don't match. Use 'as' for explicit casting."},
    {ERR_UNEXPECTED_TOKEN, "E005", "CN-E005", "Unexpected token. Check syntax near this location."},
    {ERR_MISSING_BRACE, "E006", "CN-E006", "Check block structure for unmatched braces."},
    {ERR_MISSING_PAREN, "E007", "CN-E007", "Check for unmatched parentheses."},
    {ERR_MISSING_SEMICOLON, "E008", "CN-E008", "Add a semicolon at the end of the statement."},
    {ERR_DUPLICATE_DECL, "E009", "CN-E009", "This name is already declared in this scope."},
    {ERR_INVALID_ASSIGNMENT, "E010", "CN-E010", "Cannot assign to this expression."},
    {ERR_TOO_MANY_ARGS, "E011", "CN-E011", "Check function signature for correct parameter count."},
    {ERR_TOO_FEW_ARGS, "E012", "CN-E012", "Missing required arguments. Check function signature."},
    {ERR_RETURN_TYPE, "E013", "CN-E013", "Function returns void. Remove return value or add return type."},
    {ERR_BREAK_OUTSIDE, "E014", "CN-E014", "'break' can only be used inside loops or switch."},
    {ERR_CONTINUE_OUTSIDE, "E015", "CN-E015", "'continue' can only be used inside loops."},
    {ERR_DIVISION_ZERO, "E016", "CN-E016", "Division by zero is undefined."},
    {ERR_OVERFLOW, "E017", "CN-E017", "Numeric literal is too large for this type."},
    {ERR_MISSING_METHOD, "E018", "CN-E018", "Implement the missing method in the class body."},
    {ERR_SEM_UNDECLARED, "E100", "CN-E100", "Declare the variable first: var name = value"},
    {ERR_SEM_TYPE_MISMATCH, "E101", "CN-E101", "Types don't match. Use 'as' for explicit casting."},
    {ERR_SEM_CONST_MUTATE, "E102", "CN-E102", "Cannot modify a const variable."},
    {ERR_SEM_DUPLICATE_DECL, "E103", "CN-E103", "This name is already declared in this scope."},
    {ERR_SEM_INVALID_ASSIGN, "E104", "CN-E104", "Cannot assign to this expression."},
    {ERR_SEM_BREAK_OUTSIDE, "E105", "CN-E105", "'break' can only be used inside loops or switch."},
    {ERR_SEM_CONTINUE_OUTSIDE, "E106", "CN-E106", "'continue' can only be used inside loops."},
    {ERR_SEM_YIELD_OUTSIDE, "E107", "CN-E107", "'yield' can only be used inside a generator function."},
    {ERR_SEM_NO_MAIN, "E108", "CN-E108", "Add a 'main' block to your program."},
    {ERR_SEM_CANT_EXTEND, "E109", "CN-E109", "Cannot extend a final class."},
    {ERR_SEM_MISSING_INIT, "E110", "CN-E110", "Variables declared with 'var' must have an initializer."},
    {ERR_SEM_FIELD_INIT, "E111", "CN-E111", "Class fields cannot have initializers."},
    {ERR_SEM_INVALID_POSTFIX, "E112", "CN-E112", "Invalid target for ++ or -- operator."},
    {ERR_SEM_UNKNOWN_TYPE, "E113", "CN-E113", "Check spelling or declare this type."},
    {ERR_SEM_CANT_CALL, "E114", "CN-E114", "Only functions can be called."},
    {ERR_SEM_ABSTRACT_INST, "E115", "CN-E115", "Cannot instantiate an abstract class."},
    {ERR_SEM_UNKNOWN_CLASS, "E116", "CN-E116", "Check spelling or import the module."},
    {ERR_SEM_MISSING_METHOD, "E117", "CN-E117", "Implement the missing method in the class body."},
    {ERR_SEM_GENERIC, "E118", "CN-E118", "Check the declaration or expression at this location."},
    {ERR_SEM_RETURN_TYPE, "E119", "CN-E119", "Return an expression of the declared type, or change the function's return type."},
    {ERR_SEM_DEFER_CONTROL, "E120", "CN-E120", "Move the control transfer outside the deferred body. It cannot change the enclosing function's flow."},
    {ERR_CODEGEN_IO, "E200", "CN-E200", "Check file permissions and available disk space."},
    {ERR_CODEGEN_UNHANDLED, "E201", "CN-E201", "This AST node is not implemented by the selected backend."},
    {ERR_BACKEND_FAILED, "E202", "CN-E202", "Check the backend output above and adjust the generated program."},
    {ERR_MAX, "E999", "CN-E999", NULL}
};

const DiagnosticInfo* get_diagnostic_info(ErrorCode code) {
    if (code < 0 || code >= ERR_MAX) return &diagnostic_info[ERR_MAX];
    return &diagnostic_info[code];
}

const char* error_code_string(ErrorCode code) {
    return get_diagnostic_info(code)->id;
}

const char* diag_rule_string(ErrorCode code) {
    return get_diagnostic_info(code)->rule;
}

const char* diag_help_string(ErrorCode code) {
    return get_diagnostic_info(code)->help;
}

static const char* diag_color(DiagLevel level) {
    if (!_diag_color_support) return "";
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
    size_t shown = line_len < DIAG_MAX_CONTEXT_COLUMN ? line_len : DIAG_MAX_CONTEXT_COLUMN;
    if (shown > 0) {
        fprintf(stderr, "%s  | %.*s%s%s\n", COLOR_DIM, (int)shown, line_start,
                line_len > shown ? "..." : "", COLOR_RESET);
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
    const char* line_start = p;
    while (*p && *p != '\n') p++;
    size_t line_len = (size_t)(p - line_start);
    size_t max_col = line_len + 1;
    if (max_col > DIAG_MAX_CONTEXT_COLUMN) max_col = DIAG_MAX_CONTEXT_COLUMN;
    if (col < 1) col = 1;
    if ((size_t)col > max_col) col = (int)max_col;
    if (length < 1) length = 1;
    fprintf(stderr, "%s  | ", COLOR_DIM);
    for (int i = 1; i < col; i++) fprintf(stderr, " ");
    fprintf(stderr, "%s^", COLOR_GREEN);
    for (int i = 1; i < length && i < 40; i++) fprintf(stderr, "~");
    fprintf(stderr, "%s\n", COLOR_RESET);
}

void diag_emit_context(DiagLevel level, ErrorCode code, int line, int col,
                       int length, const char* source, const char* message,
                       const char* expected, const char* found,
                       const char* hint, const char* note)
{
    init_diagnostics();
    if (level == DIAG_ERROR) _diag_error_count++;
    if (level == DIAG_WARNING) _diag_warning_count++;

    const char* color = diag_color(level);
    const char* label = diag_label(level);
    const char* code_str = error_code_string(code);
    int out_col = col > 0 ? col : 1;
    int out_line = line > 0 ? line : 0;

    fprintf(stderr, "%s%s%s %s[%s]%s %s[%d:%d]%s: %s\n",
        color, COLOR_BOLD, label, COLOR_MAGENTA, code_str, COLOR_RESET,
        COLOR_DIM, out_line, out_col, COLOR_RESET, message);

    if (source && out_line > 0) {
        print_source_context(source, out_line);
        print_caret_at(source, out_line, out_col, length);
    }

    const char* help = hint ? hint : diag_help_string(code);
    if (expected) {
        fprintf(stderr, "%s  = expected: %s%s\n", COLOR_CYAN, expected, COLOR_RESET);
    }
    if (found) {
        fprintf(stderr, "%s  = found: %s%s\n", COLOR_CYAN, found, COLOR_RESET);
    }
    if (help && level != DIAG_NOTE && level != DIAG_HINT) {
        fprintf(stderr, "%s  = help: %s%s\n", COLOR_GREEN, help, COLOR_RESET);
    }
    if (note) {
        fprintf(stderr, "%s  = note: %s%s\n", COLOR_CYAN, note, COLOR_RESET);
    }
    if (code != ERR_NONE && level != DIAG_NOTE && level != DIAG_HINT) {
        fprintf(stderr, "%s  = rule: %s%s\n", COLOR_DIM, diag_rule_string(code), COLOR_RESET);
    }
}

void diag_emit(DiagLevel level, ErrorCode code, int line, int col,
               const char* message, const char* hint, const char* note)
{
    diag_emit_context(level, code, line, col, 0, NULL, message,
                      NULL, NULL, hint, note);
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

const char* get_hint_for_error(ErrorCode code) {
    return diag_help_string(code);
}
