#include "optimizer.h"
#include "checked_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>

static int opt_count = 0;

int get_optimization_count(void) {
    return opt_count;
}

// Parse integer value from token text
static int64_t parse_int_value(Token token) {
    char buf[64];
    int len = token.length < 63 ? token.length : 63;
    memcpy(buf, token.start, len);
    buf[len] = '\0';
    return (int64_t)strtoll(buf, NULL, 10);
}

// Parse float value from token text
static double parse_float_value(Token token) {
    char buf[64];
    int len = token.length < 63 ? token.length : 63;
    memcpy(buf, token.start, len);
    buf[len] = '\0';
    return strtod(buf, NULL);
}

// Create a literal node from a computed value
static ASTNode* make_int_literal(int64_t value, Token token) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%lld", (long long)value);
    char* text = (char*)checked_malloc((size_t)len + 3);
    memcpy(text, buf, (size_t)len);
    text[len] = '\0';
    Token t = token;
    t.start = text;
    t.length = len;
    t.type = TOKEN_NUMBER;
    ASTNode* node = create_node(AST_LITERAL, t);
    node->owned_token_text = text;
    node->expr_type = TOKEN_INT_TYPE;
    return node;
}

static ASTNode* make_float_literal(double value, Token token) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%.17g", value);
    if (len < 0 || len >= (int)sizeof(buf)) len = 0;
    bool has_point = strchr(buf, '.') != NULL;
    bool has_exponent = strchr(buf, 'e') != NULL || strchr(buf, 'E') != NULL;
    if (!has_point && !has_exponent && len + 2 < (int)sizeof(buf)) {
        buf[len++] = '.';
        buf[len++] = '0';
        buf[len] = '\0';
    }
    char* text = (char*)checked_malloc((size_t)len + 1);
    memcpy(text, buf, (size_t)len);
    text[len] = '\0';
    Token t = token;
    t.start = text;
    t.length = len;
    t.type = TOKEN_FLOAT_LITERAL;
    ASTNode* node = create_node(AST_LITERAL, t);
    node->owned_token_text = text;
    node->expr_type = TOKEN_DOUBLE_TYPE;
    return node;
}

static void replace_node_token(ASTNode* node, Token token, char* owned_text) {
    if (token.start == node->owned_token_text) {
        if (owned_text && owned_text != node->owned_token_text) free(owned_text);
        node->token = token;
        return;
    }
    free(node->owned_token_text);
    node->owned_token_text = owned_text;
    node->token = token;
}

static void free_ast_owned_fields(ASTNode* node) {
    if (!node) return;
    for (int i = 0; i < node->child_count; i++) {
        free_ast(node->children[i]);
    }
    free(node->children);
    node->children = NULL;
    node->child_count = 0;
    node->child_capacity = 0;
    for (int i = 0; i < node->type_param_count; i++) {
        free_ast(node->type_params[i]);
    }
    free(node->type_params);
    node->type_params = NULL;
    node->type_param_count = 0;
    node->type_param_capacity = 0;
    for (int i = 0; i < node->type_arg_count; i++) {
        free_ast(node->type_args[i]);
    }
    free(node->type_args);
    node->type_args = NULL;
    node->type_arg_count = 0;
    node->type_arg_capacity = 0;
    free_ast(node->left);
    node->left = NULL;
    free_ast(node->right);
    node->right = NULL;
    free_ast(node->condition);
    node->condition = NULL;
    free_ast(node->init);
    node->init = NULL;
    free_ast(node->increment);
    node->increment = NULL;
    free_ast(node->default_value);
    node->default_value = NULL;
    free(node->type_name);
    node->type_name = NULL;
    free(node->parent_name);
    node->parent_name = NULL;
    free(node->implements_names);
    node->implements_names = NULL;
    free(node->attribute_name);
    node->attribute_name = NULL;
    free(node->owned_token_text);
    node->owned_token_text = NULL;
}

static void move_ast_node(ASTNode* target, ASTNode* source) {
    if (!target || !source || target == source) return;
    free_ast_owned_fields(target);
    *target = *source;
    memset(source, 0, sizeof(*source));
    free(source);
}

static ASTNode* make_owned_identifier(Token token) {
    char* text = checked_strndup(token.start, (size_t)token.length);
    Token t = token;
    t.start = text;
    ASTNode* node = create_node(AST_IDENTIFIER, t);
    node->owned_token_text = text;
    return node;
}

static Token make_owned_operator(Token source, const char* text, CnextTokenType type) {
    char* owned = checked_strdup(text);
    Token t = source;
    t.start = owned;
    t.length = (int)strlen(text);
    t.type = type;
    return t;
}


// Check if a node is a compile-time constant integer literal
static bool is_int_literal(ASTNode* node) {
    return node && node->type == AST_LITERAL && node->token.type == TOKEN_NUMBER;
}

// Check if a node is a compile-time constant float literal
static bool is_float_literal(ASTNode* node) {
    return node && node->type == AST_LITERAL && node->token.type == TOKEN_FLOAT_LITERAL;
}

// Check if a node is a literal (any numeric type)
static bool is_literal(ASTNode* node) {
    return node && node->type == AST_LITERAL &&
           (node->token.type == TOKEN_NUMBER || node->token.type == TOKEN_FLOAT_LITERAL);
}

static bool is_plain_numeric(ASTNode* node) {
    if (!node || node->type_name) return false;
    switch (node->expr_type) {
        case TOKEN_INT_TYPE:
        case TOKEN_LONG_TYPE:
        case TOKEN_FLOAT_TYPE:
        case TOKEN_DOUBLE_TYPE:
        case TOKEN_BYTE_TYPE:
        case TOKEN_UINT_TYPE:
        case TOKEN_ULONG_TYPE:
        case TOKEN_USHORT_TYPE:
        case TOKEN_UBYTE_TYPE:
            return true;
        default:
            return false;
    }
}

static bool fold_result_fits(ASTNode* node, int64_t value) {
    if (!node || node->expr_type != TOKEN_INT_TYPE) return true;
    return value >= INT32_MIN && value <= INT32_MAX;
}

// Check if two tokens represent the same identifier
static bool same_identifier(Token a, Token b) {
    return a.length == b.length && memcmp(a.start, b.start, a.length) == 0;
}

// Helper to determine if an LHS expression is an Lvalue representing `name`
static bool is_lvalue_for(ASTNode* lhs, const char* name) {
    if (!lhs) return false;
    if (lhs->type == AST_IDENTIFIER) {
        return lhs->token.length == (int)strlen(name) &&
               memcmp(lhs->token.start, name, lhs->token.length) == 0;
    }
    if (lhs->type == AST_MEMBER_ACCESS || lhs->type == AST_INDEX || lhs->type == AST_SLICE || lhs->type == AST_TUPLE_ACCESS) {
        return is_lvalue_for(lhs->left, name);
    }
    if (lhs->type == AST_TUPLE || lhs->type == AST_DESTRUCTURE) {
        for (int i = 0; i < lhs->child_count; i++) {
            if (is_lvalue_for(lhs->children[i], name)) return true;
        }
    }
    return false;
}

// Helper to check if a variable is mutated in the given AST subtree
static bool is_mutated(ASTNode* node, const char* name) {
    if (!node) return false;

    if (node->type == AST_ASSIGN && node->left) {
        if (is_lvalue_for(node->left, name)) return true;
    }
    if (node->type == AST_POSTFIX && node->left) {
        if (node->token.type == TOKEN_INCREMENT || node->token.type == TOKEN_DECREMENT) {
            if (is_lvalue_for(node->left, name)) return true;
        }
    }
    if (node->type == AST_UNARY && node->right) {
        if (node->token.type == TOKEN_INCREMENT || node->token.type == TOKEN_DECREMENT) {
            if (is_lvalue_for(node->right, name)) return true;
        }
    }

    for (int i = 0; i < node->child_count; i++) {
        if (is_mutated(node->children[i], name)) return true;
    }
    if (is_mutated(node->left, name)) return true;
    if (is_mutated(node->right, name)) return true;
    if (is_mutated(node->condition, name)) return true;
    if (is_mutated(node->init, name)) return true;
    if (is_mutated(node->increment, name)) return true;

    return false;
}

// Helper to count mutation occurrences of a variable in the given AST subtree
static int count_mutations(ASTNode* node, const char* name) {
    if (!node) return 0;
    int count = 0;

    if (node->type == AST_ASSIGN && node->left) {
        if (is_lvalue_for(node->left, name)) count++;
    }
    if (node->type == AST_POSTFIX && node->left) {
        if (node->token.type == TOKEN_INCREMENT || node->token.type == TOKEN_DECREMENT) {
            if (is_lvalue_for(node->left, name)) count++;
        }
    }
    if (node->type == AST_UNARY && node->right) {
        if (node->token.type == TOKEN_INCREMENT || node->token.type == TOKEN_DECREMENT) {
            if (is_lvalue_for(node->right, name)) count++;
        }
    }

    for (int i = 0; i < node->child_count; i++) {
        count += count_mutations(node->children[i], name);
    }
    count += count_mutations(node->left, name);
    count += count_mutations(node->right, name);
    count += count_mutations(node->condition, name);
    count += count_mutations(node->init, name);
    count += count_mutations(node->increment, name);

    return count;
}

// ========================================================================
// PASS 1: Constant Propagation
// ========================================================================

typedef struct ConstEntry {
    char* name;
    ASTNode* value;  // The literal node
    struct ConstEntry* next;
} ConstEntry;

static ConstEntry* const_table = NULL;

static void const_table_push(const char* name, ASTNode* value) {
    ConstEntry* e = (ConstEntry*)checked_malloc(sizeof(ConstEntry));
    e->name = checked_strdup(name);
    e->value = value;
    e->next = const_table;
    const_table = e;
}

static ASTNode* const_table_find(const char* name) {
    for (ConstEntry* e = const_table; e; e = e->next) {
        if (e->name && strcmp(e->name, name) == 0) return e->value;
    }
    return NULL;
}

static void const_table_pop_scope(void) {
    while (const_table && const_table->name != NULL) {
        ConstEntry* e = const_table;
        const_table = e->next;
        free(e->name);
        free(e);
    }
    if (const_table && const_table->name == NULL) {
        ConstEntry* e = const_table;
        const_table = e->next;
        free(e);
    }
}

static void const_table_push_scope(void) {
    ConstEntry* e = (ConstEntry*)checked_malloc(sizeof(ConstEntry));
    e->name = NULL;
    e->value = NULL;
    e->next = const_table;
    const_table = e;
}

static void const_table_shadow_parameters(ASTNode* node) {
    if (!node || (node->type != AST_FUNC_DECL && node->type != AST_LAMBDA &&
                  node->type != AST_COROUTINE_DECL && node->type != AST_ASYNC_FUNC_DECL)) return;
    for (int i = 0; i < node->child_count; i++) {
        ASTNode* param = node->children[i];
        if (!param || param->type != AST_VAR_DECL) continue;
        char* name = checked_strndup(param->token.start, (size_t)param->token.length);
        const_table_push(name, NULL);
        free(name);
    }
}

static void const_table_clear(void) {
    while (const_table) {
        ConstEntry* e = const_table;
        const_table = e->next;
        free(e->name);
        free(e);
    }
}

static void propagate_constants(ASTNode* node, ASTNode* scope) {
    if (!node) return;

    // Push scope on entering blocks
    bool pushed_scope = false;
    if (node->type == AST_BLOCK || node->type == AST_FUNC_DECL ||
        node->type == AST_MAIN || node->type == AST_IF ||
        node->type == AST_WHILE || node->type == AST_FOR ||
        node->type == AST_FOR_IN || node->type == AST_LAMBDA ||
        node->type == AST_COROUTINE_DECL || node->type == AST_ASYNC_FUNC_DECL) {
        const_table_push_scope();
        const_table_shadow_parameters(node);
        pushed_scope = true;
    }

    // Determine the next scope pointer
    ASTNode* next_scope = scope;
    if (node->type == AST_FUNC_DECL || node->type == AST_LAMBDA ||
        node->type == AST_COROUTINE_DECL || node->type == AST_ASYNC_FUNC_DECL ||
        node->type == AST_MAIN) {
        next_scope = node;
    }

    // Recurse into children first, but skip body for generators/coroutines/async
    bool skip_body = (node->type == AST_COROUTINE_DECL || node->type == AST_ASYNC_FUNC_DECL ||
                      (node->type == AST_FUNC_DECL && node->is_generator));

    for (int i = 0; i < node->child_count; i++) {
        propagate_constants(node->children[i], next_scope);
    }

    bool skip_left = false;
    if (node->type == AST_ASSIGN) {
        skip_left = true;
    }
    if (node->type == AST_POSTFIX && (node->token.type == TOKEN_INCREMENT || node->token.type == TOKEN_DECREMENT)) {
        skip_left = true;
    }
    bool skip_right = false;
    if (node->type == AST_UNARY && (node->token.type == TOKEN_INCREMENT || node->token.type == TOKEN_DECREMENT)) {
        skip_right = true;
    }

    if (!skip_body && !skip_left) {
        propagate_constants(node->left, next_scope);
    }
    if (!skip_right) {
        propagate_constants(node->right, next_scope);
    }
    propagate_constants(node->condition, next_scope);
    propagate_constants(node->init, next_scope);
    propagate_constants(node->increment, next_scope);

    // Track const/var assignments: var x = literal
    if (node->type == AST_VAR_DECL && node->init) {
        if (is_literal(node->init)) {
            char name[256];
            int nlen = node->token.length < 255 ? node->token.length : 255;
            memcpy(name, node->token.start, nlen);
            name[nlen] = '\0';
            if (!is_mutated(next_scope, name)) {
                const_table_push(name, node->init);
            }
        }
    }

    // Replace identifier references with their constant values
    if (node->type == AST_IDENTIFIER && node->token.type == TOKEN_IDENTIFIER) {
        char name[256];
        int nlen = node->token.length < 255 ? node->token.length : 255;
        memcpy(name, node->token.start, nlen);
        name[nlen] = '\0';
        ASTNode* const_val = const_table_find(name);
        if (const_val) {
            char* text = checked_strndup(const_val->token.start, (size_t)const_val->token.length);
            Token replacement = const_val->token;
            replacement.start = text;
            node->type = const_val->type;
            replace_node_token(node, replacement, text);
            opt_count++;
        }
    }

    // Pop scope on leaving blocks
    if (pushed_scope) {
        const_table_pop_scope();
    }
}

// ========================================================================
// PASS 2: Copy Propagation
// ========================================================================

typedef struct CopyEntry {
    char* dest;
    char* src;
    struct CopyEntry* next;
} CopyEntry;

static CopyEntry* copy_table = NULL;

static void copy_table_push(const char* dest, const char* src) {
    CopyEntry* e = (CopyEntry*)checked_malloc(sizeof(CopyEntry));
    e->dest = checked_strdup(dest);
    e->src = src ? checked_strdup(src) : NULL;
    e->next = copy_table;
    copy_table = e;
}

static const char* copy_table_find(const char* name) {
    for (CopyEntry* e = copy_table; e; e = e->next) {
        if (e->dest && strcmp(e->dest, name) == 0) return e->src;
    }
    return NULL;
}

static void copy_table_pop_scope(void) {
    while (copy_table && copy_table->dest != NULL) {
        CopyEntry* e = copy_table;
        copy_table = e->next;
        free(e->dest);
        free(e->src);
        free(e);
    }
    if (copy_table && copy_table->dest == NULL) {
        CopyEntry* e = copy_table;
        copy_table = e->next;
        free(e);
    }
}

static void copy_table_push_scope(void) {
    CopyEntry* e = (CopyEntry*)checked_malloc(sizeof(CopyEntry));
    e->dest = NULL;
    e->src = NULL;
    e->next = copy_table;
    copy_table = e;
}

static void copy_table_shadow_parameters(ASTNode* node) {
    if (!node || (node->type != AST_FUNC_DECL && node->type != AST_LAMBDA &&
                  node->type != AST_COROUTINE_DECL && node->type != AST_ASYNC_FUNC_DECL)) return;
    for (int i = 0; i < node->child_count; i++) {
        ASTNode* param = node->children[i];
        if (!param || param->type != AST_VAR_DECL) continue;
        char* name = checked_strndup(param->token.start, (size_t)param->token.length);
        copy_table_push(name, NULL);
        free(name);
    }
}

static void copy_table_clear(void) {
    while (copy_table) {
        CopyEntry* e = copy_table;
        copy_table = e->next;
        free(e->dest);
        free(e->src);
        free(e);
    }
}

static void propagate_copies(ASTNode* node, ASTNode* scope) {
    if (!node) return;

    // Push scope on entering blocks
    bool pushed_scope = false;
    if (node->type == AST_BLOCK || node->type == AST_FUNC_DECL ||
        node->type == AST_MAIN || node->type == AST_IF ||
        node->type == AST_WHILE || node->type == AST_FOR ||
        node->type == AST_FOR_IN || node->type == AST_LAMBDA ||
        node->type == AST_COROUTINE_DECL || node->type == AST_ASYNC_FUNC_DECL) {
        copy_table_push_scope();
        copy_table_shadow_parameters(node);
        pushed_scope = true;
    }

    // Determine the next scope pointer
    ASTNode* next_scope = scope;
    if (node->type == AST_FUNC_DECL || node->type == AST_LAMBDA ||
        node->type == AST_COROUTINE_DECL || node->type == AST_ASYNC_FUNC_DECL ||
        node->type == AST_MAIN) {
        next_scope = node;
    }

    // Track assignments: y = x (where x is a simple identifier)
    if (node->type == AST_ASSIGN && node->left && node->right) {
        if (node->left->type == AST_IDENTIFIER && node->right->type == AST_IDENTIFIER) {
            char dest[256], src[256];
            int dlen = node->left->token.length < 255 ? node->left->token.length : 255;
            int slen = node->right->token.length < 255 ? node->right->token.length : 255;
            memcpy(dest, node->left->token.start, dlen);
            dest[dlen] = '\0';
            memcpy(src, node->right->token.start, slen);
            src[slen] = '\0';
            if (count_mutations(next_scope, dest) <= 1 && count_mutations(next_scope, src) == 0) {
                copy_table_push(dest, src);
            }
        }
    }

    // Replace copies in identifiers (safe copy with owned memory)
    if (node->type == AST_IDENTIFIER && node->token.type == TOKEN_IDENTIFIER) {
        char name[256];
        int nlen = node->token.length < 255 ? node->token.length : 255;
        memcpy(name, node->token.start, nlen);
        name[nlen] = '\0';
        const char* src = copy_table_find(name);
        if (src) {
            char* copy = checked_strdup(src);
            Token replacement = node->token;
            replacement.start = copy;
            replacement.length = (int)strlen(copy);
            replace_node_token(node, replacement, copy);
            opt_count++;
        }
    }

    // Recurse into children, but skip body for generators/coroutines/async
    bool skip_body = (node->type == AST_COROUTINE_DECL || node->type == AST_ASYNC_FUNC_DECL ||
                      (node->type == AST_FUNC_DECL && node->is_generator));

    for (int i = 0; i < node->child_count; i++) {
        propagate_copies(node->children[i], next_scope);
    }

    bool skip_left = false;
    if (node->type == AST_ASSIGN) {
        skip_left = true;
    }
    if (node->type == AST_POSTFIX && (node->token.type == TOKEN_INCREMENT || node->token.type == TOKEN_DECREMENT)) {
        skip_left = true;
    }
    bool skip_right = false;
    if (node->type == AST_UNARY && (node->token.type == TOKEN_INCREMENT || node->token.type == TOKEN_DECREMENT)) {
        skip_right = true;
    }

    if (!skip_body && !skip_left) {
        propagate_copies(node->left, next_scope);
    }
    if (!skip_right) {
        propagate_copies(node->right, next_scope);
    }
    propagate_copies(node->condition, next_scope);
    propagate_copies(node->init, next_scope);
    propagate_copies(node->increment, next_scope);

    // Pop scope on leaving blocks
    if (pushed_scope) {
        copy_table_pop_scope();
    }
}

// ========================================================================
// PASS 3: Identity Elimination
// ========================================================================

static ASTNode* eliminate_identities(ASTNode* node) {
    if (!node || node->type != AST_BINARY) return NULL;
    if (!node->left || !node->right) return NULL;

    ASTNode* left = node->left;
    ASTNode* right = node->right;

    // x + 0 -> x
    if (node->token.type == TOKEN_PLUS && is_int_literal(right) &&
        is_plain_numeric(left) && is_plain_numeric(right)) {
        if (parse_int_value(right->token) == 0) {
            opt_count++;
            ASTNode* result = left;
            node->left = NULL;
            return result;
        }
    }
    // 0 + x -> x
    if (node->token.type == TOKEN_PLUS && is_int_literal(left) &&
        is_plain_numeric(left) && is_plain_numeric(right)) {
        if (parse_int_value(left->token) == 0) {
            opt_count++;
            ASTNode* result = right;
            node->right = NULL;
            return result;
        }
    }
    // x - 0 -> x
    if (node->token.type == TOKEN_MINUS && is_int_literal(right) &&
        is_plain_numeric(left) && is_plain_numeric(right)) {
        if (parse_int_value(right->token) == 0) {
            opt_count++;
            ASTNode* result = left;
            node->left = NULL;
            return result;
        }
    }
    // x * 1 -> x
    if (node->token.type == TOKEN_STAR && is_int_literal(right) &&
        is_plain_numeric(left) && is_plain_numeric(right)) {
        if (parse_int_value(right->token) == 1) {
            opt_count++;
            ASTNode* result = left;
            node->left = NULL;
            return result;
        }
    }
    // 1 * x -> x
    if (node->token.type == TOKEN_STAR && is_int_literal(left) &&
        is_plain_numeric(left) && is_plain_numeric(right)) {
        if (parse_int_value(left->token) == 1) {
            opt_count++;
            ASTNode* result = right;
            node->right = NULL;
            return result;
        }
    }

    // x == x -> true (same identifier, integer type only to avoid NaN issues)
    if (node->token.type == TOKEN_EQ_EQ && left->type == AST_IDENTIFIER && right->type == AST_IDENTIFIER) {
        if (same_identifier(left->token, right->token) && left->expr_type == TOKEN_INT_TYPE) {
            opt_count++;
            return make_int_literal(1, node->token);
        }
    }
    // x != x -> false (same identifier, integer type only)
    if (node->token.type == TOKEN_BANG_EQ && left->type == AST_IDENTIFIER && right->type == AST_IDENTIFIER) {
        if (same_identifier(left->token, right->token) && left->expr_type == TOKEN_INT_TYPE) {
            opt_count++;
            return make_int_literal(0, node->token);
        }
    }
    return NULL;
}

// ========================================================================
// PASS 4: Peephole Optimization
// ========================================================================

static ASTNode* peephole_optimize(ASTNode* node) {
    if (!node || node->type != AST_BINARY) return NULL;
    if (!node->left || !node->right) return NULL;

    ASTNode* left = node->left;
    ASTNode* right = node->right;

    // x * 2 -> x + x (identifier operands only)
    if (node->token.type == TOKEN_STAR && is_int_literal(right)) {
        if (parse_int_value(right->token) == 2 && left->type == AST_IDENTIFIER &&
            is_plain_numeric(left) && is_plain_numeric(right)) {
            opt_count++;
            Token t = make_owned_operator(node->token, "+", TOKEN_PLUS);
            ASTNode* add = create_node(AST_BINARY, t);
            add->owned_token_text = (char*)t.start;
            add->expr_type = node->expr_type;
            add->left = left;
            node->left = NULL;
            free_ast(node->right);
            node->right = NULL;
            add->right = make_owned_identifier(left->token);
            return add;
        }
    }
    return NULL;
}

// ========================================================================
// PASS 5: Constant Fold Binary/Unary
// ========================================================================

static bool fold_int_op(int64_t l, int64_t r, CnextTokenType op, int64_t* result) {
    switch (op) {
        case TOKEN_PLUS:
            if ((r > 0 && l > INT64_MAX - r) || (r < 0 && l < INT64_MIN - r)) return false;
            *result = l + r;
            return true;
        case TOKEN_MINUS:
            if ((r < 0 && l > INT64_MAX + r) || (r > 0 && l < INT64_MIN + r)) return false;
            *result = l - r;
            return true;
        case TOKEN_STAR:
            if (l > 0) {
                if ((r > 0 && l > INT64_MAX / r) || (r < 0 && r < INT64_MIN / l)) return false;
            } else if (l < 0) {
                if ((r > 0 && l < INT64_MIN / r) || (r < 0 && l < INT64_MAX / r)) return false;
            }
            *result = l * r;
            return true;
        case TOKEN_SLASH:
            if (r == 0 || (l == INT64_MIN && r == -1)) return false;
            *result = l / r;
            return true;
        case TOKEN_EQ_EQ:    *result = (l == r); return true;
        case TOKEN_BANG_EQ:  *result = (l != r); return true;
        case TOKEN_LESS:     *result = (l < r); return true;
        case TOKEN_LESS_EQ:  *result = (l <= r); return true;
        case TOKEN_GREATER:  *result = (l > r); return true;
        case TOKEN_GREATER_EQ: *result = (l >= r); return true;
        default: return false;
    }
}

static ASTNode* fold_binary(ASTNode* node) {
    if (!node || node->type != AST_BINARY) return NULL;
    if (!node->left || !node->right) return NULL;

    ASTNode* left = node->left;
    ASTNode* right = node->right;

    // int op int
    if (is_int_literal(left) && is_int_literal(right)) {
        int64_t l = parse_int_value(left->token);
        int64_t r = parse_int_value(right->token);
        int64_t result = 0;

        if (fold_int_op(l, r, node->token.type, &result) && fold_result_fits(node, result)) {
            opt_count++;
            return make_int_literal(result, node->token);
        }
    }

    // float op float
    if (is_float_literal(left) && is_float_literal(right)) {
        double l = parse_float_value(left->token);
        double r = parse_float_value(right->token);
        double result = 0;
        bool valid = true;

        switch (node->token.type) {
            case TOKEN_PLUS:       result = l + r; break;
            case TOKEN_MINUS:      result = l - r; break;
            case TOKEN_STAR:       result = l * r; break;
            case TOKEN_SLASH:
                if (r == 0.0) { valid = false; break; }
                result = l / r; break;
            case TOKEN_EQ_EQ:      result = (l == r); break;
            case TOKEN_BANG_EQ:    result = (l != r); break;
            case TOKEN_LESS:       result = (l < r); break;
            case TOKEN_LESS_EQ:    result = (l <= r); break;
            case TOKEN_GREATER:    result = (l > r); break;
            case TOKEN_GREATER_EQ: result = (l >= r); break;
            default: valid = false; break;
        }

        if (valid) {
            opt_count++;
            if (node->token.type == TOKEN_EQ_EQ || node->token.type == TOKEN_BANG_EQ ||
                node->token.type == TOKEN_LESS || node->token.type == TOKEN_LESS_EQ ||
                node->token.type == TOKEN_GREATER || node->token.type == TOKEN_GREATER_EQ) {
                return make_int_literal(result != 0 ? 1 : 0, node->token);
            }
            if (!isfinite(result)) return NULL;
            return make_float_literal(result, node->token);
        }
    }

    // int op float or float op int -> promote to float
    if ((is_int_literal(left) && is_float_literal(right)) ||
        (is_float_literal(left) && is_int_literal(right))) {
        double l = is_float_literal(left) ? parse_float_value(left->token) : (double)parse_int_value(left->token);
        double r = is_float_literal(right) ? parse_float_value(right->token) : (double)parse_int_value(right->token);
        double result = 0;
        bool valid = true;

        switch (node->token.type) {
            case TOKEN_PLUS:       result = l + r; break;
            case TOKEN_MINUS:      result = l - r; break;
            case TOKEN_STAR:       result = l * r; break;
            case TOKEN_SLASH:
                if (r == 0.0) { valid = false; break; }
                result = l / r; break;
            default: valid = false; break;
        }

        if (valid) {
            if (!isfinite(result)) return NULL;
            opt_count++;
            return make_float_literal(result, node->token);
        }
    }

    return NULL;
}

static ASTNode* fold_unary(ASTNode* node) {
    if (!node || node->type != AST_UNARY) return NULL;
    if (!node->right) return NULL;

    ASTNode* operand = node->right;

    if (is_int_literal(operand)) {
        int64_t val = parse_int_value(operand->token);
        if (node->token.type == TOKEN_MINUS) {
            int64_t result = 0;
            if (!fold_int_op(0, val, TOKEN_MINUS, &result) || !fold_result_fits(node, result)) return NULL;
            opt_count++;
            return make_int_literal(result, node->token);
        } else if (node->token.type == TOKEN_BANG) {
            opt_count++;
            return make_int_literal(val == 0 ? 1 : 0, node->token);
        }
    }

    if (is_float_literal(operand)) {
        double val = parse_float_value(operand->token);
        if (node->token.type == TOKEN_MINUS) {
            double result = -val;
            if (!isfinite(result)) return NULL;
            opt_count++;
            return make_float_literal(result, node->token);
        } else if (node->token.type == TOKEN_BANG) {
            opt_count++;
            return make_int_literal(val == 0.0 ? 1 : 0, node->token);
        }
    }

    return NULL;
}

// ========================================================================
// PASS 6: Dead Code Elimination
// ========================================================================

static bool is_terminating(ASTNode* node) {
    if (!node) return false;
    switch (node->type) {
        case AST_RETURN:
        case AST_THROW:
        case AST_BREAK:
        case AST_CONTINUE:
            return true;
        case AST_EXPR_STMT:
            return node->left && is_terminating(node->left);
        default:
            return false;
    }
}

static void eliminate_dead_code(ASTNode* block_node) {
    if (!block_node || block_node->type != AST_BLOCK) return;
    int last_live = block_node->child_count;
    for (int i = 0; i < block_node->child_count; i++) {
        if (is_terminating(block_node->children[i])) {
            last_live = i + 1;
            break;
        }
    }
    for (int i = last_live; i < block_node->child_count; i++) {
        free_ast(block_node->children[i]);
        block_node->children[i] = NULL;
    }
    block_node->child_count = last_live;
}

// ========================================================================
// PASS 7: Branch Simplification
// ========================================================================

static ASTNode* fold_if(ASTNode* node) {
    if (!node || node->type != AST_IF) return NULL;
    if (!node->condition) return NULL;

    if (is_int_literal(node->condition)) {
        int64_t val = parse_int_value(node->condition->token);
        ASTNode* folded = NULL;
        if (val != 0) {
            folded = node->left;
            node->left = NULL;
            free_ast(node->right);
            node->right = NULL;
        } else {
            folded = node->right;
            node->right = NULL;
            free_ast(node->left);
            node->left = NULL;
        }
        free_ast(node->condition);
        node->condition = NULL;
        opt_count++;
        return folded;
    }
    return NULL;
}

static ASTNode* fold_while_false(ASTNode* node) {
    if (!node || node->type != AST_WHILE) return NULL;
    if (!node->condition) return NULL;

    if (is_int_literal(node->condition)) {
        int64_t val = parse_int_value(node->condition->token);
        if (val == 0) {
            opt_count++;
            return create_node(AST_BLOCK, node->token);
        }
    }
    return NULL;
}

// ========================================================================
// Main optimization pipeline
// ========================================================================

static void optimize_node(ASTNode* node) {
    if (!node) return;

    // Optimize children first (bottom-up)
    for (int i = 0; i < node->child_count; i++) {
        optimize_node(node->children[i]);
    }
    optimize_node(node->left);
    optimize_node(node->right);
    optimize_node(node->condition);
    optimize_node(node->init);
    optimize_node(node->increment);

    // Constant fold binary expressions
    if (node->type == AST_BINARY) {
        ASTNode* folded = fold_binary(node);
        if (folded) {
            free_ast(node->left);
            node->left = NULL;
            free_ast(node->right);
            node->right = NULL;
            move_ast_node(node, folded);
        }
    }

    // Peephole optimization
    if (node->type == AST_BINARY) {
        ASTNode* reduced = peephole_optimize(node);
        if (reduced) {
            move_ast_node(node, reduced);
        }
    }

    // Identity elimination
    if (node->type == AST_BINARY) {
        ASTNode* old_left = node->left;
        ASTNode* old_right = node->right;
        ASTNode* simplified = eliminate_identities(node);
        if (simplified) {
            bool simplified_is_old_left = (simplified == old_left);
            bool simplified_is_old_right = (simplified == old_right);
            node->left = NULL;
            node->right = NULL;
            move_ast_node(node, simplified);
            if (!simplified_is_old_left) {
                free_ast(old_left);
            }
            if (!simplified_is_old_right) {
                free_ast(old_right);
            }
        }
    }

    // Constant fold unary expressions
    if (node->type == AST_UNARY) {
        ASTNode* folded = fold_unary(node);
        if (folded) {
            free_ast(node->right);
            node->right = NULL;
            move_ast_node(node, folded);
        }
    }

    // Fold if (true/false)
    if (node->type == AST_IF) {
        ASTNode* folded = fold_if(node);
        if (folded) {
            move_ast_node(node, folded);
        } else if (is_int_literal(node->condition)) {
            int64_t val = parse_int_value(node->condition->token);
            if (val == 0) {
                free_ast(node->left);
                node->left = NULL;
                free_ast(node->right);
                node->right = NULL;
                free_ast(node->condition);
                node->condition = NULL;
                node->type = AST_BLOCK;
                opt_count++;
            }
        }
    }

    // Fold while (false)
    if (node->type == AST_WHILE) {
        ASTNode* folded = fold_while_false(node);
        if (folded) {
            free_ast(node->left);
            node->left = NULL;
            free_ast(node->right);
            node->right = NULL;
            free_ast(node->condition);
            node->condition = NULL;
            free_ast(node->init);
            node->init = NULL;
            free_ast(node->increment);
            node->increment = NULL;
            move_ast_node(node, folded);
        }
    }

    // Dead code elimination in blocks
    if (node->type == AST_BLOCK) {
        eliminate_dead_code(node);
    }
}

// ========================================================================
// Main optimization pipeline
// ========================================================================

bool optimize_ast(ASTNode* program) {
    if (!program) return false;
    opt_count = 0;

    // Pass 1: Constant propagation
    const_table_clear();
    propagate_constants(program, program);

    // Pass 2: Copy propagation
    copy_table_clear();
    propagate_copies(program, program);

    // Pass 3-6: Bottom-up optimizations (fold, peephole, identity, DCE, branch)
    optimize_node(program);

    // Cleanup
    const_table_clear();
    copy_table_clear();

    return true;
}
