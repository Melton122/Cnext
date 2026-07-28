#include "codegen_internal.h"

static int binop_precedence(CnextTokenType op) {
    switch (op) {
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_PERCENT:  return 2;
        case TOKEN_PLUS:
        case TOKEN_MINUS:    return 1;
        case TOKEN_LESS:
        case TOKEN_LESS_EQ:
        case TOKEN_GREATER:
        case TOKEN_GREATER_EQ: return 0;
        case TOKEN_EQ_EQ:
        case TOKEN_BANG_EQ:  return 0;
        default:             return -1;
    }
}

static bool needs_parens(CnextTokenType parent_op, CnextTokenType child_op, bool is_right) {
    int pp = binop_precedence(parent_op);
    int cp = binop_precedence(child_op);
    if (cp < 0) return false;
    if (cp < pp) return true;
    if (cp == pp && is_right) return true;
    return false;
}

/* --- Built-in Function Dispatch Table --- */
static const struct {
    const char* name;
    const char* c_emit;
} builtin_dispatch[] = {
    /* String */
    {"str_upper", "cnext_str_to_upper("},
    {"str_lower", "cnext_str_to_lower("},
    {"str_trim", "cnext_str_trim("},
    {"str_to_upper", "cnext_str_to_upper("},
    {"str_to_lower", "cnext_str_to_lower("},
    {"str_contains", "cnext_str_contains("},
    {"str_starts_with", "cnext_str_starts_with("},
    {"str_ends_with", "cnext_str_ends_with("},
    {"str_index_of", "cnext_str_find("},
    {"str_find", "cnext_str_find("},
    {"str_last_index_of", "cnext_str_last_index_of("},
    {"str_replace", "cnext_str_replace("},
    {"str_substring", "cnext_str_sub("},
    {"str_repeat", "cnext_str_repeat("},
    {"str_reverse", "cnext_str_reverse("},
    {"str_count", "cnext_str_count("},
    {"str_is_empty", "cnext_str_is_empty("},
    {"str_char_at", "cnext_str_char_at("},
    {"str_capitalize", "cnext_str_capitalize("},
    {"str_title", "cnext_str_title("},
    {"str_pad_left", "cnext_str_pad_left("},
    {"str_pad_right", "cnext_str_pad_right("},
    {"str_remove", "cnext_str_remove("},
    {"str_insert", "cnext_str_insert("},
    {"str_split", "cnext_str_split("},
    {"str_join", "cnext_str_join("},
    {"str_to_int", "cnext_str_to_int("},
    {"str_to_float", "cnext_str_parse_float("},
    /* Type Conversion */
    {"to_int", "cnext_to_int("},
    {"to_float", "cnext_to_float("},
    {"to_str", "cnext_to_string("},
    {"to_bool", "cnext_to_bool("},
    {"to_char", "cnext_to_char("},
    {"parse_int", "cnext_parse_int("},
    {"parse_float", "cnext_parse_float("},
    {"char", "cnext_char_fn("},
    {"ord", "cnext_ord("},
    {"bytes", "cnext_bytes("},
    /* Core */
    {"clone", "cnext_clone("},
    {"is_null", "cnext_is_null_ptr("},
    {"unwrap", "cnext_unwrap_ptr("},
    {"expect", "cnext_expect_str("},
    {"free", "cnext_free("},
    /* Math */
    {"math_abs", "cnext_abs("},
    {"math_min", "cnext_min("},
    {"math_max", "cnext_max("},
    {"math_clamp", "cnext_clamp("},
    {"math_round", "cnext_round("},
    {"math_floor", "cnext_floor("},
    {"math_ceil", "cnext_ceil("},
    {"math_sqrt", "cnext_sqrt("},
    {"math_pow", "cnext_pow("},
    {"math_log", "cnext_log("},
    {"math_exp", "cnext_exp("},
    {"math_sin", "cnext_sin("},
    {"math_cos", "cnext_cos("},
    {"math_tan", "cnext_tan("},
    {"math_random", "cnext_random("},
    {"math_random_range", "cnext_random_range("},
    {"math_gcd", "cnext_gcd("},
    {"math_lcm", "cnext_lcm("},
    {"math_factorial", "cnext_factorial("},
    {"math_fibonacci", "cnext_fibonacci("},
    /* Time */
    {"time_now", "cnext_timestamp("},
    {"time_sleep", "cnext_time_sleep("},
    {"time_timestamp", "cnext_timestamp("},
    {"time_date", "cnext_date("},
    {"time_time", "cnext_time_str("},
    {"time_stopwatch_start", "cnext_stopwatch_start("},
    {"time_stopwatch_stop", "cnext_stopwatch_stop("},
    {"time_format_time", "cnext_format_time("},
    /* File */
    {"read_file", "cnext_read_file("},
    {"write_file", "cnext_write_file("},
    {"append_file", "cnext_append_file("},
    {"delete_file", "cnext_delete_file("},
    {"copy_file", "cnext_copy_file("},
    {"move_file", "cnext_move_file("},
    {"file_exists", "cnext_file_exists("},
    {"file_size", "cnext_file_size("},
    /* System */
    {"cwd", "cnext_cwd("},
    {"chdir", "cnext_chdir("},
    {"platform", "cnext_platform("},
    {"sys_args", "cnext_args_str("},
    {"sys_args_count", "cnext_arg_count("},
    {"sys_arg_at", "cnext_arg_at("},
    {"getenv", "cnext_getenv_str("},
    {"setenv", "cnext_setenv_str("},
    {"hostname", "cnext_hostname_str("},
    {"username", "cnext_username_str("},
    {"cpu_count", "cnext_cpu_count("},
    {"temp_dir", "cnext_temp_dir("},
    {"home_dir", "cnext_home_dir("},
    {"sys_exec", "cnext_exec("},
    {"sys_shell", "cnext_shell("},
    /* JSON */
    {"json_parse", "cnext_json_parse("},
    {"json_stringify", "cnext_json_stringify("},
    /* Encoding */
    {"base64_encode", "cnext_base64_encode("},
    {"base64_decode", "cnext_base64_decode("},
    /* Crypto */
    {"md5", "cnext_md5_str("},
    {"sha1", "cnext_sha1_str("},
    {"sha256", "cnext_sha256_str("},
    {"uuid", "cnext_uuid("},
    /* Utility */
    {"debug", "cnext_debug("},
    {"gc", "cnext_gc("},
    {"stacktrace", "/* stacktrace */ "},
    {"benchmark", "cnext_benchmark("},
    {NULL, NULL}
};

static void gen_with_parens(ASTNode* child, CnextTokenType parent_op, bool is_right) {
    if (child && child->type == AST_BINARY && needs_parens(parent_op, child->token.type, is_right)) {
        fprintf(out, "(");
        generate_expression(child);
        fprintf(out, ")");
    } else {
        generate_expression(child);
    }
}

static void gen_lambda_expr(ASTNode* node) {
    int current_lambda = lambda_counter++;
    CapturedVar* captures = NULL;
    detect_captures_walk(node->left, &captures, node, program_node);
    int capture_count = 0;
    for (CapturedVar* cv = captures; cv; cv = cv->next) capture_count++;

    if (capture_count > 0) {
        int cl_id = closure_counter++;
        ClosureInfo* ci = (ClosureInfo*)checked_malloc(sizeof(ClosureInfo));
        ci->id = cl_id;
        ci->captures = captures;
        ci->capture_count = capture_count;
        ci->next = closure_list;
        closure_list = ci;

        FILE* saved_out = out;
        out = closure_defs_out;
        fprintf(out, "struct _cl_env_%d {\n", cl_id);
        for (CapturedVar* cv = captures; cv; cv = cv->next) {
            fprintf(out, "    ");
            const char* ct = type_token_to_c(cv->type);
            fprintf(out, "%s", ct ? ct : "void*");
            if (cv->is_pointer || cv->is_class) fprintf(out, "*");
            fprintf(out, " %s;\n", cv->name);
        }
        fprintf(out, "};\n");

        generate_lambda_return_type(node->left);
        fprintf(out, " _cl_fn_%d(void* _env_raw", cl_id);
        for (int i = 0; i < node->child_count; i++) {
            fprintf(out, ", ");
            generate_type(node->children[i]->var_type, false);
            fprintf(out, " %.*s", node->children[i]->token.length, node->children[i]->token.start);
        }
        fprintf(out, ") {\n");
        fprintf(out, "    struct _cl_env_%d* _env = (struct _cl_env_%d*)_env_raw;\n", cl_id, cl_id);
        for (CapturedVar* cv = captures; cv; cv = cv->next) {
            fprintf(out, "    #define %s (_env->%s)\n", cv->name, cv->name);
        }
        if (node->left->type == AST_BLOCK) {
            generate_block(node->left);
        } else {
            fprintf(out, "    return ");
            generate_expression(node->left);
            fprintf(out, ";\n");
        }
        for (CapturedVar* cv = captures; cv; cv = cv->next) {
            fprintf(out, "    #undef %s\n", cv->name);
        }
        fprintf(out, "}\n\n");
        out = saved_out;

        fprintf(out, "({ struct _cl_env_%d* _ce = (struct _cl_env_%d*)malloc(sizeof(struct _cl_env_%d)); if (!_ce) { fprintf(stderr, \"Cnext runtime: out of memory.\\n\"); exit(70); }", cl_id, cl_id, cl_id);
        for (CapturedVar* cv = captures; cv; cv = cv->next) {
            fprintf(out, "_ce->%s = %s;", cv->name, cv->name);
        }
        fprintf(out, "(CnextClosure){ (void*)_cl_fn_%d, _ce, 1 }; })", cl_id);
    } else {
        int lambda_id = current_lambda++;
        FILE* saved_out = out;
        out = closure_defs_out;
        generate_lambda_return_type(node->left);
        fprintf(out, " _lambda_%d(void* _env_raw", lambda_id);
        for (int i = 0; i < node->child_count; i++) {
            fprintf(out, ", ");
            generate_type(node->children[i]->var_type, false);
            fprintf(out, " %.*s", node->children[i]->token.length, node->children[i]->token.start);
        }
        fprintf(out, ") ");
        if (node->left->type == AST_BLOCK) {
            generate_block(node->left);
        } else {
            fprintf(out, "{ return ");
            generate_expression(node->left);
            fprintf(out, "; }");
        }
        fprintf(out, "\n");
        out = saved_out;
        fprintf(out, "(void*)_lambda_%d", lambda_id);
        free_captures(captures);
    }
}

void generate_expression(ASTNode* node) {
    if (!node) return;
    switch (node->type) {
        case AST_LITERAL:
            if (node->token.type == TOKEN_STRING_LITERAL) {
                generate_string_interpolation(node->token.start, node->token.length);
            } else if (node->token.type == TOKEN_RAW_STRING) {
                // Emit raw string content with C-level escaping
                const char* start = node->token.start + 2; // skip 'r' and '"'
                const char* end = node->token.start + node->token.length - 1; // before closing '"'
                fprintf(out, "(CnextString){(char*)\"");
                for (const char* p = start; p < end; p++) {
                    if (*p == '"') fprintf(out, "\\\"");
                    else if (*p == '\\') fprintf(out, "\\\\");
                    else fprintf(out, "%c", *p);
                }
                fprintf(out, "\", %d}", (int)(end - start));
            } else if (node->token.type == TOKEN_TRUE) {
                fprintf(out, "true");
            } else if (node->token.type == TOKEN_FALSE) {
                fprintf(out, "false");
            } else {
                fprintf(out, "%.*s", node->token.length, node->token.start);
            }
            break;
        case AST_LITERAL_ARRAY:
            fprintf(out, "{");
            for (int i = 0; i < node->child_count; i++) {
                generate_expression(node->children[i]);
                if (i < node->child_count - 1) fprintf(out, ", ");
            }
            fprintf(out, "}");
            break;
        case AST_TUPLE:
            fprintf(out, "({ CnextString _tr = (CnextString){(char*)\"(\", 1}; ");
            for (int i = 0; i < node->child_count; i++) {
                if (i > 0) {
                    fprintf(out, "_tr = cnext_concat(_tr, (CnextString){(char*)\", \", 2}); ");
                }
                fprintf(out, "_tr = cnext_concat(_tr, cnext_to_string(");
                generate_expression(node->children[i]);
                fprintf(out, ")); ");
            }
            fprintf(out, "_tr = cnext_concat(_tr, (CnextString){(char*)\")\", 1}); ");
            fprintf(out, "(CnextTuple){%d, _tr}; })", node->child_count);
            break;
        case AST_OPTION_SOME: {
            // option(value) -> CnextVariant with tag=1 and payload
            fprintf(out, "({ CnextVariant _opt = {1, 0, {0}}; ");
            fprintf(out, "__typeof__(");
            generate_expression(node->left);
            fprintf(out, ") _val = (");
            generate_expression(node->left);
            fprintf(out, "); memcpy(_opt.payload, &_val, sizeof(_val)); _opt.size = sizeof(_val); _opt; })");
            break;
        }
        case AST_OPTION_NONE:
            fprintf(out, "((CnextVariant){0, 0, {0}})");
            break;
        case AST_RESULT_OK: {
            // ok(value) -> CnextVariant with tag=0 and payload
            fprintf(out, "({ CnextVariant _res = {0, 0, {0}}; ");
            fprintf(out, "__typeof__(");
            generate_expression(node->left);
            fprintf(out, ") _val = (");
            generate_expression(node->left);
            fprintf(out, "); memcpy(_res.payload, &_val, sizeof(_val)); _res.size = sizeof(_val); _res; })");
            break;
        }
        case AST_RESULT_ERR: {
            // err(value) -> CnextVariant with tag=1 and payload
            fprintf(out, "({ CnextVariant _res = {1, 0, {0}}; ");
            fprintf(out, "__typeof__(");
            generate_expression(node->left);
            fprintf(out, ") _val = (");
            generate_expression(node->left);
            fprintf(out, "); memcpy(_res.payload, &_val, sizeof(_val)); _res.size = sizeof(_val); _res; })");
            break;
        }
        case AST_AWAIT_EXPR:
            // await expr: if in generator context, yield; otherwise run to completion
            fprintf(out, "({ __auto_type _af = (");
            generate_expression(node->left);
            fprintf(out, "); while (_af->_next(_af)); _af->current; })");
            break;
        case AST_IDENTIFIER:
            fprintf(out, "%.*s", node->token.length, node->token.start);
            break;
        case AST_MEMBER_ACCESS:
            generate_expression(node->left);
            if (node->is_pointer_access || (node->left && node->left->is_class) ||
                (node->left && node->left->is_pointer) ||
                (node->left && node->left->type == AST_IDENTIFIER &&
                node->left->token.length == 4 &&
                strncmp(node->left->token.start, "self", 4) == 0)) {
                fprintf(out, "->%.*s", node->token.length, node->token.start);
            } else {
                fprintf(out, ".%.*s", node->token.length, node->token.start);
            }
            break;
        case AST_TUPLE_ACCESS:
            generate_expression(node->left);
            fprintf(out, "._%.*s", node->token.length, node->token.start);
            break;
        case AST_INDEX:
            fprintf(out, "CNEXT_ARRAY_IDX(");
            generate_expression(node->left);
            fprintf(out, ", ");
            generate_expression(node->right);
            fprintf(out, ")");
            break;
        case AST_SLICE:
            fprintf(out, "CNEXT_SLICE(");
            generate_expression(node->left);
            fprintf(out, ", ");
            if (node->init) {
                generate_expression(node->init);
            } else {
                fprintf(out, "0");
            }
            fprintf(out, ", ");
            if (node->right) {
                generate_expression(node->right);
            } else {
                generate_expression(node->left);
                fprintf(out, ".length");
            }
            fprintf(out, ")");
            break;
        case AST_POSTFIX:
            generate_expression(node->left);
            fprintf(out, "%.*s", node->token.length, node->token.start);
            break;
        case AST_CALL: {
            if (node->left->type == AST_SUPER_EXPR) {
                if (current_parent_class && current_parent_class[0] != '\0') {
                    fprintf(out, "%s_%.*s((%s*)self", current_parent_class,
                        node->left->token.length, node->left->token.start,
                        current_parent_class);
                    for (int i = 0; i < node->child_count; i++) {
                        fprintf(out, ", ");
                        generate_expression(node->children[i]);
                    }
                    fprintf(out, ")");
                } else {
                    fprintf(out, "/* invalid super call */");
                }
            } else if (node->left->type == AST_MEMBER_ACCESS) {
                if (node->left->type_name && node->left->type_name[0] != '\0') {
                    const char* dispatch_name = node->left->type_name;
                    int dispatch_name_len = (int)strlen(dispatch_name);
                    char mangled_buf[512] = {0};
                    ASTNode* obj = node->left->left;

                    // Check if this is a static method call
                    bool is_static_method = false;
                    if (obj && obj->type == AST_IDENTIFIER && find_class_decl(program_node, node->left->type_name)) {
                        ASTNode* cdecl = find_class_decl(program_node, node->left->type_name);
                        for (int mi = 0; mi < cdecl->child_count; mi++) {
                            ASTNode* child = cdecl->children[mi];
                            if ((child->type == AST_FUNC_DECL) &&
                                child->token.length == node->left->token.length &&
                                strncmp(child->token.start, node->left->token.start, node->left->token.length) == 0 &&
                                child->is_static) {
                                is_static_method = true;
                                break;
                            }
                        }
                    }

                    if (obj && obj->type == AST_IDENTIFIER && find_class_decl(program_node, node->left->type_name)) {
                        ASTNode* cdecl = find_class_decl(program_node, node->left->type_name);
                        if (cdecl && cdecl->type_param_count > 0) {
                            for (int vi = 0; vi < program_node->child_count; vi++) {
                                ASTNode* prog_child = program_node->children[vi];
                                if (prog_child->type == AST_MAIN && prog_child->left) {
                                    for (int si = 0; si < prog_child->left->child_count; si++) {
                                        ASTNode* stmt = prog_child->left->children[si];
                                        if (stmt->type == AST_VAR_DECL &&
                                            stmt->token.length == obj->token.length &&
                                            strncmp(stmt->token.start, obj->token.start, obj->token.length) == 0 &&
                                            stmt->init && stmt->init->type == AST_NEW_EXPR &&
                                            stmt->init->type_arg_count > 0) {
                                            int blen = stmt->init->token.length < 200 ? stmt->init->token.length : 200;
                                            strncpy(mangled_buf, stmt->init->token.start, blen);
                                            mangled_buf[blen] = '\0';
                                            for (int ai = 0; ai < stmt->init->type_arg_count; ai++) {
                                                char arg_str[256] = {0};
                                                ASTNode* arg = stmt->init->type_args[ai];
                                                snprintf(arg_str, sizeof(arg_str), "_%.*s", arg->token.length, arg->token.start);
                                                strncat(mangled_buf, arg_str, sizeof(mangled_buf) - strlen(mangled_buf) - 1);
                                            }
                                            dispatch_name = mangled_buf;
                                            dispatch_name_len = (int)strlen(mangled_buf);
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    fprintf(out, "%.*s_%.*s(", dispatch_name_len, dispatch_name,
                        node->left->token.length, node->left->token.start);
                    if (is_static_method) {
                        // Static methods: no self argument, just pass regular args
                        if (node->child_count > 0) {
                            for (int i = 0; i < node->child_count; i++) {
                                if (i > 0) fprintf(out, ", ");
                                generate_expression(node->children[i]);
                            }
                        }
                    } else {
                        bool is_builtin_ext = (strncmp(node->left->type_name, "str", 3) == 0 ||
                                               strncmp(node->left->type_name, "int", 3) == 0 ||
                                               strncmp(node->left->type_name, "float", 5) == 0 ||
                                               strncmp(node->left->type_name, "bool", 4) == 0 ||
                                               strncmp(node->left->type_name, "char", 4) == 0);
                        if (!is_builtin_ext && !node->left->is_pointer_access) {
                            fprintf(out, "&");
                        }
                        generate_expression(node->left->left);
                        if (node->child_count > 0) {
                            fprintf(out, ", ");
                            for (int i = 0; i < node->child_count; i++) {
                                generate_expression(node->children[i]);
                                if (i < node->child_count - 1) fprintf(out, ", ");
                            }
                        }
                    }
                    fprintf(out, ")");
                } else {
                    bool is_prefixed = false;
                    if (node->left->left && node->left->left->type == AST_IDENTIFIER) {
                        static const struct { const char* name; int len; } prefixed[] = {
                            {"json", 4}, {"math", 4}, {"os", 2}, {"time", 4},
                            {"regex", 5}, {"crypto", 6}, {"path", 4},
                            {"process", 7}, {"random", 6}, {NULL, 0}
                        };
                        for (int m = 0; prefixed[m].name; m++) {
                            if (node->left->left->token.length == prefixed[m].len &&
                                strncmp(node->left->left->token.start, prefixed[m].name, prefixed[m].len) == 0) {
                                is_prefixed = true;
                                fprintf(out, "%s_%.*s(", prefixed[m].name, node->left->token.length, node->left->token.start);
                                break;
                            }
                        }
                    }
                    if (!is_prefixed && node->left->left && node->left->left->type == AST_IDENTIFIER) {
                        const char* class_name = lookup_class_var(
                            node->left->left->token.start, node->left->left->token.length);
                        if (class_name) {
                            fprintf(out, "%.*s_%.*s(", (int)strlen(class_name), class_name,
                                node->left->token.length, node->left->token.start);
                            generate_expression(node->left->left);
                            if (node->child_count > 0) {
                                fprintf(out, ", ");
                                for (int i = 0; i < node->child_count; i++) {
                                    generate_expression(node->children[i]);
                                    if (i < node->child_count - 1) fprintf(out, ", ");
                                }
                            }
                            fprintf(out, ")");
                            break;
                        }
                    }
                    if (!is_prefixed) {
                        fprintf(out, "%.*s(", node->left->token.length, node->left->token.start);
                    }
                    for (int i = 0; i < node->child_count; i++) {
                        generate_expression(node->children[i]);
                        if (i < node->child_count - 1) fprintf(out, ", ");
                    }
                    fprintf(out, ")");
                }
            } else if (node->left->type == AST_SAFE_ACCESS) {
                const char* zero_val = "NULL";
                bool got_zero = false;
                if (node->type_name) {
                    if (strncmp(node->type_name, "str", 3) == 0 && node->type_name[3] == '\0')
                        { zero_val = "(CnextString){NULL, 0}"; got_zero = true; }
                    else if (strncmp(node->type_name, "int", 3) == 0 && node->type_name[3] == '\0')
                        { zero_val = "0"; got_zero = true; }
                    else if (strncmp(node->type_name, "float", 5) == 0 && node->type_name[5] == '\0')
                        { zero_val = "0.0f"; got_zero = true; }
                    else if (strncmp(node->type_name, "bool", 4) == 0 && node->type_name[4] == '\0')
                        { zero_val = "0"; got_zero = true; }
                }
                if (!got_zero && node->expr_type != TOKEN_EOF) {
                    const char* zt = type_token_to_c(node->expr_type);
                    if (node->expr_type == TOKEN_STR_TYPE) zero_val = "(CnextString){NULL, 0}";
                    else if (zt) zero_val = "0";
                    else zero_val = "0";
                }
                const char* class_type = NULL;
                if (node->left->left && node->left->left->type_name) {
                    class_type = node->left->left->type_name;
                }
                fprintf(out, "(((");
                generate_expression(node->left->left);
                fprintf(out, ") != NULL) ? (");
                if (class_type && class_type[0] != '\0') {
                    fprintf(out, "%s_%.*s(", class_type,
                        node->left->token.length, node->left->token.start);
                    generate_expression(node->left->left);
                    for (int i = 0; i < node->child_count; i++) {
                        fprintf(out, ", ");
                        generate_expression(node->children[i]);
                    }
                    fprintf(out, ")");
                }
                fprintf(out, ") : (%s))", zero_val);
            } else {
                bool is_printin = (node->left->token.length == 7 && strncmp(node->left->token.start, "printin", 7) == 0);
                bool is_print = (node->left->token.length == 5 && strncmp(node->left->token.start, "print", 5) == 0);
                bool is_input = (node->left->token.length == 5 && strncmp(node->left->token.start, "input", 5) == 0);
                bool is_free = (node->left->token.length == 4 && strncmp(node->left->token.start, "free", 4) == 0);
                bool is_len = (node->left->token.length == 3 && strncmp(node->left->token.start, "len", 3) == 0);
                bool is_is_null = (node->left->token.length == 7 && strncmp(node->left->token.start, "is_null", 7) == 0);
                bool is_split = (node->left->token.length == 5 && strncmp(node->left->token.start, "split", 5) == 0);
                bool is_join = (node->left->token.length == 4 && strncmp(node->left->token.start, "join", 4) == 0);
                bool is_unwrap = (node->left->token.length == 6 && strncmp(node->left->token.start, "unwrap", 6) == 0);
                bool is_expect = (node->left->token.length == 6 && strncmp(node->left->token.start, "expect", 6) == 0);
                bool is_str_to_int = (node->left->token.length == 10 && strncmp(node->left->token.start, "str_to_int", 10) == 0);
                bool is_str_to_float = (node->left->token.length == 12 && strncmp(node->left->token.start, "str_to_float", 12) == 0);
                bool is_panic = (node->left->token.length == 5 && strncmp(node->left->token.start, "panic", 5) == 0);
                bool is_exit = (node->left->token.length == 4 && strncmp(node->left->token.start, "exit", 4) == 0);
                bool is_assert = (node->left->token.length == 6 && strncmp(node->left->token.start, "assert", 6) == 0);
                bool is_typeof = (node->left->token.length == 6 && strncmp(node->left->token.start, "typeof", 6) == 0);

                if (is_printin) {
                    if (node->child_count > 1) {
                        for (int pi = 0; pi < node->child_count - 1; pi++) {
                            fprintf(out, "print_raw(");
                            generate_expression(node->children[pi]);
                            fprintf(out, "); ");
                        }
                        fprintf(out, "printin(");
                        generate_expression(node->children[node->child_count - 1]);
                        fprintf(out, ")");
                    } else {
                        fprintf(out, "printin(");
                        if (node->child_count > 0) generate_expression(node->children[0]);
                        fprintf(out, ")");
                    }
                } else if (is_print) {
                    if (node->child_count > 1) {
                        for (int pi = 0; pi < node->child_count; pi++) {
                            fprintf(out, "print_raw(");
                            generate_expression(node->children[pi]);
                            fprintf(out, ")");
                            if (pi < node->child_count - 1) fprintf(out, "; ");
                        }
                    } else {
                        fprintf(out, "print_raw(");
                        if (node->child_count > 0) generate_expression(node->children[0]);
                        fprintf(out, ")");
                    }
                } else if (is_input) {
                    fprintf(out, "cnext_input(");
                    if (node->child_count > 0) generate_expression(node->children[0]);
                    fprintf(out, ")");
                } else if (is_free) {
                    fprintf(out, "cnext_free(");
                    if (node->child_count > 0) generate_expression(node->children[0]);
                    fprintf(out, ")");
                } else if (is_len) {
                    fprintf(out, "(int)(");
                    if (node->child_count > 0) generate_expression(node->children[0]);
                    fprintf(out, ".length)");
                } else if (is_is_null) {
                    if (node->child_count > 0) {
                        ASTNode* arg = node->children[0];
                        bool is_str = (arg->type_name && strncmp(arg->type_name, "str", 3) == 0) ||
                                      (arg->type == AST_LITERAL && arg->token.type == TOKEN_STRING_LITERAL) ||
                                      (arg->expr_type == TOKEN_STR_TYPE);
                        if (is_str) {
                            fprintf(out, "cnext_is_null_str(");
                            generate_expression(arg);
                            fprintf(out, ")");
                        } else {
                            fprintf(out, "cnext_is_null_ptr((void*)(");
                            generate_expression(arg);
                            fprintf(out, "))");
                        }
                    } else {
                        fprintf(out, "true");
                    }
                } else if (is_split) {
                    fprintf(out, "cnext_str_split(");
                    if (node->child_count > 0) generate_expression(node->children[0]);
                    if (node->child_count > 1) { fprintf(out, ", "); generate_expression(node->children[1]); }
                    fprintf(out, ")");
                } else if (is_join) {
                    fprintf(out, "cnext_str_join(");
                    if (node->child_count > 0) generate_expression(node->children[0]);
                    if (node->child_count > 1) { fprintf(out, ", "); generate_expression(node->children[1]); }
                    fprintf(out, ")");
                } else if (is_unwrap) {
                    fprintf(out, "cnext_unwrap_str(");
                    if (node->child_count > 0) generate_expression(node->children[0]);
                    fprintf(out, ", \"unwrap failed: value is null\")");
                } else if (is_expect) {
                    fprintf(out, "cnext_expect_str(");
                    if (node->child_count > 0) generate_expression(node->children[0]);
                    fprintf(out, ", ");
                    if (node->child_count > 1) generate_expression(node->children[1]);
                    else fprintf(out, "\"expect failed: value is null\"");
                    fprintf(out, ")");
                } else if (is_str_to_int) {
                    fprintf(out, "cnext_str_to_int(");
                    if (node->child_count > 0) generate_expression(node->children[0]);
                    fprintf(out, ")");
                } else if (is_str_to_float) {
                    fprintf(out, "cnext_str_parse_float(");
                    if (node->child_count > 0) generate_expression(node->children[0]);
                    fprintf(out, ")");
                } else if (is_panic) {
                    fprintf(out, "cnext_panic(");
                    if (node->child_count > 0) generate_expression(node->children[0]);
                    else fprintf(out, "(CnextString){(char*)\"panic\", 5}");
                    fprintf(out, ")");
                } else if (is_exit) {
                    fprintf(out, "cnext_exit_fn(");
                    if (node->child_count > 0) generate_expression(node->children[0]);
                    else fprintf(out, "0");
                    fprintf(out, ")");
                } else if (is_assert) {
                    fprintf(out, "cnext_assert_fn(");
                    if (node->child_count > 0) generate_expression(node->children[0]);
                    fprintf(out, ", ");
                    if (node->child_count > 1) generate_expression(node->children[1]);
                    else fprintf(out, "(CnextString){(char*)\"assertion failed\", 16}");
                    fprintf(out, ")");
                } else if (is_typeof) {
                    fprintf(out, "cnext_typeof(");
                    if (node->child_count > 0) generate_expression(node->children[0]);
                    fprintf(out, ")");
                } else {
                    /* Table-driven dispatch for all other built-in functions */
                    bool found_builtin = false;
                    if (node->left && node->left->type == AST_IDENTIFIER) {
                        for (int b = 0; builtin_dispatch[b].name; b++) {
                            if (node->left->token.length == (int)strlen(builtin_dispatch[b].name) &&
                                strncmp(node->left->token.start, builtin_dispatch[b].name, node->left->token.length) == 0) {
                                fprintf(out, "%s", builtin_dispatch[b].c_emit);
                                for (int i = 0; i < node->child_count; i++) {
                                    generate_expression(node->children[i]);
                                    if (i < node->child_count - 1) fprintf(out, ", ");
                                }
                                fprintf(out, ")");
                                found_builtin = true;
                                break;
                            }
                        }
                    }
                    if (!found_builtin && node->left && node->left->type == AST_IDENTIFIER && node->left->type_arg_count > 0) {
                        char fname[256];
                        int flen = node->left->token.length < 255 ? node->left->token.length : 255;
                        strncpy(fname, node->left->token.start, flen);
                        fname[flen] = '\0';
                        ASTNode* fdecl = find_func_decl(program_node, fname);
                        if (fdecl && fdecl->type_param_count > 0) {
                            enqueue_spec_work(fdecl, node->left);
                        }
                        char* mangled = mangle_generic_name(node->left);
                        fprintf(out, "%s(", mangled);
                        free(mangled);
                    } else if (!found_builtin && node->left && node->left->type == AST_IDENTIFIER) {
                        char fname[256];
                        int flen = node->left->token.length < 255 ? node->left->token.length : 255;
                        strncpy(fname, node->left->token.start, flen);
                        fname[flen] = '\0';
                        ClosureVar* clvar = find_closure_var(fname);
                        if (clvar) {
                            ASTNode* lambda = clvar->lambda_node;
                            fprintf(out, "((");
                            if (lambda->left->type != AST_BLOCK) {
                                const char* ct = type_token_to_c(lambda->left->expr_type);
                                fprintf(out, "%s", ct ? ct : "void");
                            } else {
                                for (int ri = 0; ri < lambda->left->child_count; ri++) {
                                    if (lambda->left->children[ri]->type == AST_RETURN &&
                                        lambda->left->children[ri]->left) {
                                        const char* ct = type_token_to_c(lambda->left->children[ri]->left->expr_type);
                                        fprintf(out, "%s", ct ? ct : "void");
                                        break;
                                    }
                                }
                            }
                            fprintf(out, "(*)(");
                            fprintf(out, "void*");
                            for (int pi = 0; pi < lambda->child_count; pi++) {
                                fprintf(out, ", ");
                                generate_type(lambda->children[pi]->var_type, false);
                            }
                            fprintf(out, "))");
                            fprintf(out, "%s.fn)(%s.env", fname, fname);
                            for (int ai = 0; ai < node->child_count; ai++) {
                                fprintf(out, ", ");
                                generate_expression(node->children[ai]);
                            }
                            fprintf(out, ")");
                            goto call_done;
                        }
                        if (is_func_param(fname, flen) || is_func_typed_var(fname, flen)) {
                            fprintf(out, "((int(*)(void*, int))((CnextClosure)%s).fn)(((CnextClosure)%s).env", fname, fname);
                            for (int ai = 0; ai < node->child_count; ai++) {
                                fprintf(out, ", ");
                                generate_expression(node->children[ai]);
                            }
                            fprintf(out, ")");
                            goto call_done;
                        }
                        ASTNode* fdecl = find_func_decl(program_node, fname);
                        if (fdecl && fdecl->type_param_count > 0 && node->left->type_arg_count == 0) {
                            infer_generic_type_args(node, fdecl);
                        }
                        if (node->left->type_arg_count > 0) {
                            enqueue_spec_work(fdecl, node->left);
                            char* mangled = mangle_generic_name(node->left);
                            fprintf(out, "%s(", mangled);
                            free(mangled);
                        } else if (!found_builtin) {
                            fprintf(out, "%.*s(", node->left->token.length, node->left->token.start);
                        }
                    } else if (!found_builtin) {
                        fprintf(out, "%.*s(", node->left->token.length, node->left->token.start);
                    }

                    /* Skip argument handling for table-dispatched builtins (args already emitted) */
                    if (found_builtin) goto call_done;

                    ASTNode* func_decl = NULL;
                    if (node->left && node->left->type == AST_IDENTIFIER) {
                        char fname[256];
                        int flen = node->left->token.length < 255 ? node->left->token.length : 255;
                        strncpy(fname, node->left->token.start, flen);
                        fname[flen] = '\0';
                        func_decl = find_func_decl(program_node, fname);
                    }
                    
                    bool first_arg = true;
                    if (func_decl && func_decl->type == AST_FUNC_DECL) {
                        bool* provided = (bool*)checked_calloc(func_decl->child_count, sizeof(bool));
                        ASTNode** named_args = (ASTNode**)checked_calloc(node->child_count, sizeof(ASTNode*));
                        int named_count = 0;
                        
                        for (int i = 0; i < node->child_count; i++) {
                            if (node->children[i]->type == AST_NAMED_ARG) {
                                named_args[named_count++] = node->children[i];
                            }
                        }
                        
                        int pos_idx = 0;
                        for (int i = 0; i < node->child_count; i++) {
                            if (node->children[i]->type == AST_NAMED_ARG) continue;
                            if (!first_arg) fprintf(out, ", ");
                            generate_expression(node->children[i]);
                            first_arg = false;
                            if (pos_idx < func_decl->child_count) {
                                provided[pos_idx] = true;
                            }
                            pos_idx++;
                        }
                        
                        for (int i = 0; i < func_decl->child_count; i++) {
                            if (provided[i]) continue;
                            
                            bool found_named = false;
                            for (int j = 0; j < named_count; j++) {
                                if (named_args[j]->named_arg_name.length == func_decl->children[i]->token.length &&
                                    strncmp(named_args[j]->named_arg_name.start, func_decl->children[i]->token.start, 
                                           func_decl->children[i]->token.length) == 0) {
                                    if (!first_arg) fprintf(out, ", ");
                                    generate_expression(named_args[j]->right);
                                    first_arg = false;
                                    found_named = true;
                                    break;
                                }
                            }
                            
                            if (!found_named && func_decl->children[i]->default_value) {
                                if (!first_arg) fprintf(out, ", ");
                                generate_expression(func_decl->children[i]->default_value);
                                first_arg = false;
                            }
                        }
                        
                        free(provided);
                        free(named_args);
                    } else {
                        for (int i = 0; i < node->child_count; i++) {
                            if (!first_arg) fprintf(out, ", ");
                            if (node->children[i]->type == AST_NAMED_ARG) {
                                generate_expression(node->children[i]->right);
                            } else {
                                generate_expression(node->children[i]);
                            }
                            first_arg = false;
                        }
                    }
                    fprintf(out, ")");
                }
            }
            call_done:
            break;
        }
        case AST_UNARY:
            fprintf(out, "%.*s", node->token.length, node->token.start);
            generate_expression(node->right);
            break;
        case AST_BINARY:
            if ((node->token.type == TOKEN_EQ_EQ || node->token.type == TOKEN_BANG_EQ) &&
                ((node->left && node->left->expr_type == TOKEN_STR_TYPE) ||
                 (node->right && node->right->expr_type == TOKEN_STR_TYPE))) {
                if (node->token.type == TOKEN_BANG_EQ) fprintf(out, "!");
                fprintf(out, "cnext_str_eq(");
                generate_expression(node->left);
                fprintf(out, ", ");
                generate_expression(node->right);
                fprintf(out, ")");
            } else if (node->expr_type == TOKEN_STR_TYPE && node->token.type == TOKEN_PLUS) {
                fprintf(out, "cnext_concat(cnext_to_string(");
                generate_expression(node->left);
                fprintf(out, "), cnext_to_string(");
                generate_expression(node->right);
                fprintf(out, "))");
            } else if (node->left && node->left->type_name && node->left->type_name[0] != '\0') {
                Token subst_type;
                bool subst_array;
                char resolved_name[256] = {0};
                if (find_type_subst(node->left->type_name, &subst_type, &subst_array)) {
                    int rlen = subst_type.length < 255 ? subst_type.length : 255;
                    strncpy(resolved_name, subst_type.start, rlen);
                    resolved_name[rlen] = '\0';
                } else {
                    strncpy(resolved_name, node->left->type_name, 255);
                }
                if (strncmp(resolved_name, "str", 3) != 0 &&
                    strncmp(resolved_name, "int", 3) != 0 &&
                    strncmp(resolved_name, "long", 4) != 0 &&
                    strncmp(resolved_name, "float", 5) != 0 &&
                    strncmp(resolved_name, "double", 6) != 0 &&
                    strncmp(resolved_name, "bool", 4) != 0 &&
                    strncmp(resolved_name, "char", 4) != 0 &&
                    strncmp(resolved_name, "byte", 4) != 0 &&
                    strncmp(resolved_name, "uint", 4) != 0 &&
                    strncmp(resolved_name, "ulong", 5) != 0 &&
                    strncmp(resolved_name, "ushort", 6) != 0 &&
                    strncmp(resolved_name, "ubyte", 5) != 0) {
                const char* op_name = "op";
                if (node->token.type == TOKEN_PLUS) op_name = "plus";
                else if (node->token.type == TOKEN_MINUS) op_name = "minus";
                else if (node->token.type == TOKEN_STAR) op_name = "star";
                else if (node->token.type == TOKEN_SLASH) op_name = "slash";
                else if (node->token.type == TOKEN_EQ_EQ) op_name = "eq";
                else if (node->token.type == TOKEN_BANG_EQ) op_name = "neq";
                else if (node->token.type == TOKEN_LESS) op_name = "lt";
                else if (node->token.type == TOKEN_LESS_EQ) op_name = "lte";
                else if (node->token.type == TOKEN_GREATER) op_name = "gt";
                else if (node->token.type == TOKEN_GREATER_EQ) op_name = "gte";
                else if (node->token.type == TOKEN_PERCENT) op_name = "mod";
                else if (node->token.type == TOKEN_AND_AND) op_name = "and";
                else if (node->token.type == TOKEN_OR_OR) op_name = "or";
                fprintf(out, "%s_operator_%s(", resolved_name, op_name);
                generate_expression(node->left);
                fprintf(out, ", ");
                generate_expression(node->right);
                fprintf(out, ")");
                } else {
                    generate_expression(node->left);
                    fprintf(out, " %.*s ", node->token.length, node->token.start);
                    generate_expression(node->right);
                }
            } else {
                gen_with_parens(node->left, node->token.type, false);
                fprintf(out, " %.*s ", node->token.length, node->token.start);
                gen_with_parens(node->right, node->token.type, true);
            }
            break;
        case AST_ASSIGN:
            generate_expression(node->left);
            fprintf(out, " %.*s ", node->token.length, node->token.start);
            generate_expression(node->right);
            break;
        case AST_NEW_EXPR: {
            const char* cname = NULL;
            int cname_len = node->token.length;
            char mangled_buf[512] = {0};
            if (node->type_arg_count > 0) {
                char cname_tmp[256] = {0};
                int clen_tmp = node->token.length < 255 ? node->token.length : 255;
                strncpy(cname_tmp, node->token.start, clen_tmp);
                cname_tmp[clen_tmp] = '\0';
                ASTNode* cdecl = find_class_decl(program_node, cname_tmp);
                if (cdecl && cdecl->type == AST_CLASS_DECL) {
                    int blen = node->token.length < 200 ? node->token.length : 200;
                    strncpy(mangled_buf, node->token.start, blen);
                    mangled_buf[blen] = '\0';
                    for (int i = 0; i < node->type_arg_count; i++) {
                        char arg_str[256] = {0};
                        ASTNode* arg = node->type_args[i];
                        snprintf(arg_str, sizeof(arg_str), "_%.*s", arg->token.length, arg->token.start);
                        strncat(mangled_buf, arg_str, sizeof(mangled_buf) - strlen(mangled_buf) - 1);
                    }
                    cname = mangled_buf;
                    cname_len = (int)strlen(mangled_buf);
                }
            }
            if (!cname) {
                cname = node->token.start;
                cname_len = node->token.length;
            }
            fprintf(out, "({ %.*s* _obj = (%.*s*)malloc(sizeof(%.*s)); if (!_obj) { fprintf(stderr, \"Cnext runtime: out of memory.\\n\"); exit(70); } _cnext_track(_obj); ",
                cname_len, cname, cname_len, cname, cname_len, cname);
            fprintf(out, "%.*s_new(_obj", cname_len, cname);
            for (int i = 0; i < node->child_count; i++) {
                fprintf(out, ", ");
                generate_expression(node->children[i]);
            }
            fprintf(out, "); _obj; })");
            break;
        }
        case AST_SUPER_EXPR:
            fprintf(out, "%.*s", node->token.length, node->token.start);
            break;
        case AST_NULL_LITERAL:
            if (node->expr_type == TOKEN_STR_TYPE) {
                fprintf(out, "(CnextString){NULL, 0}");
            } else {
                fprintf(out, "NULL");
            }
            break;
        case AST_NULL_COALESCE: {
            bool is_str_type = (node->left && node->left->type_name &&
                strncmp(node->left->type_name, "str", 3) == 0 && node->left->type_name[3] == '\0');
            bool is_class_type = (node->left && node->left->type_name &&
                !is_str_type &&
                strncmp(node->left->type_name, "int", 3) != 0 &&
                strncmp(node->left->type_name, "float", 5) != 0 &&
                strncmp(node->left->type_name, "bool", 4) != 0 &&
                strncmp(node->left->type_name, "char", 4) != 0);
            if (is_class_type) {
                fprintf(out, "(((");
                generate_expression(node->left);
                fprintf(out, ") != NULL) ? (");
                generate_expression(node->left);
                fprintf(out, ") : (");
                generate_expression(node->right);
                fprintf(out, "))");
            } else if (is_str_type) {
                fprintf(out, "(((");
                generate_expression(node->left);
                fprintf(out, ").data != NULL) ? (");
                generate_expression(node->left);
                fprintf(out, ") : (");
                generate_expression(node->right);
                fprintf(out, "))");
            } else {
                generate_expression(node->left);
            }
            break;
        }
        case AST_TERNARY: {
            fprintf(out, "(");
            generate_expression(node->condition);
            fprintf(out, ") ? (");
            generate_expression(node->left);
            fprintf(out, ") : (");
            generate_expression(node->right);
            fprintf(out, ")");
            break;
        }
        case AST_SAFE_ACCESS: {
            const char* zero_val = "NULL";
            bool is_str = (node->type_name && strncmp(node->type_name, "str", 3) == 0 && node->type_name[3] == '\0');
            bool is_int = (node->type_name && strncmp(node->type_name, "int", 3) == 0 && node->type_name[3] == '\0');
            bool is_float = (node->type_name && strncmp(node->type_name, "float", 5) == 0 && node->type_name[5] == '\0');
            bool is_bool = (node->type_name && strncmp(node->type_name, "bool", 4) == 0 && node->type_name[4] == '\0');
            if (is_str) zero_val = "(CnextString){NULL, 0}";
            else if (is_int) zero_val = "0";
            else if (is_float) zero_val = "0.0f";
            else if (is_bool) zero_val = "0";

            fprintf(out, "(((");
            generate_expression(node->left);
            fprintf(out, ") != NULL) ? (");
            generate_expression(node->left);
            if (node->is_pointer_access || (node->left && node->left->is_class) ||
                (node->left && node->left->is_pointer) ||
                (node->left && node->left->type == AST_IDENTIFIER &&
                node->left->token.length == 4 &&
                strncmp(node->left->token.start, "self", 4) == 0)) {
                fprintf(out, "->%.*s", node->token.length, node->token.start);
            } else {
                fprintf(out, ".%.*s", node->token.length, node->token.start);
            }
            fprintf(out, ") : (%s))", zero_val);
            break;
        }
        case AST_CAST:
            if (node->var_type.type == TOKEN_STR_TYPE) {
                fprintf(out, "cnext_to_string(");
                generate_expression(node->left);
                fprintf(out, ")");
            } else {
                fprintf(out, "((");
                generate_type(node->var_type, false);
                fprintf(out, ")(");
                generate_expression(node->left);
                fprintf(out, "))");
            }
            break;
        case AST_TRY_EXPR: {
            // expr? — if used as expression, unwrap; if as statement, propagate
            // Expression-level: just pass through the value (statement-level handles return)
            generate_expression(node->left);
            break;
        }
        case AST_TYPEOF: {
            const char* type_str = "unknown";
            if (node->left && node->left->type_name) {
                type_str = node->left->type_name;
            } else if (node->left) {
                switch (node->left->expr_type) {
                    case TOKEN_INT_TYPE: type_str = "int"; break;
                    case TOKEN_LONG_TYPE: type_str = "long"; break;
                    case TOKEN_FLOAT_TYPE: type_str = "float"; break;
                    case TOKEN_DOUBLE_TYPE: type_str = "double"; break;
                    case TOKEN_STR_TYPE: type_str = "str"; break;
                    case TOKEN_BOOL_TYPE: type_str = "bool"; break;
                    case TOKEN_CHAR_TYPE: type_str = "char"; break;
                    case TOKEN_BYTE_TYPE: type_str = "byte"; break;
                    case TOKEN_UINT_TYPE: type_str = "uint"; break;
                    case TOKEN_ULONG_TYPE: type_str = "ulong"; break;
                    case TOKEN_USHORT_TYPE: type_str = "ushort"; break;
                    case TOKEN_UBYTE_TYPE: type_str = "ubyte"; break;
                    default: break;
                }
            }
            fprintf(out, "(CnextString){(char*)\"%s\", %d}", type_str, (int)strlen(type_str));
            break;
        }
        case AST_LAMBDA:
            gen_lambda_expr(node);
            break;
        case AST_OWN_EXPR:
            generate_expression(node->left);
            break;
        case AST_RANGE:
        case AST_RANGE_INCLUSIVE:
            fprintf(out, "({ struct { int start; int end; } _r; ");
            fprintf(out, "_r.start = (int)(");
            generate_expression(node->left);
            fprintf(out, "); ");
            fprintf(out, "_r.end = (int)(");
            generate_expression(node->right);
            fprintf(out, "); _r; })");
            break;
        case AST_WHEN: {
            fprintf(out, "({ ");
            for (int i = 0; i < node->child_count; i++) {
                ASTNode* arm = node->children[i];
                if (arm->is_const) {
                    if (i > 0) fprintf(out, "else ");
                } else if (arm->condition) {
                    if (i > 0) fprintf(out, "else ");
                    fprintf(out, "if (");
                    generate_expression(arm->condition);
                    fprintf(out, ") ");
                }
                if (arm->left) {
                    fprintf(out, "(");
                    generate_expression(arm->left);
                    fprintf(out, ")");
                }
                if (!arm->is_const && !arm->condition) {
                    fprintf(out, " ");
                }
            }
            fprintf(out, "; })");
            break;
        }
        default:
            fprintf(stderr, "warning: unhandled expression AST node type %d at line %d\n", node->type, node->token.line);
            break;
    }
}
