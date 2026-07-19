#include "ast.h"
#include "checked_alloc.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

ASTNode* create_node(ASTNodeType type, Token token) {
    ASTNode* node = (ASTNode*)checked_calloc(1, sizeof(ASTNode));
    node->type = type;
    node->token = token;
    node->var_type.type = TOKEN_EOF;
    node->return_type.type = TOKEN_EOF;
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

typedef struct FreedNode {
    ASTNode* node;
    struct FreedNode* next;
} FreedNode;

// Minimal hash set for cycle detection — O(1) amortized lookup
#define SEEN_TABLE_SIZE 1024

typedef struct {
    FreedNode* buckets[SEEN_TABLE_SIZE];
    int count;
} SeenSet;

static inline unsigned int seen_hash(ASTNode* node) {
    return (unsigned int)((uintptr_t)node >> 3) % SEEN_TABLE_SIZE;
}

static bool was_seen(SeenSet* set, ASTNode* node) {
    unsigned int idx = seen_hash(node);
    for (FreedNode* e = set->buckets[idx]; e; e = e->next) {
        if (e->node == node) return true;
    }
    return false;
}

static void remember_node(SeenSet* set, ASTNode* node) {
    unsigned int idx = seen_hash(node);
    FreedNode* entry = (FreedNode*)checked_malloc(sizeof(FreedNode));
    entry->node = node;
    entry->next = set->buckets[idx];
    set->buckets[idx] = entry;
    set->count++;
}

static void free_seen_set(SeenSet* set) {
    for (int i = 0; i < SEEN_TABLE_SIZE; i++) {
        FreedNode* e = set->buckets[i];
        while (e) {
            FreedNode* next = e->next;
            free(e);
            e = next;
        }
    }
}

static void free_ast_internal(ASTNode* node, SeenSet* set) {
    if (!node) return;
    if (was_seen(set, node)) return;
    remember_node(set, node);

    for (int i = 0; i < node->child_count; i++) {
        free_ast_internal(node->children[i], set);
    }
    free(node->children);
    for (int i = 0; i < node->type_param_count; i++) {
        free_ast_internal(node->type_params[i], set);
    }
    free(node->type_params);
    for (int i = 0; i < node->type_arg_count; i++) {
        free_ast_internal(node->type_args[i], set);
    }
    free(node->type_args);
    free_ast_internal(node->left, set);
    free_ast_internal(node->right, set);
    free_ast_internal(node->condition, set);
    free_ast_internal(node->init, set);
    free_ast_internal(node->increment, set);
    free_ast_internal(node->default_value, set);
    free(node->type_name);
    free(node->parent_name);
    free(node->implements_names);
    free(node->attribute_name);
    free(node);
}

void free_ast(ASTNode* node) {
    SeenSet set;
    memset(&set, 0, sizeof(set));
    free_ast_internal(node, &set);
    free_seen_set(&set);
}
