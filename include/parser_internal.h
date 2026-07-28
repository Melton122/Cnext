#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include "parser.h"
#include "ast.h"
#include "lexer.h"
#include "checked_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/* --- Parser State (defined in parser_core.c) --- */
typedef struct {
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
} Parser;

extern Parser parser;
extern const char* parser_source;

/* --- Core Functions (parser_core.c) --- */
void print_source_line(int line);
void print_caret(Token* token);
void error_at(Token* token, const char* message);
void error(const char* message);
void error_at_current(const char* message);
void advance_token(void);
void consume(CnextTokenType type, const char* message);
bool check(CnextTokenType type);
bool match_token(CnextTokenType type);
void optionally_consume_semicolon(void);
bool check_identifier_text(const char* text, int len);
bool match_identifier_text(const char* text, int len);
char* parse_attribute_name(void);
void synchronize(void);

/* --- Macro System (parser_macro.c) --- */
typedef struct MacroEntry {
    char* name;
    ASTNode* decl;  // AST_MACRO_DECL node
    struct MacroEntry* next;
} MacroEntry;

ASTNode* lookup_macro(const char* name, int length);
void register_macro(ASTNode* decl);
ASTNode* deep_copy_ast(ASTNode* node);
void substitute_params(ASTNode* node, ASTNode** params, int param_count,
                       ASTNode** args, int arg_count);
bool macro_depth_ok(void);
void macro_depth_push(void);
void macro_depth_pop(void);

/* --- Type Parsing (parser_type.c) --- */
bool is_name_token(CnextTokenType type);
void consume_name(const char* message);
bool is_type(void);
ASTNode* parse_type(void);
ASTNode* parse_type_union(void);
bool is_generic_call_lookahead(void);

// Helper: propagate union type info from typeNode to a declaration node
static inline void assign_type_from_node(ASTNode* dest, ASTNode* typeNode) {
    if (typeNode->is_union_type) {
        dest->is_union_type = true;
        // Override var_type to emit CnextVariant in C codegen
        dest->var_type = (Token){TOKEN_IDENTIFIER, "CnextVariant", 12, typeNode->token.line};
    } else {
        dest->var_type = typeNode->token;
        dest->is_array = typeNode->is_array;
    }
}

static inline void assign_return_type_from_node(ASTNode* dest, ASTNode* typeNode) {
    if (typeNode->is_union_type) {
        dest->return_type = (Token){TOKEN_IDENTIFIER, "CnextVariant", 12, typeNode->token.line};
    } else {
        dest->return_type = typeNode->token;
    }
}

/* --- Expression Parsing (parser_expr.c) --- */
ASTNode* expression(void);

/* --- Statement Parsing (parser_stmt.c) --- */
ASTNode* statement(void);
ASTNode* block(void);
ASTNode* var_declaration(bool is_const);
ASTNode* assign_stmt(ASTNode* expr);
ASTNode* expr_stmt(void);

/* --- Declaration Parsing (parser_decl.c) --- */
ASTNode* declaration(void);
ASTNode* function_declaration(bool require_body);
ASTNode* operator_declaration(void);
ASTNode* macro_declaration(void);
ASTNode* bench_statement(void);
ASTNode* assert_statement(void);

#endif /* PARSER_INTERNAL_H */
