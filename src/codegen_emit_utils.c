#include "codegen_internal.h"

const char* type_token_to_c(CnextTokenType type) {
    switch (type) {
        case TOKEN_INT_TYPE: return "int";
        case TOKEN_LONG_TYPE: return "long long";
        case TOKEN_FLOAT_TYPE: return "float";
        case TOKEN_DOUBLE_TYPE: return "double";
        case TOKEN_STR_TYPE: return "CnextString";
        case TOKEN_BOOL_TYPE: return "bool";
        case TOKEN_CHAR_TYPE: return "char";
        case TOKEN_BYTE_TYPE: return "char";
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

// An array whose element type is a class stores class instances, which are
// pointers, so it uses CnextArrayObj rather than a typed value array.
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
            return subst_type.type == TOKEN_IDENTIFIER;
        }
        return true;
    }
    return false;
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
    generate_expression(literal);
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
        return;
    }

    track_source_line(node->token.line);
    ASTNode* prev_func_params = current_func_params;
    current_func_params = node;
    current_func_returns_void = (node->return_type.type == TOKEN_EOF || type_token_to_c(node->return_type.type) == NULL);
    // A function whose declared return type names a class returns a pointer,
    // because class instances are always heap-allocated.
    generate_type(node->return_type, node->is_class);
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
        } else {
            if (!first_param) fprintf(out, ", ");
            generate_node_type(node->children[i], node->children[i]->is_pointer);
            fprintf(out, " %.*s", node->children[i]->token.length, node->children[i]->token.start);
            first_param = false;
        }
    }
    fprintf(out, ") ");
    generate_block(node->left);
    fprintf(out, "\n");
    current_func_params = prev_func_params;
}

// Writes the C signature of a top-level function without its body, followed by
// a semicolon. Must stay in sync with generate_function above, which emits the
// same header and then the block instead.
void generate_function_prototype(ASTNode* node) {
    generate_type(node->return_type, node->is_class);
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
