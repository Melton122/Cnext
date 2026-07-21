#include "semantics_internal.h"

const char* semantics_source = NULL;
Scope* sem_current_scope = NULL;
bool has_semantic_error = false;
int loop_depth = 0;
int switch_depth = 0;
int generator_depth = 0;

char* copy_cstring(const char* value) {
    if (!value) return NULL;
    size_t length = strlen(value);
    char* copy = (char*)checked_malloc(length + 1);
    memcpy(copy, value, length + 1);
    return copy;
}

char* sem_copy_token_text(Token token) {
    size_t length = token.length > 0 ? (size_t)token.length : 0;
    char* copy = (char*)checked_malloc(length + 1);
    if (length > 0 && token.start) {
        memcpy(copy, token.start, length);
    }
    copy[length] = '\0';
    return copy;
}

static void print_source_line(int line) {
    if (!semantics_source) return;
    const char* p = semantics_source;
    int current_line = 1;
    while (*p && current_line < line) {
        if (*p == '\n') current_line++;
        p++;
    }
    if (*p == '\0') return;
    const char* line_start = p;
    while (*p && *p != '\n') p++;
    size_t line_len = (size_t)(p - line_start);
    if (line_len > 0)
        fprintf(stderr, "  | %.*s\n", (int)line_len, line_start);
}

void report_error(int line, const char* message, const char* detail) {
    if (detail && detail[0] != '\0') {
        fprintf(stderr, "[line %d] Semantic Error: %s %s\n", line, message, detail);
    } else {
        fprintf(stderr, "[line %d] Semantic Error: %s\n", line, message);
    }
    print_source_line(line);
    has_semantic_error = true;
}

void report_token_error(Token token, const char* message) {
    char* detail = sem_copy_token_text(token);
    report_error(token.line, message, detail);
    free(detail);
}

void sem_push_scope(void) {
    Scope* scope = (Scope*)checked_malloc(sizeof(Scope));
    scope->symbols = NULL;
    scope->parent = sem_current_scope;
    sem_current_scope = scope;
}

void sem_pop_scope(void) {
    if (!sem_current_scope) return;
    Scope* scope = sem_current_scope;
    sem_current_scope = scope->parent;

    Symbol* sym = scope->symbols;
    while (sym) {
        Symbol* next = sym->next;
        free(sym->name);
        free(sym->type_name);
        free(sym);
        sym = next;
    }
    free(scope);
}

bool token_matches_name(Token token, const char* name) {
    size_t name_length = strlen(name);
    return name_length == (size_t)token.length &&
           strncmp(name, token.start, name_length) == 0;
}

Symbol* resolve_symbol_in_scope(Scope* scope, Token token) {
    if (!scope) return NULL;
    Symbol* sym = scope->symbols;
    while (sym) {
        if (token_matches_name(token, sym->name)) return sym;
        sym = sym->next;
    }
    return NULL;
}

Symbol* resolve_current_symbol(Token token) {
    return resolve_symbol_in_scope(sem_current_scope, token);
}

Symbol* resolve_symbol(Token token) {
    Scope* scope = sem_current_scope;
    while (scope) {
        Symbol* sym = resolve_symbol_in_scope(scope, token);
        if (sym) return sym;
        scope = scope->parent;
    }
    return NULL;
}

void define_symbol(Token token, CnextTokenType type, bool is_const, const char* type_name, ASTNode* decl_node) {
    if (!sem_current_scope) return;
    Symbol* existing = resolve_current_symbol(token);
    if (existing) {
        report_error(token.line, "Name already declared in this scope:", existing->name);
        return;
    }
    Symbol* sym = (Symbol*)checked_malloc(sizeof(Symbol));
    sym->name = checked_strndup(token.start, token.length);
    sym->type = type;
    sym->is_const = is_const;
    sym->type_name = copy_cstring(type_name);
    sym->decl_node = decl_node;
    if (!sym->name) { free(sym); return; }
    sym->next = sem_current_scope->symbols;
    sem_current_scope->symbols = sym;
}

void define_type_symbol(ASTNode* node, CnextTokenType type) {
    if (!sem_current_scope) return;
    Symbol* sym = (Symbol*)checked_malloc(sizeof(Symbol));
    sym->name = checked_strndup(node->token.start, node->token.length);
    sym->type = type;
    sym->is_const = true;
    sym->type_name = checked_strndup(node->token.start, node->token.length);
    if (!sym->name || !sym->type_name) { free(sym->name); free(sym->type_name); free(sym); return; }
    sym->decl_node = node;
    sym->next = sem_current_scope->symbols;
    sem_current_scope->symbols = sym;
}

void define_symbol_if_missing(Token token, CnextTokenType type, bool is_const, const char* type_name) {
    if (!resolve_current_symbol(token)) {
        define_symbol(token, type, is_const, type_name, NULL);
    }
}

bool is_builtin_value_type(CnextTokenType type) {
    return type == TOKEN_INT_TYPE ||
           type == TOKEN_LONG_TYPE ||
           type == TOKEN_FLOAT_TYPE ||
           type == TOKEN_DOUBLE_TYPE ||
           type == TOKEN_STR_TYPE ||
           type == TOKEN_BOOL_TYPE ||
           type == TOKEN_CHAR_TYPE ||
           type == TOKEN_BYTE_TYPE ||
           type == TOKEN_UINT_TYPE ||
           type == TOKEN_ULONG_TYPE ||
           type == TOKEN_USHORT_TYPE ||
           type == TOKEN_UBYTE_TYPE ||
           type == TOKEN_VAR;
}

bool is_named_type_symbol(CnextTokenType type) {
    return type == TOKEN_STRUCT || type == TOKEN_ENUM || type == TOKEN_CLASS;
}

bool validate_type_token(Token token) {
    if (token.type == TOKEN_EOF || is_builtin_value_type(token.type) ||
        token.type == TOKEN_ITER || token.type == TOKEN_FUNC) return true;
    if (token.type != TOKEN_IDENTIFIER) {
        report_token_error(token, "Invalid type:");
        return false;
    }

    Symbol* type_symbol = resolve_symbol(token);
    if (!type_symbol) {
        report_token_error(token, "Unknown type:");
        return false;
    }
    // Type parameters (generics) and user-defined types are valid type tokens
    if (!is_named_type_symbol(type_symbol->type) && type_symbol->type != TOKEN_IDENTIFIER) {
        report_token_error(token, "Unknown type:");
        return false;
    }
    return true;
}
