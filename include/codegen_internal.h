#ifndef CODEGEN_INTERNAL_H
#define CODEGEN_INTERNAL_H

#include "codegen.h"
#include "ast.h"
#include "sourcemap.h"
#include "checked_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* --- Shared State (defined in codegen.c) --- */
extern FILE* out;
extern FILE* spec_out;
extern FILE* closure_defs_out;
extern int indent_level;
extern const char* current_parent_class;
extern int lambda_counter;
extern bool codegen_test_mode;
extern bool codegen_in_test;
extern int codegen_test_count;
extern ASTNode* program_node;
extern int generator_counter;
extern bool codegen_profile_mode;
extern SourceMap* codegen_sourcemap;
extern int codegen_gen_line;
extern int codegen_last_src_line;

/* --- Counters (defined in codegen_node.c) --- */
extern int bench_counter;
extern int loop_counter;
extern int try_counter;
extern int match_counter;
extern int defer_counter;

/* --- Utility Functions (codegen_emit.c) --- */
const char* type_token_to_c(CnextTokenType type);
void write_indent(void);
void track_source_line(int src_line);
char* copy_token_text(Token token);
void generate_type(Token token, bool is_pointer);
void generate_block(ASTNode* node);
void generate_expression(ASTNode* node);
void generate_node(ASTNode* node);
void generate_function(ASTNode* node, const char* prefix);
ASTNode* find_func_decl(ASTNode* prog, const char* name);
ASTNode* find_class_decl(ASTNode* prog, const char* name);

/* --- String Interpolation (codegen_emit.c) --- */
bool is_valid_interpolation(const char* start, const char* end);
void generate_interp_expression(const char* start, const char* end);
void generate_string_interpolation(const char* start, int length);
void generate_lambda_return_type(ASTNode* body);

/* --- Type Substitution (codegen_types.c) --- */
typedef struct TypeSubst {
    char* param_name;
    Token concrete_type;
    bool is_array;
    struct TypeSubst* next;
} TypeSubst;

extern TypeSubst* type_subst_map;

void push_type_subst(const char* name, Token type, bool is_array);
void pop_type_subst(void);
void clear_type_subst(void);
bool find_type_subst(const char* name, Token* out_type, bool* out_is_array);

/* --- Class Variable Tracking (codegen_types.c) --- */
typedef struct ClassVar {
    char* var_name;
    char* class_name;
    struct ClassVar* next;
} ClassVar;

extern ClassVar* class_vars;

void track_class_var(const char* var_name, int var_len, const char* class_name, int class_len);
const char* lookup_class_var(const char* var_name, int var_len);

/* --- Generic Spec Tracking (codegen_types.c) --- */
typedef struct GenSpec {
    char* mangled_name;
    Token return_type;
    bool return_is_array;
    struct GenSpec* next;
} GenSpec;

extern GenSpec* gen_specs;

bool has_gen_spec(const char* name);
void add_gen_spec(const char* name, Token ret_type, bool ret_is_array);
void free_gen_specs(void);

/* --- Generator Support (codegen_gen.c) --- */
typedef struct GenLocal {
    char* name;
    int name_len;
    CnextTokenType type;
    bool is_pointer;
    bool is_class;
    struct GenLocal* next;
} GenLocal;

void collect_locals_from_block(ASTNode* block, GenLocal** list);
void free_gen_locals(GenLocal* list);
void generate_generator_code(ASTNode* node, const char* func_name);

/* --- Closure Support (codegen_closure.c) --- */
typedef struct CapturedVar {
    char* name;
    CnextTokenType type;
    bool is_pointer;
    bool is_class;
    struct CapturedVar* next;
} CapturedVar;

typedef struct ClosureInfo {
    int id;
    CapturedVar* captures;
    int capture_count;
    struct ClosureInfo* next;
} ClosureInfo;

typedef struct ClosureVar {
    char* var_name;
    int closure_id;
    ASTNode* lambda_node;
    struct ClosureVar* next;
} ClosureVar;

typedef struct ScopeClosureVar {
    char* name;
    struct ScopeClosureVar* next;
} ScopeClosureVar;

typedef struct Scope {
    ScopeClosureVar* closures;
    struct Scope* parent;
} Scope;

extern ClosureInfo* closure_list;
extern int closure_counter;
extern ClosureVar* closure_vars;
extern Scope* current_scope;
extern ASTNode* current_func_params;
extern bool current_func_returns_void;

void push_scope(void);
void pop_scope(void);
void register_scope_closure(const char* name, int len);
bool is_lambda_param(ASTNode* lambda, const char* name, int name_len);
void detect_captures_walk(ASTNode* node, CapturedVar** list, ASTNode* lambda, ASTNode* prog);
bool find_var_type_in_block(ASTNode* block, const char* name, int nlen,
                            CnextTokenType* out_type, bool* out_is_pointer, bool* out_is_class);
ClosureVar* find_closure_var(const char* name);
void free_captures(CapturedVar* list);
bool is_func_param(const char* name, int name_len);
bool is_func_typed_var(const char* name, int name_len);

/* --- Generic Function Specialization (codegen_generic.c) --- */
typedef struct SpecWork {
    ASTNode* func_decl;
    char* mangled_name;
    Token* resolved_args;
    bool* resolved_is_array;
    int resolved_count;
    struct SpecWork* next;
} SpecWork;

extern SpecWork* spec_work_queue;
extern SpecWork* spec_work_last;

char* mangle_generic_name(ASTNode* callee);
bool infer_generic_type_args(ASTNode* call_node, ASTNode* func_decl);
void scan_node_for_generic_calls(ASTNode* node);
void enqueue_spec_work(ASTNode* func_decl, ASTNode* callee);
void process_spec_queue(void);
void free_spec_work_queue(void);
void generate_generic_specialization(ASTNode* func_decl, const char* mangled_name,
                                     Token* resolved_args, bool* resolved_is_array, int resolved_count);

/* --- Generic Class Specialization (codegen_classspec.c) --- */
typedef struct ClassSpecWork {
    ASTNode* class_decl;
    char* mangled_name;
    Token* resolved_args;
    bool* resolved_is_array;
    int resolved_count;
    struct ClassSpecWork* next;
} ClassSpecWork;

extern ClassSpecWork* class_spec_queue;
extern ClassSpecWork* class_spec_last;

void enqueue_class_spec_work(ASTNode* class_decl, ASTNode* new_node);
void generate_generic_class_specialization(ASTNode* class_decl, const char* mangled_name,
                                           Token* resolved_args, bool* resolved_is_array, int resolved_count);
void scan_node_for_new_exprs(ASTNode* node);
void process_class_spec_queue(void);
void free_class_spec_queue(void);

#endif /* CODEGEN_INTERNAL_H */
