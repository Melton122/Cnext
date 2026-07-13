#include "parser_internal.h"

static ASTNode* class_declaration(void);
static ASTNode* interface_declaration(void);
static ASTNode* trait_declaration(void);
static ASTNode* struct_declaration(void);
static ASTNode* enum_declaration(void);
static ASTNode* constexpr_declaration(void);
static ASTNode* test_declaration(void);
static ASTNode* coroutine_declaration(void);
static ASTNode* async_func_declaration(void);
static ASTNode* main_block(void);
static ASTNode* import_declaration(void);
static ASTNode* extend_declaration(void);
static ASTNode* extern_declaration(void);

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
            } else if (check(TOKEN_OPERATOR)) {
                // func operator+(Type other) -> ReturnType { body }
                ASTNode* method = operator_declaration();
                if (method) {
                    // Set self parameter type and pointer status
                    Token self_token = {TOKEN_IDENTIFIER, "self", 4, class_name_token.line};
                    ASTNode* self_param = create_node(AST_VAR_DECL, self_token);
                    self_param->var_type = class_name_token;
                    self_param->is_pointer = true;
                    // Replace existing self param (operator_declaration adds one without proper type)
                    if (method->child_count > 0) {
                        free_ast(method->children[0]);
                        method->children[0] = self_param;
                    }
                    // Mark class-type parameters as pointers (they are passed as pointers in C)
                    for (int j = 1; j < method->child_count; j++) {
                        ASTNode* param = method->children[j];
                        if (param->var_type.type == TOKEN_IDENTIFIER) {
                            param->is_pointer = true;
                            param->is_class = true;
                        }
                    }
                    add_child(node, method);
                }
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

ASTNode* function_declaration(bool require_body) {
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

ASTNode* macro_declaration() {
    consume(TOKEN_IDENTIFIER, "Expect macro name.");
    if (parser.had_error && parser.previous.type != TOKEN_IDENTIFIER) return NULL;
    ASTNode* node = create_node(AST_MACRO_DECL, parser.previous);
    consume(TOKEN_LPAREN, "Expect '(' after macro name.");
    if (!check(TOKEN_RPAREN)) {
        do {
            consume(TOKEN_IDENTIFIER, "Expect macro parameter name.");
            if (parser.had_error && parser.previous.type != TOKEN_IDENTIFIER) {
                free_ast(node);
                return NULL;
            }
            ASTNode* param = create_node(AST_VAR_DECL, parser.previous);
            add_child(node, param);
        } while (match_token(TOKEN_COMMA));
    }
    consume(TOKEN_RPAREN, "Expect ')' after macro parameters.");
    node->left = block();
    register_macro(node);  // Register for compile-time expansion
    return node;
}

static ASTNode* constexpr_declaration() {
    ASTNode* typeNode = parse_type();
    if (!typeNode) return NULL;
    consume(TOKEN_IDENTIFIER, "Expect constant name after type.");
    ASTNode* node = create_node(AST_CONSTEXPR_DECL, parser.previous);
    node->var_type = typeNode->token;
    node->is_array = typeNode->is_array;
    free_ast(typeNode);
    consume(TOKEN_EQUAL, "Expect '=' after constexpr name.");
    node->left = expression();
    optionally_consume_semicolon();
    return node;
}

static ASTNode* test_declaration() {
    consume(TOKEN_STRING_LITERAL, "Expect test description string after 'test'.");
    ASTNode* node = create_node(AST_TEST, parser.previous);
    node->left = block();
    return node;
}

ASTNode* assert_statement() {
    ASTNode* node = create_node(AST_ASSERT, parser.previous);
    node->left = expression();
    optionally_consume_semicolon();
    return node;
}

static ASTNode* coroutine_declaration() {
    // Parse: coroutine func name(params) -> ReturnType { body }
    consume(TOKEN_FUNC, "Expect 'func' after 'coroutine'.");
    ASTNode* node = function_declaration(true);
    if (node) {
        node->type = AST_COROUTINE_DECL;
        node->is_generator = true; // Coroutines use the same state machine as generators
    }
    return node;
}

static ASTNode* async_func_declaration() {
    // Parse: async func name(params) -> ReturnType { body }
    consume(TOKEN_FUNC, "Expect 'func' after 'async'.");
    ASTNode* node = function_declaration(true);
    if (node) {
        node->type = AST_ASYNC_FUNC_DECL;
        // Async functions are regular functions in synchronous cooperative model
    }
    return node;
}

static ASTNode* main_block() {
    ASTNode* node = create_node(AST_MAIN, parser.previous);
    node->left = block();
    return node;
}

static ASTNode* import_declaration() {
    ASTNode* node = create_node(AST_IMPORT, parser.previous);
    // Accept identifier or 'thread' keyword as module name
    if (check(TOKEN_IDENTIFIER) || check(TOKEN_THREAD)) {
        advance_token();
    } else {
        consume(TOKEN_IDENTIFIER, "Expect module name after import.");
    }
    node->token = parser.previous;
    optionally_consume_semicolon();
    return node;
}

ASTNode* operator_declaration() {
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

ASTNode* declaration() {
    char* attr_name = NULL;
    if (check(TOKEN_AT)) {
        attr_name = parse_attribute_name();
    }
    ASTNode* node = NULL;
    if (match_token(TOKEN_CONST)) node = var_declaration(true);
    else if (match_token(TOKEN_CONSTEXPR)) node = constexpr_declaration();
    else if (match_token(TOKEN_IMPORT)) node = import_declaration();
    else if (match_token(TOKEN_STRUCT)) node = struct_declaration();
    else if (match_token(TOKEN_ENUM)) node = enum_declaration();
    else if (match_token(TOKEN_CLASS)) node = class_declaration();
    else if (match_token(TOKEN_INTERFACE)) node = interface_declaration();
    else if (match_token(TOKEN_TRAIT)) node = trait_declaration();
    else if (match_token(TOKEN_TEST)) node = test_declaration();
    else if (match_token(TOKEN_MACRO)) node = macro_declaration();
    else if (match_token(TOKEN_COROUTINE)) node = coroutine_declaration();
    else if (match_token(TOKEN_ASYNC)) node = async_func_declaration();
    else if (match_token(TOKEN_FUNC)) {
        // Check if this is an operator declaration
        if (check(TOKEN_OPERATOR)) {
            node = operator_declaration();
        } else {
            node = function_declaration(true);
        }
    }     else if (match_token(TOKEN_EXTEND)) node = extend_declaration();
    else if (match_token(TOKEN_EXTERN)) node = extern_declaration();
    else if (match_token(TOKEN_MAIN)) node = main_block();
    else if (is_type()) node = var_declaration(false);
    else node = statement();

    if (node && attr_name) {
        node->attribute_name = attr_name;
    } else {
        free(attr_name);
    }
    return node;
}

static ASTNode* extern_declaration() {
    ASTNode* node = create_node(AST_EXTERN_DECL, parser.previous);
    if (match_token(TOKEN_STRING_LITERAL)) {
        // Store optional link target name
    }
    consume(TOKEN_LBRACE, "Expect '{' after 'extern'.");
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        if (match_token(TOKEN_FUNC)) {
            consume(TOKEN_IDENTIFIER, "Expect function name.");
            ASTNode* func = create_node(AST_FUNC_DECL, parser.previous);
            consume(TOKEN_LPAREN, "Expect '(' after function name.");
            if (!check(TOKEN_RPAREN)) {
                do {
                    ASTNode* typeNode = parse_type();
                    bool variadic = match_token(TOKEN_ELLIPSIS);
                    consume(TOKEN_IDENTIFIER, "Expect parameter name.");
                    ASTNode* param = create_node(AST_VAR_DECL, parser.previous);
                    if (typeNode) {
                        param->var_type = typeNode->token;
                        param->is_array = typeNode->is_array;
                        free_ast(typeNode);
                    }
                    param->is_variadic = variadic;
                    add_child(func, param);
                } while (match_token(TOKEN_COMMA));
            }
            consume(TOKEN_RPAREN, "Expect ')' after parameters.");
            if (match_token(TOKEN_COLON)) {
                ASTNode* typeNode = parse_type();
                if (typeNode) {
                    func->return_type = typeNode->token;
                    free_ast(typeNode);
                }
            }
            add_child(node, func);
        }
        optionally_consume_semicolon();
    }
    consume(TOKEN_RBRACE, "Expect '}' after extern block.");
    return node;
}

ASTNode* bench_statement() {
    ASTNode* node = create_node(AST_BENCH_DECL, parser.previous);
    node->left = block();
    return node;
}
