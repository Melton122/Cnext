#include "optimizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int opt_count = 0;

int get_optimization_count(void) {
    return opt_count;
}

// Parse integer value from token text
static long parse_int_value(Token token) {
    char buf[64];
    int len = token.length < 63 ? token.length : 63;
    memcpy(buf, token.start, len);
    buf[len] = '\0';
    return strtol(buf, NULL, 10);
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
static ASTNode* make_int_literal(long value, Token token) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%ld", value);
    char* text = (char*)malloc(len + 1);
    memcpy(text, buf, len);
    text[len] = '\0';
    Token t = token;
    t.start = text;
    t.length = len;
    t.type = TOKEN_NUMBER;
    ASTNode* node = create_node(AST_LITERAL, t);
    return node;
}

static ASTNode* make_float_literal(double value, Token token) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%g", value);
    char* text = (char*)malloc(len + 1);
    memcpy(text, buf, len);
    text[len] = '\0';
    Token t = token;
    t.start = text;
    t.length = len;
    t.type = TOKEN_FLOAT_LITERAL;
    ASTNode* node = create_node(AST_LITERAL, t);
    return node;
}

// Check if a node is a compile-time constant integer literal
static bool is_int_literal(ASTNode* node) {
    return node && node->type == AST_LITERAL &&
           (node->token.type == TOKEN_NUMBER ||
            (node->token.type == TOKEN_FLOAT_LITERAL && strchr(node->token.start, '.') == NULL));
}

// Check if a node is a compile-time constant float literal
static bool is_float_literal(ASTNode* node) {
    return node && node->type == AST_LITERAL && node->token.type == TOKEN_FLOAT_LITERAL;
}

// Constant fold a binary expression
static ASTNode* fold_binary(ASTNode* node) {
    if (!node || node->type != AST_BINARY) return NULL;
    if (!node->left || !node->right) return NULL;

    ASTNode* left = node->left;
    ASTNode* right = node->right;

    // int op int
    if (is_int_literal(left) && is_int_literal(right)) {
        long l = parse_int_value(left->token);
        long r = parse_int_value(right->token);
        long result = 0;
        bool valid = true;

        switch (node->token.type) {
            case TOKEN_PLUS:       result = l + r; break;
            case TOKEN_MINUS:      result = l - r; break;
            case TOKEN_STAR:       result = l * r; break;
            case TOKEN_SLASH:
                if (r == 0) { valid = false; break; }
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
            opt_count++;
            return make_float_literal(result, node->token);
        }
    }

    return NULL;
}

// Constant fold a unary expression
static ASTNode* fold_unary(ASTNode* node) {
    if (!node || node->type != AST_UNARY) return NULL;
    if (!node->right) return NULL;

    ASTNode* operand = node->right;

    if (is_int_literal(operand)) {
        long val = parse_int_value(operand->token);
        if (node->token.type == TOKEN_MINUS) {
            opt_count++;
            return make_int_literal(-val, node->token);
        } else if (node->token.type == TOKEN_BANG) {
            opt_count++;
            return make_int_literal(val == 0 ? 1 : 0, node->token);
        }
    }

    if (is_float_literal(operand)) {
        double val = parse_float_value(operand->token);
        if (node->token.type == TOKEN_MINUS) {
            opt_count++;
            return make_float_literal(-val, node->token);
        } else if (node->token.type == TOKEN_BANG) {
            opt_count++;
            return make_int_literal(val == 0.0 ? 1 : 0, node->token);
        }
    }

    return NULL;
}

// Dead code elimination: detect unreachable code after return/break/continue/throw
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

// Remove dead code from a block (statements after a terminating statement)
static void eliminate_dead_code(ASTNode* block_node) {
    if (!block_node || block_node->type != AST_BLOCK) return;
    int last_live = block_node->child_count;
    for (int i = 0; i < block_node->child_count; i++) {
        if (is_terminating(block_node->children[i])) {
            last_live = i + 1;
            break;
        }
    }
    // Free statements after the last terminating one
    for (int i = last_live; i < block_node->child_count; i++) {
        free_ast(block_node->children[i]);
        block_node->children[i] = NULL;
    }
    block_node->child_count = last_live;
}

// Optimize if (true) / if (false)
static ASTNode* fold_if(ASTNode* node) {
    if (!node || node->type != AST_IF) return NULL;
    if (!node->condition) return NULL;

    ASTNode* folded = NULL;

    if (is_int_literal(node->condition)) {
        long val = parse_int_value(node->condition->token);
        if (val != 0) {
            // if (true) -> then branch
            folded = node->left;
            node->left = NULL; // don't free
        } else {
            // if (false) -> else branch
            folded = node->right;
            node->right = NULL; // don't free
        }
        opt_count++;
    }

    return folded;
}

// Optimize while (false) -> remove loop
static ASTNode* fold_while_false(ASTNode* node) {
    if (!node || node->type != AST_WHILE) return NULL;
    if (!node->condition) return NULL;

    if (is_int_literal(node->condition)) {
        long val = parse_int_value(node->condition->token);
        if (val == 0) {
            opt_count++;
            return create_node(AST_BLOCK, node->token);
        }
    }
    return NULL;
}

// Recursive optimization pass
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
            node->type = folded->type;
            node->token = folded->token;
            free_ast(node->left);
            free_ast(node->right);
            node->left = NULL;
            node->right = NULL;
            free(folded); // don't recurse into freed node
        }
    }

    // Constant fold unary expressions
    if (node->type == AST_UNARY) {
        ASTNode* folded = fold_unary(node);
        if (folded) {
            node->type = folded->type;
            node->token = folded->token;
            free_ast(node->right);
            node->right = NULL;
            free(folded);
        }
    }

    // Fold if (true/false)
    if (node->type == AST_IF) {
        ASTNode* folded = fold_if(node);
        if (folded) {
            node->type = folded->type;
            node->token = folded->token;
            node->left = folded->left;
            node->right = folded->right;
            node->condition = folded->condition;
            node->init = folded->init;
            node->increment = folded->increment;
            // Copy children
            node->children = folded->children;
            node->child_count = folded->child_count;
            node->child_capacity = folded->child_capacity;
            free(folded);
        }
    }

    // Fold while (false)
    if (node->type == AST_WHILE) {
        ASTNode* folded = fold_while_false(node);
        if (folded) {
            node->type = folded->type;
            node->token = folded->token;
            node->left = folded->left;
            node->right = folded->right;
            node->condition = NULL;
            free(folded);
        }
    }

    // Dead code elimination in blocks
    if (node->type == AST_BLOCK) {
        eliminate_dead_code(node);
    }
}

bool optimize_ast(ASTNode* program) {
    if (!program) return false;
    opt_count = 0;
    optimize_node(program);
    return true;
}
