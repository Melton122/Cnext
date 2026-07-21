#include "codegen_internal.h"

// Generic function specialization work queue

char* mangle_generic_name(ASTNode* callee) {
    char base[2048] = {0};
    snprintf(base, sizeof(base), "%.*s", callee->token.length, callee->token.start);
    for (int i = 0; i < callee->type_arg_count; i++) {
        char arg_str[512] = {0};
        ASTNode* arg = callee->type_args[i];
        char type_name[256] = {0};
        int tlen = arg->token.length < 255 ? arg->token.length : 255;
        strncpy(type_name, arg->token.start, tlen);
        type_name[tlen] = '\0';
        Token subst_type;
        bool subst_array;
        if (find_type_subst(type_name, &subst_type, &subst_array)) {
            snprintf(arg_str, sizeof(arg_str), "_%.*s", subst_type.length, subst_type.start);
        } else {
            snprintf(arg_str, sizeof(arg_str), "_%s", type_name);
        }
        strncat(base, arg_str, sizeof(base) - strlen(base) - 1);
    }
    return checked_strdup(base);
}

bool infer_generic_type_args(ASTNode* call_node, ASTNode* func_decl) {
    if (!call_node || !func_decl || func_decl->type_param_count == 0) return false;
    if (call_node->left->type_arg_count > 0) return false;

    ASTNode* callee = call_node->left;
    bool inferred = false;

    for (int tp = 0; tp < func_decl->type_param_count; tp++) {
        ASTNode* type_param = func_decl->type_params[tp];
        char tp_name[256] = {0};
        int tp_len = type_param->token.length < 255 ? type_param->token.length : 255;
        strncpy(tp_name, type_param->token.start, tp_len);
        tp_name[tp_len] = '\0';

        for (int pi = 0; pi < func_decl->child_count; pi++) {
            ASTNode* param = func_decl->children[pi];
            if (param->var_type.type == TOKEN_IDENTIFIER &&
                param->var_type.length == tp_len &&
                strncmp(param->var_type.start, tp_name, tp_len) == 0) {
                if (pi < call_node->child_count) {
                    ASTNode* arg = call_node->children[pi];
                    Token concrete = {TOKEN_EOF, NULL, 0, 0};
                    bool is_arr = false;

                    if (arg->expr_type != TOKEN_EOF) {
                        concrete.type = arg->expr_type;
                        concrete.start = arg->token.start;
                        concrete.length = arg->token.length;
                        concrete.line = arg->token.line;
                    }
                    if (arg->type_name && arg->type_name[0] != '\0') {
                        concrete.type = TOKEN_IDENTIFIER;
                        concrete.start = arg->type_name;
                        concrete.length = (int)strlen(arg->type_name);
                        concrete.line = 0;
                    }
                    if (arg->is_array) is_arr = true;

                    if (concrete.type != TOKEN_EOF && concrete.start) {
                        ASTNode* type_arg = create_node(AST_IDENTIFIER, concrete);
                        type_arg->is_array = is_arr;
                        add_type_arg(callee, type_arg);
                        inferred = true;
                        break;
                    }
                }
            }
        }
    }
    return inferred;
}

void scan_node_for_generic_calls(ASTNode* node) {
    if (!node) return;
    if (node->type == AST_CALL && node->left && node->left->type == AST_IDENTIFIER) {
        char fname[256];
        int flen = node->left->token.length < 255 ? node->left->token.length : 255;
        strncpy(fname, node->left->token.start, flen);
        fname[flen] = '\0';
        ASTNode* fdecl = find_func_decl(program_node, fname);
        if (fdecl && fdecl->type_param_count > 0) {
            if (node->left->type_arg_count == 0) {
                infer_generic_type_args(node, fdecl);
            }
            if (node->left->type_arg_count > 0) {
                enqueue_spec_work(fdecl, node->left);
            }
        }
    }
    scan_node_for_generic_calls(node->left);
    scan_node_for_generic_calls(node->right);
    scan_node_for_generic_calls(node->condition);
    scan_node_for_generic_calls(node->init);
    scan_node_for_generic_calls(node->increment);
    for (int i = 0; i < node->child_count; i++) {
        scan_node_for_generic_calls(node->children[i]);
    }
}

void enqueue_spec_work(ASTNode* func_decl, ASTNode* callee) {
    char* mangled = mangle_generic_name(callee);
    if (has_gen_spec(mangled)) {
        free(mangled);
        return;
    }
    Token ret_type = func_decl->return_type;
    bool ret_array = false;
    if (ret_type.type == TOKEN_IDENTIFIER) {
        int tp_idx = -1;
        for (int ti = 0; ti < func_decl->type_param_count; ti++) {
            int plen = func_decl->type_params[ti]->token.length;
            if (plen == ret_type.length && strncmp(func_decl->type_params[ti]->token.start, ret_type.start, plen) == 0) {
                tp_idx = ti;
                break;
            }
        }
        if (tp_idx >= 0 && tp_idx < (callee ? callee->type_arg_count : 0)) {
            ASTNode* ta = callee->type_args[tp_idx];
            char tname[256] = {0};
            int tlen = ta->token.length < 255 ? ta->token.length : 255;
            strncpy(tname, ta->token.start, tlen);
            Token subst_t; bool subst_a;
            if (ta->token.type == TOKEN_IDENTIFIER && find_type_subst(tname, &subst_t, &subst_a)) {
                ret_type = subst_t;
                ret_array = subst_a;
            } else {
                ret_type = ta->token;
                ret_array = ta->is_array;
            }
        }
    }
    add_gen_spec(mangled, ret_type, ret_array);
    
    int acount = callee ? callee->type_arg_count : 0;
    Token* res_args = NULL;
    bool* res_is_array = NULL;
    if (acount > 0) {
        res_args = (Token*)checked_malloc(sizeof(Token) * acount);
        res_is_array = (bool*)checked_malloc(sizeof(bool) * acount);
        for (int i = 0; i < acount; i++) {
            ASTNode* ta = callee->type_args[i];
            char tname[256] = {0};
            int tlen = ta->token.length < 255 ? ta->token.length : 255;
            strncpy(tname, ta->token.start, tlen);
            tname[tlen] = '\0';
            Token subst_type; bool subst_array;
            if (ta->token.type == TOKEN_IDENTIFIER && find_type_subst(tname, &subst_type, &subst_array)) {
                res_args[i] = subst_type;
                res_is_array[i] = subst_array;
            } else {
                res_args[i] = ta->token;
                res_is_array[i] = ta->is_array;
            }
        }
    }
    
    SpecWork* sw = (SpecWork*)checked_malloc(sizeof(SpecWork));
    sw->func_decl = func_decl;
    sw->mangled_name = mangled;
    sw->resolved_args = res_args;
    sw->resolved_is_array = res_is_array;
    sw->resolved_count = acount;
    sw->next = NULL;
    if (spec_work_last) {
        spec_work_last->next = sw;
    } else {
        spec_work_queue = sw;
    }
    spec_work_last = sw;
}

void process_spec_queue(void) {
    while (spec_work_queue) {
        SpecWork* sw = spec_work_queue;
        spec_work_queue = sw->next;
        if (!spec_work_queue) spec_work_last = NULL;
        generate_generic_specialization(sw->func_decl, sw->mangled_name, sw->resolved_args, sw->resolved_is_array, sw->resolved_count);
        if (sw->resolved_args) free(sw->resolved_args);
        if (sw->resolved_is_array) free(sw->resolved_is_array);
        free(sw->mangled_name);
        free(sw);
    }
}

void free_spec_work_queue(void) {
    while (spec_work_queue) {
        SpecWork* sw = spec_work_queue;
        spec_work_queue = sw->next;
        if (sw->resolved_args) free(sw->resolved_args);
        if (sw->resolved_is_array) free(sw->resolved_is_array);
        if (sw->mangled_name) free(sw->mangled_name);
        free(sw);
    }
    spec_work_last = NULL;
}

void generate_generic_specialization(ASTNode* func_decl, const char* mangled_name, Token* resolved_args, bool* resolved_is_array, int resolved_count) {
    for (int i = 0; i < func_decl->type_param_count && i < resolved_count; i++) {
        ASTNode* tp = func_decl->type_params[i];
        push_type_subst(
            checked_strndup(tp->token.start, tp->token.length),
            resolved_args[i],
            resolved_is_array[i]
        );
    }
    
    generate_type(func_decl->return_type, false);
    fprintf(out, " %s(", mangled_name);
    for (int i = 0; i < func_decl->child_count; i++) {
        ASTNode* param = func_decl->children[i];
        generate_type(param->var_type, param->is_pointer);
        fprintf(out, " %.*s", param->token.length, param->token.start);
        if (i < func_decl->child_count - 1) fprintf(out, ", ");
    }
    fprintf(out, ") ");
    generate_block(func_decl->left);
    fprintf(out, "\n");
    
    for (int i = 0; i < func_decl->type_param_count && i < resolved_count; i++) {
        pop_type_subst();
    }
}
