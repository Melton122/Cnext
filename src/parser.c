#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char* parser_source = NULL;

typedef struct {
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
} Parser;

static Parser parser;

static void print_source_line(int line) {
    if (!parser_source) return;
    const char* p = parser_source;
    int current_line = 1;
    while (*p && current_line < line) {
        if (*p == '\n') current_line++;
        p++;
    }
    if (!*p) return;
    const char* line_start = p;
    while (*p && *p != '\n') p++;
    size_t line_len = (size_t)(p - line_start);
    if (line_len > 0) {
        fprintf(stderr, "  | %.*s\n", (int)line_len, line_start);
    }
}

static void print_caret(Token* token) {
    if (!parser_source) return;
    int col = 1;
    const char* p = parser_source;
    int current_line = 1;
    while (*p && current_line < token->line) {
        if (*p == '\n') current_line++;
        p++;
    }
    while (*p && *p != '\n' && p < token->start) {
        col++;
        p++;
    }
    fprintf(stderr, "  | ");
    for (int i = 1; i < col; i++) fprintf(stderr, " ");
    if (token->length > 0) {
        fprintf(stderr, "^");
        for (int i = 1; i < token->length && i < 40; i++) fprintf(stderr, "~");
    } else {
        fprintf(stderr, "^");
    }
    fprintf(stderr, "\n");
}

static void error_at(Token* token, const char* message) {
    if (parser.panic_mode) return;
    parser.panic_mode = true;
    fprintf(stderr, "[line %d] Error", token->line);
    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }
    fprintf(stderr, ": %s\n", message);
    print_source_line(token->line);
    if (token->type != TOKEN_EOF) {
        print_caret(token);
    }
    parser.had_error = true;
}

static void error(const char* message) {
    error_at(&parser.previous, message);
}

static void error_at_current(const char* message) {
    error_at(&parser.current, message);
}

static void advance_token() {
    parser.previous = parser.current;
    for (;;) {
        parser.current = next_token();
        if (parser.current.type != TOKEN_ERROR) break;
        error_at_current(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance_token();
        return;
    }
    error_at_current(message);
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match_token(TokenType type) {
    if (!check(type)) return false;
    advance_token();
    return true;
}

static void optionally_consume_semicolon() {
    match_token(TOKEN_SEMICOLON);
}

static bool check_identifier_text(const char* text, int len) {
    return check(TOKEN_IDENTIFIER) && parser.current.length == len && 
           memcmp(parser.current.start, text, len) == 0;
}

static bool match_identifier_text(const char* text, int len) {
    if (!check_identifier_text(text, len)) return false;
    advance_token();
    return true;
}

// Forward declarations
static ASTNode* declaration();
static ASTNode* statement();
static ASTNode* expression();
static ASTNode* block();
static void synchronize();
static ASTNode* function_declaration(bool require_body);
static ASTNode* test_declaration();
static ASTNode* assert_statement();

static bool is_name_token(TokenType type) {
    return type == TOKEN_IDENTIFIER ||
           type == TOKEN_INT_TYPE || type == TOKEN_FLOAT_TYPE ||
           type == TOKEN_STR_TYPE || type == TOKEN_BOOL_TYPE ||
           type == TOKEN_CHAR_TYPE;
}

static void consume_name(const char* message) {
    if (is_name_token(parser.current.type)) {
        advance_token();
        return;
    }
    error_at_current(message);
}

static bool is_type() {
    TokenType type = parser.current.type;
    if (type == TOKEN_INT_TYPE || type == TOKEN_FLOAT_TYPE || 
        type == TOKEN_STR_TYPE || type == TOKEN_BOOL_TYPE || 
        type == TOKEN_CHAR_TYPE || type == TOKEN_VAR) {
        return true;
    }
    if (type == TOKEN_IDENTIFIER) {
        if (parser.current.start[0] >= 'A' && parser.current.start[0] <= 'Z') {
            return true;
        }
    }
    return false;
}

static ASTNode* parse_type() {
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
        return node;
    }
    error_at_current("Expect type.");
    return NULL;
}

static bool is_generic_call_lookahead() {
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
        // new ClassName(args)
        consume(TOKEN_IDENTIFIER, "Expect class name after 'new'.");
        Token class_token = parser.previous;
        ASTNode* node = create_node(AST_NEW_EXPR, class_token);
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

static ASTNode* logic_or() {
    ASTNode* expr = logic_and();
    while (match_token(TOKEN_OR_OR)) {
        ASTNode* node = create_node(AST_BINARY, parser.previous);
        node->left = expr;
        node->right = logic_and();
        expr = node;
    }
    return expr;
}

static ASTNode* expression() {
    return logic_or();
}

static ASTNode* var_declaration(bool is_const) {
    ASTNode* typeNode = parse_type();
    if (!typeNode) return NULL;
    consume(TOKEN_IDENTIFIER, "Expect variable name.");
    if (parser.had_error && parser.previous.type != TOKEN_IDENTIFIER) {
        free_ast(typeNode);
        return NULL;
    }
    ASTNode* node = create_node(AST_VAR_DECL, parser.previous);
    node->var_type = typeNode->token;
    node->is_array = typeNode->is_array;
    node->is_const = is_const;
    free_ast(typeNode);
    if (match_token(TOKEN_EQUAL)) {
        node->init = expression();
    }
    optionally_consume_semicolon();
    return node;
}

static ASTNode* assign_stmt(ASTNode* expr) {
    if (match_token(TOKEN_EQUAL) || match_token(TOKEN_PLUS_EQUAL) ||
        match_token(TOKEN_MINUS_EQUAL) || match_token(TOKEN_STAR_EQUAL) ||
        match_token(TOKEN_SLASH_EQUAL)) {
        ASTNode* assign = create_node(AST_ASSIGN, parser.previous);
        assign->left = expr;
        assign->right = expression();
        optionally_consume_semicolon();
        return assign;
    }
    error("Invalid assignment target.");
    return NULL;
}

static ASTNode* expr_stmt() {
    ASTNode* expr = expression();
    if (check(TOKEN_EQUAL) || check(TOKEN_PLUS_EQUAL) || check(TOKEN_MINUS_EQUAL) ||
        check(TOKEN_STAR_EQUAL) || check(TOKEN_SLASH_EQUAL)) {
        return assign_stmt(expr);
    }
    ASTNode* stmt = create_node(AST_EXPR_STMT, parser.previous);
    stmt->left = expr;
    optionally_consume_semicolon();
    return stmt;
}

static ASTNode* break_statement() {
    ASTNode* node = create_node(AST_BREAK, parser.previous);
    optionally_consume_semicolon();
    return node;
}

static ASTNode* continue_statement() {
    ASTNode* node = create_node(AST_CONTINUE, parser.previous);
    optionally_consume_semicolon();
    return node;
}

static ASTNode* if_statement() {
    ASTNode* node = create_node(AST_IF, parser.previous);
    node->condition = expression();
    node->left = block();
    if (match_token(TOKEN_ELSE)) {
        if (check(TOKEN_IF)) {
            advance_token();
            node->right = if_statement();
        } else {
            node->right = block();
        }
    }
    return node;
}

static ASTNode* while_statement() {
    ASTNode* node = create_node(AST_WHILE, parser.previous);
    node->condition = expression();
    node->left = block();
    return node;
}

static ASTNode* for_statement() {
    ASTNode* node = create_node(AST_FOR, parser.previous);
    
    if (match_token(TOKEN_CONST)) {
        node->init = var_declaration(true);
    } else if (parser.current.type == TOKEN_VAR) {
        match_token(TOKEN_VAR);
        if (check(TOKEN_IDENTIFIER)) {
            Token name = parser.current;
            advance_token();
            if (match_token(TOKEN_IN)) {
                node->type = AST_FOR_IN;
                ASTNode* vdecl = create_node(AST_VAR_DECL, name);
                vdecl->var_type.type = TOKEN_VAR;
                node->init = vdecl;
                node->condition = expression();
                node->left = block();
                return node;
            }
            // Not a for-in loop; parse as var declaration
            ASTNode* vdecl = create_node(AST_VAR_DECL, name);
            vdecl->var_type.type = TOKEN_VAR;
            vdecl->is_const = false;
            if (match_token(TOKEN_EQUAL)) {
                vdecl->init = expression();
            }
            optionally_consume_semicolon();
            node->init = vdecl;
        } else {
            node->init = expr_stmt();
        }
    } else if (parser.current.type == TOKEN_IDENTIFIER) {
        Token name = parser.current;
        advance_token();
        if (match_token(TOKEN_IN)) {
            node->type = AST_FOR_IN;
            ASTNode* vdecl = create_node(AST_VAR_DECL, name);
            vdecl->var_type.type = TOKEN_IDENTIFIER;
            node->init = vdecl;
            node->condition = expression();
            node->left = block();
            return node;
        }
        // Not a for-in loop; parse as expression statement
        ASTNode* id = create_node(AST_IDENTIFIER, name);
        if (check(TOKEN_EQUAL) || check(TOKEN_PLUS_EQUAL) || check(TOKEN_MINUS_EQUAL) ||
            check(TOKEN_STAR_EQUAL) || check(TOKEN_SLASH_EQUAL)) {
            node->init = assign_stmt(id);
        } else {
            ASTNode* stmt = create_node(AST_EXPR_STMT, parser.previous);
            stmt->left = id;
            node->init = stmt;
        }
    } else if (is_type()) {
        node->init = var_declaration(false);
    } else {
        node->init = expr_stmt();
    }
    
    if (parser.previous.type != TOKEN_SEMICOLON) {
        consume(TOKEN_SEMICOLON, "Expect ';' after for loop init.");
    }
    
    node->condition = expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");
    
    ASTNode* inc_expr = expression();
    if (check(TOKEN_EQUAL) || check(TOKEN_PLUS_EQUAL) || check(TOKEN_MINUS_EQUAL) ||
        check(TOKEN_STAR_EQUAL) || check(TOKEN_SLASH_EQUAL)) {
        node->increment = assign_stmt(inc_expr);
    } else {
        node->increment = create_node(AST_EXPR_STMT, parser.previous);
        node->increment->left = inc_expr;
    }
    
    node->left = block();
    return node;
}

static ASTNode* return_statement() {
    ASTNode* node = create_node(AST_RETURN, parser.previous);
    if (!check(TOKEN_SEMICOLON) && !check(TOKEN_RBRACE)) {
        node->left = expression();
    }
    optionally_consume_semicolon();
    return node;
}

static ASTNode* block() {
    consume(TOKEN_LBRACE, "Expect '{' before block.");
    ASTNode* node = create_node(AST_BLOCK, parser.previous);
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        ASTNode* decl = declaration();
        if (decl) add_child(node, decl);
        if (parser.panic_mode) synchronize();
    }
    consume(TOKEN_RBRACE, "Expect '}' after block.");
    return node;
}

static ASTNode* switch_statement() {
    ASTNode* node = create_node(AST_SWITCH, parser.previous);
    node->condition = expression();
    consume(TOKEN_LBRACE, "Expect '{' before switch body.");
    
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        if (match_token(TOKEN_CASE)) {
            ASTNode* c_node = create_node(AST_CASE, parser.previous);
            c_node->condition = expression();
            consume(TOKEN_COLON, "Expect ':' after case expression.");
            if (check(TOKEN_LBRACE)) {
                c_node->left = block();
            } else {
                c_node->left = create_node(AST_BLOCK, parser.previous);
                while (!check(TOKEN_CASE) && !check(TOKEN_DEFAULT) && !check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
                    ASTNode* decl = declaration();
                    if (decl) add_child(c_node->left, decl);
                    if (parser.panic_mode) synchronize();
                }
            }
            add_child(node, c_node);
        } else if (match_token(TOKEN_DEFAULT)) {
            ASTNode* d_node = create_node(AST_DEFAULT, parser.previous);
            consume(TOKEN_COLON, "Expect ':' after default.");
            if (check(TOKEN_LBRACE)) {
                d_node->left = block();
            } else {
                d_node->left = create_node(AST_BLOCK, parser.previous);
                while (!check(TOKEN_CASE) && !check(TOKEN_DEFAULT) && !check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
                    ASTNode* decl = declaration();
                    if (decl) add_child(d_node->left, decl);
                    if (parser.panic_mode) synchronize();
                }
            }
            add_child(node, d_node);
        } else {
            error_at_current("Expect 'case' or 'default' inside switch.");
            break;
        }
    }
    consume(TOKEN_RBRACE, "Expect '}' after switch body.");
    return node;
}

static ASTNode* try_statement() {
    ASTNode* node = create_node(AST_TRY, parser.previous);
    node->left = block(); // try body
    
    if (match_token(TOKEN_CATCH)) {
        ASTNode* catch_node = create_node(AST_CATCH, parser.previous);
        if (match_token(TOKEN_LPAREN)) {
            // catch (str error)
            if (is_type()) {
                ASTNode* typeNode = parse_type();
                if (typeNode) {
                    consume(TOKEN_IDENTIFIER, "Expect error variable name.");
                    catch_node->token = parser.previous;
                    catch_node->var_type = typeNode->token;
                    free_ast(typeNode);
                }
            } else {
                consume(TOKEN_IDENTIFIER, "Expect error variable name.");
                catch_node->token = parser.previous;
                catch_node->var_type.type = TOKEN_STR_TYPE;
            }
            consume(TOKEN_RPAREN, "Expect ')' after catch parameter.");
        }
        catch_node->left = block();
        node->right = catch_node;
    }
    
    // Parse finally block (stored as condition of AST_TRY for reuse)
    if (match_token(TOKEN_FINALLY)) {
        node->condition = (ASTNode*)block(); // reuse condition for finally block
    }
    
    return node;
}

static ASTNode* throw_statement() {
    ASTNode* node = create_node(AST_THROW, parser.previous);
    node->left = expression();
    optionally_consume_semicolon();
    return node;
}

static ASTNode* match_statement() {
    ASTNode* node = create_node(AST_MATCH, parser.previous);
    node->condition = expression();
    consume(TOKEN_LBRACE, "Expect '{' before match body.");
    
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        ASTNode* arm = create_node(AST_MATCH_ARM, parser.current);
        
        // Parse pattern(s)
        if (check(TOKEN_IDENTIFIER) && parser.current.length == 1 && parser.current.start[0] == '_') {
            // Wildcard: _
            advance_token();
            arm->token = parser.previous;
            arm->is_const = true; // Flag: is wildcard/default arm
        } else {
            // Value pattern - can have multiple values with comma
            add_child(arm, expression());
            while (match_token(TOKEN_COMMA)) {
                add_child(arm, expression());
            }
        }
        
        consume(TOKEN_FAT_ARROW, "Expect '=>' after match pattern.");
        
        // Parse arm body
        if (check(TOKEN_LBRACE)) {
            arm->left = block();
        } else {
            arm->left = create_node(AST_BLOCK, parser.previous);
            ASTNode* stmt = statement();
            if (stmt) add_child(arm->left, stmt);
        }
        
        add_child(node, arm);
    }
    consume(TOKEN_RBRACE, "Expect '}' after match body.");
    return node;
}

static ASTNode* destructure_declaration() {
    // Parse: Type1 name1, Type2 name2 = expr
    ASTNode* node = create_node(AST_DESTRUCTURE, parser.previous);
    
    // Parse first type and name
    ASTNode* typeNode = parse_type();
    if (!typeNode) {
        free_ast(node);
        return NULL;
    }
    consume(TOKEN_IDENTIFIER, "Expect variable name.");
    ASTNode* var1 = create_node(AST_VAR_DECL, parser.previous);
    var1->var_type = typeNode->token;
    var1->is_array = typeNode->is_array;
    free_ast(typeNode);
    add_child(node, var1);
    
    // Parse additional type-name pairs
    while (match_token(TOKEN_COMMA)) {
        typeNode = parse_type();
        if (!typeNode) {
            free_ast(node);
            return NULL;
        }
        consume(TOKEN_IDENTIFIER, "Expect variable name.");
        ASTNode* var = create_node(AST_VAR_DECL, parser.previous);
        var->var_type = typeNode->token;
        var->is_array = typeNode->is_array;
        free_ast(typeNode);
        add_child(node, var);
    }
    
    // Expect = expr
    consume(TOKEN_EQUAL, "Expect '=' after destructuring pattern.");
    node->right = expression();
    optionally_consume_semicolon();
    
    return node;
}

static ASTNode* statement() {
    if (match_token(TOKEN_SWITCH)) return switch_statement();
    if (match_token(TOKEN_IF)) return if_statement();
    if (match_token(TOKEN_WHILE)) return while_statement();
    if (match_token(TOKEN_FOR)) return for_statement();
    if (match_token(TOKEN_RETURN)) return return_statement();
    if (match_token(TOKEN_BREAK)) return break_statement();
    if (match_token(TOKEN_CONTINUE)) return continue_statement();
    if (match_token(TOKEN_TRY)) return try_statement();
    if (match_token(TOKEN_THROW)) return throw_statement();
    if (match_token(TOKEN_MATCH)) return match_statement();
    if (match_token(TOKEN_ASSERT)) return assert_statement();
    if (check(TOKEN_LBRACE)) return block();
    if (is_type()) {
        // Check if this is a destructuring: Type1 name1, Type2 name2 = expr
        // Save state and check
        Token saved_type = parser.current;
        advance_token(); // consume type
        if (parser.current.type == TOKEN_IDENTIFIER) {
            advance_token(); // consume name
            if (parser.current.type == TOKEN_COMMA) {
                // This is a destructuring declaration
                // Restore and parse
                parser.current = saved_type;
                return destructure_declaration();
            }
            // Not a destructuring, restore and parse as var declaration
            parser.current = saved_type;
        } else {
            parser.current = saved_type;
        }
        return var_declaration(false);
    }
    return expr_stmt();
}

static ASTNode* class_declaration() {
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    Token class_name_token = parser.previous;
    ASTNode* node = create_node(AST_CLASS_DECL, class_name_token);
    
    // Generic type parameters: class Box<T>
    if (match_token(TOKEN_LESS)) {
        do {
            consume_name("Expect type parameter name.");
            ASTNode* tp = create_node(AST_TYPE_PARAM, parser.previous);
            add_type_param(node, tp);
        } while (match_token(TOKEN_COMMA));
        consume(TOKEN_GREATER, "Expect '>' after generic type parameters.");
    }
    
    // Parse 'extends ParentClass'
    if (match_token(TOKEN_EXTENDS)) {
        consume(TOKEN_IDENTIFIER, "Expect parent class name after 'extends'.");
        int len = parser.previous.length;
        node->parent_name = (char*)malloc(len + 1);
        if (node->parent_name) {
            memcpy(node->parent_name, parser.previous.start, len);
            node->parent_name[len] = '\0';
        }
    }
    
    // Parse 'implements Interface1, Interface2, ...'
    if (match_token(TOKEN_IMPLEMENTS)) {
        char buffer[1024] = {0};
        size_t buf_pos = 0;
        do {
            consume(TOKEN_IDENTIFIER, "Expect interface or trait name after 'implements'.");
            int name_len = parser.previous.length;
            if (name_len > 0 && buf_pos + name_len + 1 < sizeof(buffer)) {
                if (buf_pos > 0) buffer[buf_pos++] = ',';
                memcpy(buffer + buf_pos, parser.previous.start, name_len);
                buf_pos += name_len;
            }
        } while (match_token(TOKEN_COMMA));
        if (buf_pos > 0) {
            buffer[buf_pos] = '\0';
            node->implements_names = (char*)malloc(buf_pos + 1);
            if (node->implements_names) {
                memcpy(node->implements_names, buffer, buf_pos + 1);
            }
        }
    }
    
    consume(TOKEN_LBRACE, "Expect '{' before class body.");
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        if (match_token(TOKEN_FUNC)) {
            // Check for constructor: func new(...)
            if (check(TOKEN_NEW)) {
                advance_token(); // consume 'new'
                ASTNode* ctor = create_node(AST_CONSTRUCTOR, parser.previous);
                consume(TOKEN_LPAREN, "Expect '(' after 'new'.");
                if (!check(TOKEN_RPAREN)) {
                    do {
                        ASTNode* typeNode = parse_type();
                        if (!typeNode) break;
                        consume(TOKEN_IDENTIFIER, "Expect parameter name.");
                        ASTNode* param = create_node(AST_VAR_DECL, parser.previous);
                        param->var_type = typeNode->token;
                        param->is_array = typeNode->is_array;
                        free_ast(typeNode);
                        add_child(ctor, param);
                    } while (match_token(TOKEN_COMMA));
                }
                consume(TOKEN_RPAREN, "Expect ')' after constructor parameters.");
                ctor->left = block();
                add_child(node, ctor);
            } else {
                // Check for 'override' keyword before func
                bool is_override = false;
                if (check(TOKEN_OVERRIDE)) {
                    advance_token();
                    is_override = true;
                }
                (void)is_override; // Will be used by semantic analyzer
                
                // Parse method declaration
                ASTNode* method = function_declaration(true);
                if (method) {
                    // Add implicit 'self' parameter as first argument
                    Token self_token = {TOKEN_IDENTIFIER, "self", 4, class_name_token.line};
                    ASTNode* self_param = create_node(AST_VAR_DECL, self_token);
                    self_param->var_type = class_name_token;
                    self_param->is_pointer = true;
                    // Prepend self to parameters
                    ASTNode** old_children = method->children;
                    int old_count = method->child_count;
                    method->children = NULL;
                    method->child_count = 0;
                    method->child_capacity = 0;
                    add_child(method, self_param);
                    for (int i = 0; i < old_count; i++) {
                        add_child(method, old_children[i]);
                    }
                    free(old_children);
                    add_child(node, method);
                }
            }
        } else if (match_token(TOKEN_OVERRIDE)) {
            // override func method(...)
            consume(TOKEN_FUNC, "Expect 'func' after 'override'.");
            ASTNode* method = function_declaration(true);
            if (method) {
                Token self_token = {TOKEN_IDENTIFIER, "self", 4, class_name_token.line};
                ASTNode* self_param = create_node(AST_VAR_DECL, self_token);
                self_param->var_type = class_name_token;
                self_param->is_pointer = true;
                ASTNode** old_children = method->children;
                int old_count = method->child_count;
                method->children = NULL;
                method->child_count = 0;
                method->child_capacity = 0;
                add_child(method, self_param);
                for (int i = 0; i < old_count; i++) {
                    add_child(method, old_children[i]);
                }
                free(old_children);
                add_child(node, method);
            }
        } else {
            ASTNode* decl = var_declaration(false);
            if (decl) add_child(node, decl);
        }
        if (parser.panic_mode) synchronize();
    }
    consume(TOKEN_RBRACE, "Expect '}' after class body.");
    return node;
}

static ASTNode* interface_declaration() {
    consume(TOKEN_IDENTIFIER, "Expect interface name.");
    ASTNode* node = create_node(AST_INTERFACE_DECL, parser.previous);
    consume(TOKEN_LBRACE, "Expect '{' before interface body.");
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        consume(TOKEN_FUNC, "Expect 'func' in interface body.");
        ASTNode* method = function_declaration(false);
        if (method) add_child(node, method);
        if (parser.panic_mode) synchronize();
    }
    consume(TOKEN_RBRACE, "Expect '}' after interface body.");
    return node;
}

static ASTNode* trait_declaration() {
    consume(TOKEN_IDENTIFIER, "Expect trait name.");
    Token trait_name_token = parser.previous;
    ASTNode* node = create_node(AST_TRAIT_DECL, trait_name_token);
    consume(TOKEN_LBRACE, "Expect '{' before trait body.");
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        consume(TOKEN_FUNC, "Expect 'func' in trait body.");
        ASTNode* method = function_declaration(true);
        if (method) {
            // Add implicit 'self' parameter as first argument (like class methods)
            Token self_token = {TOKEN_IDENTIFIER, "self", 4, trait_name_token.line};
            ASTNode* self_param = create_node(AST_VAR_DECL, self_token);
            self_param->var_type = trait_name_token;
            self_param->is_pointer = true;
            ASTNode** old_children = method->children;
            int old_count = method->child_count;
            method->children = NULL;
            method->child_count = 0;
            method->child_capacity = 0;
            add_child(method, self_param);
            for (int i = 0; i < old_count; i++) {
                add_child(method, old_children[i]);
            }
            free(old_children);
            add_child(node, method);
        }
        if (parser.panic_mode) synchronize();
    }
    consume(TOKEN_RBRACE, "Expect '}' after trait body.");
    return node;
}

static ASTNode* struct_declaration() {
    consume(TOKEN_IDENTIFIER, "Expect struct name.");
    ASTNode* node = create_node(AST_STRUCT_DECL, parser.previous);
    consume(TOKEN_LBRACE, "Expect '{' before struct body.");
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        ASTNode* decl = var_declaration(false);
        if (decl) add_child(node, decl);
        if (parser.panic_mode) synchronize();
    }
    consume(TOKEN_RBRACE, "Expect '}' after struct body.");
    return node;
}

static ASTNode* enum_declaration() {
    consume(TOKEN_IDENTIFIER, "Expect enum name.");
    ASTNode* node = create_node(AST_ENUM_DECL, parser.previous);
    consume(TOKEN_LBRACE, "Expect '{' before enum body.");
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        if (match_token(TOKEN_IDENTIFIER)) {
            ASTNode* member = create_node(AST_IDENTIFIER, parser.previous);
            add_child(node, member);
            match_token(TOKEN_COMMA);
        } else {
            error_at_current("Expect enum member name.");
            break;
        }
    }
    consume(TOKEN_RBRACE, "Expect '}' after enum body.");
    return node;
}

static ASTNode* function_declaration(bool require_body) {
    consume(TOKEN_IDENTIFIER, "Expect function name.");
    if (parser.had_error && parser.previous.type != TOKEN_IDENTIFIER) return NULL;
    ASTNode* node = create_node(AST_FUNC_DECL, parser.previous);
    // Generic type parameters: func foo<T, U>(
    if (match_token(TOKEN_LESS)) {
        do {
            consume_name("Expect type parameter name.");
            ASTNode* tp = create_node(AST_TYPE_PARAM, parser.previous);
            add_type_param(node, tp);
        } while (match_token(TOKEN_COMMA));
        consume(TOKEN_GREATER, "Expect '>' after generic type parameters.");
    }
    consume(TOKEN_LPAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RPAREN)) {
        do {
            ASTNode* typeNode = parse_type();
            if (!typeNode) return node;
            // Check for variadic: type... name
            bool variadic = match_token(TOKEN_ELLIPSIS);
            consume(TOKEN_IDENTIFIER, "Expect parameter name.");
            if (parser.had_error && parser.previous.type != TOKEN_IDENTIFIER) {
                free_ast(typeNode);
                return node;
            }
            ASTNode* param = create_node(AST_VAR_DECL, parser.previous);
            param->var_type = typeNode->token;
            param->is_array = typeNode->is_array;
            param->is_variadic = variadic;
            free_ast(typeNode);
            // Check for default value: name = expr
            if (match_token(TOKEN_EQUAL)) {
                param->default_value = expression();
            }
            add_child(node, param);
        } while (match_token(TOKEN_COMMA));
    }
    consume(TOKEN_RPAREN, "Expect ')' after parameters.");
    if (match_token(TOKEN_ARROW)) {
        ASTNode* typeNode = parse_type();
        if (!typeNode) return node;
        node->return_type = typeNode->token;
        free_ast(typeNode);
    }
    if (require_body) {
        node->left = block();
    }
    return node;
}

static ASTNode* test_declaration() {
    consume(TOKEN_STRING_LITERAL, "Expect test description string after 'test'.");
    ASTNode* node = create_node(AST_TEST, parser.previous);
    node->left = block();
    return node;
}

static ASTNode* assert_statement() {
    ASTNode* node = create_node(AST_ASSERT, parser.previous);
    node->left = expression();
    optionally_consume_semicolon();
    return node;
}

static ASTNode* main_block() {
    ASTNode* node = create_node(AST_MAIN, parser.previous);
    node->left = block();
    return node;
}

static ASTNode* import_declaration() {
    ASTNode* node = create_node(AST_IMPORT, parser.previous);
    consume(TOKEN_IDENTIFIER, "Expect module name after import.");
    node->token = parser.previous;
    optionally_consume_semicolon();
    return node;
}

static ASTNode* operator_declaration() {
    // Parse: func operator+(Type other) -> ReturnType { body }
    consume(TOKEN_OPERATOR, "Expect 'operator' keyword.");
    
    // Parse operator token
    Token op_token;
    if (match_token(TOKEN_PLUS) || match_token(TOKEN_MINUS) ||
        match_token(TOKEN_STAR) || match_token(TOKEN_SLASH) ||
        match_token(TOKEN_EQ_EQ) || match_token(TOKEN_BANG_EQ) ||
        match_token(TOKEN_LESS) || match_token(TOKEN_LESS_EQ) ||
        match_token(TOKEN_GREATER) || match_token(TOKEN_GREATER_EQ) ||
        match_token(TOKEN_LBRACKET) || match_token(TOKEN_LBRACE)) {
        op_token = parser.previous;
    } else {
        error_at_current("Expect operator token (+, -, *, /, ==, !=, <, <=, >, >=, [, {).");
        return NULL;
    }
    
    ASTNode* node = create_node(AST_OPERATOR_DECL, op_token);
    node->operator_token = op_token;
    
    consume(TOKEN_LPAREN, "Expect '(' after operator.");
    // Add implicit 'self' parameter
    Token self_token = {TOKEN_IDENTIFIER, "self", 4, op_token.line};
    ASTNode* self_param = create_node(AST_VAR_DECL, self_token);
    add_child(node, self_param);
    if (!check(TOKEN_RPAREN)) {
        do {
            ASTNode* typeNode = parse_type();
            if (!typeNode) return node;
            consume(TOKEN_IDENTIFIER, "Expect parameter name.");
            if (parser.had_error && parser.previous.type != TOKEN_IDENTIFIER) {
                free_ast(typeNode);
                return node;
            }
            ASTNode* param = create_node(AST_VAR_DECL, parser.previous);
            param->var_type = typeNode->token;
            param->is_array = typeNode->is_array;
            free_ast(typeNode);
            add_child(node, param);
        } while (match_token(TOKEN_COMMA));
    }
    consume(TOKEN_RPAREN, "Expect ')' after parameters.");
    
    if (match_token(TOKEN_ARROW)) {
        ASTNode* typeNode = parse_type();
        if (!typeNode) return node;
        node->return_type = typeNode->token;
        // Set self parameter type to match return type
        if (node->child_count > 0) {
            node->children[0]->var_type = typeNode->token;
        }
        free_ast(typeNode);
    }
    
    node->left = block();
    return node;
}

static ASTNode* extend_declaration() {
    // Parse: extend TypeName { func method() { ... } }
    if (check(TOKEN_IDENTIFIER) || is_type()) {
        advance_token();
    } else {
        error_at_current("Expect type name after 'extend'.");
    }
    
    ASTNode* node = create_node(AST_EXTEND_DECL, parser.previous);
    
    consume(TOKEN_LBRACE, "Expect '{' after type name in 'extend'.");
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        if (match_token(TOKEN_FUNC)) {
            ASTNode* method = function_declaration(true);
            if (method) {
                // Add implicit 'self' parameter as first argument
                Token self_token = {TOKEN_IDENTIFIER, "self", 4, node->token.line};
                ASTNode* self_param = create_node(AST_VAR_DECL, self_token);
                self_param->var_type = node->token;
                self_param->is_pointer = false;
                // Prepend self to parameters
                ASTNode** old_children = method->children;
                int old_count = method->child_count;
                method->children = NULL;
                method->child_count = 0;
                method->child_capacity = 0;
                add_child(method, self_param);
                for (int i = 0; i < old_count; i++) {
                    add_child(method, old_children[i]);
                }
                free(old_children);
                add_child(node, method);
            }
        } else {
            error_at_current("Expect 'func' in extend body.");
            break;
        }
        if (parser.panic_mode) synchronize();
    }
    consume(TOKEN_RBRACE, "Expect '}' after extend body.");
    return node;
}

static ASTNode* declaration() {
    if (match_token(TOKEN_CONST)) return var_declaration(true);
    if (match_token(TOKEN_IMPORT)) return import_declaration();
    if (match_token(TOKEN_STRUCT)) return struct_declaration();
    if (match_token(TOKEN_ENUM)) return enum_declaration();
    if (match_token(TOKEN_CLASS)) return class_declaration();
    if (match_token(TOKEN_INTERFACE)) return interface_declaration();
    if (match_token(TOKEN_TRAIT)) return trait_declaration();
    if (match_token(TOKEN_TEST)) return test_declaration();
    if (match_token(TOKEN_FUNC)) {
        // Check if this is an operator declaration
        if (check(TOKEN_OPERATOR)) {
            return operator_declaration();
        }
        return function_declaration(true);
    }
    if (match_token(TOKEN_EXTEND)) return extend_declaration();
    if (match_token(TOKEN_MAIN)) return main_block();
    if (is_type()) return var_declaration(false);
    return statement();
}

static void synchronize() {
    parser.panic_mode = false;
    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_TRAIT:
            case TOKEN_FUNC:
            case TOKEN_EXTEND:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_FOR:
            case TOKEN_RETURN:
            case TOKEN_MAIN:
            case TOKEN_TEST:
            case TOKEN_RBRACE:
            case TOKEN_CASE:
            case TOKEN_DEFAULT:
                return;
            default:
                ;
        }
        advance_token();
    }
}

ASTNode* parse_program(const char* source) {
    parser_source = source;
    init_lexer(source);
    parser.had_error = false;
    parser.panic_mode = false;
    Token eof = {TOKEN_EOF, "", 0, 1};
    parser.current = eof;
    parser.previous = eof;
    advance_token();
    
    ASTNode* program = create_node(AST_PROGRAM, parser.previous);
    while (!match_token(TOKEN_EOF)) {
        ASTNode* decl = declaration();
        if (decl) {
            if (decl->type == AST_EXPR_STMT || decl->type == AST_IF || decl->type == AST_WHILE ||
                decl->type == AST_FOR || decl->type == AST_FOR_IN || decl->type == AST_SWITCH ||
                decl->type == AST_RETURN || decl->type == AST_BREAK || decl->type == AST_CONTINUE ||
                decl->type == AST_ASSIGN || decl->type == AST_BLOCK ||
                decl->type == AST_TRY || decl->type == AST_THROW || decl->type == AST_MATCH ||
                decl->type == AST_ASSERT) {
                error_at(&decl->token, "Statements cannot appear at the top level.");
            } else {
                add_child(program, decl);
            }
        }
        if (parser.panic_mode) synchronize();
    }
    return parser.had_error ? NULL : program;
}
