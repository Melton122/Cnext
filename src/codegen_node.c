#include "codegen_internal.h"

void generate_block(ASTNode* node) {
    push_scope();
    track_source_line(node->token.line);
    fprintf(out, "{\n");
    codegen_gen_line++;
    indent_level++;
    for (int i = 0; i < node->child_count; i++) {
        write_indent();
        generate_node(node->children[i]);
    }
    pop_scope();
    indent_level--;
    write_indent();
    fprintf(out, "}\n");
    codegen_gen_line++;
}

void generate_node(ASTNode* node) {
    if (!node) return;
    track_source_line(node->token.line);
    switch (node->type) {
        case AST_CLASS_DECL:
        {
            if (node->type_param_count > 0) break;
            
            const char* previous_parent_class = current_parent_class;
            current_parent_class = node->parent_name;

            fprintf(out, "typedef struct {\n");
            indent_level++;
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_VAR_DECL) {
                    write_indent();
                    generate_type(node->children[i]->var_type, false);
                    fprintf(out, " %.*s;\n", node->children[i]->token.length, node->children[i]->token.start);
                }
            }
            indent_level--;
            fprintf(out, "} %.*s;\n\n", node->token.length, node->token.start);
            
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_CONSTRUCTOR) {
                    char* prefix = copy_token_text(node->token);
                    fprintf(out, "void %s_new(%s* self", prefix, prefix);
                    for (int j = 0; j < node->children[i]->child_count; j++) {
                        fprintf(out, ", ");
                        generate_type(node->children[i]->children[j]->var_type, false);
                        fprintf(out, " %.*s", node->children[i]->children[j]->token.length,
                            node->children[i]->children[j]->token.start);
                    }
                    fprintf(out, ") ");
                    generate_block(node->children[i]->left);
                    fprintf(out, "\n");
                    free(prefix);
                }
            }
            
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_FUNC_DECL) {
                    char* prefix = copy_token_text(node->token);
                    if (node->children[i]->child_count > 0 && node->children[i]->children[0]) {
                        Token saved_self_type = node->children[i]->children[0]->var_type;
                        node->children[i]->children[0]->var_type = node->token;
                        generate_function(node->children[i], prefix);
                        node->children[i]->children[0]->var_type = saved_self_type;
                    } else {
                        generate_function(node->children[i], prefix);
                    }
                    free(prefix);
                }
            }
            
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_OPERATOR_DECL) {
                    char* prefix = copy_token_text(node->token);
                    char op_name[64];
                    if (node->children[i]->operator_token.type == TOKEN_PLUS) snprintf(op_name, sizeof(op_name), "_plus");
                    else if (node->children[i]->operator_token.type == TOKEN_MINUS) snprintf(op_name, sizeof(op_name), "_minus");
                    else if (node->children[i]->operator_token.type == TOKEN_STAR) snprintf(op_name, sizeof(op_name), "_star");
                    else if (node->children[i]->operator_token.type == TOKEN_SLASH) snprintf(op_name, sizeof(op_name), "_slash");
                    else if (node->children[i]->operator_token.type == TOKEN_EQ_EQ) snprintf(op_name, sizeof(op_name), "_eq");
                    else if (node->children[i]->operator_token.type == TOKEN_BANG_EQ) snprintf(op_name, sizeof(op_name), "_neq");
                    else if (node->children[i]->operator_token.type == TOKEN_LESS) snprintf(op_name, sizeof(op_name), "_lt");
                    else if (node->children[i]->operator_token.type == TOKEN_LESS_EQ) snprintf(op_name, sizeof(op_name), "_lte");
                    else if (node->children[i]->operator_token.type == TOKEN_GREATER) snprintf(op_name, sizeof(op_name), "_gt");
                    else if (node->children[i]->operator_token.type == TOKEN_GREATER_EQ) snprintf(op_name, sizeof(op_name), "_gte");
                    else if (node->children[i]->operator_token.type == TOKEN_LBRACKET) snprintf(op_name, sizeof(op_name), "_idx");
                    
                    bool return_is_class = (node->children[i]->return_type.type == TOKEN_IDENTIFIER);
                    generate_type(node->children[i]->return_type, return_is_class);
                    fprintf(out, " %s_operator%s(", prefix, op_name);
                    for (int j = 0; j < node->children[i]->child_count; j++) {
                        if (j > 0) fprintf(out, ", ");
                        ASTNode* param = node->children[i]->children[j];
                        if (j == 0) {
                            fprintf(out, "%s* self", prefix);
                        } else {
                            bool param_is_class = (param->var_type.type == TOKEN_IDENTIFIER);
                            generate_type(param->var_type, param_is_class);
                            fprintf(out, " %.*s", param->token.length, param->token.start);
                        }
                    }
                    fprintf(out, ") ");
                    generate_block(node->children[i]->left);
                    fprintf(out, "\n");
                    free(prefix);
                }
            }
            current_parent_class = previous_parent_class;
            break;
        }
        case AST_FUNC_DECL:
            if (node->type_param_count == 0) {
                generate_function(node, NULL);
            }
            break;
        case AST_COROUTINE_DECL:
            if (node->type_param_count == 0) {
                generate_function(node, NULL);
            }
            break;
        case AST_ASYNC_FUNC_DECL:
            if (node->type_param_count == 0) {
                generate_function(node, NULL);
            }
            break;
        case AST_RESUME_EXPR: {
            ASTNode* co_expr = node->left;
            if (node->right) {
                write_indent();
                fprintf(out, "/* resume with value */\n");
            }
            fprintf(out, "({ ");
            fprintf(out, "__auto_type _co = ");
            generate_expression(co_expr);
            fprintf(out, "; ");
            if (node->right) {
                fprintf(out, "_co->_received = ");
                generate_expression(node->right);
                fprintf(out, "; ");
            }
            fprintf(out, "_co; })");
            break;
        }
        case AST_AWAIT_EXPR: {
            generate_expression(node->left);
            break;
        }
        case AST_RUN_ASYNC: {
            if (node->left) {
                generate_expression(node->left);
                fprintf(out, ";\n");
            }
            break;
        }
        case AST_STRUCT_DECL:
            fprintf(out, "typedef struct {\n");
            indent_level++;
            for (int i = 0; i < node->child_count; i++) {
                write_indent();
                generate_type(node->children[i]->var_type, false);
                fprintf(out, " %.*s;\n", node->children[i]->token.length, node->children[i]->token.start);
            }
            indent_level--;
            fprintf(out, "} %.*s;\n", node->token.length, node->token.start);
            break;
        case AST_ENUM_DECL:
            fprintf(out, "typedef enum {\n");
            indent_level++;
            for (int i = 0; i < node->child_count; i++) {
                write_indent();
                fprintf(out, "%.*s", node->children[i]->token.length, node->children[i]->token.start);
                if (i < node->child_count - 1) fprintf(out, ",");
                fprintf(out, "\n");
            }
            indent_level--;
            fprintf(out, "} %.*s;\n", node->token.length, node->token.start);
            break;
        case AST_IMPORT: {
            static const char* std_modules[] = {"io", "file", "net", "json", "math", "os", "string_utils", "time", "regex", "collections", "crypto", "path", "encoding", "process", "random", "thread", NULL};
            bool is_std = false;
            for (int i = 0; std_modules[i]; i++) {
                int len = (int)strlen(std_modules[i]);
                if (node->token.length == len &&
                    strncmp(node->token.start, std_modules[i], len) == 0) {
                    is_std = true;
                    break;
                }
            }
            if (is_std) {
                static const char* renamed[] = {
                    "io", "cnext_io.h",
                    "time", "cnext_time.h",
                    "process", "cnext_process.h",
                    NULL
                };
                char header_name[64];
                bool found_renamed = false;
                for (int j = 0; renamed[j]; j += 2) {
                    int nlen = (int)strlen(renamed[j]);
                    if (node->token.length == nlen &&
                        strncmp(node->token.start, renamed[j], nlen) == 0) {
                        snprintf(header_name, sizeof(header_name), "%s", renamed[j + 1]);
                        found_renamed = true;
                        break;
                    }
                }
                if (!found_renamed) {
                    snprintf(header_name, sizeof(header_name), "%.*s.h",
                        node->token.length, node->token.start);
                }
                fprintf(out, "#include \"%s\"\n", header_name);
            }
            break;
        }
        case AST_VAR_DECL:
            if (node->is_const) fprintf(out, "const ");
            if (node->is_array) {
                fprintf(out, "struct { ");
                generate_type(node->var_type, false);
                fprintf(out, "* data; size_t length; } %.*s", node->token.length, node->token.start);
                if (node->init && node->init->type == AST_LITERAL_ARRAY) {
                    fprintf(out, " = { (");
                    generate_type(node->var_type, false);
                    fprintf(out, "[])");
                    generate_expression(node->init);
                    fprintf(out, ", %d }", node->init->child_count);
                } else if (node->init && node->init->type == AST_SLICE) {
                    fprintf(out, ";\n");
                    write_indent();
                    fprintf(out, "CNEXT_SLICE_TO(%.*s, ", node->token.length, node->token.start);
                    generate_expression(node->init->left);
                    fprintf(out, ", ");
                    if (node->init->init) {
                        generate_expression(node->init->init);
                    } else {
                        fprintf(out, "0");
                    }
                    fprintf(out, ", ");
                    if (node->init->right) {
                        generate_expression(node->init->right);
                    } else {
                        generate_expression(node->init->left);
                        fprintf(out, ".length");
                    }
                    fprintf(out, ")");
                }
                fprintf(out, ";\n");
            } else {
                if (node->is_class && node->init && node->init->type == AST_NEW_EXPR && node->init->type_arg_count > 0) {
                    char mangled_buf[512] = {0};
                    int blen = node->init->token.length < 200 ? node->init->token.length : 200;
                    strncpy(mangled_buf, node->init->token.start, blen);
                    mangled_buf[blen] = '\0';
                    for (int i = 0; i < node->init->type_arg_count; i++) {
                        char arg_str[256] = {0};
                        ASTNode* arg = node->init->type_args[i];
                        snprintf(arg_str, sizeof(arg_str), "_%.*s", arg->token.length, arg->token.start);
                        strncat(mangled_buf, arg_str, sizeof(mangled_buf) - strlen(mangled_buf) - 1);
                    }
                    fprintf(out, "%s* %.*s", mangled_buf, node->token.length, node->token.start);
                } else {
                    generate_type(node->var_type, node->is_pointer || node->is_class);
                    fprintf(out, " %.*s", node->token.length, node->token.start);
                }
                if (node->init) {
                    fprintf(out, " = ");
                    generate_expression(node->init);
                }
                fprintf(out, ";\n");
            }
            if (node->var_type.type == TOKEN_IDENTIFIER && node->var_type.length > 0 &&
                node->var_type.start[0] >= 'A' && node->var_type.start[0] <= 'Z') {
                track_class_var(node->token.start, node->token.length,
                                node->var_type.start, node->var_type.length);
            }
            if (node->init && node->init->type == AST_LAMBDA) {
                CapturedVar* caps = NULL;
                detect_captures_walk(node->init->left, &caps, node->init, program_node);
                int cap_count = 0;
                for (CapturedVar* cv = caps; cv; cv = cv->next) cap_count++;
                if (cap_count > 0) {
                    ClosureVar* cv = (ClosureVar*)malloc(sizeof(ClosureVar));
                    cv->var_name = strndup(node->token.start, node->token.length);
                    cv->closure_id = closure_counter - 1;
                    cv->lambda_node = node->init;
                    cv->next = closure_vars;
                    closure_vars = cv;
                    register_scope_closure(node->token.start, node->token.length);
                }
                free_captures(caps);
            }
            break;
        case AST_CONSTEXPR_DECL:
            fprintf(out, "static const ");
            generate_type(node->var_type, false);
            fprintf(out, " %.*s", node->token.length, node->token.start);
            if (node->left) {
                fprintf(out, " = ");
                generate_expression(node->left);
            }
            fprintf(out, ";\n");
            break;
        case AST_ATTRIBUTE:
            fprintf(out, "/* @%.*s */\n", node->token.length, node->token.start);
            break;
        case AST_MACRO_DECL:
            break;
        case AST_EXTERN_DECL:
            for (int i = 0; i < node->child_count; i++) {
                ASTNode* func = node->children[i];
                if (func->type == AST_FUNC_DECL) {
                    generate_type(func->return_type, false);
                    fprintf(out, " %.*s(", func->token.length, func->token.start);
                    for (int j = 0; j < func->child_count; j++) {
                        if (j > 0) fprintf(out, ", ");
                        generate_type(func->children[j]->var_type, func->children[j]->is_variadic);
                        fprintf(out, " %.*s", func->children[j]->token.length, func->children[j]->token.start);
                        if (func->children[j]->is_variadic) fprintf(out, "...");
                    }
                    fprintf(out, ");\n");
                }
            }
            break;
        case AST_BENCH_DECL: {
            static int bench_counter = 0;
            int bench_id = bench_counter++;
            write_indent();
            fprintf(out, "{\n");
            indent_level++;
            write_indent();
            fprintf(out, "clock_t _bench_start_%d = clock();\n", bench_id);
            if (node->left) {
                generate_block(node->left);
            }
            write_indent();
            fprintf(out, "clock_t _bench_end_%d = clock();\n", bench_id);
            write_indent();
            fprintf(out, "double _bench_ms_%d = (double)(_bench_end_%d - _bench_start_%d) * 1000.0 / CLOCKS_PER_SEC;\n",
                bench_id, bench_id, bench_id);
            write_indent();
            fprintf(out, "printf(\"bench: %%0.3f ms\\n\", _bench_ms_%d);\n", bench_id);
            indent_level--;
            write_indent();
            fprintf(out, "}\n");
            break;
        }
        case AST_MAIN:
            push_scope();
            fprintf(out, "int main(void) {\n");
            indent_level++;
            if (codegen_test_count > 0) {
                write_indent();
                fprintf(out, "int _cnext_tp = 0, _cnext_tf = 0;\n");
                for (int i = 0; i < codegen_test_count; i++) {
                    write_indent();
                    fprintf(out, "if (_cnext_test_%d()) { _cnext_tp++; } else { _cnext_tf++; }\n", i);
                }
                write_indent();
                fprintf(out, "printf(\"Tests: %%d passed, %%d failed\\n\", _cnext_tp, _cnext_tf);\n");
                write_indent();
                fprintf(out, "if (_cnext_tf > 0) { _cnext_free_all(); return 1; }\n");
            }
            for (int i = 0; i < node->left->child_count; i++) {
                write_indent();
                generate_node(node->left->children[i]);
            }
            pop_scope();
            write_indent();
            fprintf(out, "_cnext_free_all();\n");
            if (codegen_profile_mode) {
                write_indent();
                fprintf(out, "_cnext_profile_report();\n");
            }
            write_indent();
            fprintf(out, "return 0;\n");
            indent_level--;
            fprintf(out, "}\n\n");
            break;
        case AST_EXPR_STMT:
            generate_expression(node->left);
            fprintf(out, ";\n");
            break;
        case AST_ASSIGN:
            if (node->right && node->right->type == AST_SLICE) {
                fprintf(out, "CNEXT_SLICE_TO(");
                generate_expression(node->left);
                fprintf(out, ", ");
                generate_expression(node->right->left);
                fprintf(out, ", ");
                if (node->right->init) {
                    generate_expression(node->right->init);
                } else {
                    fprintf(out, "0");
                }
                fprintf(out, ", ");
                if (node->right->right) {
                    generate_expression(node->right->right);
                } else {
                    generate_expression(node->right->left);
                    fprintf(out, ".length");
                }
                fprintf(out, ")");
            } else {
                generate_expression(node->left);
                fprintf(out, " %.*s ", node->token.length, node->token.start);
                generate_expression(node->right);
            }
            fprintf(out, ";\n");
            break;
        case AST_IF:
            fprintf(out, "if (");
            generate_expression(node->condition);
            fprintf(out, ") ");
            generate_block(node->left);
            if (node->right) {
                write_indent();
                fprintf(out, "else ");
                if (node->right->type == AST_IF) generate_node(node->right);
                else generate_block(node->right);
            }
            break;
        case AST_WHILE:
            fprintf(out, "while (");
            generate_expression(node->condition);
            fprintf(out, ") ");
            generate_block(node->left);
            break;
        case AST_FOR:
            fprintf(out, "for (");
            if (node->init->type == AST_VAR_DECL) {
                generate_type(node->init->var_type, false);
                fprintf(out, " %.*s", node->init->token.length, node->init->token.start);
                if (node->init->init) {
                    fprintf(out, " = ");
                    generate_expression(node->init->init);
                }
            } else if (node->init->type == AST_ASSIGN) {
                generate_expression(node->init->left);
                fprintf(out, " %.*s ", node->init->token.length, node->init->token.start);
                generate_expression(node->init->right);
            } else if (node->init->type == AST_EXPR_STMT) {
                generate_expression(node->init->left);
            }
            fprintf(out, "; ");
            generate_expression(node->condition);
            fprintf(out, "; ");
            if (node->increment->type == AST_ASSIGN) {
                generate_expression(node->increment->left);
                fprintf(out, " %.*s ", node->increment->token.length, node->increment->token.start);
                generate_expression(node->increment->right);
            } else if (node->increment->type == AST_EXPR_STMT) {
                generate_expression(node->increment->left);
            } else {
                generate_expression(node->increment);
            }
            fprintf(out, ") ");
            generate_block(node->left);
            break;
        case AST_FOR_IN: {
            static int loop_counter = 0;
            int current_loop = loop_counter++;
            
            bool is_generator_call = false;
            char gen_func_name[256] = {0};
            if (node->condition->type == AST_CALL && node->condition->left &&
                node->condition->left->type == AST_IDENTIFIER) {
                char call_name[256];
                int clen = node->condition->left->token.length < 255 ? 
                           node->condition->left->token.length : 255;
                memcpy(call_name, node->condition->left->token.start, clen);
                call_name[clen] = '\0';
                ASTNode* fdecl = find_func_decl(program_node, call_name);
                if (fdecl && fdecl->is_generator) {
                    is_generator_call = true;
                    strncpy(gen_func_name, call_name, sizeof(gen_func_name) - 1);
                }
            }
            
            if (is_generator_call) {
                fprintf(out, "{\n");
                indent_level++;
                write_indent();
                fprintf(out, "_gen_%s_frame* _iter%d = ", 
                        gen_func_name, current_loop);
                generate_expression(node->condition);
                fprintf(out, ";\n");
                write_indent();
                fprintf(out, "while (_gen_%s_next(_iter%d)) {\n", gen_func_name, current_loop);
                indent_level++;
                write_indent();
                fprintf(out, "__auto_type %.*s = _iter%d->current;\n",
                        node->init->token.length, node->init->token.start, current_loop);
                
                for (int i = 0; i < node->left->child_count; i++) {
                    write_indent();
                    generate_node(node->left->children[i]);
                }
                indent_level--;
                write_indent();
                fprintf(out, "}\n");
                indent_level--;
                write_indent();
                fprintf(out, "}\n");
            } else {
                fprintf(out, "for (size_t _i%d = 0; _i%d < (", current_loop, current_loop);
                generate_expression(node->condition);
                fprintf(out, ").length; _i%d++) {\n", current_loop);
                indent_level++;
                write_indent();
                fprintf(out, "__auto_type %.*s = (", node->init->token.length, node->init->token.start);
                generate_expression(node->condition);
                fprintf(out, ").data[_i%d];\n", current_loop);
                
                for (int i = 0; i < node->left->child_count; i++) {
                    write_indent();
                    generate_node(node->left->children[i]);
                }
                indent_level--;
                write_indent();
                fprintf(out, "}\n");
            }
            break;
        }
        case AST_SWITCH:
            fprintf(out, "switch (");
            generate_expression(node->condition);
            fprintf(out, ") {\n");
            indent_level++;
            for (int i = 0; i < node->child_count; i++) {
                generate_node(node->children[i]);
            }
            indent_level--;
            write_indent();
            fprintf(out, "}\n");
            break;
        case AST_CASE:
            write_indent();
            fprintf(out, "case ");
            generate_expression(node->condition);
            fprintf(out, ":\n");
            indent_level++;
            for (int i = 0; i < node->left->child_count; i++) {
                write_indent();
                generate_node(node->left->children[i]);
            }
            write_indent();
            fprintf(out, "break;\n");
            indent_level--;
            break;
        case AST_DEFAULT:
            write_indent();
            fprintf(out, "default:\n");
            indent_level++;
            for (int i = 0; i < node->left->child_count; i++) {
                write_indent();
                generate_node(node->left->children[i]);
            }
            write_indent();
            fprintf(out, "break;\n");
            indent_level--;
            break;
        case AST_RETURN:
            fprintf(out, "return");
            if (node->left) {
                fprintf(out, " ");
                generate_expression(node->left);
            }
            fprintf(out, ";\n");
            break;
        case AST_BREAK:
            fprintf(out, "break;\n");
            break;
        case AST_CONTINUE:
            fprintf(out, "continue;\n");
            break;
        case AST_TRY: {
            static int try_counter = 0;
            int current_try = try_counter++;
            fprintf(out, "{\n");
            indent_level++;
            write_indent();
            fprintf(out, "_cnext_push_try();\n");
            write_indent();
            fprintf(out, "if (setjmp(_cnext_try_stack[_cnext_try_depth - 1].buf) == 0) ");
            generate_block(node->left);
            if (node->right) {
                write_indent();
                fprintf(out, "else ");
                fprintf(out, "{\n");
                indent_level++;
                if (node->right->token.start && node->right->token.length > 0) {
                    write_indent();
                    fprintf(out, "CnextString %.*s = _cnext_error_message;\n",
                        node->right->token.length, node->right->token.start);
                }
                if (node->right->left) {
                    for (int i = 0; i < node->right->left->child_count; i++) {
                        write_indent();
                        generate_node(node->right->left->children[i]);
                    }
                }
                indent_level--;
                write_indent();
                fprintf(out, "}\n");
            }
            write_indent();
            fprintf(out, "_cnext_pop_try();\n");
            if (node->condition) {
                for (int i = 0; i < node->condition->child_count; i++) {
                    write_indent();
                    generate_node(node->condition->children[i]);
                }
            }
            indent_level--;
            write_indent();
            fprintf(out, "}\n");
            (void)current_try;
            break;
        }
        case AST_THROW:
            fprintf(out, "cnext_throw(");
            generate_expression(node->left);
            fprintf(out, ");\n");
            break;
        case AST_MATCH: {
            static int match_counter = 0;
            int current_match = match_counter++;
            fprintf(out, "{\n");
            indent_level++;
            write_indent();
            fprintf(out, "__auto_type _match_%d = ", current_match);
            generate_expression(node->condition);
            fprintf(out, ";\n");
            bool first = true;
            for (int i = 0; i < node->child_count; i++) {
                ASTNode* arm = node->children[i];
                write_indent();
                if (arm->is_const) {
                    if (!first) {
                        fprintf(out, "else ");
                    }
                } else if (arm->child_count > 0) {
                    if (!first) {
                        fprintf(out, "else ");
                    }
                    fprintf(out, "if (");
                    for (int j = 0; j < arm->child_count; j++) {
                        if (j > 0) fprintf(out, " || ");
                        fprintf(out, "_match_%d == ", current_match);
                        generate_expression(arm->children[j]);
                    }
                    if (arm->condition) {
                        fprintf(out, " && (");
                        generate_expression(arm->condition);
                        fprintf(out, ")");
                    }
                    fprintf(out, ") ");
                }
                generate_block(arm->left);
                first = false;
            }
            indent_level--;
            write_indent();
            fprintf(out, "}\n");
            break;
        }
        case AST_ASSERT:
            fprintf(out, "if (!(");
            generate_expression(node->left);
            fprintf(out, ")) {\n");
            indent_level++;
            write_indent();
            if (codegen_in_test) {
                fprintf(out, "_cnext_test_ok = false;\n");
            }
            write_indent();
            fprintf(out, "fprintf(stderr, \"  FAIL [line %%d]\\n\", %d);\n", node->token.line);
            if (!codegen_in_test) {
                write_indent();
                fprintf(out, "_cnext_free_all();\n");
                write_indent();
                fprintf(out, "exit(1);\n");
            }
            indent_level--;
            write_indent();
            fprintf(out, "}\n");
            break;
        case AST_TRAIT_DECL:
            break;
        case AST_INTERFACE_DECL:
            break;
        case AST_OPERATOR_DECL: {
            char op_name[64];
            snprintf(op_name, sizeof(op_name), "_%.*s", node->operator_token.length, node->operator_token.start);
            if (node->operator_token.type == TOKEN_PLUS) snprintf(op_name, sizeof(op_name), "_plus");
            else if (node->operator_token.type == TOKEN_MINUS) snprintf(op_name, sizeof(op_name), "_minus");
            else if (node->operator_token.type == TOKEN_STAR) snprintf(op_name, sizeof(op_name), "_star");
            else if (node->operator_token.type == TOKEN_SLASH) snprintf(op_name, sizeof(op_name), "_slash");
            else if (node->operator_token.type == TOKEN_EQ_EQ) snprintf(op_name, sizeof(op_name), "_eq");
            else if (node->operator_token.type == TOKEN_BANG_EQ) snprintf(op_name, sizeof(op_name), "_neq");
            else if (node->operator_token.type == TOKEN_LESS) snprintf(op_name, sizeof(op_name), "_lt");
            else if (node->operator_token.type == TOKEN_LESS_EQ) snprintf(op_name, sizeof(op_name), "_lte");
            else if (node->operator_token.type == TOKEN_GREATER) snprintf(op_name, sizeof(op_name), "_gt");
            else if (node->operator_token.type == TOKEN_GREATER_EQ) snprintf(op_name, sizeof(op_name), "_gte");
            else if (node->operator_token.type == TOKEN_LBRACKET) snprintf(op_name, sizeof(op_name), "_idx");
            
            generate_type(node->return_type, false);
            fprintf(out, " operator%s(", op_name);
            for (int i = 0; i < node->child_count; i++) {
                generate_type(node->children[i]->var_type, node->children[i]->is_pointer);
                fprintf(out, " %.*s", node->children[i]->token.length, node->children[i]->token.start);
                if (i < node->child_count - 1) fprintf(out, ", ");
            }
            fprintf(out, ") ");
            generate_block(node->left);
            fprintf(out, "\n");
            break;
        }
        case AST_EXTEND_DECL: {
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_FUNC_DECL) {
                    char* prefix = copy_token_text(node->token);
                    generate_function(node->children[i], prefix);
                    free(prefix);
                }
            }
            break;
        }
        case AST_DESTRUCTURE: {
            write_indent();
            fprintf(out, "auto _dstr = ");
            generate_expression(node->right);
            fprintf(out, ";\n");
            for (int i = 0; i < node->child_count; i++) {
                write_indent();
                generate_type(node->children[i]->var_type, false);
                fprintf(out, " %.*s = ", node->children[i]->token.length, node->children[i]->token.start);
                fprintf(out, "_dstr.e%d;\n", i);
            }
            break;
        }
        default:
            break;
    }
}
