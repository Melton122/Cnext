#include "codegen_internal.h"

// Generator support: collect local variables from a block

void collect_locals_from_block(ASTNode* block, GenLocal** list) {
    if (!block) return;
    for (int i = 0; i < block->child_count; i++) {
        ASTNode* stmt = block->children[i];
        if (stmt->type == AST_VAR_DECL) {
            GenLocal* existing = *list;
            bool found = false;
            while (existing) {
                if (existing->name_len == stmt->token.length &&
                    strncmp(existing->name, stmt->token.start, stmt->token.length) == 0) {
                    found = true;
                    break;
                }
                existing = existing->next;
            }
            if (!found) {
                GenLocal* gl = (GenLocal*)malloc(sizeof(GenLocal));
                gl->name = strndup(stmt->token.start, stmt->token.length);
                gl->name_len = stmt->token.length;
                gl->type = stmt->var_type.type;
                gl->is_pointer = stmt->is_pointer;
                gl->is_class = stmt->is_class;
                gl->next = *list;
                *list = gl;
            }
        }
        if (stmt->type == AST_BLOCK) collect_locals_from_block(stmt, list);
        else if (stmt->type == AST_IF) {
            collect_locals_from_block(stmt->left, list);
            collect_locals_from_block(stmt->right, list);
        } else if (stmt->type == AST_FOR || stmt->type == AST_WHILE || stmt->type == AST_FOR_IN) {
            collect_locals_from_block(stmt->left, list);
        }
    }
}

void free_gen_locals(GenLocal* list) {
    while (list) {
        GenLocal* next = list->next;
        free(list->name);
        free(list);
        list = next;
    }
}

static int gen_yield_counter = 0;

static void generate_gen_body_node(ASTNode* node, GenLocal* all_vars) {
    if (!node) return;
    
    switch (node->type) {
        case AST_YIELD: {
            gen_yield_counter++;
            fprintf(out, "        f->current = (");
            if (node->left) generate_expression(node->left);
            else fprintf(out, "0");
            fprintf(out, ");\n");
            fprintf(out, "        f->_pc = %d;\n", gen_yield_counter);
            fprintf(out, "        return true;\n");
            fprintf(out, "    case %d:\n", gen_yield_counter);
            break;
        }
        case AST_VAR_DECL: {
            for (GenLocal* gl = all_vars; gl; gl = gl->next) {
                if (gl->name_len == node->token.length &&
                    strncmp(gl->name, node->token.start, node->token.length) == 0) {
                    write_indent();
                    fprintf(out, "%.*s", node->token.length, node->token.start);
                    if (node->init) {
                        fprintf(out, " = ");
                        generate_expression(node->init);
                    }
                    fprintf(out, ";\n");
                    return;
                }
            }
            write_indent();
            generate_node(node);
            break;
        }
        case AST_BLOCK: {
            write_indent();
            fprintf(out, "{\n");
            indent_level++;
            for (int i = 0; i < node->child_count; i++) {
                generate_gen_body_node(node->children[i], all_vars);
            }
            indent_level--;
            write_indent();
            fprintf(out, "}\n");
            break;
        }
        case AST_WHILE: {
            write_indent();
            fprintf(out, "while (");
            generate_expression(node->condition);
            fprintf(out, ") {\n");
            indent_level++;
            generate_gen_body_node(node->left, all_vars);
            indent_level--;
            write_indent();
            fprintf(out, "}\n");
            break;
        }
        case AST_FOR: {
            write_indent();
            fprintf(out, "for (");
            if (node->init) {
                if (node->init->type == AST_VAR_DECL) {
                    bool lifted = false;
                    for (GenLocal* gl = all_vars; gl; gl = gl->next) {
                        if (gl->name_len == node->init->token.length &&
                            strncmp(gl->name, node->init->token.start, node->init->token.length) == 0) {
                            fprintf(out, "%.*s = ", node->init->token.length, node->init->token.start);
                            if (node->init->init) generate_expression(node->init->init);
                            lifted = true;
                            break;
                        }
                    }
                    if (!lifted) generate_node(node->init);
                } else {
                    generate_node(node->init);
                }
            }
            fprintf(out, "; ");
            if (node->condition) generate_expression(node->condition);
            fprintf(out, "; ");
            if (node->increment) {
                if (node->increment->type == AST_EXPR_STMT && node->increment->left) {
                    generate_expression(node->increment->left);
                } else {
                    generate_node(node->increment);
                }
            }
            fprintf(out, ") ");
            generate_gen_body_node(node->left, all_vars);
            break;
        }
        case AST_FOR_IN: {
            write_indent();
            generate_node(node);
            break;
        }
        case AST_IF: {
            write_indent();
            fprintf(out, "if (");
            generate_expression(node->condition);
            fprintf(out, ") ");
            generate_gen_body_node(node->left, all_vars);
            if (node->right) {
                fprintf(out, " else ");
                generate_gen_body_node(node->right, all_vars);
            }
            break;
        }
        default: {
            write_indent();
            generate_node(node);
            break;
        }
    }
}

void generate_generator_code(ASTNode* node, const char* func_name) {
    FILE* saved_out = out;
    int saved_indent = indent_level;
    
    GenLocal* params = NULL;
    for (int i = node->child_count - 1; i >= 0; i--) {
        ASTNode* param = node->children[i];
        GenLocal* gl = (GenLocal*)malloc(sizeof(GenLocal));
        gl->name = strndup(param->token.start, param->token.length);
        gl->name_len = param->token.length;
        gl->type = param->var_type.type;
        gl->is_pointer = param->is_pointer;
        gl->is_class = param->is_class;
        gl->next = params;
        params = gl;
    }
    
    GenLocal* locals = NULL;
    collect_locals_from_block(node->left, &locals);
    
    GenLocal* all_vars = NULL;
    for (GenLocal* gl = params; gl; gl = gl->next) {
        GenLocal* copy = (GenLocal*)malloc(sizeof(GenLocal));
        copy->name = strndup(gl->name, gl->name_len);
        copy->name_len = gl->name_len;
        copy->type = gl->type;
        copy->is_pointer = gl->is_pointer;
        copy->is_class = gl->is_class;
        copy->next = all_vars;
        all_vars = copy;
    }
    for (GenLocal* gl = locals; gl; gl = gl->next) {
        GenLocal* copy = (GenLocal*)malloc(sizeof(GenLocal));
        copy->name = strndup(gl->name, gl->name_len);
        copy->name_len = gl->name_len;
        copy->type = gl->type;
        copy->is_pointer = gl->is_pointer;
        copy->is_class = gl->is_class;
        copy->next = all_vars;
        all_vars = copy;
    }
    
    out = closure_defs_out;
    fprintf(out, "typedef struct {\n");
    fprintf(out, "    int _pc;\n");
    fprintf(out, "    int current;\n");
    fprintf(out, "    bool done;\n");
    fprintf(out, "    int _received;\n");
    for (GenLocal* gl = params; gl; gl = gl->next) {
        fprintf(out, "    ");
        switch (gl->type) {
            case TOKEN_INT_TYPE: fprintf(out, "int"); break;
            case TOKEN_FLOAT_TYPE: fprintf(out, "float"); break;
            case TOKEN_STR_TYPE: fprintf(out, "CnextString"); break;
            case TOKEN_BOOL_TYPE: fprintf(out, "bool"); break;
            case TOKEN_CHAR_TYPE: fprintf(out, "char"); break;
            default: fprintf(out, "__auto_type"); break;
        }
        if (gl->is_pointer || gl->is_class) fprintf(out, "*");
        fprintf(out, " %.*s;\n", gl->name_len, gl->name);
    }
    for (GenLocal* gl = locals; gl; gl = gl->next) {
        fprintf(out, "    ");
        switch (gl->type) {
            case TOKEN_INT_TYPE: fprintf(out, "int"); break;
            case TOKEN_FLOAT_TYPE: fprintf(out, "float"); break;
            case TOKEN_STR_TYPE: fprintf(out, "CnextString"); break;
            case TOKEN_BOOL_TYPE: fprintf(out, "bool"); break;
            case TOKEN_CHAR_TYPE: fprintf(out, "char"); break;
            default: fprintf(out, "__auto_type"); break;
        }
        if (gl->is_pointer || gl->is_class) fprintf(out, "*");
        fprintf(out, " %.*s;\n", gl->name_len, gl->name);
    }
    fprintf(out, "} _gen_%s_frame;\n\n", func_name);
    
    fprintf(out, "bool _gen_%s_next(_gen_%s_frame* f) {\n", func_name, func_name);
    fprintf(out, "    switch (f->_pc) {\n");
    
    for (GenLocal* gl = all_vars; gl; gl = gl->next) {
        fprintf(out, "    #define %.*s (f->%.*s)\n", gl->name_len, gl->name, gl->name_len, gl->name);
    }
    fprintf(out, "    case 0:\n");
    
    gen_yield_counter = 0;
    generate_gen_body_node(node->left, all_vars);
    
    for (GenLocal* gl = all_vars; gl; gl = gl->next) {
        fprintf(out, "    #undef %.*s\n", gl->name_len, gl->name);
    }
    
    fprintf(out, "    }\n");
    fprintf(out, "    f->done = true;\n");
    fprintf(out, "    return false;\n");
    fprintf(out, "}\n\n");
    
    fprintf(out, "_gen_%s_frame* %s(", func_name, func_name);
    bool first = true;
    for (GenLocal* gl = params; gl; gl = gl->next) {
        if (!first) fprintf(out, ", ");
        switch (gl->type) {
            case TOKEN_INT_TYPE: fprintf(out, "int"); break;
            case TOKEN_FLOAT_TYPE: fprintf(out, "float"); break;
            case TOKEN_STR_TYPE: fprintf(out, "CnextString"); break;
            case TOKEN_BOOL_TYPE: fprintf(out, "bool"); break;
            case TOKEN_CHAR_TYPE: fprintf(out, "char"); break;
            default: fprintf(out, "__auto_type"); break;
        }
        if (gl->is_pointer || gl->is_class) fprintf(out, "*");
        fprintf(out, " %.*s", gl->name_len, gl->name);
        first = false;
    }
    fprintf(out, ") {\n");
    fprintf(out, "    _gen_%s_frame* f = (_gen_%s_frame*)malloc(sizeof(_gen_%s_frame));\n", 
            func_name, func_name, func_name);
    fprintf(out, "    _cnext_track(f);\n");
    fprintf(out, "    f->_pc = 0;\n");
    fprintf(out, "    f->done = false;\n");
    fprintf(out, "    f->current = 0;\n");
    fprintf(out, "    f->_received = 0;\n");
    for (GenLocal* gl = params; gl; gl = gl->next) {
        fprintf(out, "    f->%.*s = %.*s;\n", gl->name_len, gl->name, gl->name_len, gl->name);
    }
    fprintf(out, "    return f;\n");
    fprintf(out, "}\n\n");
    
    out = saved_out;
    indent_level = saved_indent;
    
    free_gen_locals(all_vars);
    free_gen_locals(locals);
    free_gen_locals(params);
}
