#include "parser_internal.h"

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
                decl->type == AST_ASSERT || decl->type == AST_RESUME_EXPR ||
                decl->type == AST_AWAIT_EXPR || decl->type == AST_RUN_ASYNC) {
                error_at(&decl->token, "Statements cannot appear at the top level.");
            } else {
                add_child(program, decl);
            }
        }
        if (parser.panic_mode) synchronize();
    }
    return parser.had_error ? NULL : program;
}
