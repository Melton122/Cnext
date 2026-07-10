#include "codegen.h"
#include <stdlib.h>
#include <string.h>

static FILE* out;
static FILE* spec_out = NULL; // For buffering generic specializations
static int indent_level = 0;
static const char* current_parent_class = NULL;
static int lambda_counter = 0;
static bool codegen_test_mode = false;
static bool codegen_in_test = false;
static int codegen_test_count = 0;
static ASTNode* program_node = NULL; // For generic function lookups

// Forward declarations
static ASTNode* find_func_decl(ASTNode* prog, const char* name);
static void generate_generic_specialization(ASTNode* func_decl, const char* mangled_name, Token* resolved_args, bool* resolved_is_array, int resolved_count);
static void enqueue_spec_work(ASTNode* func_decl, ASTNode* callee);
static void process_spec_queue();

// Track class variables for pointer access in string interpolation
typedef struct ClassVar {
    char* var_name;
    char* class_name;
    struct ClassVar* next;
} ClassVar;
static ClassVar* class_vars = NULL;

// Generic type parameter substitution map
typedef struct TypeSubst {
    char* param_name;
    Token concrete_type;
    bool is_array;
    struct TypeSubst* next;
} TypeSubst;
static TypeSubst* type_subst_map = NULL;

static void push_type_subst(const char* name, Token type, bool is_array) {
    TypeSubst* ts = (TypeSubst*)malloc(sizeof(TypeSubst));
    ts->param_name = strdup(name);
    ts->concrete_type = type;
    ts->is_array = is_array;
    ts->next = type_subst_map;
    type_subst_map = ts;
}

static void pop_type_subst() {
    if (type_subst_map) {
        TypeSubst* tmp = type_subst_map;
        type_subst_map = tmp->next;
        free(tmp->param_name);
        free(tmp);
    }
}

static void clear_type_subst() {
    while (type_subst_map) {
        pop_type_subst();
    }
}

static bool find_type_subst(const char* name, Token* out_type, bool* out_is_array) {
    for (TypeSubst* ts = type_subst_map; ts; ts = ts->next) {
        if (strcmp(ts->param_name, name) == 0) {
            *out_type = ts->concrete_type;
            *out_is_array = ts->is_array;
            return true;
        }
    }
    return false;
}

// Track generated generic specializations to avoid duplication
typedef struct GenSpec {
    char* mangled_name;
    Token return_type;
    bool return_is_array;
    struct GenSpec* next;
} GenSpec;
static GenSpec* gen_specs = NULL;

static bool has_gen_spec(const char* name) {
    for (GenSpec* gs = gen_specs; gs; gs = gs->next) {
        if (strcmp(gs->mangled_name, name) == 0) return true;
    }
    return false;
}

static void add_gen_spec(const char* name, Token ret_type, bool ret_is_array) {
    GenSpec* gs = (GenSpec*)malloc(sizeof(GenSpec));
    gs->mangled_name = strdup(name);
    gs->return_type = ret_type;
    gs->return_is_array = ret_is_array;
    gs->next = gen_specs;
    gen_specs = gs;
}

static void free_gen_specs() {
    while (gen_specs) {
        GenSpec* tmp = gen_specs;
        gen_specs = tmp->next;
        free(tmp->mangled_name);
        free(tmp);
    }
}


static void track_class_var(const char* var_name, int var_len, const char* class_name, int class_len) {
    ClassVar* cv = (ClassVar*)malloc(sizeof(ClassVar));
    cv->var_name = strndup(var_name, var_len);
    cv->class_name = strndup(class_name, class_len);
    cv->next = class_vars;
    class_vars = cv;
}

static const char* lookup_class_var(const char* var_name, int var_len) {
    for (ClassVar* cv = class_vars; cv; cv = cv->next) {
        if ((int)strlen(cv->var_name) == var_len && strncmp(cv->var_name, var_name, var_len) == 0) {
            return cv->class_name;
        }
    }
    return NULL;
}

static char* copy_token_text(Token token) {
    size_t length = token.length > 0 ? (size_t)token.length : 0;
    char* copy = (char*)malloc(length + 1);
    if (!copy) {
        fprintf(stderr, "Out of memory.\n");
        exit(70);
    }
    if (length > 0 && token.start) {
        memcpy(copy, token.start, length);
    }
    copy[length] = '\0';
    return copy;
}

static void generate_lambda_return_type(ASTNode* body) {
    if (body->type != AST_BLOCK) {
        if (body->expr_type == TOKEN_INT_TYPE) fprintf(out, "int");
        else if (body->expr_type == TOKEN_FLOAT_TYPE) fprintf(out, "float");
        else if (body->expr_type == TOKEN_STR_TYPE) fprintf(out, "CnextString");
        else if (body->expr_type == TOKEN_BOOL_TYPE) fprintf(out, "bool");
        else if (body->expr_type == TOKEN_CHAR_TYPE) fprintf(out, "char");
        else fprintf(out, "void");
    } else {
        TokenType ret_type = TOKEN_EOF;
        for (int i = 0; i < body->child_count; i++) {
            if (body->children[i]->type == AST_RETURN) {
                if (body->children[i]->left) {
                    ret_type = body->children[i]->left->expr_type;
                }
                break;
            }
        }
        if (ret_type == TOKEN_INT_TYPE) fprintf(out, "int");
        else if (ret_type == TOKEN_FLOAT_TYPE) fprintf(out, "float");
        else if (ret_type == TOKEN_STR_TYPE) fprintf(out, "CnextString");
        else if (ret_type == TOKEN_BOOL_TYPE) fprintf(out, "bool");
        else if (ret_type == TOKEN_CHAR_TYPE) fprintf(out, "char");
        else fprintf(out, "void");
    }
}

static void write_indent() {
    for (int i = 0; i < indent_level; i++) {
        fprintf(out, "    ");
    }
}

static void generate_block(ASTNode* node);
static void generate_node(ASTNode* node);

static void generate_type(Token token, bool is_pointer) {
    if (token.type == TOKEN_INT_TYPE) fprintf(out, "int");
    else if (token.type == TOKEN_FLOAT_TYPE) fprintf(out, "float");
    else if (token.type == TOKEN_STR_TYPE) fprintf(out, "CnextString");
    else if (token.type == TOKEN_BOOL_TYPE) fprintf(out, "bool");
    else if (token.type == TOKEN_CHAR_TYPE) fprintf(out, "char");
    else if (token.type == TOKEN_VAR || token.type == TOKEN_FUNC) fprintf(out, "__auto_type");
    else if (token.type == TOKEN_IDENTIFIER) {
        // Check if this is a type parameter that should be substituted
        char name[256];
        int nlen = token.length < 255 ? token.length : 255;
        strncpy(name, token.start, nlen);
        name[nlen] = '\0';
        Token subst_type;
        bool subst_array;
        if (find_type_subst(name, &subst_type, &subst_array)) {
            generate_type(subst_type, subst_array);
        } else {
            fprintf(out, "%.*s", token.length, token.start);
        }
    }
    else fprintf(out, "void");
    if (is_pointer) fprintf(out, "*");
}

static bool is_valid_interpolation(const char* start, const char* end) {
    const char* c = start;
    bool found_close = false;
    while (c < end) {
        if (*c == '}') {
            found_close = true;
            break;
        }
        if (*c == '"' || *c == '\\') {
            return false;
        }
        c++;
    }
    if (!found_close) return false;
    if (c == start) return false;
    return true;
}

static void generate_interp_expression(const char* start, const char* end) {
    int expr_len = (int)(end - start);
    // Check for self.field pattern
    if (expr_len > 5 && strncmp(start, "self.", 5) == 0) {
        fprintf(out, "self->%.*s", expr_len - 5, start + 5);
        return;
    }
    // Check for var.method(args) pattern - method call on class variable
    const char* paren = NULL;
    const char* dot = NULL;
    for (const char* s = start; s < end; s++) {
        if (*s == '(' && !paren) paren = s;
        if (*s == '.' && !dot && (!paren || s < paren)) dot = s;
    }
    if (dot && paren) {
        int var_len = (int)(dot - start);
        int method_len = (int)(paren - dot - 1);
        const char* class_name = lookup_class_var(start, var_len);
        if (class_name) {
            fprintf(out, "%s_%.*s(", class_name, method_len, dot + 1);
            fprintf(out, "%.*s", var_len, start);
            const char* args = paren + 1;
            int args_len = (int)(end - args - 1);
            if (args_len > 0) {
                fprintf(out, ", %.*s", args_len, args);
            }
            fprintf(out, ")");
            return;
        }
    }
    // Check for var.field pattern - field access on class variable
    if (dot && !paren) {
        int var_len = (int)(dot - start);
        const char* class_name = lookup_class_var(start, var_len);
        if (class_name) {
            fprintf(out, "%.*s->%.*s", var_len, start, expr_len - var_len - 1, dot + 1);
            return;
        }
    }
    // Default: output as-is
    fprintf(out, "%.*s", expr_len, start);
}

static void generate_string_interpolation(const char* start, int length) {
    const char* p = start + 1;
    const char* end = start + length - 1;
    bool has_interp = false;
    for (const char* c = p; c < end; c++) {
        if (*c == '{') {
            if (is_valid_interpolation(c + 1, end)) {
                has_interp = true;
                break;
            }
        }
    }
    
    if (!has_interp) {
        fprintf(out, "(CnextString){(char*)\"%.*s\", sizeof(\"%.*s\") - 1}", (int)(end - p), p, (int)(end - p), p);
        return;
    }
    
    int concat_calls = 0;
    const char* chunk_start = p;
    for (const char* c = p; c < end; c++) {
        if (*c == '{' && is_valid_interpolation(c + 1, end)) {
            if (c > chunk_start) {
                fprintf(out, "cnext_concat((CnextString){(char*)\"%.*s\", sizeof(\"%.*s\") - 1}, ", (int)(c - chunk_start), chunk_start, (int)(c - chunk_start), chunk_start);
                concat_calls++;
            }
            c++;
            const char* var_start = c;
            while (c < end && *c != '}') c++;
            fprintf(out, "cnext_concat(cnext_to_string(");
            generate_interp_expression(var_start, c);
            fprintf(out, "), ");
            concat_calls++;
            chunk_start = c + 1;
        }
    }
    if (chunk_start < end) {
        fprintf(out, "(CnextString){(char*)\"%.*s\", sizeof(\"%.*s\") - 1}", (int)(end - chunk_start), chunk_start, (int)(end - chunk_start), chunk_start);
    } else {
        fprintf(out, "(CnextString){(char*)\"\", 0}");
    }
    for (int i = 0; i < concat_calls; i++) {
        fprintf(out, ")");
    }
}

static void generate_function(ASTNode* node, const char* prefix) {
    generate_type(node->return_type, false);
    if (prefix) {
        fprintf(out, " %s_%.*s(", prefix, node->token.length, node->token.start);
    } else {
        fprintf(out, " %.*s(", node->token.length, node->token.start);
    }
    
    bool has_variadic = false;
    bool first_param = true;
    for (int i = 0; i < node->child_count; i++) {
        if (node->children[i]->is_variadic) {
            has_variadic = true;
            if (!first_param) fprintf(out, ", ");
            generate_type(node->children[i]->var_type, node->children[i]->is_pointer);
            fprintf(out, " %.*s", node->children[i]->token.length, node->children[i]->token.start);
            fprintf(out, ", ...");
            first_param = false;
        } else {
            if (!first_param) fprintf(out, ", ");
            generate_type(node->children[i]->var_type, node->children[i]->is_pointer);
            fprintf(out, " %.*s", node->children[i]->token.length, node->children[i]->token.start);
            first_param = false;
        }
    }
    fprintf(out, ") ");
    generate_block(node->left);
    fprintf(out, "\n");
}

// Find a function declaration by name in the program AST
static ASTNode* find_func_decl(ASTNode* prog, const char* name) {
    if (!prog) return NULL;
    for (int i = 0; i < prog->child_count; i++) {
        ASTNode* child = prog->children[i];
        if (child->type == AST_FUNC_DECL &&
            (int)strlen(name) == child->token.length &&
            strncmp(name, child->token.start, child->token.length) == 0) {
            return child;
        }
    }
    return NULL;
}

// Build a mangled name for a generic specialization: func_name_type1_type2...
static char* mangle_generic_name(ASTNode* callee) {
    char base[2048] = {0};
    snprintf(base, sizeof(base), "%.*s", callee->token.length, callee->token.start);
    for (int i = 0; i < callee->type_arg_count; i++) {
        char arg_str[512] = {0};
        ASTNode* arg = callee->type_args[i];
        char type_name[256] = {0};
        int tlen = arg->token.length < 255 ? arg->token.length : 255;
        strncpy(type_name, arg->token.start, tlen);
        type_name[tlen] = '\0';
        // Check if type arg is a type parameter that should be substituted
        Token subst_type;
        bool subst_array;
        if (find_type_subst(type_name, &subst_type, &subst_array)) {
            snprintf(arg_str, sizeof(arg_str), "_%.*s", subst_type.length, subst_type.start);
        } else {
            snprintf(arg_str, sizeof(arg_str), "_%s", type_name);
        }
        strncat(base, arg_str, sizeof(base) - strlen(base) - 1);
    }
    return strdup(base);
}

// Work queue for generic specializations
typedef struct SpecWork {
    ASTNode* func_decl;
    char* mangled_name;
    Token* resolved_args;
    bool* resolved_is_array;
    int resolved_count;
    struct SpecWork* next;
} SpecWork;
static SpecWork* spec_work_queue = NULL;
static SpecWork* spec_work_last = NULL;

// Scan an AST subtree for generic calls and enqueue their specializations
static void scan_node_for_generic_calls(ASTNode* node) {
    if (!node) return;
    if (node->type == AST_CALL && node->left && node->left->type_arg_count > 0) {
        char fname[256];
        int flen = node->left->token.length < 255 ? node->left->token.length : 255;
        strncpy(fname, node->left->token.start, flen);
        fname[flen] = '\0';
        ASTNode* fdecl = find_func_decl(program_node, fname);
        if (fdecl && fdecl->type_param_count > 0) {
            enqueue_spec_work(fdecl, node->left);
        }
    }
    scan_node_for_generic_calls(node->left);
    scan_node_for_generic_calls(node->right);
    scan_node_for_generic_calls(node->condition);
    scan_node_for_generic_calls(node->init);
    scan_node_for_generic_calls(node->increment);
    for (int i = 0; i < node->child_count; i++) {
        scan_node_for_generic_calls(node->children[i]);
    }
}

static void enqueue_spec_work(ASTNode* func_decl, ASTNode* callee) {
    char* mangled = mangle_generic_name(callee);
    if (has_gen_spec(mangled)) {
        free(mangled);
        return;
    }
    // Resolve the return type through the callee's type arguments (not the global type_subst_map,
    // which may have conflicting names from outer contexts)
    Token ret_type = func_decl->return_type;
    bool ret_array = false;
    if (ret_type.type == TOKEN_IDENTIFIER) {
        // Find which type parameter index the return type refers to
        int tp_idx = -1;
        for (int ti = 0; ti < func_decl->type_param_count; ti++) {
            int plen = func_decl->type_params[ti]->token.length;
            if (plen == ret_type.length && strncmp(func_decl->type_params[ti]->token.start, ret_type.start, plen) == 0) {
                tp_idx = ti;
                break;
            }
        }
        if (tp_idx >= 0 && tp_idx < (callee ? callee->type_arg_count : 0)) {
            // Resolve the callee's type arg at that position through find_type_subst
            ASTNode* ta = callee->type_args[tp_idx];
            char tname[256] = {0};
            int tlen = ta->token.length < 255 ? ta->token.length : 255;
            strncpy(tname, ta->token.start, tlen);
            Token subst_t; bool subst_a;
            if (ta->token.type == TOKEN_IDENTIFIER && find_type_subst(tname, &subst_t, &subst_a)) {
                ret_type = subst_t;
                ret_array = subst_a;
            } else {
                ret_type = ta->token;
                ret_array = ta->is_array;
            }
        }
    }
    add_gen_spec(mangled, ret_type, ret_array);
    
    int acount = callee ? callee->type_arg_count : 0;
    Token* res_args = NULL;
    bool* res_is_array = NULL;
    if (acount > 0) {
        res_args = (Token*)malloc(sizeof(Token) * acount);
        res_is_array = (bool*)malloc(sizeof(bool) * acount);
        for (int i = 0; i < acount; i++) {
            ASTNode* ta = callee->type_args[i];
            char tname[256] = {0};
            int tlen = ta->token.length < 255 ? ta->token.length : 255;
            strncpy(tname, ta->token.start, tlen);
            tname[tlen] = '\0';
            Token subst_type; bool subst_array;
            if (ta->token.type == TOKEN_IDENTIFIER && find_type_subst(tname, &subst_type, &subst_array)) {
                res_args[i] = subst_type;
                res_is_array[i] = subst_array;
            } else {
                res_args[i] = ta->token;
                res_is_array[i] = ta->is_array;
            }
        }
    }
    
    SpecWork* sw = (SpecWork*)malloc(sizeof(SpecWork));
    sw->func_decl = func_decl;
    sw->mangled_name = mangled;
    sw->resolved_args = res_args;
    sw->resolved_is_array = res_is_array;
    sw->resolved_count = acount;
    sw->next = NULL;
    if (spec_work_last) {
        spec_work_last->next = sw;
    } else {
        spec_work_queue = sw;
    }
    spec_work_last = sw;
}

static void process_spec_queue() {
    while (spec_work_queue) {
        SpecWork* sw = spec_work_queue;
        spec_work_queue = sw->next;
        if (!spec_work_queue) spec_work_last = NULL;
        generate_generic_specialization(sw->func_decl, sw->mangled_name, sw->resolved_args, sw->resolved_is_array, sw->resolved_count);
        if (sw->resolved_args) free(sw->resolved_args);
        if (sw->resolved_is_array) free(sw->resolved_is_array);
        free(sw->mangled_name);
        free(sw);
    }
}

static void free_spec_work_queue() {
    while (spec_work_queue) {
        SpecWork* sw = spec_work_queue;
        spec_work_queue = sw->next;
        if (sw->resolved_args) free(sw->resolved_args);
        if (sw->resolved_is_array) free(sw->resolved_is_array);
        free(sw);
    }
    spec_work_last = NULL;
}

// Generate a specialized version of a generic function
static void generate_generic_specialization(ASTNode* func_decl, const char* mangled_name, Token* resolved_args, bool* resolved_is_array, int resolved_count) {
    // Set up type parameter substitution using pre-resolved concrete types
    for (int i = 0; i < func_decl->type_param_count && i < resolved_count; i++) {
        ASTNode* tp = func_decl->type_params[i];
        push_type_subst(
            strndup(tp->token.start, tp->token.length),
            resolved_args[i],
            resolved_is_array[i]
        );
    }
    
    // Generate the specialized function with mangled name
    // Not using 'static' so that calls before definition work with GNU11 implicit declarations
    generate_type(func_decl->return_type, false);
    fprintf(out, " %s(", mangled_name);
    for (int i = 0; i < func_decl->child_count; i++) {
        ASTNode* param = func_decl->children[i];
        generate_type(param->var_type, param->is_pointer);
        fprintf(out, " %.*s", param->token.length, param->token.start);
        if (i < func_decl->child_count - 1) fprintf(out, ", ");
    }
    fprintf(out, ") ");
    // Generate body with type substitution active
    generate_block(func_decl->left);
    fprintf(out, "\n");
    
    // Clean up type substitution
    for (int i = 0; i < func_decl->type_param_count && i < resolved_count; i++) {
        pop_type_subst();
    }
}

static void generate_expression(ASTNode* node) {
    if (!node) return;
    switch (node->type) {
        case AST_LITERAL:
            if (node->token.type == TOKEN_STRING_LITERAL) {
                generate_string_interpolation(node->token.start, node->token.length);
            } else if (node->token.type == TOKEN_TRUE) {
                fprintf(out, "true");
            } else if (node->token.type == TOKEN_FALSE) {
                fprintf(out, "false");
            } else {
                fprintf(out, "%.*s", node->token.length, node->token.start);
            }
            break;
        case AST_LITERAL_ARRAY:
            fprintf(out, "{");
            for (int i = 0; i < node->child_count; i++) {
                generate_expression(node->children[i]);
                if (i < node->child_count - 1) fprintf(out, ", ");
            }
            fprintf(out, "}");
            break;
        case AST_TUPLE:
            // Generate tuple as anonymous struct compound literal with .e0, .e1, ... fields
            fprintf(out, "({ struct { ");
            for (int i = 0; i < node->child_count; i++) {
                fprintf(out, "long e%d; ", i);
            }
            fprintf(out, "} _tup; ");
            for (int i = 0; i < node->child_count; i++) {
                fprintf(out, "_tup.e%d = (long)(", i);
                generate_expression(node->children[i]);
                fprintf(out, "); ");
            }
            fprintf(out, "_tup; })");
            break;
        case AST_OPTION_SOME:
            fprintf(out, "({ struct { int tag; long value; } _opt; _opt.tag = 1; _opt.value = 0; _opt; })");
            break;
        case AST_OPTION_NONE:
            fprintf(out, "({ struct { int tag; long value; } _opt; _opt.tag = 0; _opt.value = 0; _opt; })");
            break;
        case AST_RESULT_OK:
            fprintf(out, "({ struct { int tag; long value; } _res; _res.tag = 0; _res.value = 0; _res; })");
            break;
        case AST_RESULT_ERR:
            fprintf(out, "({ struct { int tag; long value; } _res; _res.tag = 1; _res.value = 0; _res; })");
            break;
        case AST_IDENTIFIER:
            fprintf(out, "%.*s", node->token.length, node->token.start);
            break;
        case AST_MEMBER_ACCESS:
            generate_expression(node->left);
            if (node->is_pointer_access || (node->left && node->left->type == AST_IDENTIFIER &&
                node->left->token.length == 4 &&
                strncmp(node->left->token.start, "self", 4) == 0)) {
                fprintf(out, "->%.*s", node->token.length, node->token.start);
            } else {
                fprintf(out, ".%.*s", node->token.length, node->token.start);
            }
            break;
        case AST_INDEX:
            fprintf(out, "CNEXT_ARRAY_IDX(");
            generate_expression(node->left);
            fprintf(out, ", ");
            generate_expression(node->right);
            fprintf(out, ")");
            break;
        case AST_SLICE:
            fprintf(out, "CNEXT_SLICE(");
            generate_expression(node->left);
            fprintf(out, ", ");
            if (node->init) {
                generate_expression(node->init);
            } else {
                fprintf(out, "0");
            }
            fprintf(out, ", ");
            if (node->right) {
                generate_expression(node->right);
            } else {
                generate_expression(node->left);
                fprintf(out, ".length");
            }
            fprintf(out, ")");
            break;
        case AST_POSTFIX:
            generate_expression(node->left);
            fprintf(out, "%.*s", node->token.length, node->token.start);
            break;
        case AST_CALL: {
            if (node->left->type == AST_SUPER_EXPR) {
                if (current_parent_class && current_parent_class[0] != '\0') {
                    fprintf(out, "%s_%.*s((%s*)self", current_parent_class,
                        node->left->token.length, node->left->token.start,
                        current_parent_class);
                    for (int i = 0; i < node->child_count; i++) {
                        fprintf(out, ", ");
                        generate_expression(node->children[i]);
                    }
                    fprintf(out, ")");
                } else {
                    fprintf(out, "/* invalid super call */");
                }
            } else if (node->left->type == AST_MEMBER_ACCESS) {
                if (node->left->type_name && node->left->type_name[0] != '\0') {
                    // Class method call or extension method: TypeName_method(object, args)
                    fprintf(out, "%s_%.*s(", node->left->type_name,
                        node->left->token.length, node->left->token.start);
                    // For extension methods on built-in types, pass by value (no &)
                    bool is_builtin_ext = (strncmp(node->left->type_name, "str", 3) == 0 ||
                                           strncmp(node->left->type_name, "int", 3) == 0 ||
                                           strncmp(node->left->type_name, "float", 5) == 0 ||
                                           strncmp(node->left->type_name, "bool", 4) == 0 ||
                                           strncmp(node->left->type_name, "char", 4) == 0);
                    if (!is_builtin_ext && !node->left->is_pointer_access) {
                        fprintf(out, "&");
                    }
                    generate_expression(node->left->left);
                    if (node->child_count > 0) {
                        fprintf(out, ", ");
                        for (int i = 0; i < node->child_count; i++) {
                            generate_expression(node->children[i]);
                            if (i < node->child_count - 1) fprintf(out, ", ");
                        }
                    }
                    fprintf(out, ")");
                } else {
                    // Module function call: prefix function names for modules whose C API uses
                    // the module name as prefix (e.g., json, math, random, os, time, etc.)
                    bool is_prefixed = false;
                    if (node->left->left && node->left->left->type == AST_IDENTIFIER) {
                        // Modules that prefix their C functions with the module name
                        static const struct { const char* name; int len; } prefixed[] = {
                            {"json", 4}, {"math", 4}, {"os", 2}, {"time", 4},
                            {"regex", 5}, {"crypto", 6}, {"path", 4},
                            {"process", 7}, {"random", 6}, {NULL, 0}
                        };
                        for (int m = 0; prefixed[m].name; m++) {
                            if (node->left->left->token.length == prefixed[m].len &&
                                strncmp(node->left->left->token.start, prefixed[m].name, prefixed[m].len) == 0) {
                                is_prefixed = true;
                                fprintf(out, "%s_%.*s(", prefixed[m].name, node->left->token.length, node->left->token.start);
                                break;
                            }
                        }
                    }
                    if (!is_prefixed) {
                        fprintf(out, "%.*s(", node->left->token.length, node->left->token.start);
                    }
                    for (int i = 0; i < node->child_count; i++) {
                        generate_expression(node->children[i]);
                        if (i < node->child_count - 1) fprintf(out, ", ");
                    }
                    fprintf(out, ")");
                }
            } else {
                bool is_printin = (node->left->token.length == 7 && strncmp(node->left->token.start, "printin", 7) == 0);
                bool is_input = (node->left->token.length == 5 && strncmp(node->left->token.start, "input", 5) == 0);
                bool is_free = (node->left->token.length == 4 && strncmp(node->left->token.start, "free", 4) == 0);
                
                if (is_printin) {
                    fprintf(out, "printin(");
                    if (node->child_count > 0) generate_expression(node->children[0]);
                    fprintf(out, ")");
                } else if (is_input) {
                    fprintf(out, "cnext_input(");
                    if (node->child_count > 0) generate_expression(node->children[0]);
                    fprintf(out, ")");
                } else if (is_free) {
                    fprintf(out, "cnext_free(");
                    if (node->child_count > 0) generate_expression(node->children[0]);
                    fprintf(out, ")");
                } else {
                    // Check for generic function call (callee has type_args)
                    if (node->left && node->left->type_arg_count > 0) {
                        // Find func decl and enqueue specialization work
                        char fname[256];
                        int flen = node->left->token.length < 255 ? node->left->token.length : 255;
                        strncpy(fname, node->left->token.start, flen);
                        fname[flen] = '\0';
                        ASTNode* fdecl = find_func_decl(program_node, fname);
                        if (fdecl && fdecl->type_param_count > 0) {
                            enqueue_spec_work(fdecl, node->left);
                        }
                        char* mangled = mangle_generic_name(node->left);
                        fprintf(out, "%s(", mangled);
                        free(mangled);
                    } else {
                        fprintf(out, "%.*s(", node->left->token.length, node->left->token.start);
                    }
                    
                    // Find function declaration for default/named argument handling
                    ASTNode* func_decl = NULL;
                    if (node->left && node->left->type == AST_IDENTIFIER) {
                        char fname[256];
                        int flen = node->left->token.length < 255 ? node->left->token.length : 255;
                        strncpy(fname, node->left->token.start, flen);
                        fname[flen] = '\0';
                        func_decl = find_func_decl(program_node, fname);
                    }
                    
                    // Generate arguments, handling named and default arguments
                    bool first_arg = true;
                    if (func_decl && func_decl->type == AST_FUNC_DECL) {
                        // Track which arguments have been provided
                        bool* provided = (bool*)calloc(func_decl->child_count, sizeof(bool));
                        ASTNode** named_args = (ASTNode**)calloc(node->child_count, sizeof(ASTNode*));
                        int named_count = 0;
                        
                        // Separate positional and named arguments
                        for (int i = 0; i < node->child_count; i++) {
                            if (node->children[i]->type == AST_NAMED_ARG) {
                                named_args[named_count++] = node->children[i];
                            }
                        }
                        
                        // Generate positional arguments
                        int pos_idx = 0;
                        for (int i = 0; i < node->child_count; i++) {
                            if (node->children[i]->type == AST_NAMED_ARG) continue;
                            if (!first_arg) fprintf(out, ", ");
                            generate_expression(node->children[i]);
                            first_arg = false;
                            if (pos_idx < func_decl->child_count) {
                                provided[pos_idx] = true;
                            }
                            pos_idx++;
                        }
                        
                        // Generate remaining parameters with defaults or named values
                        for (int i = 0; i < func_decl->child_count; i++) {
                            if (provided[i]) continue;
                            
                            // Check if this parameter has a named argument
                            bool found_named = false;
                            for (int j = 0; j < named_count; j++) {
                                if (named_args[j]->named_arg_name.length == func_decl->children[i]->token.length &&
                                    strncmp(named_args[j]->named_arg_name.start, func_decl->children[i]->token.start, 
                                           func_decl->children[i]->token.length) == 0) {
                                    if (!first_arg) fprintf(out, ", ");
                                    generate_expression(named_args[j]->right);
                                    first_arg = false;
                                    found_named = true;
                                    break;
                                }
                            }
                            
                            // Use default value if no named argument found
                            if (!found_named && func_decl->children[i]->default_value) {
                                if (!first_arg) fprintf(out, ", ");
                                generate_expression(func_decl->children[i]->default_value);
                                first_arg = false;
                            }
                        }
                        
                        free(provided);
                        free(named_args);
                    } else {
                        // No func decl found, just generate all arguments
                        for (int i = 0; i < node->child_count; i++) {
                            if (!first_arg) fprintf(out, ", ");
                            if (node->children[i]->type == AST_NAMED_ARG) {
                                generate_expression(node->children[i]->right);
                            } else {
                                generate_expression(node->children[i]);
                            }
                            first_arg = false;
                        }
                    }
                    fprintf(out, ")");
                }
            }
            break;
        }
        case AST_UNARY:
            fprintf(out, "%.*s", node->token.length, node->token.start);
            generate_expression(node->right);
            break;
        case AST_BINARY:
            if ((node->token.type == TOKEN_EQ_EQ || node->token.type == TOKEN_BANG_EQ) &&
                ((node->left && node->left->expr_type == TOKEN_STR_TYPE) ||
                 (node->right && node->right->expr_type == TOKEN_STR_TYPE))) {
                if (node->token.type == TOKEN_BANG_EQ) fprintf(out, "!");
                fprintf(out, "cnext_str_eq(");
                generate_expression(node->left);
                fprintf(out, ", ");
                generate_expression(node->right);
                fprintf(out, ")");
            } else if (node->expr_type == TOKEN_STR_TYPE && node->token.type == TOKEN_PLUS) {
                fprintf(out, "cnext_concat(cnext_to_string(");
                generate_expression(node->left);
                fprintf(out, "), cnext_to_string(");
                generate_expression(node->right);
                fprintf(out, "))");
            } else if (node->left && node->left->type_name && node->left->type_name[0] != '\0' &&
                       strncmp(node->left->type_name, "str", 3) != 0 &&
                       strncmp(node->left->type_name, "int", 3) != 0 &&
                       strncmp(node->left->type_name, "float", 5) != 0 &&
                       strncmp(node->left->type_name, "bool", 4) != 0 &&
                       strncmp(node->left->type_name, "char", 4) != 0) {
                // Operator overloading: TypeName_operator_op(&left, right)
                const char* op_name = "op";
                if (node->token.type == TOKEN_PLUS) op_name = "plus";
                else if (node->token.type == TOKEN_MINUS) op_name = "minus";
                else if (node->token.type == TOKEN_STAR) op_name = "star";
                else if (node->token.type == TOKEN_SLASH) op_name = "slash";
                else if (node->token.type == TOKEN_EQ_EQ) op_name = "eq";
                else if (node->token.type == TOKEN_BANG_EQ) op_name = "neq";
                else if (node->token.type == TOKEN_LESS) op_name = "lt";
                else if (node->token.type == TOKEN_LESS_EQ) op_name = "lte";
                else if (node->token.type == TOKEN_GREATER) op_name = "gt";
                else if (node->token.type == TOKEN_GREATER_EQ) op_name = "gte";
                fprintf(out, "%s_operator_%s(", node->left->type_name, op_name);
                fprintf(out, "&");
                generate_expression(node->left);
                fprintf(out, ", ");
                generate_expression(node->right);
                fprintf(out, ")");
            } else {
                generate_expression(node->left);
                fprintf(out, " %.*s ", node->token.length, node->token.start);
                generate_expression(node->right);
            }
            break;
        case AST_ASSIGN:
            generate_expression(node->left);
            fprintf(out, " %.*s ", node->token.length, node->token.start);
            generate_expression(node->right);
            break;
        case AST_NEW_EXPR: {
            // new ClassName(args) -> heap allocate + call constructor
            fprintf(out, "({ %.*s* _obj = (%.*s*)malloc(sizeof(%.*s)); _cnext_track(_obj); ",
                node->token.length, node->token.start,
                node->token.length, node->token.start,
                node->token.length, node->token.start);
            // Call constructor if it exists: ClassName_new(_obj, args)
            fprintf(out, "%.*s_new(_obj", node->token.length, node->token.start);
            for (int i = 0; i < node->child_count; i++) {
                fprintf(out, ", ");
                generate_expression(node->children[i]);
            }
            fprintf(out, "); _obj; })");
            break;
        }
        case AST_SUPER_EXPR:
            // super.method -> ParentClass_method
            fprintf(out, "%.*s", node->token.length, node->token.start);
            break;
        case AST_NULL_LITERAL:
            if (node->expr_type == TOKEN_STR_TYPE) {
                fprintf(out, "(CnextString){NULL, 0}");
            } else {
                fprintf(out, "NULL");
            }
            break;
        case AST_CAST:
            if (node->var_type.type == TOKEN_STR_TYPE) {
                fprintf(out, "cnext_to_string(");
                generate_expression(node->left);
                fprintf(out, ")");
            } else {
                fprintf(out, "((");
                generate_type(node->var_type, false);
                fprintf(out, ")(");
                generate_expression(node->left);
                fprintf(out, "))");
            }
            break;
        case AST_LAMBDA: {
            int current_lambda = lambda_counter++;
            fprintf(out, "({ ");
            generate_lambda_return_type(node->left);
            fprintf(out, " _lambda_%d(", current_lambda);
            for (int i = 0; i < node->child_count; i++) {
                generate_type(node->children[i]->var_type, false);
                fprintf(out, " %.*s", node->children[i]->token.length, node->children[i]->token.start);
                if (i < node->child_count - 1) fprintf(out, ", ");
            }
            fprintf(out, ") ");
            if (node->left->type == AST_BLOCK) {
                generate_block(node->left);
            } else {
                fprintf(out, "{ return ");
                generate_expression(node->left);
                fprintf(out, "; }");
            }
            fprintf(out, " _lambda_%d; })", current_lambda);
            break;
        }
        default:
            break;
    }
}

static void generate_block(ASTNode* node) {
    fprintf(out, "{\n");
    indent_level++;
    for (int i = 0; i < node->child_count; i++) {
        write_indent();
        generate_node(node->children[i]);
    }
    indent_level--;
    write_indent();
    fprintf(out, "}\n");
}

static void generate_node(ASTNode* node) {
    if (!node) return;
    switch (node->type) {
        case AST_CLASS_DECL:
        {
            const char* previous_parent_class = current_parent_class;
            current_parent_class = node->parent_name;

            fprintf(out, "typedef struct {\n");
            indent_level++;
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_VAR_DECL) {
                    write_indent();
                    generate_type(node->children[i]->var_type, false);
                    fprintf(out, " %.*s;\n", node->children[i]->token.length, node->children[i]->token.start);
                }
            }
            indent_level--;
            fprintf(out, "} %.*s;\n\n", node->token.length, node->token.start);
            
            // Generate constructor if present
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_CONSTRUCTOR) {
                    char* prefix = copy_token_text(node->token);
                    // Generate: void ClassName_new(ClassName* self, params) { body }
                    fprintf(out, "void %s_new(%s* self", prefix, prefix);
                    for (int j = 0; j < node->children[i]->child_count; j++) {
                        fprintf(out, ", ");
                        generate_type(node->children[i]->children[j]->var_type, false);
                        fprintf(out, " %.*s", node->children[i]->children[j]->token.length,
                            node->children[i]->children[j]->token.start);
                    }
                    fprintf(out, ") ");
                    generate_block(node->children[i]->left);
                    fprintf(out, "\n");
                    free(prefix);
                }
            }
            
            // Generate methods (fix self parameter for trait-inherited methods)
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_FUNC_DECL) {
                    char* prefix = copy_token_text(node->token);
                    // Override self parameter type to the class name (handles trait inheritance)
                    if (node->children[i]->child_count > 0 && node->children[i]->children[0]) {
                        Token saved_self_type = node->children[i]->children[0]->var_type;
                        node->children[i]->children[0]->var_type = node->token;
                        generate_function(node->children[i], prefix);
                        node->children[i]->children[0]->var_type = saved_self_type;
                    } else {
                        generate_function(node->children[i], prefix);
                    }
                    free(prefix);
                }
            }
            current_parent_class = previous_parent_class;
            break;
        }
        case AST_FUNC_DECL:
            // Skip generic function declarations (specializations are generated on demand)
            if (node->type_param_count == 0) {
                generate_function(node, NULL);
            }
            break;
        case AST_STRUCT_DECL:
            fprintf(out, "typedef struct {\n");
            indent_level++;
            for (int i = 0; i < node->child_count; i++) {
                write_indent();
                generate_type(node->children[i]->var_type, false);
                fprintf(out, " %.*s;\n", node->children[i]->token.length, node->children[i]->token.start);
            }
            indent_level--;
            fprintf(out, "} %.*s;\n", node->token.length, node->token.start);
            break;
        case AST_ENUM_DECL:
            fprintf(out, "typedef enum {\n");
            indent_level++;
            for (int i = 0; i < node->child_count; i++) {
                write_indent();
                fprintf(out, "%.*s", node->children[i]->token.length, node->children[i]->token.start);
                if (i < node->child_count - 1) fprintf(out, ",");
                fprintf(out, "\n");
            }
            indent_level--;
            fprintf(out, "} %.*s;\n", node->token.length, node->token.start);
            break;
        case AST_IMPORT: {
            // Only generate #include for standard library modules
            static const char* std_modules[] = {"io", "file", "net", "json", "math", "os", "string_utils", "time", "regex", "collections", "crypto", "path", "encoding", "process", "random", NULL};
            bool is_std = false;
            for (int i = 0; std_modules[i]; i++) {
                int len = (int)strlen(std_modules[i]);
                if (node->token.length == len &&
                    strncmp(node->token.start, std_modules[i], len) == 0) {
                    is_std = true;
                    break;
                }
            }
            if (is_std) {
                fprintf(out, "#include \"%.*s.h\"\n", node->token.length, node->token.start);
            }
            break;
        }
        case AST_VAR_DECL:
            if (node->is_const) fprintf(out, "const ");
            if (node->is_array) {
                fprintf(out, "struct { ");
                generate_type(node->var_type, false);
                fprintf(out, "* data; size_t length; } %.*s", node->token.length, node->token.start);
                if (node->init && node->init->type == AST_LITERAL_ARRAY) {
                    fprintf(out, " = { (");
                    generate_type(node->var_type, false);
                    fprintf(out, "[])");
                    generate_expression(node->init);
                    fprintf(out, ", %d }", node->init->child_count);
                } else if (node->init && node->init->type == AST_SLICE) {
                    fprintf(out, ";\n");
                    write_indent();
                    // Emit bounds check + slice using a do-while macro that writes to destination
                    fprintf(out, "CNEXT_SLICE_TO(%.*s, ", node->token.length, node->token.start);
                    generate_expression(node->init->left);
                    fprintf(out, ", ");
                    if (node->init->init) {
                        generate_expression(node->init->init);
                    } else {
                        fprintf(out, "0");
                    }
                    fprintf(out, ", ");
                    if (node->init->right) {
                        generate_expression(node->init->right);
                    } else {
                        generate_expression(node->init->left);
                        fprintf(out, ".length");
                    }
                    fprintf(out, ")");
                }
                fprintf(out, ";\n");
            } else {
                generate_type(node->var_type, node->is_pointer || node->is_class);
                fprintf(out, " %.*s", node->token.length, node->token.start);
                if (node->init) {
                    fprintf(out, " = ");
                    generate_expression(node->init);
                }
                fprintf(out, ";\n");
            }
            // Track class variables for interpolation pointer access
            if (node->var_type.type == TOKEN_IDENTIFIER && node->var_type.length > 0 &&
                node->var_type.start[0] >= 'A' && node->var_type.start[0] <= 'Z') {
                track_class_var(node->token.start, node->token.length,
                                node->var_type.start, node->var_type.length);
            }
            break;
        case AST_MAIN:
            fprintf(out, "int main(void) {\n");
            indent_level++;
            if (codegen_test_count > 0) {
                write_indent();
                fprintf(out, "int _cnext_tp = 0, _cnext_tf = 0;\n");
                for (int i = 0; i < codegen_test_count; i++) {
                    write_indent();
                    fprintf(out, "if (_cnext_test_%d()) { _cnext_tp++; } else { _cnext_tf++; }\n", i);
                }
                write_indent();
                fprintf(out, "printf(\"Tests: %%d passed, %%d failed\\n\", _cnext_tp, _cnext_tf);\n");
                write_indent();
                fprintf(out, "if (_cnext_tf > 0) { _cnext_free_all(); return 1; }\n");
            }
            for (int i = 0; i < node->left->child_count; i++) {
                write_indent();
                generate_node(node->left->children[i]);
            }
            write_indent();
            fprintf(out, "_cnext_free_all();\n");
            write_indent();
            fprintf(out, "return 0;\n");
            indent_level--;
            fprintf(out, "}\n\n");
            break;
        case AST_EXPR_STMT:
            generate_expression(node->left);
            fprintf(out, ";\n");
            break;
        case AST_ASSIGN:
            if (node->right && node->right->type == AST_SLICE) {
                fprintf(out, "CNEXT_SLICE_TO(");
                generate_expression(node->left);
                fprintf(out, ", ");
                generate_expression(node->right->left);
                fprintf(out, ", ");
                if (node->right->init) {
                    generate_expression(node->right->init);
                } else {
                    fprintf(out, "0");
                }
                fprintf(out, ", ");
                if (node->right->right) {
                    generate_expression(node->right->right);
                } else {
                    generate_expression(node->right->left);
                    fprintf(out, ".length");
                }
                fprintf(out, ")");
            } else {
                generate_expression(node->left);
                fprintf(out, " %.*s ", node->token.length, node->token.start);
                generate_expression(node->right);
            }
            fprintf(out, ";\n");
            break;
        case AST_IF:
            fprintf(out, "if (");
            generate_expression(node->condition);
            fprintf(out, ") ");
            generate_block(node->left);
            if (node->right) {
                write_indent();
                fprintf(out, "else ");
                if (node->right->type == AST_IF) generate_node(node->right);
                else generate_block(node->right);
            }
            break;
        case AST_WHILE:
            fprintf(out, "while (");
            generate_expression(node->condition);
            fprintf(out, ") ");
            generate_block(node->left);
            break;
        case AST_FOR:
            fprintf(out, "for (");
            if (node->init->type == AST_VAR_DECL) {
                generate_type(node->init->var_type, false);
                fprintf(out, " %.*s", node->init->token.length, node->init->token.start);
                if (node->init->init) {
                    fprintf(out, " = ");
                    generate_expression(node->init->init);
                }
            } else if (node->init->type == AST_ASSIGN) {
                generate_expression(node->init->left);
                fprintf(out, " %.*s ", node->init->token.length, node->init->token.start);
                generate_expression(node->init->right);
            } else if (node->init->type == AST_EXPR_STMT) {
                generate_expression(node->init->left);
            }
            fprintf(out, "; ");
            generate_expression(node->condition);
            fprintf(out, "; ");
            if (node->increment->type == AST_ASSIGN) {
                generate_expression(node->increment->left);
                fprintf(out, " %.*s ", node->increment->token.length, node->increment->token.start);
                generate_expression(node->increment->right);
            } else if (node->increment->type == AST_EXPR_STMT) {
                generate_expression(node->increment->left);
            } else {
                generate_expression(node->increment);
            }
            fprintf(out, ") ");
            generate_block(node->left);
            break;
        case AST_FOR_IN: {
            static int loop_counter = 0;
            int current_loop = loop_counter++;
            fprintf(out, "for (size_t _i%d = 0; _i%d < (", current_loop, current_loop);
            generate_expression(node->condition);
            fprintf(out, ").length; _i%d++) {\n", current_loop);
            indent_level++;
            write_indent();
            fprintf(out, "__auto_type %.*s = (", node->init->token.length, node->init->token.start);
            generate_expression(node->condition);
            fprintf(out, ").data[_i%d];\n", current_loop);
            
            for (int i = 0; i < node->left->child_count; i++) {
                write_indent();
                generate_node(node->left->children[i]);
            }
            indent_level--;
            write_indent();
            fprintf(out, "}\n");
            break;
        }
        case AST_SWITCH:
            fprintf(out, "switch (");
            generate_expression(node->condition);
            fprintf(out, ") {\n");
            indent_level++;
            for (int i = 0; i < node->child_count; i++) {
                generate_node(node->children[i]);
            }
            indent_level--;
            write_indent();
            fprintf(out, "}\n");
            break;
        case AST_CASE:
            write_indent();
            fprintf(out, "case ");
            generate_expression(node->condition);
            fprintf(out, ":\n");
            indent_level++;
            for (int i = 0; i < node->left->child_count; i++) {
                write_indent();
                generate_node(node->left->children[i]);
            }
            write_indent();
            fprintf(out, "break;\n");
            indent_level--;
            break;
        case AST_DEFAULT:
            write_indent();
            fprintf(out, "default:\n");
            indent_level++;
            for (int i = 0; i < node->left->child_count; i++) {
                write_indent();
                generate_node(node->left->children[i]);
            }
            write_indent();
            fprintf(out, "break;\n");
            indent_level--;
            break;
        case AST_RETURN:
            fprintf(out, "return");
            if (node->left) {
                fprintf(out, " ");
                generate_expression(node->left);
            }
            fprintf(out, ";\n");
            break;
        case AST_BREAK:
            fprintf(out, "break;\n");
            break;
        case AST_CONTINUE:
            fprintf(out, "continue;\n");
            break;
        case AST_TRY: {
            static int try_counter = 0;
            int current_try = try_counter++;
            // Generate setjmp-based try/catch
            // Pattern: _cnext_push_try(); if (setjmp(...) == 0) { try } else { catch } _cnext_pop_try() happens in catch path via longjmp
            fprintf(out, "{\n");
            indent_level++;
            write_indent();
            fprintf(out, "_cnext_push_try();\n");
            write_indent();
            fprintf(out, "if (setjmp(_cnext_try_stack[_cnext_try_depth - 1].buf) == 0) ");
            generate_block(node->left); // try body
            if (node->right) {
                write_indent();
                fprintf(out, "else ");
                // In the catch path, the try frame was already popped by cnext_throw/longjmp
                fprintf(out, "{\n");
                indent_level++;
                // Declare the error variable
                if (node->right->token.start && node->right->token.length > 0) {
                    write_indent();
                    fprintf(out, "CnextString %.*s = _cnext_error_message;\n",
                        node->right->token.length, node->right->token.start);
                }
                // Generate catch body statements
                if (node->right->left) {
                    for (int i = 0; i < node->right->left->child_count; i++) {
                        write_indent();
                        generate_node(node->right->left->children[i]);
                    }
                }
                indent_level--;
                write_indent();
                fprintf(out, "}\n");
            }
            write_indent();
            fprintf(out, "_cnext_pop_try();\n");
            // Finally block
            if (node->condition) {
                for (int i = 0; i < node->condition->child_count; i++) {
                    write_indent();
                    generate_node(node->condition->children[i]);
                }
            }
            indent_level--;
            write_indent();
            fprintf(out, "}\n");
            (void)current_try;
            break;
        }
        case AST_THROW:
            fprintf(out, "cnext_throw(");
            generate_expression(node->left);
            fprintf(out, ");\n");
            break;
        case AST_MATCH: {
            // Generate as if-else chain
            static int match_counter = 0;
            int current_match = match_counter++;
            fprintf(out, "{\n");
            indent_level++;
            write_indent();
            fprintf(out, "__auto_type _match_%d = ", current_match);
            generate_expression(node->condition);
            fprintf(out, ";\n");
            bool first = true;
            for (int i = 0; i < node->child_count; i++) {
                ASTNode* arm = node->children[i];
                write_indent();
                if (arm->is_const) {
                    // Wildcard/default arm
                    if (!first) {
                        fprintf(out, "else ");
                    }
                } else if (arm->child_count > 0) {
                    if (!first) {
                        fprintf(out, "else ");
                    }
                    fprintf(out, "if (");
                    for (int j = 0; j < arm->child_count; j++) {
                        if (j > 0) fprintf(out, " || ");
                        fprintf(out, "_match_%d == ", current_match);
                        generate_expression(arm->children[j]);
                    }
                    fprintf(out, ") ");
                }
                generate_block(arm->left);
                first = false;
            }
            indent_level--;
            write_indent();
            fprintf(out, "}\n");
            break;
        }
        case AST_ASSERT:
            fprintf(out, "if (!(");
            generate_expression(node->left);
            fprintf(out, ")) {\n");
            indent_level++;
            write_indent();
            if (codegen_in_test) {
                fprintf(out, "_cnext_test_ok = false;\n");
            }
            write_indent();
            fprintf(out, "fprintf(stderr, \"  FAIL [line %%d]\\n\", %d);\n", node->token.line);
            if (!codegen_in_test) {
                write_indent();
                fprintf(out, "_cnext_free_all();\n");
                write_indent();
                fprintf(out, "exit(1);\n");
            }
            indent_level--;
            write_indent();
            fprintf(out, "}\n");
            break;
        case AST_TRAIT_DECL:
            // Traits don't generate standalone C code.
            // Default method implementations are inherited by classes during semantic analysis.
            break;
        case AST_INTERFACE_DECL:
            // Interfaces don't generate C code (they're compile-time only)
            break;
        case AST_OPERATOR_DECL: {
            // Generate operator overload as a function
            // Format: ReturnType TypeName_operator(ClassName self, Type other)
            char op_name[64];
            snprintf(op_name, sizeof(op_name), "_%.*s", node->operator_token.length, node->operator_token.start);
            // Map operator tokens to C operator names
            if (node->operator_token.type == TOKEN_PLUS) snprintf(op_name, sizeof(op_name), "_plus");
            else if (node->operator_token.type == TOKEN_MINUS) snprintf(op_name, sizeof(op_name), "_minus");
            else if (node->operator_token.type == TOKEN_STAR) snprintf(op_name, sizeof(op_name), "_star");
            else if (node->operator_token.type == TOKEN_SLASH) snprintf(op_name, sizeof(op_name), "_slash");
            else if (node->operator_token.type == TOKEN_EQ_EQ) snprintf(op_name, sizeof(op_name), "_eq");
            else if (node->operator_token.type == TOKEN_BANG_EQ) snprintf(op_name, sizeof(op_name), "_neq");
            else if (node->operator_token.type == TOKEN_LESS) snprintf(op_name, sizeof(op_name), "_lt");
            else if (node->operator_token.type == TOKEN_LESS_EQ) snprintf(op_name, sizeof(op_name), "_lte");
            else if (node->operator_token.type == TOKEN_GREATER) snprintf(op_name, sizeof(op_name), "_gt");
            else if (node->operator_token.type == TOKEN_GREATER_EQ) snprintf(op_name, sizeof(op_name), "_gte");
            else if (node->operator_token.type == TOKEN_LBRACKET) snprintf(op_name, sizeof(op_name), "_idx");
            
            generate_type(node->return_type, false);
            fprintf(out, " operator%s(", op_name);
            for (int i = 0; i < node->child_count; i++) {
                generate_type(node->children[i]->var_type, node->children[i]->is_pointer);
                fprintf(out, " %.*s", node->children[i]->token.length, node->children[i]->token.start);
                if (i < node->child_count - 1) fprintf(out, ", ");
            }
            fprintf(out, ") ");
            generate_block(node->left);
            fprintf(out, "\n");
            break;
        }
        case AST_EXTEND_DECL: {
            // Generate extension methods as regular functions with type prefix
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_FUNC_DECL) {
                    char* prefix = copy_token_text(node->token);
                    generate_function(node->children[i], prefix);
                    free(prefix);
                }
            }
            break;
        }
        case AST_DESTRUCTURE: {
            // Generate destructuring as field access via temp variable
            write_indent();
            fprintf(out, "auto _dstr = ");
            generate_expression(node->right);
            fprintf(out, ";\n");
            for (int i = 0; i < node->child_count; i++) {
                write_indent();
                generate_type(node->children[i]->var_type, false);
                fprintf(out, " %.*s = ", node->children[i]->token.length, node->children[i]->token.start);
                fprintf(out, "_dstr.e%d;\n", i);
            }
            break;
        }
        default:
            break;
    }
}

static void generate_runtime_preamble() {
    fprintf(out, "#include \"runtime.h\"\n\n");
}

bool generate_c_code(ASTNode* program, const char* output_filename, bool test_mode) {
    codegen_test_mode = test_mode;
    codegen_in_test = false;
    program_node = program;
    clear_type_subst();
    free_gen_specs();

    // Count tests
    codegen_test_count = 0;
    for (int i = 0; i < program->child_count; i++) {
        if (program->children[i]->type == AST_TEST) codegen_test_count++;
    }
    int test_count = codegen_test_count;

    // Open temp files for body (function defs) and specs (generic specializations)
    const char* body_temp_name = "_cnext_body.tmp";
    const char* spec_temp_name = "_cnext_specs.tmp";
    FILE* body_temp = fopen(body_temp_name, "w");
    spec_out = fopen(spec_temp_name, "w");
    if (!body_temp || !spec_out) {
        fprintf(stderr, "Could not create temporary files.\n");
        if (body_temp) fclose(body_temp);
        if (spec_out) fclose(spec_out);
        return false;
    }
    
    // Track test description strings for the runner
    char** test_descriptions = NULL;
    if (test_count > 0) {
        test_descriptions = (char**)malloc(sizeof(char*) * test_count);
        int idx = 0;
        for (int i = 0; i < program->child_count; i++) {
            if (program->children[i]->type == AST_TEST) {
                ASTNode* tn = program->children[i];
                test_descriptions[idx] = strndup(tn->token.start, tn->token.length);
                idx++;
            }
        }
    }
    
    out = body_temp;
    indent_level = 0;
    
    // Phase 1: Discover all generic specializations
    // Walk non-generic functions and main to find initial generic calls
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        if (child->type == AST_MAIN) {
            scan_node_for_generic_calls(child);
        } else if (child->type == AST_FUNC_DECL && child->type_param_count == 0) {
            scan_node_for_generic_calls(child);
        }
    }
    // Recursively discover nested calls in specialization bodies
    {
        SpecWork* sw = spec_work_queue;
        while (sw) {
            for (int i = 0; i < sw->func_decl->type_param_count && i < sw->resolved_count; i++) {
                ASTNode* tp = sw->func_decl->type_params[i];
                push_type_subst(
                    strndup(tp->token.start, tp->token.length),
                    sw->resolved_args[i],
                    sw->resolved_is_array[i]
                );
            }
            scan_node_for_generic_calls(sw->func_decl->left);
            for (int i = 0; i < sw->func_decl->type_param_count && i < sw->resolved_count; i++) pop_type_subst();
            sw = sw->next;
        }
    }
    // Write forward declarations for all discovered specializations
    // (needed because specializations may call each other)
    {
        FILE* saved_out = out;
        out = spec_out;
        for (SpecWork* sw = spec_work_queue; sw; sw = sw->next) {
            // Temporarily push type substitutions to resolve param types correctly
            for (int ti = 0; ti < sw->func_decl->type_param_count && ti < sw->resolved_count; ti++) {
                ASTNode* tp = sw->func_decl->type_params[ti];
                push_type_subst(strndup(tp->token.start, tp->token.length), sw->resolved_args[ti], sw->resolved_is_array[ti]);
            }
            generate_type(sw->func_decl->return_type, false);
            fprintf(out, " %s(", sw->mangled_name);
            for (int pi = 0; pi < sw->func_decl->child_count; pi++) {
                ASTNode* param = sw->func_decl->children[pi];
                if (pi > 0) fprintf(out, ", ");
                generate_type(param->var_type, param->is_pointer);
            }
            fprintf(out, ");\n");
            for (int ti = 0; ti < sw->func_decl->type_param_count && ti < sw->resolved_count; ti++) {
                pop_type_subst();
            }
        }
        if (gen_specs) fprintf(out, "\n");
        out = saved_out;
    }
    
    // Phase 2: Generate all nodes (function defs, main, etc.) to body_temp
    // Generic function calls will emit mangled names (specializations already discovered)
    codegen_in_test = false;
    int test_idx = 0;
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        if (child->type == AST_TEST) {
            codegen_in_test = true;
            fprintf(out, "static bool _cnext_test_%d(void) {\n", test_idx);
            indent_level++;
            write_indent();
            fprintf(out, "bool _cnext_test_ok = true;\n");
            if (child->left) {
                for (int j = 0; j < child->left->child_count; j++) {
                    write_indent();
                    generate_node(child->left->children[j]);
                }
            }
            write_indent();
            fprintf(out, "return _cnext_test_ok;\n");
            indent_level--;
            fprintf(out, "}\n\n");
            codegen_in_test = false;
            test_idx++;
        } else {
            generate_node(child);
        }
    }
    
    // If no main exists but tests do, generate a main that runs tests
    if (test_count > 0) {
        bool has_main = false;
        for (int i = 0; i < program->child_count; i++) {
            if (program->children[i]->type == AST_MAIN) { has_main = true; break; }
        }
        if (!has_main) {
            fprintf(out, "int main(void) {\n");
            indent_level++;
            write_indent();
            fprintf(out, "int _cnext_tp = 0, _cnext_tf = 0;\n");
            for (int i = 0; i < test_count; i++) {
                write_indent();
                fprintf(out, "if (_cnext_test_%d()) { _cnext_tp++; } else { _cnext_tf++; }\n", i);
            }
            write_indent();
            fprintf(out, "printf(\"Tests: %%d passed, %%d failed\\n\", _cnext_tp, _cnext_tf);\n");
            write_indent();
            fprintf(out, "_cnext_free_all();\n");
            write_indent();
            fprintf(out, "return _cnext_tf > 0 ? 1 : 0;\n");
            indent_level--;
            fprintf(out, "}\n\n");
        }
    }
    
    // Free test descriptions
    for (int i = 0; i < test_count; i++) {
        free(test_descriptions[i]);
    }
    free(test_descriptions);
    
    // Close body temp file
    bool ok = !ferror(body_temp);
    if (fclose(body_temp) != 0) ok = false;
    
    // Process the generic specialization work queue (generates to spec_out)
    out = spec_out;
    indent_level = 0;
    clear_type_subst();
    process_spec_queue();
    clear_type_subst();
    free_gen_specs();
    out = NULL;
    
    if (fclose(spec_out) != 0) ok = false;
    spec_out = NULL;
    
    if (!ok) {
        fprintf(stderr, "Error writing temporary files.\n");
        remove(body_temp_name);
        remove(spec_temp_name);
        return false;
    }
    
    // Combine: open final output, write preamble + prototypes + specs + body
    out = fopen(output_filename, "w");
    if (!out) {
        fprintf(stderr, "Could not open output file \"%s\".\n", output_filename);
        remove(body_temp_name);
        remove(spec_temp_name);
        return false;
    }
    
    generate_runtime_preamble();
    for (int i = 0; i < test_count; i++) {
        fprintf(out, "static bool _cnext_test_%d(void);\n", i);
    }
    if (test_count > 0) fprintf(out, "\n");
    
    // Write specializations
    {
        FILE* sf = fopen(spec_temp_name, "r");
        if (sf) {
            char line[4096];
            while (fgets(line, sizeof(line), sf)) {
                fprintf(out, "%s", line);
            }
            fclose(sf);
        }
        if (test_count > 0 || program->child_count > 0) fprintf(out, "\n");
    }
    
    // Write body (function definitions, main, etc.)
    {
        FILE* bf = fopen(body_temp_name, "r");
        if (bf) {
            char line[4096];
            while (fgets(line, sizeof(line), bf)) {
                fprintf(out, "%s", line);
            }
            fclose(bf);
        }
    }
    
    ok = !ferror(out);
    if (fclose(out) != 0) ok = false;
    if (!ok) {
        fprintf(stderr, "Could not write output file \"%s\".\n", output_filename);
    }
    
    remove(body_temp_name);
    remove(spec_temp_name);
    out = NULL;
    return ok;
}
