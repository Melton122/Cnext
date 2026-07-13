#include "parser_internal.h"

static bool is_lambda_lookahead() {
    const char* p = parser.current.start;
    int depth = 1; // We assume we are right after the opening '('
    bool in_string = false;
    char string_char = 0;
    
    while (*p != '\0') {
        if (in_string) {
            if (*p == '\\' && *(p + 1) != '\0') {
                p += 2;
                continue;
            }
            if (*p == string_char) {
                in_string = false;
            }
        } else {
            if (*p == '"' || *p == '\'') {
                in_string = true;
                string_char = *p;
            } else if (*p == '/' && *(p + 1) == '/') {
                while (*p != '\0' && *p != '\n') p++;
                if (*p == '\0') break;
            } else if (*p == '/' && *(p + 1) == '*') {
                p += 2;
                while (*p != '\0' && !(*p == '*' && *(p + 1) == '/')) p++;
                if (*p != '\0') p += 2;
                continue;
            } else if (*p == '(') {
                depth++;
            } else if (*p == ')') {
                depth--;
                if (depth == 0) {
                    p++; // skip ')'
                    // skip whitespace
                    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
                        p++;
                    }
                    if (*p == '=' && *(p + 1) == '>') {
                        return true;
                    }
                    return false;
                }
            }
        }
        p++;
    }
    return false;
}

static bool is_paren_after_id() {
    const char* p = parser.current.start + parser.current.length;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return *p == '(';
}

static bool is_named_arg_lookahead() {
    const char* p = parser.current.start + parser.current.length;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '=') return false;
    p++;
    if (*p == '=' || *p == '>' || *p == '+' || *p == '-' || *p == '*' || *p == '/') return false;
    return true;
}

static ASTNode* primary() {
    if (match_token(TOKEN_LBRACE)) {
        ASTNode* node = create_node(AST_LITERAL_ARRAY, parser.previous);
        if (!check(TOKEN_RBRACE)) {
            do {
                add_child(node, expression());
            } while (match_token(TOKEN_COMMA));
        }
        consume(TOKEN_RBRACE, "Expect '}' after array elements.");
        return node;
    }
    if (match_token(TOKEN_FALSE) || match_token(TOKEN_TRUE) ||
        match_token(TOKEN_NUMBER) || match_token(TOKEN_FLOAT_LITERAL) ||
        match_token(TOKEN_STRING_LITERAL) || match_token(TOKEN_CHAR_LITERAL)) {
        return create_node(AST_LITERAL, parser.previous);
    }
    if (match_token(TOKEN_NULL)) {
        return create_node(AST_NULL_LITERAL, parser.previous);
    }
    if (match_token(TOKEN_TYPEOF)) {
        consume(TOKEN_LPAREN, "Expect '(' after 'typeof'.");
        ASTNode* node = create_node(AST_TYPEOF, parser.previous);
        node->left = expression();
        consume(TOKEN_RPAREN, "Expect ')' after typeof expression.");
        return node;
    }
    if (match_token(TOKEN_OWN)) {
        ASTNode* node = create_node(AST_OWN_EXPR, parser.previous);
        node->left = expression();
        return node;
    }
    if (check(TOKEN_IDENTIFIER) && is_paren_after_id() && match_identifier_text("option", 6)) {
        // option(value)
        Token opt_token = parser.previous;
        ASTNode* node = create_node(AST_OPTION_SOME, opt_token);
        consume(TOKEN_LPAREN, "Expect '(' after 'option'.");
        node->left = expression();
        consume(TOKEN_RPAREN, "Expect ')' after option value.");
        return node;
    }
    if (check(TOKEN_IDENTIFIER) && is_paren_after_id() && match_identifier_text("ok", 2)) {
        // ok(value)
        Token ok_token = parser.previous;
        ASTNode* node = create_node(AST_RESULT_OK, ok_token);
        consume(TOKEN_LPAREN, "Expect '(' after 'ok'.");
        node->left = expression();
        consume(TOKEN_RPAREN, "Expect ')' after ok value.");
        return node;
    }
    if (check(TOKEN_IDENTIFIER) && is_paren_after_id() && match_identifier_text("err", 3)) {
        // err(value)
        Token err_token = parser.previous;
        ASTNode* node = create_node(AST_RESULT_ERR, err_token);
        consume(TOKEN_LPAREN, "Expect '(' after 'err'.");
        node->left = expression();
        consume(TOKEN_RPAREN, "Expect ')' after err value.");
        return node;
    }
    if (match_token(TOKEN_IDENTIFIER)) {
        ASTNode* node = create_node(AST_IDENTIFIER, parser.previous);
        // Generic call: func_name<T, U>(args)
        if (check(TOKEN_LESS) && is_generic_call_lookahead()) {
            advance_token(); // consume '<'
            ASTNode* generic_call = create_node(AST_IDENTIFIER, node->token);
            free_ast(node);
            node = generic_call;
            do {
                ASTNode* arg = parse_type();
                if (!arg) {
                    free_ast(node);
                    return NULL;
                }
                add_type_arg(node, arg);
            } while (match_token(TOKEN_COMMA));
            consume(TOKEN_GREATER, "Expect '>' after generic type arguments.");
            // Now parse the function call: (args)
            if (match_token(TOKEN_LPAREN)) {
                ASTNode* call = create_node(AST_CALL, node->token);
                call->left = node;
                if (!check(TOKEN_RPAREN)) {
                    do {
                        add_child(call, expression());
                    } while (match_token(TOKEN_COMMA));
                }
                consume(TOKEN_RPAREN, "Expect ')' after arguments.");
                return call;
            }
            return node;
        }
        if (match_token(TOKEN_DOT)) { // member access or method call
            consume_name("Expect property name after '.'.");
            Token member_name = parser.previous;
            if (match_token(TOKEN_LPAREN)) { // Method call: obj.method(args)
                ASTNode* member = create_node(AST_MEMBER_ACCESS, member_name);
                member->left = node;
                ASTNode* call = create_node(AST_CALL, member_name);
                call->left = member;
                if (!check(TOKEN_RPAREN)) {
                    do {
                        add_child(call, expression());
                    } while (match_token(TOKEN_COMMA));
                }
                consume(TOKEN_RPAREN, "Expect ')' after arguments.");
                return call;
            }
            ASTNode* access = create_node(AST_MEMBER_ACCESS, member_name);
            access->left = node;
            return access;
        }
        if (match_token(TOKEN_QUESTION_DOT)) { // safe member access: obj?.field or obj?.method(args)
            consume_name("Expect property name after '?.'.");
            Token member_name = parser.previous;
            if (match_token(TOKEN_LPAREN)) { // Safe method call: obj?.method(args)
                ASTNode* member = create_node(AST_SAFE_ACCESS, member_name);
                member->left = node;
                ASTNode* call = create_node(AST_CALL, member_name);
                call->left = member;
                if (!check(TOKEN_RPAREN)) {
                    do {
                        add_child(call, expression());
                    } while (match_token(TOKEN_COMMA));
                }
                consume(TOKEN_RPAREN, "Expect ')' after arguments.");
                return call;
            }
            ASTNode* access = create_node(AST_SAFE_ACCESS, member_name);
            access->left = node;
            return access;
        }
        if (match_token(TOKEN_LBRACKET)) {
            if (check(TOKEN_COLON)) {
                advance_token();
                ASTNode* slice = create_node(AST_SLICE, parser.previous);
                slice->left = node;
                if (!check(TOKEN_RBRACKET)) slice->right = expression();
                consume(TOKEN_RBRACKET, "Expect ']' after slice.");
                return slice;
            }
            ASTNode* first = expression();
            if (match_token(TOKEN_COLON)) {
                ASTNode* slice = create_node(AST_SLICE, parser.previous);
                slice->left = node;
                slice->init = first;
                if (!check(TOKEN_RBRACKET)) slice->right = expression();
                consume(TOKEN_RBRACKET, "Expect ']' after slice.");
                return slice;
            }
            ASTNode* index = create_node(AST_INDEX, parser.previous);
            index->left = node;
            index->right = first;
            consume(TOKEN_RBRACKET, "Expect ']' after index.");
            return index;
        }
        if (match_token(TOKEN_LPAREN)) { // Function call
            ASTNode* call = create_node(AST_CALL, parser.previous);
            call->left = node;
            // Check for macro expansion
            ASTNode* macro_decl = lookup_macro(node->token.start, node->token.length);
            if (macro_decl && macro_decl->type == AST_MACRO_DECL) {
                // Collect arguments
                ASTNode** args = NULL;
                int arg_count = 0;
                int arg_cap = 0;
                if (!check(TOKEN_RPAREN)) {
                    do {
                        if (arg_count >= arg_cap) {
                            arg_cap = arg_cap ? arg_cap * 2 : 8;
                            args = realloc(args, sizeof(ASTNode*) * arg_cap);
                        }
                        args[arg_count++] = expression();
                    } while (match_token(TOKEN_COMMA));
                }
                consume(TOKEN_RPAREN, "Expect ')' after arguments.");
                // Expand macro: deep copy body and substitute parameters
                ASTNode* expanded = deep_copy_ast(macro_decl->left);
                substitute_params(expanded, macro_decl->children, macro_decl->child_count, args, arg_count);
                free(args);
                free_ast(call);
                // Return the expanded block as an expression statement
                ASTNode* expr_stmt = create_node(AST_EXPR_STMT, node->token);
                expr_stmt->left = expanded;
                return expr_stmt;
            }
            if (!check(TOKEN_RPAREN)) {
                do {
                    // Check for named argument: identifier = expr (lookahead without advancing lexer)
                    if (check(TOKEN_IDENTIFIER) && is_named_arg_lookahead()) {
                        Token arg_name = parser.current;
                        advance_token(); // consume identifier
                        advance_token(); // consume '='
                        ASTNode* named = create_node(AST_NAMED_ARG, arg_name);
                        named->named_arg_name = arg_name;
                        named->right = expression();
                        add_child(call, named);
                    } else {
                        add_child(call, expression());
                    }
                } while (match_token(TOKEN_COMMA));
            }
            consume(TOKEN_RPAREN, "Expect ')' after arguments.");
            return call;
        }
        return node;
    }
    if (match_token(TOKEN_LPAREN)) {
        if (is_lambda_lookahead()) {
            ASTNode* lambda_node = create_node(AST_LAMBDA, parser.previous);
            if (!check(TOKEN_RPAREN)) {
                do {
                    ASTNode* typeNode = parse_type();
                    if (!typeNode) {
                        free_ast(lambda_node);
                        return NULL;
                    }
                    consume(TOKEN_IDENTIFIER, "Expect parameter name.");
                    ASTNode* param = create_node(AST_VAR_DECL, parser.previous);
                    param->var_type = typeNode->token;
                    param->is_array = typeNode->is_array;
                    free_ast(typeNode);
                    add_child(lambda_node, param);
                } while (match_token(TOKEN_COMMA));
            }
            consume(TOKEN_RPAREN, "Expect ')' after lambda parameters.");
            consume(TOKEN_FAT_ARROW, "Expect '=>' after lambda parameters.");
            if (check(TOKEN_LBRACE)) {
                lambda_node->left = block();
            } else {
                lambda_node->left = expression();
            }
            return lambda_node;
        }
        ASTNode* first = expression();
        if (match_token(TOKEN_COMMA)) {
            // Tuple literal: (expr1, expr2, ...)
            ASTNode* tuple = create_node(AST_TUPLE, parser.previous);
            add_child(tuple, first);
            do {
                add_child(tuple, expression());
            } while (match_token(TOKEN_COMMA));
            consume(TOKEN_RPAREN, "Expect ')' after tuple elements.");
            return tuple;
        }
        consume(TOKEN_RPAREN, "Expect ')' after expression.");
        return first;
    }
    if (match_token(TOKEN_NEW)) {
        // new ClassName<T>(args) or new ClassName(args)
        consume(TOKEN_IDENTIFIER, "Expect class name after 'new'.");
        Token class_token = parser.previous;
        ASTNode* node = create_node(AST_NEW_EXPR, class_token);
        // Optional type arguments: new Box<int>(42)
        if (match_token(TOKEN_LESS)) {
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
        consume(TOKEN_LPAREN, "Expect '(' after class name in 'new'.");
        if (!check(TOKEN_RPAREN)) {
            do {
                add_child(node, expression());
            } while (match_token(TOKEN_COMMA));
        }
        consume(TOKEN_RPAREN, "Expect ')' after 'new' arguments.");
        return node;
    }
    if (match_token(TOKEN_SUPER)) {
        // super.method(args) or super.field
        consume(TOKEN_DOT, "Expect '.' after 'super'.");
        if (check(TOKEN_IDENTIFIER) || check(TOKEN_NEW)) {
            advance_token();
        } else {
            error_at_current("Expect method name after 'super.'.");
        }
        Token member_name = parser.previous;
        ASTNode* super_node = create_node(AST_SUPER_EXPR, member_name);
        if (match_token(TOKEN_LPAREN)) {
            // super.method(args) - it's a call
            ASTNode* call = create_node(AST_CALL, member_name);
            call->left = super_node;
            if (!check(TOKEN_RPAREN)) {
                do {
                    add_child(call, expression());
                } while (match_token(TOKEN_COMMA));
            }
            consume(TOKEN_RPAREN, "Expect ')' after super call arguments.");
            return call;
        }
        return super_node;
    }
    if (match_token(TOKEN_AWAIT)) {
        // await expr — can be used in expressions
        ASTNode* node = create_node(AST_AWAIT_EXPR, parser.previous);
        node->left = expression();
        return node;
    }
    error_at_current("Expect expression.");
    return NULL;
}

static ASTNode* postfix() {
    ASTNode* node = primary();
    for (;;) {
        if (match_token(TOKEN_INCREMENT) || match_token(TOKEN_DECREMENT)) {
            ASTNode* op = create_node(AST_POSTFIX, parser.previous);
            op->left = node;
            node = op;
        } else if (match_token(TOKEN_AS)) {
            ASTNode* cast_node = create_node(AST_CAST, parser.previous);
            cast_node->left = node;
            ASTNode* type_node = parse_type();
            if (!type_node) {
                free_ast(cast_node);
                return NULL;
            }
            cast_node->var_type = type_node->token;
            cast_node->is_array = type_node->is_array;
            free_ast(type_node);
            node = cast_node;
        } else {
            break;
        }
    }
    return node;
}

static ASTNode* unary() {
    if (match_token(TOKEN_BANG) || match_token(TOKEN_MINUS)) {
        ASTNode* node = create_node(AST_UNARY, parser.previous);
        node->right = unary();
        return node;
    }
    return postfix();
}

static ASTNode* factor() {
    ASTNode* expr = unary();
    while (match_token(TOKEN_STAR) || match_token(TOKEN_SLASH)) {
        ASTNode* node = create_node(AST_BINARY, parser.previous);
        node->left = expr;
        node->right = unary();
        expr = node;
    }
    return expr;
}

static ASTNode* term() {
    ASTNode* expr = factor();
    while (match_token(TOKEN_PLUS) || match_token(TOKEN_MINUS)) {
        ASTNode* node = create_node(AST_BINARY, parser.previous);
        node->left = expr;
        node->right = factor();
        expr = node;
    }
    return expr;
}

static ASTNode* comparison() {
    ASTNode* expr = term();
    while (match_token(TOKEN_GREATER) || match_token(TOKEN_GREATER_EQ) ||
           match_token(TOKEN_LESS) || match_token(TOKEN_LESS_EQ)) {
        ASTNode* node = create_node(AST_BINARY, parser.previous);
        node->left = expr;
        node->right = term();
        expr = node;
    }
    return expr;
}

static ASTNode* equality() {
    ASTNode* expr = comparison();
    while (match_token(TOKEN_BANG_EQ) || match_token(TOKEN_EQ_EQ)) {
        ASTNode* node = create_node(AST_BINARY, parser.previous);
        node->left = expr;
        node->right = comparison();
        expr = node;
    }
    return expr;
}

static ASTNode* logic_and() {
    ASTNode* expr = equality();
    while (match_token(TOKEN_AND_AND)) {
        ASTNode* node = create_node(AST_BINARY, parser.previous);
        node->left = expr;
        node->right = equality();
        expr = node;
    }
    return expr;
}

static ASTNode* null_coalesce() {
    ASTNode* expr = logic_and();
    while (match_token(TOKEN_QUESTION_QUESTION)) {
        ASTNode* node = create_node(AST_NULL_COALESCE, parser.previous);
        node->left = expr;
        node->right = logic_and();
        expr = node;
    }
    return expr;
}

static ASTNode* logic_or() {
    ASTNode* expr = null_coalesce();
    while (match_token(TOKEN_OR_OR)) {
        ASTNode* node = create_node(AST_BINARY, parser.previous);
        node->left = expr;
        node->right = null_coalesce();
        expr = node;
    }
    return expr;
}

ASTNode* expression() {
    return logic_or();
}
