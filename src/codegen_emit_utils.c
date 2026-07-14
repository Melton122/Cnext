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

void write_indent(void) {
    for (int i = 0; i < indent_level; i++) {
        fprintf(out, "    ");
    }
}

void generate_type(Token token, bool is_pointer) {
    const char* c_type = type_token_to_c(token.type);
    if (c_type) {
        fprintf(out, "%s", c_type);
    } else if (token.type == TOKEN_VAR || token.type == TOKEN_FUNC) {
        fprintf(out, "__auto_type");
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
    return true;
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
    generate_type(node->return_type, false);
    if (prefix) {
        fprintf(out, " %s_%.*s(", prefix, node->token.length, node->token.start);
    } else {
        fprintf(out, " %.*s(", node->token.length, node->token.start);
    }
    
    bool first_param = true;
    for (int i = 0; i < node->child_count; i++) {
        if (node->children[i]->is_variadic) {
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
