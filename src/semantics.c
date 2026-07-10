#include "semantics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Symbol {
    char* name;
    char* type_name;
    TokenType type;
    bool is_const;
    ASTNode* decl_node;
    struct Symbol* next;
} Symbol;

typedef struct Scope {
    Symbol* symbols;
    struct Scope* parent;
} Scope;

static const char* semantics_source = NULL;
static Scope* current_scope = NULL;
static bool has_semantic_error = false;
static int loop_depth = 0;
static int switch_depth = 0;

static TokenType analyze_expression(ASTNode* node);
static void analyze_node(ASTNode* node);

static void* checked_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Out of memory.\n");
        exit(70);
    }
    return ptr;
}

static char* copy_cstring(const char* value) {
    if (!value) return NULL;
    size_t length = strlen(value);
    char* copy = (char*)checked_malloc(length + 1);
    memcpy(copy, value, length + 1);
    return copy;
}

static char* copy_token_text(Token token) {
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

static void print_caret_for_token(int line, const char* start, int length) {
    if (!semantics_source || !start) return;
    int col = 1;
    const char* p = semantics_source;
    int current_line = 1;
    while (*p && current_line < line) {
        if (*p == '\n') current_line++;
        p++;
    }
    while (*p && *p != '\n' && p < start) {
        col++;
        p++;
    }
    fprintf(stderr, "  | ");
    for (int i = 1; i < col; i++) fprintf(stderr, " ");
    fprintf(stderr, "^");
    for (int i = 1; i < length && i < 40; i++) fprintf(stderr, "~");
    fprintf(stderr, "\n");
}

static void report_error(int line, const char* message, const char* detail) {
    if (detail && detail[0] != '\0') {
        fprintf(stderr, "[line %d] Semantic Error: %s %s\n", line, message, detail);
    } else {
        fprintf(stderr, "[line %d] Semantic Error: %s\n", line, message);
    }
    print_source_line(line);
    has_semantic_error = true;
}

static void report_token_error(Token token, const char* message) {
    char* detail = copy_token_text(token);
    report_error(token.line, message, detail);
    free(detail);
}

static void push_scope(void) {
    Scope* scope = (Scope*)checked_malloc(sizeof(Scope));
    scope->symbols = NULL;
    scope->parent = current_scope;
    current_scope = scope;
}

static void pop_scope(void) {
    if (!current_scope) return;
    Scope* scope = current_scope;
    current_scope = scope->parent;

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

static bool token_matches_name(Token token, const char* name) {
    size_t name_length = strlen(name);
    return name_length == (size_t)token.length &&
           strncmp(name, token.start, name_length) == 0;
}

static Symbol* resolve_symbol_in_scope(Scope* scope, Token token) {
    if (!scope) return NULL;
    Symbol* sym = scope->symbols;
    while (sym) {
        if (token_matches_name(token, sym->name)) return sym;
        sym = sym->next;
    }
    return NULL;
}

static Symbol* resolve_current_symbol(Token token) {
    return resolve_symbol_in_scope(current_scope, token);
}

static Symbol* resolve_symbol(Token token) {
    Scope* scope = current_scope;
    while (scope) {
        Symbol* sym = resolve_symbol_in_scope(scope, token);
        if (sym) return sym;
        scope = scope->parent;
    }
    return NULL;
}

static void define_symbol(Token token, TokenType type, bool is_const, const char* type_name, ASTNode* decl_node) {
    if (!current_scope) return;
    Symbol* existing = resolve_current_symbol(token);
    if (existing) {
        report_error(token.line, "Name already declared in this scope:", existing->name);
        return;
    }
    Symbol* sym = (Symbol*)checked_malloc(sizeof(Symbol));
    sym->name = strndup(token.start, token.length);
    sym->type = type;
    sym->is_const = is_const;
    sym->type_name = copy_cstring(type_name);
    sym->decl_node = decl_node;
    sym->next = current_scope->symbols;
    current_scope->symbols = sym;
}

static void define_type_symbol(ASTNode* node, TokenType type) {
    if (!current_scope) return;
    Symbol* sym = (Symbol*)checked_malloc(sizeof(Symbol));
    sym->name = strndup(node->token.start, node->token.length);
    sym->type = type;
    sym->is_const = true;
    sym->type_name = strndup(node->token.start, node->token.length);
    sym->decl_node = node;
    sym->next = current_scope->symbols;
    current_scope->symbols = sym;
}

static void define_symbol_if_missing(Token token, TokenType type, bool is_const, const char* type_name) {
    if (!resolve_current_symbol(token)) {
        define_symbol(token, type, is_const, type_name, NULL);
    }
}

static bool is_builtin_value_type(TokenType type) {
    return type == TOKEN_INT_TYPE ||
           type == TOKEN_FLOAT_TYPE ||
           type == TOKEN_STR_TYPE ||
           type == TOKEN_BOOL_TYPE ||
           type == TOKEN_CHAR_TYPE ||
           type == TOKEN_VAR;
}

static bool is_named_type_symbol(TokenType type) {
    return type == TOKEN_STRUCT || type == TOKEN_ENUM || type == TOKEN_CLASS;
}

static bool validate_type_token(Token token) {
    if (token.type == TOKEN_EOF || is_builtin_value_type(token.type)) return true;
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

static char* type_name_from_token(Token token) {
    if (token.type == TOKEN_STR_TYPE) return copy_cstring("str");
    if (token.type == TOKEN_INT_TYPE) return copy_cstring("int");
    if (token.type == TOKEN_FLOAT_TYPE) return copy_cstring("float");
    if (token.type == TOKEN_BOOL_TYPE) return copy_cstring("bool");
    if (token.type == TOKEN_CHAR_TYPE) return copy_cstring("char");
    if (token.type != TOKEN_IDENTIFIER) return NULL;
    return copy_token_text(token);
}

static bool assignment_types_compatible(Token expected, TokenType actual) {
    if (expected.type == TOKEN_VAR || actual == TOKEN_EOF) return true;
    if (expected.type == actual) return true;
    if (expected.type == TOKEN_FLOAT_TYPE && actual == TOKEN_INT_TYPE) return true;

    if (actual == TOKEN_NULL) {
        if (expected.type == TOKEN_STR_TYPE) return true;
        if (expected.type == TOKEN_IDENTIFIER) {
            Symbol* type_symbol = resolve_symbol(expected);
            return type_symbol && (type_symbol->type == TOKEN_CLASS || type_symbol->type == TOKEN_STRUCT || type_symbol->type == TOKEN_INTERFACE);
        }
        return false;
    }

    if (expected.type == TOKEN_IDENTIFIER) {
        Symbol* type_symbol = resolve_symbol(expected);
        return type_symbol && type_symbol->type == TOKEN_ENUM && actual == TOKEN_INT_TYPE;
    }

    return false;
}

static void validate_var_initializer(ASTNode* node) {
    if (node->var_type.type == TOKEN_VAR && !node->init) {
        report_error(node->token.line, "Variables declared with 'var' must have an initializer:", NULL);
        return;
    }

    if (!node->init) return;

    TokenType init_type = analyze_expression(node->init);
    if (node->init->type == AST_NULL_LITERAL) {
        node->init->expr_type = node->var_type.type;
        node->init->type_name = type_name_from_token(node->var_type);
    }
    if (!assignment_types_compatible(node->var_type, init_type)) {
        report_token_error(node->token, "Initializer type does not match variable type:");
    }
}

static void analyze_var_declaration(ASTNode* node) {
    validate_type_token(node->var_type);
    validate_var_initializer(node);

    // Infer type for 'var' declarations from the initializer
    if (node->var_type.type == TOKEN_VAR && node->init) {
        TokenType inferred = node->init->expr_type;
        if (inferred != TOKEN_EOF && inferred != TOKEN_VAR) {
            node->var_type.type = inferred;
        }
    }

    if (node->var_type.type == TOKEN_IDENTIFIER) {
        Symbol* type_sym = resolve_symbol(node->var_type);
        if (type_sym && type_sym->type == TOKEN_CLASS &&
            node->init && node->init->type == AST_NEW_EXPR) {
            node->is_class = true;
        }
    }

    char* type_name = type_name_from_token(node->var_type);
    define_symbol(node->token, node->var_type.type, node->is_const, type_name, node);
    free(type_name);
}

static void analyze_field_declaration(ASTNode* node) {
    validate_type_token(node->var_type);
    if (node->init) {
        report_token_error(node->token, "Fields cannot have initializers:");
        analyze_expression(node->init);
    }
}

static void register_import(Token module) {
    define_symbol_if_missing(module, TOKEN_IDENTIFIER, true, NULL);

    if (token_matches_name(module, "io")) {
        Token read_file_token = {TOKEN_IDENTIFIER, "read_file", 9, module.line};
        Token write_file_token = {TOKEN_IDENTIFIER, "write_file", 10, module.line};
        Token append_file_token = {TOKEN_IDENTIFIER, "append_file", 11, module.line};
        define_symbol_if_missing(read_file_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(write_file_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(append_file_token, TOKEN_FUNC, true, NULL);
    } else if (token_matches_name(module, "file")) {
        Token copy_file_token = {TOKEN_IDENTIFIER, "copy_file", 9, module.line};
        Token delete_file_token = {TOKEN_IDENTIFIER, "delete_file", 11, module.line};
        Token file_exists_token = {TOKEN_IDENTIFIER, "file_exists", 11, module.line};
        Token file_size_token = {TOKEN_IDENTIFIER, "file_size", 9, module.line};
        define_symbol_if_missing(copy_file_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(delete_file_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(file_exists_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(file_size_token, TOKEN_FUNC, true, NULL);
    } else if (token_matches_name(module, "net")) {
        Token http_get_token = {TOKEN_IDENTIFIER, "http_get", 8, module.line};
        Token http_post_token = {TOKEN_IDENTIFIER, "http_post", 9, module.line};
        Token download_file_token = {TOKEN_IDENTIFIER, "download_file", 13, module.line};
        Token tcp_connect_token = {TOKEN_IDENTIFIER, "tcp_connect", 11, module.line};
        Token tcp_send_token = {TOKEN_IDENTIFIER, "tcp_send", 8, module.line};
        Token tcp_receive_token = {TOKEN_IDENTIFIER, "tcp_receive", 11, module.line};
        Token tcp_close_token = {TOKEN_IDENTIFIER, "tcp_close", 9, module.line};
        define_symbol_if_missing(http_get_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(http_post_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(download_file_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(tcp_connect_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(tcp_send_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(tcp_receive_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(tcp_close_token, TOKEN_FUNC, true, NULL);
    } else if (token_matches_name(module, "json")) {
        Token json_parse_token = {TOKEN_IDENTIFIER, "json_parse", 10, module.line};
        Token json_get_string_token = {TOKEN_IDENTIFIER, "json_get_string", 15, module.line};
        Token json_get_int_token = {TOKEN_IDENTIFIER, "json_get_int", 12, module.line};
        Token json_get_float_token = {TOKEN_IDENTIFIER, "json_get_float", 14, module.line};
        Token json_get_bool_token = {TOKEN_IDENTIFIER, "json_get_bool", 13, module.line};
        Token json_stringify_token = {TOKEN_IDENTIFIER, "json_stringify", 14, module.line};
        Token json_free_token = {TOKEN_IDENTIFIER, "json_free", 9, module.line};
        define_symbol_if_missing(json_parse_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(json_get_string_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(json_get_int_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(json_get_float_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(json_get_bool_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(json_stringify_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(json_free_token, TOKEN_FUNC, true, NULL);
    } else if (token_matches_name(module, "os")) {
        Token os_getenv_token = {TOKEN_IDENTIFIER, "os_getenv", 9, module.line};
        Token os_setenv_token = {TOKEN_IDENTIFIER, "os_setenv", 9, module.line};
        Token os_unsetenv_token = {TOKEN_IDENTIFIER, "os_unsetenv", 11, module.line};
        Token os_exec_token = {TOKEN_IDENTIFIER, "os_exec", 7, module.line};
        Token os_get_cwd_token = {TOKEN_IDENTIFIER, "os_get_cwd", 10, module.line};
        Token os_set_cwd_token = {TOKEN_IDENTIFIER, "os_set_cwd", 10, module.line};
        Token os_temp_dir_token = {TOKEN_IDENTIFIER, "os_temp_dir", 11, module.line};
        Token os_home_dir_token = {TOKEN_IDENTIFIER, "os_home_dir", 11, module.line};
        Token os_mkdir_token = {TOKEN_IDENTIFIER, "os_mkdir", 8, module.line};
        Token os_rmdir_token = {TOKEN_IDENTIFIER, "os_rmdir", 8, module.line};
        Token os_os_name_token = {TOKEN_IDENTIFIER, "os_os_name", 10, module.line};
        Token os_hostname_token = {TOKEN_IDENTIFIER, "os_hostname", 11, module.line};
        Token os_pid_token = {TOKEN_IDENTIFIER, "os_pid", 6, module.line};
        define_symbol_if_missing(os_getenv_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_setenv_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_unsetenv_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_exec_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_get_cwd_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_set_cwd_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_temp_dir_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_home_dir_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_mkdir_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_rmdir_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_os_name_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_hostname_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_pid_token, TOKEN_FUNC, true, NULL);
    } else if (token_matches_name(module, "string_utils")) {
        Token str_trim_token = {TOKEN_IDENTIFIER, "str_trim", 8, module.line};
        Token str_trim_left_token = {TOKEN_IDENTIFIER, "str_trim_left", 13, module.line};
        Token str_trim_right_token = {TOKEN_IDENTIFIER, "str_trim_right", 14, module.line};
        Token str_to_upper_token = {TOKEN_IDENTIFIER, "str_to_upper", 12, module.line};
        Token str_to_lower_token = {TOKEN_IDENTIFIER, "str_to_lower", 12, module.line};
        Token str_starts_with_token = {TOKEN_IDENTIFIER, "str_starts_with", 15, module.line};
        Token str_ends_with_token = {TOKEN_IDENTIFIER, "str_ends_with", 13, module.line};
        Token str_contains_token = {TOKEN_IDENTIFIER, "str_contains", 12, module.line};
        Token str_index_of_token = {TOKEN_IDENTIFIER, "str_index_of", 12, module.line};
        Token str_replace_token = {TOKEN_IDENTIFIER, "str_replace", 11, module.line};
        Token str_substring_token = {TOKEN_IDENTIFIER, "str_substring", 13, module.line};
        Token str_repeat_token = {TOKEN_IDENTIFIER, "str_repeat", 10, module.line};
        Token str_reverse_token = {TOKEN_IDENTIFIER, "str_reverse", 11, module.line};
        define_symbol_if_missing(str_trim_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(str_trim_left_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(str_trim_right_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(str_to_upper_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(str_to_lower_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(str_starts_with_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(str_ends_with_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(str_contains_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(str_index_of_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(str_replace_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(str_substring_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(str_repeat_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(str_reverse_token, TOKEN_FUNC, true, NULL);
    } else if (token_matches_name(module, "time")) {
        Token time_now_token = {TOKEN_IDENTIFIER, "time_now", 8, module.line};
        Token time_now_ms_token = {TOKEN_IDENTIFIER, "time_now_ms", 11, module.line};
        Token time_sleep_token = {TOKEN_IDENTIFIER, "time_sleep", 10, module.line};
        Token time_format_token = {TOKEN_IDENTIFIER, "time_format", 11, module.line};
        Token time_parse_token = {TOKEN_IDENTIFIER, "time_parse", 10, module.line};
        Token time_year_token = {TOKEN_IDENTIFIER, "time_year", 9, module.line};
        Token time_month_token = {TOKEN_IDENTIFIER, "time_month", 10, module.line};
        Token time_day_token = {TOKEN_IDENTIFIER, "time_day", 8, module.line};
        Token time_hour_token = {TOKEN_IDENTIFIER, "time_hour", 9, module.line};
        Token time_minute_token = {TOKEN_IDENTIFIER, "time_minute", 11, module.line};
        Token time_second_token = {TOKEN_IDENTIFIER, "time_second", 11, module.line};
        define_symbol_if_missing(time_now_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(time_now_ms_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(time_sleep_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(time_format_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(time_parse_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(time_year_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(time_month_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(time_day_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(time_hour_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(time_minute_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(time_second_token, TOKEN_FUNC, true, NULL);
    } else if (token_matches_name(module, "regex")) {
        Token regex_match_token = {TOKEN_IDENTIFIER, "regex_match", 11, module.line};
        Token regex_search_token = {TOKEN_IDENTIFIER, "regex_search", 12, module.line};
        Token regex_replace_token = {TOKEN_IDENTIFIER, "regex_replace", 14, module.line};
        define_symbol_if_missing(regex_match_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(regex_search_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(regex_replace_token, TOKEN_FUNC, true, NULL);
    } else if (token_matches_name(module, "collections")) {
        Token list_new_token = {TOKEN_IDENTIFIER, "list_new", 8, module.line};
        Token list_free_token = {TOKEN_IDENTIFIER, "list_free", 9, module.line};
        Token list_add_token = {TOKEN_IDENTIFIER, "list_add", 8, module.line};
        Token list_get_token = {TOKEN_IDENTIFIER, "list_get", 8, module.line};
        Token list_set_token = {TOKEN_IDENTIFIER, "list_set", 8, module.line};
        Token list_size_token = {TOKEN_IDENTIFIER, "list_size", 9, module.line};
        Token list_remove_token = {TOKEN_IDENTIFIER, "list_remove", 11, module.line};
        Token list_clear_token = {TOKEN_IDENTIFIER, "list_clear", 10, module.line};
        Token list_contains_token = {TOKEN_IDENTIFIER, "list_contains", 13, module.line};
        Token list_sort_token = {TOKEN_IDENTIFIER, "list_sort", 9, module.line};
        Token map_new_token = {TOKEN_IDENTIFIER, "map_new", 7, module.line};
        Token map_free_token = {TOKEN_IDENTIFIER, "map_free", 8, module.line};
        Token map_set_token = {TOKEN_IDENTIFIER, "map_set", 7, module.line};
        Token map_get_token = {TOKEN_IDENTIFIER, "map_get", 7, module.line};
        Token map_has_token = {TOKEN_IDENTIFIER, "map_has", 7, module.line};
        Token map_remove_token = {TOKEN_IDENTIFIER, "map_remove", 10, module.line};
        Token map_size_token = {TOKEN_IDENTIFIER, "map_size", 8, module.line};
        Token set_new_token = {TOKEN_IDENTIFIER, "set_new", 7, module.line};
        Token set_free_token = {TOKEN_IDENTIFIER, "set_free", 8, module.line};
        Token set_add_token = {TOKEN_IDENTIFIER, "set_add", 7, module.line};
        Token set_has_token = {TOKEN_IDENTIFIER, "set_has", 7, module.line};
        Token set_remove_token = {TOKEN_IDENTIFIER, "set_remove", 10, module.line};
        Token set_size_token = {TOKEN_IDENTIFIER, "set_size", 8, module.line};
        define_symbol_if_missing(list_new_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(list_free_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(list_add_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(list_get_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(list_set_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(list_size_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(list_remove_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(list_clear_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(list_contains_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(list_sort_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(map_new_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(map_free_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(map_set_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(map_get_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(map_has_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(map_remove_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(map_size_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(set_new_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(set_free_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(set_add_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(set_has_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(set_remove_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(set_size_token, TOKEN_FUNC, true, NULL);
    } else if (token_matches_name(module, "crypto")) {
        Token crypto_md5_token = {TOKEN_IDENTIFIER, "crypto_md5", 10, module.line};
        Token crypto_sha256_token = {TOKEN_IDENTIFIER, "crypto_sha256", 13, module.line};
        Token crypto_base64_encode_token = {TOKEN_IDENTIFIER, "crypto_base64_encode", 20, module.line};
        Token crypto_base64_decode_token = {TOKEN_IDENTIFIER, "crypto_base64_decode", 20, module.line};
        define_symbol_if_missing(crypto_md5_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(crypto_sha256_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(crypto_base64_encode_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(crypto_base64_decode_token, TOKEN_FUNC, true, NULL);
    } else if (token_matches_name(module, "path")) {
        Token path_join_token = {TOKEN_IDENTIFIER, "path_join", 9, module.line};
        Token path_dirname_token = {TOKEN_IDENTIFIER, "path_dirname", 12, module.line};
        Token path_basename_token = {TOKEN_IDENTIFIER, "path_basename", 13, module.line};
        Token path_extension_token = {TOKEN_IDENTIFIER, "path_extension", 14, module.line};
        Token path_normalize_token = {TOKEN_IDENTIFIER, "path_normalize", 14, module.line};
        define_symbol_if_missing(path_join_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(path_dirname_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(path_basename_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(path_extension_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(path_normalize_token, TOKEN_FUNC, true, NULL);
    } else if (token_matches_name(module, "encoding")) {
        Token base64_encode_token = {TOKEN_IDENTIFIER, "base64_encode", 13, module.line};
        Token base64_decode_token = {TOKEN_IDENTIFIER, "base64_decode", 13, module.line};
        Token hex_encode_token = {TOKEN_IDENTIFIER, "hex_encode", 10, module.line};
        Token hex_decode_token = {TOKEN_IDENTIFIER, "hex_decode", 10, module.line};
        Token url_encode_token = {TOKEN_IDENTIFIER, "url_encode", 10, module.line};
        Token url_decode_token = {TOKEN_IDENTIFIER, "url_decode", 10, module.line};
        define_symbol_if_missing(base64_encode_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(base64_decode_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(hex_encode_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(hex_decode_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(url_encode_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(url_decode_token, TOKEN_FUNC, true, NULL);
    } else if (token_matches_name(module, "process")) {
        Token process_run_token = {TOKEN_IDENTIFIER, "process_run", 11, module.line};
        Token process_run_status_token = {TOKEN_IDENTIFIER, "process_run_status", 19, module.line};
        Token process_kill_token = {TOKEN_IDENTIFIER, "process_kill", 12, module.line};
        Token process_exists_token = {TOKEN_IDENTIFIER, "process_exists", 14, module.line};
        define_symbol_if_missing(process_run_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(process_run_status_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(process_kill_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(process_exists_token, TOKEN_FUNC, true, NULL);
    } else if (token_matches_name(module, "random")) {
        Token random_seed_token = {TOKEN_IDENTIFIER, "random_seed", 11, module.line};
        Token random_int_token = {TOKEN_IDENTIFIER, "random_int", 10, module.line};
        Token random_float_token = {TOKEN_IDENTIFIER, "random_float", 12, module.line};
        Token random_uniform_token = {TOKEN_IDENTIFIER, "random_uniform", 14, module.line};
        Token random_gaussian_token = {TOKEN_IDENTIFIER, "random_gaussian", 15, module.line};
        define_symbol_if_missing(random_seed_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(random_int_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(random_float_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(random_uniform_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(random_gaussian_token, TOKEN_FUNC, true, NULL);
    }
}

static void predeclare_global(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case AST_IMPORT:
            register_import(node->token);
            break;
        case AST_FUNC_DECL:
            define_symbol(node->token, TOKEN_FUNC, true, NULL, node);
            break;
        case AST_STRUCT_DECL:
            define_type_symbol(node, TOKEN_STRUCT);
            break;
        case AST_CLASS_DECL: {
            define_type_symbol(node, TOKEN_CLASS);
            // Inherit from parent class (extends)
            if (node->parent_name) {
                Token parent_tok = {TOKEN_IDENTIFIER, node->parent_name, (int)strlen(node->parent_name), node->token.line};
                Symbol* parent_sym = resolve_symbol(parent_tok);
                if (parent_sym && parent_sym->decl_node) {
                    ASTNode* parent_node = parent_sym->decl_node;
                    int to_add_count = 0;
                    for (int i = 0; i < parent_node->child_count; i++) {
                        ASTNode* pchild = parent_node->children[i];
                        if (pchild->type == AST_VAR_DECL) {
                            to_add_count++;
                        } else if (pchild->type == AST_FUNC_DECL) {
                            bool overridden = false;
                            for (int j = 0; j < node->child_count; j++) {
                                if (node->children[j]->type == AST_FUNC_DECL &&
                                    node->children[j]->token.length == pchild->token.length &&
                                    strncmp(node->children[j]->token.start, pchild->token.start, pchild->token.length) == 0) {
                                    overridden = true;
                                    break;
                                }
                            }
                            if (!overridden) to_add_count++;
                        }
                    }
                    if (to_add_count > 0) {
                        int old_count = node->child_count;
                        int new_count = old_count + to_add_count;
                        ASTNode** new_children = (ASTNode**)malloc(sizeof(ASTNode*) * new_count);
                        int idx = 0;
                        for (int i = 0; i < parent_node->child_count; i++) {
                            if (parent_node->children[i]->type == AST_VAR_DECL) new_children[idx++] = parent_node->children[i];
                        }
                        for (int i = 0; i < old_count; i++) {
                            if (node->children[i]->type == AST_VAR_DECL) new_children[idx++] = node->children[i];
                        }
                        for (int i = 0; i < parent_node->child_count; i++) {
                            if (parent_node->children[i]->type == AST_FUNC_DECL) {
                                bool overridden = false;
                                for (int j = 0; j < old_count; j++) {
                                    if (node->children[j]->type == AST_FUNC_DECL &&
                                        node->children[j]->token.length == parent_node->children[i]->token.length &&
                                        strncmp(node->children[j]->token.start, parent_node->children[i]->token.start, parent_node->children[i]->token.length) == 0) {
                                        overridden = true;
                                        break;
                                    }
                                }
                                if (!overridden) new_children[idx++] = parent_node->children[i];
                            }
                        }
                        for (int i = 0; i < old_count; i++) {
                            if (node->children[i]->type != AST_VAR_DECL) new_children[idx++] = node->children[i];
                        }
                        free(node->children);
                        node->children = new_children;
                        node->child_count = new_count;
                        node->child_capacity = new_count;
                    }
                }
            }
            // Inherit default methods from implemented traits
            if (node->implements_names) {
                char names_copy[1024];
                strncpy(names_copy, node->implements_names, sizeof(names_copy) - 1);
                names_copy[sizeof(names_copy) - 1] = '\0';
                char* name = names_copy;
                while (name && *name) {
                    char* comma = strchr(name, ',');
                    if (comma) *comma = '\0';
                    while (*name == ' ' || *name == '\t') name++;
                    if (*name) {
                        Token trait_tok = {TOKEN_IDENTIFIER, name, (int)strlen(name), node->token.line};
                        Symbol* trait_sym = resolve_symbol(trait_tok);
                        if (trait_sym && trait_sym->decl_node && trait_sym->decl_node->type == AST_TRAIT_DECL) {
                            ASTNode* trait_node = trait_sym->decl_node;
                            int to_add = 0;
                            for (int i = 0; i < trait_node->child_count; i++) {
                                ASTNode* tchild = trait_node->children[i];
                                if (tchild->type == AST_FUNC_DECL && tchild->left) {
                                    bool overridden = false;
                                    for (int j = 0; j < node->child_count; j++) {
                                        if (node->children[j]->type == AST_FUNC_DECL &&
                                            node->children[j]->token.length == tchild->token.length &&
                                            strncmp(node->children[j]->token.start, tchild->token.start, tchild->token.length) == 0) {
                                            overridden = true;
                                            break;
                                        }
                                    }
                                    if (!overridden) to_add++;
                                }
                            }
                            if (to_add > 0) {
                                int old_count = node->child_count;
                                int new_count = old_count + to_add;
                                ASTNode** new_children = (ASTNode**)malloc(sizeof(ASTNode*) * new_count);
                                int idx = 0;
                                for (int i = 0; i < old_count; i++) {
                                    new_children[idx++] = node->children[i];
                                }
                                for (int i = 0; i < trait_node->child_count; i++) {
                                    ASTNode* tchild = trait_node->children[i];
                                    if (tchild->type == AST_FUNC_DECL && tchild->left) {
                                        bool overridden = false;
                                        for (int j = 0; j < old_count; j++) {
                                            if (node->children[j]->type == AST_FUNC_DECL &&
                                                node->children[j]->token.length == tchild->token.length &&
                                                strncmp(node->children[j]->token.start, tchild->token.start, tchild->token.length) == 0) {
                                                overridden = true;
                                                break;
                                            }
                                        }
                                        if (!overridden) {
                                            new_children[idx++] = tchild;
                                        }
                                    }
                                }
                                free(node->children);
                                node->children = new_children;
                                node->child_count = new_count;
                                node->child_capacity = new_count;
                            }
                        }
                    }
                    if (comma) name = comma + 1;
                    else break;
                }
            }
            break;
        }
        case AST_INTERFACE_DECL:
        case AST_TRAIT_DECL:
            define_type_symbol(node, TOKEN_CLASS);
            break;
        case AST_ENUM_DECL:
            define_type_symbol(node, TOKEN_ENUM);
            for (int i = 0; i < node->child_count; i++) {
                define_symbol(node->children[i]->token, TOKEN_INT_TYPE, true, NULL, node->children[i]);
            }
            break;
        default:
            break;
    }
}

static void analyze_function(ASTNode* node, bool define_name) {
    if (define_name && !resolve_current_symbol(node->token)) {
        define_symbol(node->token, TOKEN_FUNC, true, NULL, node);
    }

    push_scope();
    // Define generic type parameters in scope as valid types BEFORE any type validation
    for (int i = 0; i < node->type_param_count; i++) {
        ASTNode* tp = node->type_params[i];
        Symbol* sym = (Symbol*)checked_malloc(sizeof(Symbol));
        sym->name = strndup(tp->token.start, tp->token.length);
        sym->type = TOKEN_IDENTIFIER; // Opaque type
        sym->is_const = true;
        sym->type_name = strndup(tp->token.start, tp->token.length);
        sym->decl_node = tp;
        sym->next = current_scope->symbols;
        current_scope->symbols = sym;
    }

    validate_type_token(node->return_type);

    for (int i = 0; i < node->child_count; i++) {
        ASTNode* param = node->children[i];
        validate_type_token(param->var_type);
        char* type_name = type_name_from_token(param->var_type);
        define_symbol(param->token, param->var_type.type, false, type_name, param);
        free(type_name);
        
        // Analyze default value if present
        if (param->default_value) {
            analyze_expression(param->default_value);
        }
    }
    analyze_node(node->left);
    pop_scope();
}

static void analyze_assignment(ASTNode* node) {
    TokenType left_type = analyze_expression(node->left);
    analyze_expression(node->right);

    if (node->right && node->right->type == AST_NULL_LITERAL) {
        node->right->expr_type = left_type;
        if (node->left->type_name) {
            node->right->type_name = copy_cstring(node->left->type_name);
        } else if (node->left->type == AST_IDENTIFIER) {
            Symbol* sym = resolve_symbol(node->left->token);
            if (sym && sym->type_name) {
                node->right->type_name = copy_cstring(sym->type_name);
            }
        }
    }

    if (!node->left) {
        report_error(node->token.line, "Invalid assignment target.", NULL);
        return;
    }

    if (node->left->type == AST_IDENTIFIER) {
        Symbol* sym = resolve_symbol(node->left->token);
        if (!sym) {
            // Already reported by analyze_expression
        } else if (sym->is_const) {
            report_error(node->left->token.line, "Cannot reassign to const variable:", sym->name);
        }
        return;
    }

    if (node->left->type == AST_MEMBER_ACCESS || node->left->type == AST_INDEX) {
        return;
    }

    report_error(node->token.line, "Invalid assignment target.", NULL);
}

static void analyze_postfix(ASTNode* node) {
    if (!node->left) {
        report_error(node->token.line, "Invalid postfix target.", NULL);
        return;
    }

    if (node->left->type == AST_IDENTIFIER) {
        Symbol* sym = resolve_symbol(node->left->token);
        if (!sym) {
            report_token_error(node->left->token, "Undeclared variable:");
        } else if (sym->is_const) {
            report_error(node->left->token.line, "Cannot mutate const variable:", sym->name);
        }
        return;
    }

    if (node->left->type == AST_MEMBER_ACCESS || node->left->type == AST_INDEX) {
        analyze_expression(node->left);
        return;
    }

    analyze_expression(node->left);
    report_error(node->token.line, "Invalid postfix target.", NULL);
}

static void analyze_node(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
        case AST_BLOCK:
            push_scope();
            for (int i = 0; i < node->child_count; i++) analyze_node(node->children[i]);
            pop_scope();
            break;
        case AST_MAIN:
            analyze_node(node->left);
            break;
        case AST_VAR_DECL:
            analyze_var_declaration(node);
            break;
        case AST_FUNC_DECL:
            analyze_function(node, true);
            break;
        case AST_STRUCT_DECL:
            for (int i = 0; i < node->child_count; i++) {
                analyze_field_declaration(node->children[i]);
            }
            break;
        case AST_CLASS_DECL:
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_FUNC_DECL) {
                    analyze_function(node->children[i], false);
                } else {
                    analyze_field_declaration(node->children[i]);
                }
            }
            break;
        case AST_ENUM_DECL:
            break;
        case AST_IMPORT:
            register_import(node->token);
            break;
        case AST_ASSIGN:
            analyze_assignment(node);
            break;
        case AST_EXPR_STMT:
            analyze_expression(node->left);
            break;
        case AST_IF:
            analyze_expression(node->condition);
            analyze_node(node->left);
            if (node->right) analyze_node(node->right);
            break;
        case AST_WHILE:
            loop_depth++;
            analyze_expression(node->condition);
            analyze_node(node->left);
            loop_depth--;
            break;
        case AST_FOR:
            loop_depth++;
            push_scope();
            analyze_node(node->init);
            analyze_expression(node->condition);
            analyze_node(node->increment);
            analyze_node(node->left);
            pop_scope();
            loop_depth--;
            break;
        case AST_FOR_IN:
            loop_depth++;
            push_scope();
            analyze_expression(node->condition);
            define_symbol(node->init->token, TOKEN_VAR, false, NULL, node->init);
            analyze_node(node->left);
            pop_scope();
            loop_depth--;
            break;
        case AST_SWITCH:
            analyze_expression(node->condition);
            switch_depth++;
            for (int i = 0; i < node->child_count; i++) {
                analyze_node(node->children[i]);
            }
            switch_depth--;
            break;
        case AST_CASE:
            analyze_expression(node->condition);
            analyze_node(node->left);
            break;
        case AST_DEFAULT:
            analyze_node(node->left);
            break;
        case AST_BREAK:
            if (loop_depth == 0 && switch_depth == 0) {
                report_error(node->token.line, "Cannot use 'break' outside of a loop or switch.", NULL);
            }
            break;
        case AST_CONTINUE:
            if (loop_depth == 0) {
                report_error(node->token.line, "Cannot use 'continue' outside of a loop.", NULL);
            }
            break;
        case AST_INDEX:
            analyze_expression(node->left);
            analyze_expression(node->right);
            break;
        case AST_SLICE:
            analyze_expression(node->left);
            if (node->init) analyze_expression(node->init);
            if (node->right) analyze_expression(node->right);
            break;
        case AST_POSTFIX:
            analyze_postfix(node);
            break;
        case AST_RETURN:
            if (node->left) analyze_expression(node->left);
            break;
        case AST_TRY:
            analyze_node(node->left); 
            if (node->right) { 
                push_scope();
                if (node->right->token.start && node->right->token.length > 0) {
                    define_symbol(node->right->token, node->right->var_type.type, false, NULL, node->right);
                }
                analyze_node(node->right->left); 
                pop_scope();
            }
            if (node->condition) { 
                analyze_node(node->condition);
            }
            break;
        case AST_THROW:
            if (node->left) analyze_expression(node->left);
            break;
        case AST_MATCH:
            analyze_expression(node->condition);
            for (int i = 0; i < node->child_count; i++) {
                ASTNode* arm = node->children[i];
                for (int j = 0; j < arm->child_count; j++) {
                    analyze_expression(arm->children[j]);
                }
                if (arm->left) analyze_node(arm->left);
            }
            break;
        case AST_INTERFACE_DECL:
        case AST_TRAIT_DECL:
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_FUNC_DECL) {
                    validate_type_token(node->children[i]->return_type);
                }
            }
            if (node->type == AST_TRAIT_DECL) {
                // Traits can have default implementations - analyze their bodies
                for (int i = 0; i < node->child_count; i++) {
                    if (node->children[i]->type == AST_FUNC_DECL && node->children[i]->left) {
                        analyze_function(node->children[i], true);
                    }
                }
            }
            break;
        case AST_TEST:
            analyze_node(node->left);
            break;
        case AST_ASSERT:
            analyze_expression(node->left);
            break;
        case AST_CONSTRUCTOR:
            push_scope();
            for (int i = 0; i < node->child_count; i++) {
                ASTNode* param = node->children[i];
                validate_type_token(param->var_type);
                char* tn = type_name_from_token(param->var_type);
                define_symbol(param->token, param->var_type.type, false, tn, param);
                free(tn);
            }
            analyze_node(node->left);
            pop_scope();
            break;
        case AST_OPERATOR_DECL: {
            // Analyze operator declaration
            // Register as a function
            define_symbol(node->token, TOKEN_FUNC, true, NULL, node);
            push_scope();
            for (int i = 0; i < node->child_count; i++) {
                ASTNode* param = node->children[i];
                validate_type_token(param->var_type);
                char* type_name = type_name_from_token(param->var_type);
                define_symbol(param->token, param->var_type.type, false, type_name, param);
                free(type_name);
            }
            analyze_node(node->left);
            pop_scope();
            break;
        }
        case AST_EXTEND_DECL: {
            // Analyze extension methods
            // For now, allow extending any type (built-in or user-defined)
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_FUNC_DECL) {
                    analyze_function(node->children[i], false);
                }
            }
            break;
        }
        case AST_DESTRUCTURE: {
            // Analyze destructuring
            analyze_expression(node->right);
            for (int i = 0; i < node->child_count; i++) {
                ASTNode* var = node->children[i];
                validate_type_token(var->var_type);
                char* type_name = type_name_from_token(var->var_type);
                define_symbol(var->token, var->var_type.type, false, type_name, var);
                free(type_name);
            }
            break;
        }
        case AST_TUPLE:
            // Analyze tuple elements
            for (int i = 0; i < node->child_count; i++) {
                analyze_expression(node->children[i]);
            }
            break;
        case AST_OPTION_SOME:
        case AST_OPTION_NONE:
        case AST_RESULT_OK:
        case AST_RESULT_ERR:
            if (node->left) analyze_expression(node->left);
            break;
        case AST_NAMED_ARG:
            analyze_expression(node->right);
            break;
        default:
            break;
    }
}

static TokenType analyze_expression(ASTNode* node) {
    if (!node) return TOKEN_EOF;

    TokenType type = TOKEN_EOF;
    switch (node->type) {
        case AST_LITERAL:
            if (node->token.type == TOKEN_NUMBER) type = TOKEN_INT_TYPE;
            else if (node->token.type == TOKEN_FLOAT_LITERAL) type = TOKEN_FLOAT_TYPE;
            else if (node->token.type == TOKEN_STRING_LITERAL) type = TOKEN_STR_TYPE;
            else if (node->token.type == TOKEN_CHAR_LITERAL) type = TOKEN_CHAR_TYPE;
            else if (node->token.type == TOKEN_TRUE || node->token.type == TOKEN_FALSE) type = TOKEN_BOOL_TYPE;
            break;
        case AST_LITERAL_ARRAY:
            for (int i = 0; i < node->child_count; i++) {
                analyze_expression(node->children[i]);
            }
            break;
        case AST_IDENTIFIER: {
            Symbol* sym = resolve_symbol(node->token);
            if (!sym) {
                report_token_error(node->token, "Undeclared identifier:");
            } else {
                if (sym->type_name) {
                    free(node->type_name);
                    node->type_name = copy_cstring(sym->type_name);
                }
                type = sym->type;
            }
            break;
        }
        case AST_TYPE_PARAM: {
            Symbol* sym = resolve_symbol(node->token);
            if (!sym) {
                report_token_error(node->token, "Undeclared type parameter:");
            } else {
                type = TOKEN_IDENTIFIER;
            }
            break;
        }
        case AST_BINARY: {
            TokenType left_type = analyze_expression(node->left);
            TokenType right_type = analyze_expression(node->right);
            // Propagate type to null literal in binary comparisons
            if (node->right && node->right->type == AST_NULL_LITERAL && left_type == TOKEN_STR_TYPE) {
                node->right->expr_type = TOKEN_STR_TYPE;
                right_type = TOKEN_STR_TYPE;
            } else if (node->left && node->left->type == AST_NULL_LITERAL && right_type == TOKEN_STR_TYPE) {
                node->left->expr_type = TOKEN_STR_TYPE;
                left_type = TOKEN_STR_TYPE;
            }
            if (node->token.type == TOKEN_EQ_EQ || node->token.type == TOKEN_BANG_EQ ||
                node->token.type == TOKEN_LESS || node->token.type == TOKEN_LESS_EQ ||
                node->token.type == TOKEN_GREATER || node->token.type == TOKEN_GREATER_EQ ||
                node->token.type == TOKEN_AND_AND || node->token.type == TOKEN_OR_OR) {
                type = TOKEN_BOOL_TYPE;
            } else if (node->token.type == TOKEN_PLUS &&
                (left_type == TOKEN_STR_TYPE || right_type == TOKEN_STR_TYPE)) {
                type = TOKEN_STR_TYPE;
            } else if (left_type == TOKEN_FLOAT_TYPE || right_type == TOKEN_FLOAT_TYPE) {
                type = TOKEN_FLOAT_TYPE;
            } else if (left_type == TOKEN_INT_TYPE && right_type == TOKEN_INT_TYPE) {
                type = TOKEN_INT_TYPE;
            }
            break;
        }
        case AST_UNARY:
            type = analyze_expression(node->right);
            break;
        case AST_POSTFIX:
            analyze_postfix(node);
            break;
        case AST_CALL: {
            TokenType callee_type = analyze_expression(node->left);
            if (node->left && node->left->type == AST_IDENTIFIER && callee_type != TOKEN_FUNC) {
                report_token_error(node->left->token, "Cannot call non-function:");
            }
            for (int i = 0; i < node->child_count; i++) {
                analyze_expression(node->children[i]);
            }
            // Propagate str type to null literals passed as function arguments
            if (node->left && node->left->type == AST_IDENTIFIER) {
                Symbol* func_sym = resolve_symbol(node->left->token);
                if (func_sym && func_sym->decl_node && func_sym->decl_node->type == AST_FUNC_DECL) {
                    ASTNode* func_decl = func_sym->decl_node;
                    int param_count = func_decl->child_count < node->child_count ? func_decl->child_count : node->child_count;
                    for (int i = 0; i < param_count; i++) {
                        if (node->children[i]->type == AST_NULL_LITERAL &&
                            func_decl->children[i]->var_type.type == TOKEN_STR_TYPE) {
                            node->children[i]->expr_type = TOKEN_STR_TYPE;
                        }
                    }
                }
            }
            break;
        }
        case AST_MEMBER_ACCESS:
            analyze_expression(node->left);
            if (node->left && node->left->type_name) {
                free(node->type_name);
                node->type_name = copy_cstring(node->left->type_name);
                
                if (node->left && node->left->type == AST_IDENTIFIER) {
                    Symbol* value_sym = resolve_symbol(node->left->token);
                    if (value_sym && value_sym->decl_node &&
                        (value_sym->decl_node->is_class || value_sym->decl_node->is_pointer)) {
                        node->is_pointer_access = true;
                    }
                }
            }
            break;
        case AST_INDEX:
            analyze_expression(node->left);
            analyze_expression(node->right);
            break;
        case AST_ASSIGN:
            analyze_assignment(node);
            break;
        case AST_NEW_EXPR: {
            // Validate class name exists
            Symbol* sym = resolve_symbol(node->token);
            if (!sym || !is_named_type_symbol(sym->type)) {
                report_token_error(node->token, "Unknown class in 'new':");
            }
            for (int i = 0; i < node->child_count; i++) {
                analyze_expression(node->children[i]);
            }
            break;
        }
        case AST_SUPER_EXPR:
            // Just validate we're inside a class method (simplified check)
            break;
        case AST_NULL_LITERAL:
            type = TOKEN_NULL;
            break;
        case AST_CAST: {
            analyze_expression(node->left);
            validate_type_token(node->var_type);
            type = node->var_type.type;
            break;
        }
        case AST_LAMBDA: {
            push_scope();
            for (int i = 0; i < node->child_count; i++) {
                ASTNode* param = node->children[i];
                validate_type_token(param->var_type);
                char* tn = type_name_from_token(param->var_type);
                define_symbol(param->token, param->var_type.type, false, tn, param);
                free(tn);
            }
            if (node->left->type == AST_BLOCK) {
                analyze_node(node->left);
            } else {
                analyze_expression(node->left);
            }
            pop_scope();
            type = TOKEN_FUNC;
            break;
        }
        default:
            break;
    }
    node->expr_type = type;
    return type;
}

bool analyze_semantics(ASTNode* program, bool require_main) {
    has_semantic_error = false;
    current_scope = NULL;
    loop_depth = 0;
    switch_depth = 0;
    push_scope();

    Token printin_token = {TOKEN_IDENTIFIER, "printin", 7, 0};
    Token input_token = {TOKEN_IDENTIFIER, "input", 5, 0};
    Token free_token = {TOKEN_IDENTIFIER, "free", 4, 0};
    define_symbol_if_missing(printin_token, TOKEN_FUNC, true, NULL);
    define_symbol_if_missing(input_token, TOKEN_FUNC, true, NULL);
    define_symbol_if_missing(free_token, TOKEN_FUNC, true, NULL);

    for (int i = 0; i < program->child_count; i++) {
        predeclare_global(program->children[i]);
    }

    bool has_main = false;
    bool has_test = false;
    for (int i = 0; i < program->child_count; i++) {
        if (program->children[i]->type == AST_MAIN) {
            has_main = true;
        }
        if (program->children[i]->type == AST_TEST) {
            has_test = true;
        }
        analyze_node(program->children[i]);
    }
    if (require_main && !has_main && !has_test) {
        report_error(0, "Program must have a 'main' block.", NULL);
    }

    pop_scope();
    return !has_semantic_error;
}
