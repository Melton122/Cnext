#include "parser_internal.h"

ASTNode* var_declaration(bool is_const) {
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

ASTNode* assign_stmt(ASTNode* expr) {
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

ASTNode* expr_stmt() {
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
        Token saved_type = parser.current;
        advance_token();
        if (parser.current.type == TOKEN_IDENTIFIER) {
            Token name = parser.current;
            advance_token();
            if (match_token(TOKEN_IN)) {
                node->type = AST_FOR_IN;
                ASTNode* vdecl = create_node(AST_VAR_DECL, name);
                vdecl->var_type = saved_type;
                node->init = vdecl;
                node->condition = expression();
                node->left = block();
                return node;
            }
            parser.current = name;
        }
        parser.current = saved_type;
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

static ASTNode* yield_statement() {
    ASTNode* node = create_node(AST_YIELD, parser.previous);
    if (!check(TOKEN_SEMICOLON) && !check(TOKEN_RBRACE)) {
        node->left = expression();
    }
    optionally_consume_semicolon();
    return node;
}

ASTNode* block() {
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
        
        // Guard clause: pattern if condition => body
        if (match_token(TOKEN_IF)) {
            arm->condition = expression();
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

ASTNode* statement() {
    if (match_token(TOKEN_SWITCH)) return switch_statement();
    if (match_token(TOKEN_IF)) return if_statement();
    if (match_token(TOKEN_WHILE)) return while_statement();
    if (match_token(TOKEN_FOR)) return for_statement();
    if (match_token(TOKEN_RETURN)) return return_statement();
    if (match_token(TOKEN_YIELD)) return yield_statement();
    if (match_token(TOKEN_BREAK)) return break_statement();
    if (match_token(TOKEN_CONTINUE)) return continue_statement();
    if (match_token(TOKEN_TRY)) return try_statement();
    if (match_token(TOKEN_THROW)) return throw_statement();
    if (match_token(TOKEN_MATCH)) return match_statement();
    if (match_token(TOKEN_ASSERT)) return assert_statement();
    if (match_token(TOKEN_BENCH)) return bench_statement();
    if (match_token(TOKEN_DEFER)) {
        ASTNode* node = create_node(AST_DEFER, parser.previous);
        node->left = expression();
        optionally_consume_semicolon();
        return node;
    }
    if (match_token(TOKEN_RUN_ASYNC)) {
        // run_async func_name() or run_async func_name(args)
        ASTNode* node = create_node(AST_RUN_ASYNC, parser.previous);
        consume(TOKEN_IDENTIFIER, "Expect function name after 'run_async'.");
        ASTNode* callee = create_node(AST_IDENTIFIER, parser.previous);
        ASTNode* call = create_node(AST_CALL, parser.previous);
        call->left = callee;
        consume(TOKEN_LPAREN, "Expect '(' after function name.");
        if (!check(TOKEN_RPAREN)) {
            do {
                add_child(call, expression());
            } while (match_token(TOKEN_COMMA));
        }
        consume(TOKEN_RPAREN, "Expect ')' after arguments.");
        node->left = call;
        optionally_consume_semicolon();
        return node;
    }
    if (check(TOKEN_RESUME)) {
        // resume co / resume co with val
        ASTNode* node = create_node(AST_RESUME_EXPR, parser.current);
        advance_token(); // consume 'resume'
        node->left = expression(); // the coroutine variable
        if (match_token(TOKEN_WITH)) {
            node->right = expression(); // value to send
        }
        optionally_consume_semicolon();
        return node;
    }
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
