#ifndef SEMANTICS_INTERNAL_H
#define SEMANTICS_INTERNAL_H

#include "semantics.h"
#include "checked_alloc.h"
#include "ast.h"
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Types (defined in semantics_scope.c) --- */
typedef struct Symbol {
    char* name;
    char* type_name;
    CnextTokenType type;
    bool is_const;
    ASTNode* decl_node;
    struct Symbol* next;
} Symbol;

typedef struct Scope {
    Symbol* symbols;
    struct Scope* parent;
} Scope;

/* --- Global State (defined in semantics_scope.c) --- */
extern const char* semantics_source;
extern Scope* sem_current_scope;
extern bool has_semantic_error;
extern int loop_depth;
extern int switch_depth;
extern int generator_depth;

/* --- Core Functions (semantics_scope.c) --- */
char* copy_cstring(const char* value);
char* sem_copy_token_text(Token token);
void report_error(int line, const char* message, const char* detail);
void report_token_error(Token token, const char* message);
void sem_push_scope(void);
void sem_pop_scope(void);
bool token_matches_name(Token token, const char* name);
Symbol* resolve_symbol_in_scope(Scope* scope, Token token);
Symbol* resolve_current_symbol(Token token);
Symbol* resolve_symbol(Token token);
void define_symbol(Token token, CnextTokenType type, bool is_const, const char* type_name, ASTNode* decl_node);
void define_type_symbol(ASTNode* node, CnextTokenType type);
void define_symbol_if_missing(Token token, CnextTokenType type, bool is_const, const char* type_name);
bool is_builtin_value_type(CnextTokenType type);
bool is_named_type_symbol(CnextTokenType type);
bool validate_type_token(Token token);

/* --- Type Helpers (semantics_types.c) --- */
char* type_name_from_token(Token token);
bool assignment_types_compatible(Token expected, CnextTokenType actual);
void validate_var_initializer(ASTNode* node);
void analyze_var_declaration(ASTNode* node);
void analyze_field_declaration(ASTNode* node);
void register_import(Token module);

/* --- Predeclaration (semantics_predeclare.c) --- */
void predeclare_global(ASTNode* node);

/* --- Statement Analysis (semantics_analyze.c) --- */
void analyze_function(ASTNode* node, bool define_name);
void analyze_assignment(ASTNode* node);
void analyze_postfix(ASTNode* node);
void analyze_node(ASTNode* node);

/* --- Expression Analysis (semantics_expr.c) --- */
CnextTokenType analyze_expression(ASTNode* node);

#endif /* SEMANTICS_INTERNAL_H */
