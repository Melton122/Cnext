#include "codegen_internal.h"

// Generic class specialization

void enqueue_class_spec_work(ASTNode* class_decl, ASTNode* new_node) {
    char base[512] = {0};
    int blen = class_decl->token.length < 200 ? class_decl->token.length : 200;
    strncpy(base, class_decl->token.start, blen);
    base[blen] = '\0';
    for (int i = 0; i < new_node->type_arg_count; i++) {
        char arg_str[256] = {0};
        ASTNode* arg = new_node->type_args[i];
        snprintf(arg_str, sizeof(arg_str), "_%.*s", arg->token.length, arg->token.start);
        strncat(base, arg_str, sizeof(base) - strlen(base) - 1);
    }
    char* mangled = strdup(base);

    for (ClassSpecWork* cs = class_spec_queue; cs; cs = cs->next) {
        if (strcmp(cs->mangled_name, mangled) == 0) { free(mangled); return; }
    }

    int acount = new_node->type_arg_count;
    Token* res_args = NULL;
    bool* res_is_array = NULL;
    if (acount > 0) {
        res_args = (Token*)malloc(sizeof(Token) * acount);
        res_is_array = (bool*)malloc(sizeof(bool) * acount);
        for (int i = 0; i < acount; i++) {
            ASTNode* ta = new_node->type_args[i];
            res_args[i] = ta->token;
            res_is_array[i] = ta->is_array;
        }
    }

    ClassSpecWork* cs = (ClassSpecWork*)malloc(sizeof(ClassSpecWork));
    cs->class_decl = class_decl;
    cs->mangled_name = mangled;
    cs->resolved_args = res_args;
    cs->resolved_is_array = res_is_array;
    cs->resolved_count = acount;
    cs->next = NULL;
    if (class_spec_last) class_spec_last->next = cs;
    else class_spec_queue = cs;
    class_spec_last = cs;
}

void generate_generic_class_specialization(ASTNode* class_decl, const char* mangled_name,
    Token* resolved_args, bool* resolved_is_array, int resolved_count)
{
    for (int i = 0; i < class_decl->type_param_count && i < resolved_count; i++) {
        char tp_name[256] = {0};
        int tp_len = class_decl->type_params[i]->token.length < 255 ? class_decl->type_params[i]->token.length : 255;
        strncpy(tp_name, class_decl->type_params[i]->token.start, tp_len);
        tp_name[tp_len] = '\0';
        push_type_subst(tp_name, resolved_args[i], resolved_is_array[i]);
    }

    fprintf(out, "typedef struct %s {\n", mangled_name);
    indent_level++;
    for (int i = 0; i < class_decl->child_count; i++) {
        if (class_decl->children[i]->type == AST_VAR_DECL) {
            write_indent();
            generate_type(class_decl->children[i]->var_type, false);
            fprintf(out, " %.*s;\n", class_decl->children[i]->token.length, class_decl->children[i]->token.start);
        }
    }
    indent_level--;
    fprintf(out, "} %s;\n\n", mangled_name);

    for (int i = 0; i < class_decl->child_count; i++) {
        if (class_decl->children[i]->type == AST_CONSTRUCTOR) {
            fprintf(out, "void %s_new(%s* self", mangled_name, mangled_name);
            for (int j = 0; j < class_decl->children[i]->child_count; j++) {
                fprintf(out, ", ");
                generate_type(class_decl->children[i]->children[j]->var_type, false);
                fprintf(out, " %.*s", class_decl->children[i]->children[j]->token.length,
                    class_decl->children[i]->children[j]->token.start);
            }
            fprintf(out, ") ");
            generate_block(class_decl->children[i]->left);
            fprintf(out, "\n");
        }
    }

    for (int i = 0; i < class_decl->child_count; i++) {
        if (class_decl->children[i]->type == AST_FUNC_DECL) {
            ASTNode* method = class_decl->children[i];
            Token saved_self_type = {TOKEN_EOF, NULL, 0, 0};
            if (method->child_count > 0 && method->children[0]) {
                saved_self_type = method->children[0]->var_type;
                Token self_tok = {TOKEN_IDENTIFIER, mangled_name, (int)strlen(mangled_name), 0};
                method->children[0]->var_type = self_tok;
            }
            generate_function(method, mangled_name);
            if (method->child_count > 0 && method->children[0]) {
                method->children[0]->var_type = saved_self_type;
            }
        }
    }

    for (int i = 0; i < class_decl->child_count; i++) {
        if (class_decl->children[i]->type == AST_OPERATOR_DECL) {
            ASTNode* op = class_decl->children[i];
            char op_name[64];
            if (op->operator_token.type == TOKEN_PLUS) snprintf(op_name, sizeof(op_name), "_plus");
            else if (op->operator_token.type == TOKEN_MINUS) snprintf(op_name, sizeof(op_name), "_minus");
            else if (op->operator_token.type == TOKEN_STAR) snprintf(op_name, sizeof(op_name), "_star");
            else if (op->operator_token.type == TOKEN_SLASH) snprintf(op_name, sizeof(op_name), "_slash");
            else if (op->operator_token.type == TOKEN_EQ_EQ) snprintf(op_name, sizeof(op_name), "_eq");
            else if (op->operator_token.type == TOKEN_BANG_EQ) snprintf(op_name, sizeof(op_name), "_neq");
            else if (op->operator_token.type == TOKEN_LESS) snprintf(op_name, sizeof(op_name), "_lt");
            else if (op->operator_token.type == TOKEN_LESS_EQ) snprintf(op_name, sizeof(op_name), "_lte");
            else if (op->operator_token.type == TOKEN_GREATER) snprintf(op_name, sizeof(op_name), "_gt");
            else if (op->operator_token.type == TOKEN_GREATER_EQ) snprintf(op_name, sizeof(op_name), "_gte");
            else if (op->operator_token.type == TOKEN_LBRACKET) snprintf(op_name, sizeof(op_name), "_idx");

            bool return_is_class = (op->return_type.type == TOKEN_IDENTIFIER);
            generate_type(op->return_type, return_is_class);
            fprintf(out, " %s_operator%s(", mangled_name, op_name);
            for (int j = 0; j < op->child_count; j++) {
                if (j > 0) fprintf(out, ", ");
                if (j == 0) {
                    fprintf(out, "%s* self", mangled_name);
                } else {
                    bool param_is_class = (op->children[j]->var_type.type == TOKEN_IDENTIFIER);
                    generate_type(op->children[j]->var_type, param_is_class);
                    fprintf(out, " %.*s", op->children[j]->token.length, op->children[j]->token.start);
                }
            }
            fprintf(out, ") ");
            generate_block(op->left);
            fprintf(out, "\n");
        }
    }

    for (int i = 0; i < class_decl->type_param_count && i < resolved_count; i++) {
        pop_type_subst();
    }
}

void scan_node_for_new_exprs(ASTNode* node) {
    if (!node) return;
    if (node->type == AST_NEW_EXPR && node->type_arg_count > 0) {
        char cname[256] = {0};
        int clen = node->token.length < 255 ? node->token.length : 255;
        strncpy(cname, node->token.start, clen);
        cname[clen] = '\0';
        ASTNode* cdecl = find_class_decl(program_node, cname);
        if (cdecl && cdecl->type_param_count > 0) {
            enqueue_class_spec_work(cdecl, node);
        }
    }
    scan_node_for_new_exprs(node->left);
    scan_node_for_new_exprs(node->right);
    scan_node_for_new_exprs(node->condition);
    scan_node_for_new_exprs(node->init);
    scan_node_for_new_exprs(node->increment);
    for (int i = 0; i < node->child_count; i++) {
        scan_node_for_new_exprs(node->children[i]);
    }
}

void process_class_spec_queue(void) {
    while (class_spec_queue) {
        ClassSpecWork* cs = class_spec_queue;
        class_spec_queue = cs->next;
        if (!class_spec_queue) class_spec_last = NULL;
        generate_generic_class_specialization(cs->class_decl, cs->mangled_name,
            cs->resolved_args, cs->resolved_is_array, cs->resolved_count);
        if (cs->resolved_args) free(cs->resolved_args);
        if (cs->resolved_is_array) free(cs->resolved_is_array);
        free(cs->mangled_name);
        free(cs);
    }
}

void free_class_spec_queue(void) {
    while (class_spec_queue) {
        ClassSpecWork* cs = class_spec_queue;
        class_spec_queue = cs->next;
        if (cs->resolved_args) free(cs->resolved_args);
        if (cs->resolved_is_array) free(cs->resolved_is_array);
        free(cs->mangled_name);
        free(cs);
    }
    class_spec_last = NULL;
}
