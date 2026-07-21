#include "parser_internal.h"

static MacroEntry* macro_table = NULL;
#define MAX_MACRO_DEPTH 32
static int macro_expansion_depth = 0;

bool macro_depth_ok(void) {
    return macro_expansion_depth < MAX_MACRO_DEPTH;
}

void macro_depth_push(void) {
    macro_expansion_depth++;
}

void macro_depth_pop(void) {
    if (macro_expansion_depth > 0) macro_expansion_depth--;
}

void register_macro(ASTNode* decl) {
    MacroEntry* entry = (MacroEntry*)checked_malloc(sizeof(MacroEntry));
    entry->name = checked_strndup(decl->token.start, decl->token.length);
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
    copy->type_name = node->type_name ? checked_strdup(node->type_name) : NULL;
    copy->parent_name = node->parent_name ? checked_strdup(node->parent_name) : NULL;
    copy->implements_names = node->implements_names ? checked_strdup(node->implements_names) : NULL;
    copy->attribute_name = node->attribute_name ? checked_strdup(node->attribute_name) : NULL;
    copy->named_arg_name = node->named_arg_name;
    for (int i = 0; i < node->child_count; i++) {
        add_child(copy, deep_copy_ast(node->children[i]));
    }
    for (int i = 0; i < node->type_param_count; i++) {
        add_type_param(copy, deep_copy_ast(node->type_params[i]));
    }
    for (int i = 0; i < node->type_arg_count; i++) {
        add_type_arg(copy, deep_copy_ast(node->type_args[i]));
    }
    copy->left = deep_copy_ast(node->left);
    copy->right = deep_copy_ast(node->right);
    copy->condition = deep_copy_ast(node->condition);
    copy->init = deep_copy_ast(node->init);
    copy->increment = deep_copy_ast(node->increment);
    copy->default_value = deep_copy_ast(node->default_value);
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
                node->is_pointer_access = arg_copy->is_pointer_access;
                node->is_variadic = arg_copy->is_variadic;
                node->is_generator = arg_copy->is_generator;
                node->operator_token = arg_copy->operator_token;
                node->var_type = arg_copy->var_type;
                node->return_type = arg_copy->return_type;
                node->named_arg_name = arg_copy->named_arg_name;
                for (int j = 0; j < arg_copy->child_count; j++) {
                    add_child(node, arg_copy->children[j]);
                }
                for (int j = 0; j < arg_copy->type_param_count; j++) {
                    add_type_param(node, arg_copy->type_params[j]);
                }
                for (int j = 0; j < arg_copy->type_arg_count; j++) {
                    add_type_arg(node, arg_copy->type_args[j]);
                }
                node->left = arg_copy->left;
                node->right = arg_copy->right;
                node->condition = arg_copy->condition;
                node->init = arg_copy->init;
                node->increment = arg_copy->increment;
                node->default_value = arg_copy->default_value;
                free(arg_copy->type_name);
                free(arg_copy->parent_name);
                free(arg_copy->implements_names);
                free(arg_copy->attribute_name);
                free(arg_copy->children);
                free(arg_copy->type_params);
                free(arg_copy->type_args);
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
