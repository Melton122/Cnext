#include "semantics_internal.h"

#define IS_BUILTIN_NUMERIC(t) \
    ((t) == TOKEN_INT_TYPE || (t) == TOKEN_LONG_TYPE || \
     (t) == TOKEN_FLOAT_TYPE || (t) == TOKEN_DOUBLE_TYPE || \
     (t) == TOKEN_CHAR_TYPE || (t) == TOKEN_BYTE_TYPE || \
     (t) == TOKEN_UINT_TYPE || (t) == TOKEN_ULONG_TYPE || \
     (t) == TOKEN_USHORT_TYPE || (t) == TOKEN_UBYTE_TYPE)

#define IS_BUILTIN_UNSIGNED(t) \
    ((t) == TOKEN_UINT_TYPE || (t) == TOKEN_ULONG_TYPE || \
     (t) == TOKEN_USHORT_TYPE || (t) == TOKEN_UBYTE_TYPE)

char* type_name_from_token(Token token) {
    if (token.type == TOKEN_STR_TYPE) return copy_cstring("str");
    if (token.type == TOKEN_INT_TYPE) return copy_cstring("int");
    if (token.type == TOKEN_LONG_TYPE) return copy_cstring("long");
    if (token.type == TOKEN_FLOAT_TYPE) return copy_cstring("float");
    if (token.type == TOKEN_DOUBLE_TYPE) return copy_cstring("double");
    if (token.type == TOKEN_BOOL_TYPE) return copy_cstring("bool");
    if (token.type == TOKEN_CHAR_TYPE) return copy_cstring("char");
    if (token.type == TOKEN_BYTE_TYPE) return copy_cstring("byte");
    if (token.type == TOKEN_UINT_TYPE) return copy_cstring("uint");
    if (token.type == TOKEN_ULONG_TYPE) return copy_cstring("ulong");
    if (token.type == TOKEN_USHORT_TYPE) return copy_cstring("ushort");
    if (token.type == TOKEN_UBYTE_TYPE) return copy_cstring("ubyte");
    if (token.type == TOKEN_ITER) return copy_cstring("iter");
    if (token.type != TOKEN_IDENTIFIER) return NULL;
    return sem_copy_token_text(token);
}

bool assignment_types_compatible(Token expected, CnextTokenType actual) {
    if (expected.type == TOKEN_VAR || actual == TOKEN_EOF) return true;
    if (expected.type == actual) return true;
    // Numeric widening: smaller types promote to larger types
    if (IS_BUILTIN_NUMERIC(expected.type) && IS_BUILTIN_NUMERIC(actual)) return true;

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

void validate_var_initializer(ASTNode* node) {
    if (node->var_type.type == TOKEN_VAR && !node->init) {
        report_error(node->token.line, "Variables declared with 'var' must have an initializer:", NULL);
        return;
    }

    if (!node->init) return;

    CnextTokenType init_type = analyze_expression(node->init);
    if (node->init->type == AST_NULL_LITERAL) {
        node->init->expr_type = node->var_type.type;
        node->init->type_name = type_name_from_token(node->var_type);
    }
    if (!assignment_types_compatible(node->var_type, init_type)) {
        report_token_error(node->token, "Initializer type does not match variable type:");
    }
}

void analyze_var_declaration(ASTNode* node) {
    validate_type_token(node->var_type);
    validate_var_initializer(node);

    // Infer type for 'var' declarations from the initializer
    if (node->var_type.type == TOKEN_VAR && node->init) {
        CnextTokenType inferred = node->init->expr_type;
        if (inferred != TOKEN_EOF && inferred != TOKEN_VAR) {
            node->var_type.type = inferred;
            if (inferred == TOKEN_IDENTIFIER && node->init->token.length > 0) {
                node->var_type.start = node->init->token.start;
                node->var_type.length = node->init->token.length;
                node->var_type.line = node->init->token.line;
            }
        }
    }

    if (node->var_type.type == TOKEN_IDENTIFIER) {
        Symbol* type_sym = resolve_symbol(node->var_type);
        if (type_sym && type_sym->type == TOKEN_CLASS) {
            node->is_class = true;
        }
    }

    char* type_name = type_name_from_token(node->var_type);
    define_symbol(node->token, node->var_type.type, node->is_const, type_name, node);
    free(type_name);
}

void analyze_field_declaration(ASTNode* node) {
    validate_type_token(node->var_type);
    if (node->init) {
        report_token_error(node->token, "Fields cannot have initializers:");
        analyze_expression(node->init);
    }
}

void register_import(Token module) {
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
        Token os_exec_output_token = {TOKEN_IDENTIFIER, "os_exec_output", 14, module.line};
        Token os_exec_status_token = {TOKEN_IDENTIFIER, "os_exec_status", 14, module.line};
        Token os_arch_token = {TOKEN_IDENTIFIER, "os_arch", 7, module.line};
        Token os_file_exists_token = {TOKEN_IDENTIFIER, "os_file_exists", 14, module.line};
        Token os_is_dir_token = {TOKEN_IDENTIFIER, "os_is_dir", 9, module.line};
        Token os_file_size_token = {TOKEN_IDENTIFIER, "os_file_size", 12, module.line};
        Token os_rename_token = {TOKEN_IDENTIFIER, "os_rename", 9, module.line};
        Token os_remove_token = {TOKEN_IDENTIFIER, "os_remove", 9, module.line};
        Token os_read_dir_token = {TOKEN_IDENTIFIER, "os_read_dir", 11, module.line};
        Token os_mkdir_p_token = {TOKEN_IDENTIFIER, "os_mkdir_p", 10, module.line};
        define_symbol_if_missing(os_exec_output_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_exec_status_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_arch_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_file_exists_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_is_dir_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_file_size_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_rename_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_remove_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_read_dir_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(os_mkdir_p_token, TOKEN_FUNC, true, NULL);
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
        Token str_to_int_token = {TOKEN_IDENTIFIER, "str_to_int", 10, module.line};
        Token str_to_float_token = {TOKEN_IDENTIFIER, "str_to_float", 12, module.line};
        define_symbol_if_missing(str_to_int_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(str_to_float_token, TOKEN_FUNC, true, NULL);
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
    } else if (token_matches_name(module, "thread")) {
        Token thread_spawn_token = {TOKEN_IDENTIFIER, "thread_spawn", 12, module.line};
        Token thread_join_token = {TOKEN_IDENTIFIER, "thread_join", 11, module.line};
        Token mutex_new_token = {TOKEN_IDENTIFIER, "mutex_new", 9, module.line};
        Token mutex_lock_token = {TOKEN_IDENTIFIER, "mutex_lock", 10, module.line};
        Token mutex_unlock_token = {TOKEN_IDENTIFIER, "mutex_unlock", 12, module.line};
        Token mutex_free_token = {TOKEN_IDENTIFIER, "mutex_free", 10, module.line};
        Token channel_new_token = {TOKEN_IDENTIFIER, "channel_new", 11, module.line};
        Token channel_send_token = {TOKEN_IDENTIFIER, "channel_send", 12, module.line};
        Token channel_recv_token = {TOKEN_IDENTIFIER, "channel_recv", 12, module.line};
        Token channel_free_token = {TOKEN_IDENTIFIER, "channel_free", 12, module.line};
        define_symbol_if_missing(thread_spawn_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(thread_join_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(mutex_new_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(mutex_lock_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(mutex_unlock_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(mutex_free_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(channel_new_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(channel_send_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(channel_recv_token, TOKEN_FUNC, true, NULL);
        define_symbol_if_missing(channel_free_token, TOKEN_FUNC, true, NULL);
    } else if (token_matches_name(module, "math")) {
        static const struct { const char* name; int len; } math_funcs[] = {
            {"math_abs", 8}, {"math_max", 8}, {"math_min", 8},
            {"math_sqrt", 9}, {"math_pow", 8}, {"math_sin", 8},
            {"math_cos", 8}, {"math_tan", 8}, {"math_log", 8},
            {"math_log10", 10}, {"math_log2", 9}, {"math_logb", 9},
            {"math_exp", 8}, {"math_floor", 10}, {"math_ceil", 9},
            {"math_round", 10}, {"math_fabs", 9}, {"math_atan2", 9},
            {"math_atan", 9}, {"math_acos", 9}, {"math_asin", 9},
            {"math_sinh", 9}, {"math_cosh", 9}, {"math_tanh", 9},
            {"math_fmod", 9}, {"math_trunc", 10}, {"math_clamp", 10},
            {"math_lerp", 9}, {"math_sign", 9},
            {"math_clamp_int", 14}, {"math_random_seed", 16},
            {"math_random_int", 16}, {"math_random_float", 17},
            {NULL, 0}
        };
        for (int i = 0; math_funcs[i].name; i++) {
            Token t = {TOKEN_IDENTIFIER, math_funcs[i].name, math_funcs[i].len, module.line};
            define_symbol_if_missing(t, TOKEN_FUNC, true, NULL);
        }
    }
}
