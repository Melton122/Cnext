#include "ast.h"
#include "checked_alloc.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static ASTNode* closure_lambda_at(ASTNode* value, int depth) {
    for (; value && depth < 64; depth++) {
        if (value->type == AST_LAMBDA) return value;
        if (value->closure_signature) return value->closure_signature;
        if (value->type == AST_IDENTIFIER) value = value->binding;
        else if (value->type == AST_CALL && value->left) {
            ASTNode* callee = value->left->binding;
            if (callee && callee->type == AST_FUNC_DECL) return callee->closure_signature;
            ASTNode* lambda = closure_lambda_at(value->left, depth + 1);
            return lambda ? lambda->closure_signature : NULL;
        }
        else value = value->init;
    }
    return NULL;
}

ASTNode* ast_closure_lambda(ASTNode* value) { return closure_lambda_at(value, 0); }

ASTNode* ast_return_value(ASTNode* body) {
    if (!body || body->type == AST_LAMBDA || body->type == AST_FUNC_DECL) return NULL;
    if (body->type == AST_RETURN) return body->left;
    for (int i = 0; i < body->child_count; i++) {
        ASTNode* found = ast_return_value(body->children[i]);
        if (found) return found;
    }
    ASTNode* found = ast_return_value(body->left);
    return found ? found : ast_return_value(body->right);
}

ASTNode* ast_returned_closure(ASTNode* body) {
    return ast_closure_lambda(ast_return_value(body));
}

ASTNode* create_node(ASTNodeType type, Token token) {
    ASTNode* node = (ASTNode*)checked_calloc(1, sizeof(ASTNode));
    node->type = type;
    node->token = token;
    node->var_type.type = TOKEN_EOF;
    node->return_type.type = TOKEN_EOF;
    node->upcast_type.type = TOKEN_EOF;
    node->expr_type = TOKEN_EOF;
    node->type_params = NULL;
    node->type_param_count = 0;
    node->type_param_capacity = 0;
    node->type_args = NULL;
    node->type_arg_count = 0;
    node->type_arg_capacity = 0;
    node->default_value = NULL;
    node->named_arg_name.type = TOKEN_EOF;
    node->named_arg_name.start = NULL;
    node->named_arg_name.length = 0;
    node->named_arg_name.line = 0;
    node->is_variadic = false;
    node->operator_token.type = TOKEN_EOF;
    node->operator_token.start = NULL;
    node->operator_token.length = 0;
    node->operator_token.line = 0;
    return node;
}

void add_child(ASTNode* parent, ASTNode* child) {
    if (!parent || !child) return;
    if (parent->child_capacity == 0) {
        parent->child_capacity = 8;
        parent->children = (ASTNode**)checked_malloc(sizeof(ASTNode*) * parent->child_capacity);
    } else if (parent->child_count >= parent->child_capacity) {
        parent->child_capacity *= 2;
        parent->children = (ASTNode**)checked_realloc(parent->children, sizeof(ASTNode*) * parent->child_capacity);
    }
    parent->children[parent->child_count++] = child;
}

void add_type_param(ASTNode* parent, ASTNode* param) {
    if (!parent || !param) return;
    if (parent->type_param_capacity == 0) {
        parent->type_param_capacity = 4;
        parent->type_params = (ASTNode**)checked_malloc(sizeof(ASTNode*) * parent->type_param_capacity);
    } else if (parent->type_param_count >= parent->type_param_capacity) {
        parent->type_param_capacity *= 2;
        parent->type_params = (ASTNode**)checked_realloc(parent->type_params, sizeof(ASTNode*) * parent->type_param_capacity);
    }
    parent->type_params[parent->type_param_count++] = param;
}

void add_type_arg(ASTNode* parent, ASTNode* arg) {
    if (!parent || !arg) return;
    if (parent->type_arg_capacity == 0) {
        parent->type_arg_capacity = 4;
        parent->type_args = (ASTNode**)checked_malloc(sizeof(ASTNode*) * parent->type_arg_capacity);
    } else if (parent->type_arg_count >= parent->type_arg_capacity) {
        parent->type_arg_capacity *= 2;
        parent->type_args = (ASTNode**)checked_realloc(parent->type_args, sizeof(ASTNode*) * parent->type_arg_capacity);
    }
    parent->type_args[parent->type_arg_count++] = arg;
}

static void free_ast_internal(ASTNode* node) {
    if (!node) return;

    for (int i = 0; i < node->child_count; i++) {
        free_ast_internal(node->children[i]);
    }
    free(node->children);
    for (int i = 0; i < node->type_param_count; i++) {
        free_ast_internal(node->type_params[i]);
    }
    free(node->type_params);
    for (int i = 0; i < node->type_arg_count; i++) {
        free_ast_internal(node->type_args[i]);
    }
    free(node->type_args);
    free_ast_internal(node->left);
    free_ast_internal(node->right);
    free_ast_internal(node->condition);
    free_ast_internal(node->init);
    free_ast_internal(node->increment);
    free_ast_internal(node->default_value);
    free(node->type_name);
    free(node->parent_name);
    free(node->implements_names);
    free(node->attribute_name);
    free(node->owned_token_text);
    free(node);
}

void free_ast(ASTNode* node) {
    free_ast_internal(node);
}
