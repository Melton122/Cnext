#include "codegen_internal.h"

static int binop_precedence(CnextTokenType op) {
    switch (op) {
        case TOKEN_STAR:
        case TOKEN_SLASH:    return 2;
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

static void gen_with_parens(ASTNode* child, CnextTokenType parent_op, bool is_right) {
    if (child && child->type == AST_BINARY && needs_parens(parent_op, child->token.type, is_right)) {
        fprintf(out, "(");
        generate_expression(child);
        fprintf(out, ")");
    } else {
        generate_expression(child);
    }
}

void generate_expression(ASTNode* node) {
    if (!node) return;
    switch (node->type) {
        case AST_LITERAL:
            if (node->token.type == TOKEN_STRING_LITERAL) {
                generate_string_interpolation(node->token.start, node->token.length);
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
        case AST_OPTION_SOME:
            fprintf(out, "({ struct { int tag; long value; } _opt; _opt.tag = 1; _opt.value = 0; _opt; })");
            break;
        case AST_OPTION_NONE:
            fprintf(out, "({ struct { int tag; long value; } _opt; _opt.tag = 0; _opt.value = 0; _opt; })");
            break;
        case AST_RESULT_OK:
            fprintf(out, "({ struct { int tag; long value; } _res; _res.tag = 0; _res.value = 0; _res; })");
            break;
        case AST_RESULT_ERR:
            fprintf(out, "({ struct { int tag; long value; } _res; _res.tag = 1; _res.value = 0; _res; })");
            break;
        case AST_AWAIT_EXPR:
            generate_expression(node->left);
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
                bool is_input = (node->left->token.length == 5 && strncmp(node->left->token.start, "input", 5) == 0);
                bool is_free = (node->left->token.length == 4 && strncmp(node->left->token.start, "free", 4) == 0);
                bool is_len = (node->left->token.length == 3 && strncmp(node->left->token.start, "len", 3) == 0);

                if (is_printin) {
                    fprintf(out, "printin(");
                    if (node->child_count > 0) generate_expression(node->children[0]);
                    fprintf(out, ")");
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
                } else {
                    if (node->left && node->left->type == AST_IDENTIFIER && node->left->type_arg_count > 0) {
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
                    } else if (node->left && node->left->type == AST_IDENTIFIER) {
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
                        } else {
                            fprintf(out, "%.*s(", node->left->token.length, node->left->token.start);
                        }
                    } else {
                        fprintf(out, "%.*s(", node->left->token.length, node->left->token.start);
                    }
                    
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
            fprintf(out, "({ %.*s* _obj = (%.*s*)malloc(sizeof(%.*s)); _cnext_track(_obj); ",
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
        case AST_LAMBDA: {
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
                
                fprintf(out, "({ struct _cl_env_%d* _ce = (struct _cl_env_%d*)malloc(sizeof(struct _cl_env_%d));", cl_id, cl_id, cl_id);
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
            break;
        }
        case AST_OWN_EXPR:
            generate_expression(node->left);
            break;
        default:
            break;
    }
}
