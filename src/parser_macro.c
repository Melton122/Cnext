#include "parser_internal.h"

static MacroEntry* macro_table = NULL;

void register_macro(ASTNode* decl) {
    MacroEntry* entry = (MacroEntry*)malloc(sizeof(MacroEntry));
    entry->name = strndup(decl->token.start, decl->token.length);
    entry->decl = decl;
    entry->next = macro_table;
    macro_table = entry;
}

ASTNode* lookup_macro(const char* name, int length) {
    for (MacroEntry* e = macro_table; e; e = e->next) {
        if ((int)strlen(e->name) == length && strncmp(e->name, name, length) == 0) {
            return e->decl;
        }
    }
    return NULL;
}

ASTNode* deep_copy_ast(ASTNode* node) {
    if (!node) return NULL;
    ASTNode* copy = create_node(node->type, node->token);
    copy->is_const = node->is_const;
    copy->is_array = node->is_array;
    copy->is_pointer = node->is_pointer;
    copy->is_class = node->is_class;
    copy->is_pointer_access = node->is_pointer_access;
    copy->var_type = node->var_type;
    copy->return_type = node->return_type;
    copy->expr_type = node->expr_type;
    copy->is_variadic = node->is_variadic;
    copy->operator_token = node->operator_token;
    copy->is_generator = node->is_generator;
    for (int i = 0; i < node->child_count; i++) {
        add_child(copy, deep_copy_ast(node->children[i]));
    }
    copy->left = deep_copy_ast(node->left);
    copy->right = deep_copy_ast(node->right);
    copy->condition = deep_copy_ast(node->condition);
    copy->init = deep_copy_ast(node->init);
    copy->increment = deep_copy_ast(node->increment);
    return copy;
}

void substitute_params(ASTNode* node, ASTNode** params, int param_count, ASTNode** args, int arg_count) {
    if (!node) return;
    if (node->type == AST_IDENTIFIER) {
        for (int i = 0; i < param_count && i < arg_count; i++) {
            if ((int)node->token.length == (int)params[i]->token.length &&
                strncmp(node->token.start, params[i]->token.start, node->token.length) == 0) {
                ASTNode* arg_copy = deep_copy_ast(args[i]);
                node->type = arg_copy->type;
                node->token = arg_copy->token;
                node->is_const = arg_copy->is_const;
                node->is_array = arg_copy->is_array;
                node->is_pointer = arg_copy->is_pointer;
                node->is_class = arg_copy->is_class;
                node->expr_type = arg_copy->expr_type;
                for (int j = 0; j < arg_copy->child_count; j++) {
                    add_child(node, arg_copy->children[j]);
                }
                node->left = arg_copy->left;
                node->right = arg_copy->right;
                free(arg_copy);
                return;
            }
        }
    }
    for (int i = 0; i < node->child_count; i++) {
        substitute_params(node->children[i], params, param_count, args, arg_count);
    }
    substitute_params(node->left, params, param_count, args, arg_count);
    substitute_params(node->right, params, param_count, args, arg_count);
    substitute_params(node->condition, params, param_count, args, arg_count);
    substitute_params(node->init, params, param_count, args, arg_count);
    substitute_params(node->increment, params, param_count, args, arg_count);
}
