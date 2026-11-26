#include "codegen_internal.h"

const char* type_token_to_c(CnextTokenType type) {
    switch (type) {
        case TOKEN_INT_TYPE: return "int";
        case TOKEN_LONG_TYPE: return "long long";
        case TOKEN_FLOAT_TYPE: return "float";
        case TOKEN_DOUBLE_TYPE: return "double";
        case TOKEN_STR_TYPE: return "CnextString";
        case TOKEN_FUNC: return "CnextClosure";
        case TOKEN_BOOL_TYPE: return "bool";
        case TOKEN_CHAR_TYPE: return "char";
        /* byte is documented as an unsigned 8-bit integer (0..255). Emitting
           `char` made its signedness platform-dependent and routed printing
           through the character printer, so `byte b = 255` printed a raw byte
           instead of "255". `unsigned char` has its own print/convert paths. */
        case TOKEN_BYTE_TYPE: return "unsigned char";
        case TOKEN_UINT_TYPE: return "unsigned int";
        case TOKEN_ULONG_TYPE: return "unsigned long long";
        case TOKEN_USHORT_TYPE: return "unsigned short";
        case TOKEN_UBYTE_TYPE: return "unsigned char";
        default: return NULL;
    }
}

char* copy_token_text(Token token) {
    size_t length = token.length > 0 ? (size_t)token.length : 0;
    char* copy = (char*)checked_malloc(length + 1);
    if (length > 0 && token.start) {
        memcpy(copy, token.start, length);
    }
    copy[length] = '\0';
    return copy;
}

// --- Adapting lambdas passed to untyped `func` parameters ---
//
// A `func` parameter is untyped, so a call through it inside the function
// body uses a fixed `int`-based calling convention. A lambda whose real
// signature works on other types (a float-returning discount, say) returns
// its value in a different register, so calling it through the fixed
// convention produced garbage. To keep one ABI at the call-through site, a
// lambda passed as a `func` argument gets a static adapter that receives and
// returns `int`s, calls the real lambda with its own types, and coerces the
// result. Only signatures that survive the int round-trip (numeric/bool
// parameters and result, or void) are adapted; anything else keeps the
// previous direct pass-through.
static int adapt_counter = 0;

bool type_token_is_int_abi_safe(CnextTokenType type) {
    switch (type) {
        case TOKEN_INT_TYPE:
        case TOKEN_LONG_TYPE:
        case TOKEN_FLOAT_TYPE:
        case TOKEN_DOUBLE_TYPE:
        case TOKEN_BOOL_TYPE:
        case TOKEN_CHAR_TYPE:
        case TOKEN_BYTE_TYPE:
        case TOKEN_UINT_TYPE:
        case TOKEN_ULONG_TYPE:
        case TOKEN_USHORT_TYPE:
        case TOKEN_UBYTE_TYPE:
            return true;
        default:
            return false;
    }
}

// The CnextTokenType a lambda body produces (TOKEN_EOF when void).
CnextTokenType lambda_body_result_type(ASTNode* lambda) {
    if (!lambda || !lambda->left) return TOKEN_EOF;
    if (lambda->left->type != AST_BLOCK) return lambda->left->expr_type;
    for (int i = 0; i < lambda->left->child_count; i++) {
        ASTNode* stmt = lambda->left->children[i];
        if (stmt->type == AST_RETURN) {
            return stmt->left ? stmt->left->expr_type : TOKEN_EOF;
        }
    }
    return TOKEN_EOF;
}

bool lambda_signature_is_int_abi_safe(ASTNode* lambda) {
    if (!lambda) return false;
    CnextTokenType ret = lambda_body_result_type(lambda);
    if (ret != TOKEN_EOF && !type_token_is_int_abi_safe(ret)) return false;
    for (int i = 0; i < lambda->child_count; i++) {
        if (!type_token_is_int_abi_safe(lambda->children[i]->var_type.type)) return false;
    }
    return true;
}

// The lambda a closure-valued expression denotes, if any.
static ASTNode* adapter_lambda_for(ASTNode* arg) {
    if (!arg) return NULL;
    if (arg->type == AST_LAMBDA) return arg;
    if (arg->type == AST_IDENTIFIER && arg->binding &&
        arg->binding->type == AST_VAR_DECL) {
        return ast_closure_lambda(arg->binding);
    }
    return NULL;
}

// Emits a static `int`-ABI adapter for the lambda passed as the
// `arg_index`th argument of `call`, plus the wrapper expression that pairs
// the adapter with a copy of the real closure. Returns false when the
// argument is not an adaptable lambda (the caller falls back to direct
// codegen).
bool emit_func_param_adapter_value(ASTNode* call, ASTNode* fdecl, int arg_index) {
    if (!fdecl || fdecl->type != AST_FUNC_DECL) return false;
    if (arg_index >= fdecl->child_count || arg_index >= call->child_count) return false;
    ASTNode* param = fdecl->children[arg_index];
    if (!param || param->var_type.type != TOKEN_FUNC) return false;
    ASTNode* lambda = adapter_lambda_for(call->children[arg_index]);
    if (!lambda || !lambda_signature_is_int_abi_safe(lambda)) return false;
    ASTNode* arg = call->children[arg_index];

    int id = adapt_counter++;
    bool ret_is_void = lambda_body_result_type(lambda) == TOKEN_EOF;
    FILE* saved_out = out;
    out = closure_defs_out;
    fprintf(out, "static int _cnext_adapt_%d(void* _env", id);
    for (int p = 0; p < lambda->child_count; p++) fprintf(out, ", int _a%d", p);
    fprintf(out, ") {\n");
    fprintf(out, "    CnextClosure* _real = (CnextClosure*)_env;\n");
    if (ret_is_void) {
        fprintf(out, "    ((");
        generate_lambda_return_type(lambda->left);
        fprintf(out, "(*)(void*");
        for (int p = 0; p < lambda->child_count; p++) {
            fprintf(out, ", ");
            generate_type(lambda->children[p]->var_type, false);
        }
        fprintf(out, "))_real->fn)(_real->env");
        for (int p = 0; p < lambda->child_count; p++) {
            fprintf(out, ", (");
            generate_type(lambda->children[p]->var_type, false);
            fprintf(out, ")_a%d", p);
        }
        fprintf(out, ");\n    return 0;\n}\n\n");
    } else {
        fprintf(out, "    return (int)((");
        generate_lambda_return_type(lambda->left);
        fprintf(out, "(*)(void*");
        for (int p = 0; p < lambda->child_count; p++) {
            fprintf(out, ", ");
            generate_type(lambda->children[p]->var_type, false);
        }
        fprintf(out, "))_real->fn)(_real->env");
        for (int p = 0; p < lambda->child_count; p++) {
            fprintf(out, ", (");
            generate_type(lambda->children[p]->var_type, false);
            fprintf(out, ")_a%d", p);
        }
        fprintf(out, ");\n}\n\n");
    }
    out = saved_out;

    // The adapter's environment is a heap cell holding a copy of the real
    // closure. The call-site guard later decrefs the adapter closure, and
    // cnext_closure_decref frees a non-owner env, so the cell must live on
    // the heap - a stack address would be freed and then dereferenced.
    fprintf(out, "({ CnextClosure _adapt_rc_%d = ", id);
    if (arg->type == AST_LAMBDA) {
        // gen_lambda_expr emits a bare function pointer; only a declaration
        // site wraps it into a CnextClosure, so do that here.
        CapturedVar* caps = NULL;
        detect_captures_walk(arg->left, &caps, arg, program_node);
        int cap_count = 0;
        for (CapturedVar* cv = caps; cv; cv = cv->next) cap_count++;
        free_captures(caps);
        if (cap_count == 0) {
            fprintf(out, "(CnextClosure){ (void*)");
            generate_expression(arg);
            fprintf(out, ", NULL, 1 }");
        } else {
            generate_expression(arg);
        }
    } else {
        generate_expression(arg);
    }
    fprintf(out, "; CnextClosure* _adapt_cell_%d = (CnextClosure*)malloc(sizeof(CnextClosure)); if (!_adapt_cell_%d) { fprintf(stderr, \"Cnext runtime: out of memory.\"); exit(70); } *_adapt_cell_%d = _adapt_rc_%d; (CnextClosure){ (void*)_cnext_adapt_%d, (void*)_adapt_cell_%d, 1, NULL }; })",
            id, id, id, id, id, id);
    return true;
}

void generate_lambda_return_type(ASTNode* body) {
    if (body->type != AST_BLOCK) {
        const char* c_type = type_token_to_c(body->expr_type);
        fprintf(out, "%s", c_type ? c_type : "void");
    } else {
        CnextTokenType ret_type = TOKEN_EOF;
        for (int i = 0; i < body->child_count; i++) {
            if (body->children[i]->type == AST_RETURN) {
                if (body->children[i]->left) {
                    ret_type = body->children[i]->left->expr_type;
                }
                break;
            }
        }
        const char* c_type = type_token_to_c(ret_type);
        fprintf(out, "%s", c_type ? c_type : "void");
    }
}

void track_source_line(int src_line) {
    if (!codegen_sourcemap || src_line < 1) return;
    if (src_line != codegen_last_src_line) {
        sourcemap_add(codegen_sourcemap, src_line, 0, codegen_gen_line, 0);
        codegen_last_src_line = src_line;
    }
}

void emit_line_directive(int src_line) {
    if (!codegen_emit_lines || !codegen_debug_source) return;
    if (src_line < 1 || src_line == codegen_last_emit_line) return;
    fprintf(out, "#line %d \"%s\"\n", src_line, codegen_debug_source);
    codegen_last_emit_line = src_line;
}

void write_indent(void) {
    if (!out) return;
    for (int i = 0; i < indent_level; i++) {
        fprintf(out, "    ");
    }
}

void generate_type(Token token, bool is_pointer) {
    const char* c_type = type_token_to_c(token.type);
    if (c_type) {
        fprintf(out, "%s", c_type);
    } else if (token.type == TOKEN_VAR) {
        fprintf(out, "__auto_type");
    } else if (token.type == TOKEN_FUNC) {
        fprintf(out, "CnextClosure");
    } else if (token.type == TOKEN_IDENTIFIER) {
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

static Token array_element_token(ASTNode* node);

// An array whose element type is a class stores class instances, which are
// pointers, so it uses CnextArrayObj rather than a typed value array. A struct
// element is a value type, so it uses a typed value array instead.
bool node_array_is_object(ASTNode* node) {
    if (!node || !node->is_array) return false;
    if (node->var_type.type == TOKEN_IDENTIFIER) {
        char elem_name[256];
        int elen = node->var_type.length < 255 ? node->var_type.length : 255;
        memcpy(elem_name, node->var_type.start, elen);
        elem_name[elen] = '\0';
        Token subst_type;
        bool subst_array;
        if (find_type_subst(elem_name, &subst_type, &subst_array)) {
            if (subst_type.type == TOKEN_IDENTIFIER) {
                char subst_name[256];
                int slen = subst_type.length < 255 ? subst_type.length : 255;
                memcpy(subst_name, subst_type.start, slen);
                subst_name[slen] = '\0';
                return !type_is_struct(subst_name);
            }
            return subst_type.type == TOKEN_IDENTIFIER;
        }
        return !type_is_struct(elem_name);
    }
    return false;
}

// Emits the array type for an element type that is neither one of the
// built-in typed arrays nor a class reference: a value array of struct
// elements. The struct is given a file-scope typedef (emitted into the
// forward-declaration stream on first use) because two inline anonymous
// structs declared in different scopes are distinct C types, which would
// make the declaration and the literal initializer incompatible.
void emit_struct_array_type(ASTNode* node) {
    Token elem = array_element_token(node);
    char elem_name[256];
    int elen = elem.length < 255 ? elem.length : 255;
    memcpy(elem_name, elem.start, elen);
    elem_name[elen] = '\0';
    char type_name[300];
    snprintf(type_name, sizeof(type_name), "_CnextArray_%s", elem_name);
    if (!has_struct_array_typedef(type_name)) {
        FILE* saved_out = out;
        out = spec_out;
        fprintf(out, "typedef struct { ");
        generate_type(elem, true);
        fprintf(out, " data; size_t length; } %s;\n", type_name);
        out = saved_out;
        add_struct_array_typedef(type_name);
    }
    fprintf(out, "%s", type_name);
}

// `arr = { ... }` assigns a whole array value, so it needs a compound literal
// of the array struct. A bare braced list is not an expression in C.
bool is_array_literal_assignment(ASTNode* node) {
    return node && node->right && node->right->type == AST_LITERAL_ARRAY &&
           node->right->is_array;
}

// Emits `arr[i] = value` for an array of class instances, returning true when it
// handled the whole assignment.
//
// A read of an object-array element yields the stored pointer, which is what lets
// a following `->field` work, but that pointer is not a modifiable lvalue, so
// the value cannot be assigned through it. Reaching the slot itself is not an
// option either: CNEXT_ARRAY_IDX_OBJ is a statement expression, and GCC does not
// preserve lvalue-ness through one, so its result cannot be addressed. The store
// therefore has to happen inside the statement expression, which is what
// CNEXT_ARRAY_IDX_OBJ_ASSIGN does. Both the expression and the statement paths
// call this so the two cannot drift apart.
bool emit_object_array_element_assignment(ASTNode* node) {
    ASTNode* target = node ? node->left : NULL;
    if (!target || target->type != AST_INDEX || !target->is_class) return false;
    if (!node->right || node->right->type == AST_LITERAL_ARRAY ||
        node->right->type == AST_SLICE) {
        return false;
    }
    fprintf(out, "CNEXT_ARRAY_IDX_OBJ_ASSIGN(");
    generate_expression(target->left);
    fprintf(out, ", ");
    generate_expression(target->right);
    fprintf(out, ", ");
    generate_expression(node->right);
    fprintf(out, ")");
    return true;
}

// Emits a whole array value from a literal. type_owner supplies the element
// type (it is the array declaration, or the annotated literal for an
// assignment). The backing store is heap allocated so the array can be grown
// later by push.
void generate_array_value(ASTNode* type_owner, ASTNode* literal) {
    fprintf(out, "({ ");
    generate_node_type(type_owner, false);
    fprintf(out, " _cnarr = { (");
    if (node_array_is_object(type_owner)) fprintf(out, "void*");
    else generate_type(type_owner->var_type, false);
    fprintf(out, "[])");
    fprintf(out, "{");
    for (int i = 0; i < literal->child_count; i++) {
        if (i) fprintf(out, ", ");
        generate_expression(literal->children[i]);
    }
    fprintf(out, "}");
    fprintf(out, ", %d }; ", literal->child_count);
    if (node_array_is_object(type_owner)) {
        fprintf(out, "_cnarr.data = (void**)_cnext_array_heap_copy(_cnarr.data, _cnarr.length, sizeof(void*)); ");
    } else {
        fprintf(out, "_cnarr.data = (");
        generate_type(type_owner->var_type, false);
        fprintf(out, "*)_cnext_array_heap_copy(_cnarr.data, _cnarr.length, sizeof(");
        generate_type(type_owner->var_type, false);
        fprintf(out, ")); ");
    }
    fprintf(out, "_cnarr; })");
}

void generate_array_literal_assignment(ASTNode* literal) {
    generate_array_value(literal, literal);
}

// The element type of an array node after generic type-parameter substitution.
// `T[]` inside `List<T>` still carries `T` as its var_type while a
// specialization is generated, so the element type is only known after
// consulting the substitution map.
static Token array_element_token(ASTNode* node) {
    Token elem = node->var_type;
    if (elem.type != TOKEN_IDENTIFIER) return elem;
    char elem_name[256];
    int elen = elem.length < 255 ? elem.length : 255;
    memcpy(elem_name, elem.start, elen);
    elem_name[elen] = '\0';
    Token subst_type;
    bool subst_array;
    if (find_type_subst(elem_name, &subst_type, &subst_array) && !subst_array) {
        return subst_type;
    }
    return elem;
}

void generate_node_type(ASTNode* node, bool is_pointer) {
    if (node && node->is_array) {
        switch (array_element_token(node).type) {
            case TOKEN_INT_TYPE: fprintf(out, "CnextArrayInt"); break;
            case TOKEN_LONG_TYPE: fprintf(out, "CnextArrayLong"); break;
            case TOKEN_FLOAT_TYPE: fprintf(out, "CnextArrayFloat"); break;
            case TOKEN_DOUBLE_TYPE: fprintf(out, "CnextArrayDouble"); break;
            case TOKEN_STR_TYPE: fprintf(out, "CnextArrayString"); break;
            case TOKEN_BOOL_TYPE: fprintf(out, "CnextArrayBool"); break;
            default: {
                if (node_array_is_object(node)) fprintf(out, "CnextArrayObj");
                else if (node->var_type.type == TOKEN_IDENTIFIER) {
                    char elem_name[256];
                    int elen = node->var_type.length < 255 ? node->var_type.length : 255;
                    memcpy(elem_name, node->var_type.start, elen);
                    elem_name[elen] = '\0';
                    if (type_is_struct(elem_name)) emit_struct_array_type(node);
                    else generate_type(node->var_type, false);
                }
                else generate_type(node->var_type, false);
                break;
            }
        }
        if (is_pointer) fprintf(out, "*");
        return;
    }
    generate_type(node ? node->var_type : (Token){TOKEN_EOF}, is_pointer);
}

bool is_valid_interpolation(const char* start, const char* end) {
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
    // A placeholder has to hold an actual expression. Accepting whitespace made
    // a literal `{ }` in a message (a docs-style placeholder, as in
    // "Example: func name(params): Type { }") parse as an interpolation, and
    // the expression then generated nothing at all -- the text silently
    // vanished and the emitted `cnext_to_string()` had no argument. Treat a
    // blank placeholder as literal text instead.
    for (const char* s = start; s < c; s++) {
        if (*s != ' ' && *s != '\t' && *s != '\r' && *s != '\n') return true;
    }
    return false;
}

void generate_interp_expression(const char* start, const char* end) {
    int expr_len = (int)(end - start);
    if (expr_len > 5 && strncmp(start, "self.", 5) == 0) {
        fprintf(out, "self->%.*s", expr_len - 5, start + 5);
        return;
    }
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
    if (dot && !paren) {
        int var_len = (int)(dot - start);
        const char* class_name = lookup_class_var(start, var_len);
        if (class_name) {
            fprintf(out, "%.*s->%.*s", var_len, start, expr_len - var_len - 1, dot + 1);
            return;
        }
    }
    if (!dot && paren && paren > start) {
        int fname_len = (int)(paren - start);
        char fname[256];
        if (fname_len < 255) {
            memcpy(fname, start, fname_len);
            fname[fname_len] = '\0';
            ClosureVar* clvar = find_closure_var(fname);
            if (clvar) {
                ASTNode* lambda = clvar->lambda_node;
                fprintf(out, "((");
                if (lambda && lambda->left && lambda->left->type != AST_BLOCK) {
                    const char* ct = type_token_to_c(lambda->left->expr_type);
                    fprintf(out, "%s", ct ? ct : "void");
                } else if (lambda && lambda->left && lambda->left->type == AST_BLOCK) {
                    for (int ri = 0; ri < lambda->left->child_count; ri++) {
                        if (lambda->left->children[ri]->type == AST_RETURN &&
                            lambda->left->children[ri]->left) {
                            const char* ct = type_token_to_c(lambda->left->children[ri]->left->expr_type);
                            fprintf(out, "%s", ct ? ct : "void");
                            break;
                        }
                    }
                } else {
                    fprintf(out, "void*");
                }
                fprintf(out, "(*)(");
                fprintf(out, "void*");
                for (int ai = 0; ai < lambda->child_count; ai++) {
                    fprintf(out, ", ");
                    generate_type(lambda->children[ai]->var_type, lambda->children[ai]->is_pointer);
                }
                fprintf(out, "))");
                fprintf(out, "%.*s.fn)(%.*s.env", fname_len, start, fname_len, start);
                const char* args_start = paren + 1;
                int args_len = (int)(end - args_start - 1);
                if (args_len > 0) {
                    fprintf(out, ", %.*s", args_len, args_start);
                }
                fprintf(out, ")");
                return;
            }
        }
    }
    fprintf(out, "%.*s", expr_len, start);
}

void generate_string_interpolation(const char* start, int length) {
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

void generate_function(ASTNode* node, const char* prefix) {
    bool previous_in_main = codegen_in_main;
    codegen_in_main = false;
    emit_line_directive(node->token.line);
    if (node->is_generator) {
        char func_name[256];
        if (prefix) {
            snprintf(func_name, sizeof(func_name), "%.*s_%.*s",
                     (int)strlen(prefix), prefix, node->token.length, node->token.start);
        } else {
            snprintf(func_name, sizeof(func_name), "%.*s",
                     node->token.length, node->token.start);
        }
        generate_generator_code(node, func_name);
        codegen_in_main = previous_in_main;
        return;
    }

    track_source_line(node->token.line);
    ASTNode* prev_func_params = current_func_params;
    current_func_params = node;
    current_func_returns_void = (node->return_type.type == TOKEN_EOF || type_token_to_c(node->return_type.type) == NULL);
    // A function whose declared return type names a class returns a pointer,
    // because class instances are always heap-allocated.
    generate_return_type(node);
    if (prefix) {
        fprintf(out, " %s_%.*s(", prefix, node->token.length, node->token.start);
    } else {
        fprintf(out, " %.*s(", node->token.length, node->token.start);
    }
    
    bool first_param = true;
    for (int i = 0; i < node->child_count; i++) {
        if (node->children[i]->is_variadic) {
            if (!first_param) fprintf(out, ", ");
            generate_node_type(node->children[i], node->children[i]->is_pointer);
            fprintf(out, " %.*s", node->children[i]->token.length, node->children[i]->token.start);
            fprintf(out, ", ...");
            first_param = false;
            continue;
        }
        if (!first_param) fprintf(out, ", ");
        generate_node_type(node->children[i], node->children[i]->is_pointer);
        fprintf(out, " %.*s", node->children[i]->token.length, node->children[i]->token.start);
        first_param = false;
        // A class-typed parameter is a pointer (see the is_pointer flag), so
        // track it for string interpolation: `{s.name}` must emit `s->name`.
        if (node->children[i]->is_class && node->children[i]->var_type.type == TOKEN_IDENTIFIER) {
            track_class_var(node->children[i]->token.start, node->children[i]->token.length,
                            node->children[i]->var_type.start, node->children[i]->var_type.length);
        }
    }
    fprintf(out, ") ");
    generate_block(node->left);
    fprintf(out, "\n");
    current_func_params = prev_func_params;
    codegen_in_main = previous_in_main;
}

// Writes the C signature of a top-level function without its body, followed by
// a semicolon. Must stay in sync with generate_function above, which emits the
// same header and then the block instead.
void generate_return_type(ASTNode* node) {
    if (node->return_is_array) {
        ASTNode value = {.var_type = node->return_type, .is_array = true};
        generate_node_type(&value, false);
    } else generate_type(node->return_type, node->is_class);
}

void generate_function_prototype(ASTNode* node) {
    generate_return_type(node);
    fprintf(out, " %.*s(", node->token.length, node->token.start);
    bool first_param = true;
    for (int i = 0; i < node->child_count; i++) {
        if (!first_param) fprintf(out, ", ");
        generate_node_type(node->children[i], node->children[i]->is_pointer);
        if (node->children[i]->is_variadic) {
            fprintf(out, " %.*s, ...", node->children[i]->token.length,
                    node->children[i]->token.start);
        } else {
            fprintf(out, " %.*s", node->children[i]->token.length,
                    node->children[i]->token.start);
        }
        first_param = false;
    }
    fprintf(out, ");\n");
}

ASTNode* find_func_decl(ASTNode* prog, const char* name) {
    if (!prog) return NULL;
    for (int i = 0; i < prog->child_count; i++) {
        ASTNode* child = prog->children[i];
        if ((child->type == AST_FUNC_DECL || child->type == AST_COROUTINE_DECL || child->type == AST_ASYNC_FUNC_DECL) &&
            (int)strlen(name) == child->token.length &&
            strncmp(name, child->token.start, child->token.length) == 0) {
            return child;
        }
    }
    return NULL;
}

ASTNode* find_class_decl(ASTNode* prog, const char* name) {
    if (!prog) return NULL;
    for (int i = 0; i < prog->child_count; i++) {
        ASTNode* child = prog->children[i];
        if (child->type == AST_CLASS_DECL &&
            (int)strlen(name) == child->token.length &&
            strncmp(name, child->token.start, child->token.length) == 0) {
            return child;
        }
    }
    return NULL;
}

ASTNode* find_struct_decl(ASTNode* prog, const char* name) {
    if (!prog) return NULL;
    for (int i = 0; i < prog->child_count; i++) {
        ASTNode* child = prog->children[i];
        if (child->type == AST_STRUCT_DECL &&
            (int)strlen(name) == child->token.length &&
            strncmp(name, child->token.start, child->token.length) == 0) {
            return child;
        }
    }
    return NULL;
}

// A struct is a plain value type (unlike a class instance, which is a
// reference), so an array of structs must store values, not void* slots.
bool type_is_struct(const char* name) {
    return find_struct_decl(program_node, name) != NULL &&
           find_class_decl(program_node, name) == NULL;
}
