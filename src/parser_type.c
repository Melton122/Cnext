#include "parser_internal.h"

#define IS_BUILTIN_TYPE(t) \
    ((t) == TOKEN_INT_TYPE || (t) == TOKEN_LONG_TYPE || \
     (t) == TOKEN_FLOAT_TYPE || (t) == TOKEN_DOUBLE_TYPE || \
     (t) == TOKEN_STR_TYPE || (t) == TOKEN_BOOL_TYPE || \
     (t) == TOKEN_CHAR_TYPE || (t) == TOKEN_BYTE_TYPE || \
     (t) == TOKEN_UINT_TYPE || (t) == TOKEN_ULONG_TYPE || \
     (t) == TOKEN_USHORT_TYPE || (t) == TOKEN_UBYTE_TYPE)

bool is_name_token(CnextTokenType type) {
    return type == TOKEN_IDENTIFIER || IS_BUILTIN_TYPE(type);
}

void consume_name(const char* message) {
    if (is_name_token(parser.current.type)) {
        advance_token();
        return;
    }
    error_at_current(message);
}

bool is_type(void) {
    CnextTokenType type = parser.current.type;
    if (IS_BUILTIN_TYPE(type) || type == TOKEN_VAR ||
        type == TOKEN_ITER || type == TOKEN_FUNC) {
        return true;
    }
    if (type == TOKEN_IDENTIFIER) {
        if (parser.current.start[0] >= 'A' && parser.current.start[0] <= 'Z') {
            return true;
        }
    }
    return false;
}

ASTNode* parse_type() {
    // Check for tuple type: (Type1, Type2, ...)
    if (match_token(TOKEN_LPAREN)) {
        ASTNode* tuple_type = create_node(AST_TUPLE, parser.previous);
        do {
            ASTNode* elem_type = parse_type();
            if (!elem_type) {
                free_ast(tuple_type);
                return NULL;
            }
            add_child(tuple_type, elem_type);
        } while (match_token(TOKEN_COMMA));
        consume(TOKEN_RPAREN, "Expect ')' after tuple type.");
        return tuple_type;
    }
    // Check for Optional<T> type (identifier text match since optional is not a keyword)
    if (check_identifier_text("optional", 8) || check_identifier_text("Optional", 8)) {
        advance_token();
        ASTNode* opt_type = create_node(AST_IDENTIFIER, parser.previous);
        consume(TOKEN_LESS, "Expect '<' after 'Optional'.");
        ASTNode* inner_type = parse_type();
        if (!inner_type) {
            free_ast(opt_type);
            return NULL;
        }
        add_type_arg(opt_type, inner_type);
        consume(TOKEN_GREATER, "Expect '>' after Optional type argument.");
        return opt_type;
    }
    // Check for Result<T, E> type (identifier text match since result is not a keyword)
    if (check_identifier_text("result", 6) || check_identifier_text("Result", 6)) {
        advance_token();
        ASTNode* res_type = create_node(AST_IDENTIFIER, parser.previous);
        consume(TOKEN_LESS, "Expect '<' after 'Result'.");
        ASTNode* ok_type = parse_type();
        if (!ok_type) {
            free_ast(res_type);
            return NULL;
        }
        add_type_arg(res_type, ok_type);
        consume(TOKEN_COMMA, "Expect ',' after Result ok type.");
        ASTNode* err_type = parse_type();
        if (!err_type) {
            free_ast(res_type);
            return NULL;
        }
        add_type_arg(res_type, err_type);
        consume(TOKEN_GREATER, "Expect '>' after Result type arguments.");
        return res_type;
    }
    // Check for iter<T> type
    if (check(TOKEN_ITER)) {
        advance_token();
        ASTNode* iter_type = create_node(AST_IDENTIFIER, parser.previous);
        consume(TOKEN_LESS, "Expect '<' after 'iter'.");
        ASTNode* elem_type = parse_type();
        if (!elem_type) {
            free_ast(iter_type);
            return NULL;
        }
        add_type_arg(iter_type, elem_type);
        consume(TOKEN_GREATER, "Expect '>' after iter type argument.");
        return iter_type;
    }
    // Check for func type (opaque function pointer)
    if (check(TOKEN_FUNC)) {
        advance_token();
        ASTNode* node = create_node(AST_IDENTIFIER, parser.previous);
        return node;
    }
    if (is_type()) {
        advance_token();
        Token typeToken = parser.previous;
        ASTNode* node = create_node(AST_IDENTIFIER, typeToken);
        if (match_token(TOKEN_LESS)) {
            // Generic type arguments: Type<A, B>
            do {
                ASTNode* arg = parse_type();
                if (!arg) {
                    free_ast(node);
                    return NULL;
                }
                add_type_arg(node, arg);
            } while (match_token(TOKEN_COMMA));
            consume(TOKEN_GREATER, "Expect '>' after generic type arguments.");
        }
        if (match_token(TOKEN_LBRACKET)) {
            consume(TOKEN_RBRACKET, "Expect ']' after '[' for array type.");
            node->is_array = true;
        }
        // Nullable type: str?, int?, etc.
        if (match_token(TOKEN_QUESTION)) {
            node->is_pointer = true; // Reuse is_pointer flag for nullable
        }
        return node;
    }
    error_at_current("Expect type.");
    return NULL;
}

bool is_generic_call_lookahead(void) {
    const char* p = parser.current.start;
    // Skip whitespace
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '<') return false;
    p++;
    // Skip whitespace
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    // Must be a type name (alpha/underscore or built-in type keyword)
    if (!isalpha(*p) && *p != '_') return false;
    // Skip past the type name
    while (isalnum(*p) || *p == '_') p++;
    // Skip whitespace
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    // Handle comma-separated type list
    while (*p == ',') {
        p++;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!isalpha(*p) && *p != '_') return false;
        while (isalnum(*p) || *p == '_') p++;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    }
    if (*p != '>') return false;
    p++;
    // Skip whitespace
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    // Must be followed by '(' for a function call
    return *p == '(';
}
