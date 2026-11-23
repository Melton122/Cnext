#include "codegen_internal.h"

// Closure support: track captured variables per closure
CapturedVar* active_captures = NULL;

void emit_bound_name(ASTNode* declaration, const char* name, int length) {
    for (CapturedVar* cap = active_captures; cap; cap = cap->next) {
        if (declaration && cap->declaration == declaration) {
            fprintf(out, cap->by_reference ? "(*_env->%s)" : "(_env->%s)", cap->name);
            return;
        }
    }
    if (declaration && declaration->capture_cell) fprintf(out, "(*_cnext_cell_%.*s)", length, name);
    else fprintf(out, "%.*s", length, name);
}

void emit_capture_cell(ASTNode* declaration, bool parameter) {
    const char* name = declaration->token.start;
    int length = declaration->token.length;
    char mark_name[256];
    snprintf(mark_name, sizeof(mark_name), "_cnext_cell_mark_%.*s", length, name);
    generate_node_type(declaration, false);
    fprintf(out, "* _cnext_cell_%.*s = cnext_cell_create(sizeof(*_cnext_cell_%.*s));\n", length, name, length, name);
    write_indent();
    fprintf(out, "int %s = _cnext_defer_count;\n", mark_name);
    write_indent();
    fprintf(out, "_CnextCellGuard _cnext_cell_guard_%.*s __attribute__((cleanup(_cnext_cell_guard_cleanup))) = { _cnext_cell_%.*s, %s, true };\n", length, name, length, name, mark_name);
    write_indent();
    fprintf(out, "_cnext_defer_push(_cnext_cell_guard_run, &_cnext_cell_guard_%.*s);\n", length, name);
    if (current_scope && !current_scope->first_mark_name) current_scope->first_mark_name = checked_strdup(mark_name);
    if (parameter || declaration->init) {
        write_indent();
        fprintf(out, "*_cnext_cell_%.*s = ", length, name);
        if (parameter) fprintf(out, "%.*s", length, name);
        else generate_expression(declaration->init);
        fprintf(out, ";\n");
    }
}

void push_scope(void) {
    Scope* s = (Scope*)checked_malloc(sizeof(Scope));
    s->closures = NULL;
    s->saved_closure_vars = closure_vars;
    s->parent = current_scope;
    s->first_mark_name = NULL;
    current_scope = s;
}

void pop_scope(void) {
    if (!current_scope) return;
    if (out && current_scope->first_mark_name) {
        write_indent();
        fprintf(out, "_cnext_defer_run_to(%s);\n", current_scope->first_mark_name);
    }
    ScopeClosureVar* cv = current_scope->closures;
    while (cv) {
        ScopeClosureVar* next = cv->next;
        free(cv->name);
        free(cv);
        cv = next;
    }
    Scope* parent = current_scope->parent;
    while (closure_vars && closure_vars != current_scope->saved_closure_vars) {
        ClosureVar* next = closure_vars->next;
        free(closure_vars->var_name);
        free(closure_vars);
        closure_vars = next;
    }
    free(current_scope->first_mark_name);
    free(current_scope);
    current_scope = parent;
}

void register_scope_closure(const char* name, int len) {
    if (!current_scope) return;
    ScopeClosureVar* cv = (ScopeClosureVar*)checked_malloc(sizeof(ScopeClosureVar));
    cv->name = checked_strndup(name, len);
    cv->next = current_scope->closures;
    current_scope->closures = cv;
    if (out) {
        char mark_name[256];
        snprintf(mark_name, sizeof(mark_name), "_cnext_closure_mark_%s", cv->name);
        if (!current_scope->first_mark_name) current_scope->first_mark_name = checked_strdup(mark_name);
        write_indent();
        fprintf(out, "int %s = _cnext_defer_count;\n", mark_name);
        write_indent();
        fprintf(out, "_CnextClosureGuard _cnext_closure_guard_%s __attribute__((cleanup(_cnext_closure_guard_cleanup))) = { &%s, _cnext_closure_mark_%s, true };\n", cv->name, cv->name, cv->name);
        write_indent();
        fprintf(out, "_cnext_defer_push(_cnext_closure_guard_run, &_cnext_closure_guard_%s);\n", cv->name);
    }
}

void generate_owned_closure_expression(ASTNode* node) {
    if (!node) return;
    if (node->type == AST_IDENTIFIER || node->type == AST_MEMBER_ACCESS || node->type == AST_INDEX) {
        fprintf(out, "cnext_closure_retain(");
        generate_expression(node);
        fprintf(out, ")");
    } else if (node->type == AST_TERNARY) {
        fprintf(out, "(");
        generate_expression(node->condition);
        fprintf(out, " ? ");
        generate_owned_closure_expression(node->left);
        fprintf(out, " : ");
        generate_owned_closure_expression(node->right);
        fprintf(out, ")");
    } else if (node->type == AST_LAMBDA) {
        CapturedVar* captures = NULL;
        detect_captures_walk(node->left, &captures, node, program_node);
        bool captured = captures != NULL;
        free_captures(captures);
        if (!captured) fprintf(out, "(CnextClosure){ ");
        generate_expression(node);
        if (!captured) fprintf(out, ", NULL, 1, NULL }");
    } else {
        generate_expression(node);
    }
}

bool is_lambda_param(ASTNode* lambda, const char* name, int name_len) {
    for (int i = 0; i < lambda->child_count; i++) {
        ASTNode* param = lambda->children[i];
        if (param->token.length == name_len && strncmp(param->token.start, name, name_len) == 0) {
            return true;
        }
    }
    return false;
}

static bool is_closure_argument(ASTNode* value) {
    if (!value || value->expr_type != TOKEN_FUNC) return false;
    return value->type == AST_LAMBDA || value->type == AST_CALL || value->type == AST_TERNARY ||
        (value->type == AST_IDENTIFIER && value->binding && value->binding->type == AST_VAR_DECL);
}

bool emit_managed_closure_arguments(ASTNode* call) {
    if (!call || call->type != AST_CALL) return false;
    bool managed = false;
    for (int i = 0; i < call->child_count; i++) managed |= is_closure_argument(call->children[i]);
    if (!managed) return false;
    ASTNode** original = checked_malloc(sizeof(*original) * call->child_count);
    for (int i = 0; i < call->child_count; i++) original[i] = call->children[i];
    fprintf(out, "({ ");
    for (int i = 0; i < call->child_count; i++) {
        if (!is_closure_argument(original[i])) continue;
        char name[64];
        snprintf(name, sizeof(name), "_cnext_arg_%d", i);
        fprintf(out, "CnextClosure %s = ", name);
        generate_owned_closure_expression(original[i]);
        fprintf(out, "; _CnextClosureGuard _cnext_arg_guard_%d __attribute__((cleanup(_cnext_closure_guard_cleanup))) = { &%s, _cnext_defer_count, true }; _cnext_defer_push(_cnext_closure_guard_run, &_cnext_arg_guard_%d); ", i, name, i);
        ASTNode* temporary = create_node(AST_IDENTIFIER, (Token){.type = TOKEN_IDENTIFIER});
        temporary->owned_token_text = checked_strdup(name);
        temporary->token.start = temporary->owned_token_text;
        temporary->token.length = (int)strlen(name);
        temporary->expr_type = TOKEN_FUNC;
        call->children[i] = temporary;
    }
    // Synthetic arguments have no declaration binding, so this recursive call
    // emits the actual call instead of trying to wrap those arguments again.
    generate_expression(call);
    fprintf(out, "; })");
    for (int i = 0; i < call->child_count; i++) {
        if (call->children[i] != original[i]) free_ast(call->children[i]);
        call->children[i] = original[i];
    }
    free(original);
    return true;
}

static bool closure_target_is(ASTNode* node, const char* name, ASTNode* declaration) {
    if (!node || node->type != AST_IDENTIFIER) return false;
    if (declaration) return node->binding == declaration;
    return node->token.length == (int)strlen(name) &&
           memcmp(node->token.start, name, node->token.length) == 0;
}

static bool closure_mutates_name(ASTNode* node, const char* name, ASTNode* declaration) {
    if (!node) return false;
    if (node->type == AST_ASSIGN && closure_target_is(node->left, name, declaration)) return true;
    if ((node->type == AST_POSTFIX && node->left) ||
        (node->type == AST_UNARY && node->right)) {
        ASTNode* target = node->type == AST_POSTFIX ? node->left : node->right;
        if ((node->token.type == TOKEN_INCREMENT || node->token.type == TOKEN_DECREMENT) &&
            closure_target_is(target, name, declaration)) return true;
    }
    for (int i = 0; i < node->child_count; i++) {
        if (closure_mutates_name(node->children[i], name, declaration)) return true;
    }
    return closure_mutates_name(node->left, name, declaration) ||
           closure_mutates_name(node->right, name, declaration) ||
           closure_mutates_name(node->condition, name, declaration) ||
           closure_mutates_name(node->init, name, declaration) ||
           closure_mutates_name(node->increment, name, declaration);
}

void detect_captures_walk(ASTNode* node, CapturedVar** list, ASTNode* lambda, ASTNode* prog) {
    if (!node) return;
    if (node->type == AST_IDENTIFIER) {
        ASTNode* declaration = node->binding;
        if (declaration && declaration->type == AST_VAR_DECL &&
            declaration->lambda_depth >= lambda->lambda_depth) return;
        if (!declaration && node->expr_type == TOKEN_FUNC) return;
        char name[256];
        int nlen = node->token.length < 255 ? node->token.length : 255;
        memcpy(name, node->token.start, nlen);
        name[nlen] = '\0';
        if (!is_lambda_param(lambda, name, nlen)) {
            if (!find_func_decl(prog, name) && !find_class_decl(prog, name)) {
                CapturedVar* existing = *list;
                while (existing) {
                    if (strcmp(existing->name, name) == 0) return;
                    existing = existing->next;
                }
                CnextTokenType cap_type = TOKEN_INT_TYPE;
                bool cap_is_pointer = false;
                bool cap_is_class = false;
                bool found = false;
                if (declaration && declaration->type == AST_VAR_DECL) {
                    cap_type = declaration->var_type.type;
                    cap_is_pointer = declaration->is_pointer;
                    cap_is_class = declaration->is_class;
                    found = true;
                }
                for (int pi = 0; pi < prog->child_count && !found; pi++) {
                    ASTNode* prog_child = prog->children[pi];
                    ASTNode* block = NULL;
                    if (prog_child->type == AST_MAIN) block = prog_child->left;
                    else if (prog_child->type == AST_FUNC_DECL) block = prog_child->left;
                    if (!block) continue;
                    found = find_var_type_in_block(block, name, nlen, &cap_type, &cap_is_pointer, &cap_is_class);
                }
                CapturedVar* cv = (CapturedVar*)checked_malloc(sizeof(CapturedVar));
                cv->declaration = declaration;
                cv->name = checked_strdup(name);
                cv->type = cap_type;
                cv->is_pointer = cap_is_pointer;
                cv->is_class = cap_is_class;
                cv->by_reference = closure_mutates_name(lambda->left, name, declaration);
                cv->next = *list;
                *list = cv;
            }
        }
        return;
    }
    for (int i = 0; i < node->child_count; i++) {
        detect_captures_walk(node->children[i], list, lambda, prog);
    }
    detect_captures_walk(node->left, list, lambda, prog);
    detect_captures_walk(node->right, list, lambda, prog);
    detect_captures_walk(node->condition, list, lambda, prog);
    detect_captures_walk(node->init, list, lambda, prog);
    detect_captures_walk(node->increment, list, lambda, prog);
}

bool find_var_type_in_block(ASTNode* block, const char* name, int nlen,
                            CnextTokenType* out_type, bool* out_is_pointer, bool* out_is_class) {
    if (!block) return false;
    for (int si = 0; si < block->child_count; si++) {
        ASTNode* stmt = block->children[si];
        if (stmt->type == AST_VAR_DECL &&
            stmt->token.length == nlen &&
            strncmp(stmt->token.start, name, nlen) == 0) {
            *out_type = stmt->var_type.type;
            *out_is_pointer = stmt->is_pointer;
            *out_is_class = stmt->is_class;
            return true;
        }
        ASTNode* inner = NULL;
        if (stmt->type == AST_IF) {
            if (stmt->left && stmt->left->type == AST_BLOCK) inner = stmt->left;
            if (inner && find_var_type_in_block(inner, name, nlen, out_type, out_is_pointer, out_is_class)) return true;
            if (stmt->right && stmt->right->type == AST_BLOCK) inner = stmt->right;
            if (inner && find_var_type_in_block(inner, name, nlen, out_type, out_is_pointer, out_is_class)) return true;
        } else if (stmt->type == AST_FOR || stmt->type == AST_WHILE || stmt->type == AST_FOR_IN) {
            if (stmt->left && stmt->left->type == AST_BLOCK) inner = stmt->left;
            if (inner && find_var_type_in_block(inner, name, nlen, out_type, out_is_pointer, out_is_class)) return true;
        } else if (stmt->type == AST_BLOCK) {
            if (find_var_type_in_block(stmt, name, nlen, out_type, out_is_pointer, out_is_class)) return true;
        }
    }
    return false;
}

ClosureVar* find_closure_var(const char* name) {
    for (ClosureVar* cv = closure_vars; cv; cv = cv->next) {
        if (strcmp(cv->var_name, name) == 0) return cv;
    }
    return NULL;
}

void free_captures(CapturedVar* list) {
    while (list) {
        CapturedVar* next = list->next;
        free(list->name);
        free(list);
        list = next;
    }
}

bool is_func_param(const char* name, int name_len) {
    if (!current_func_params) return false;
    for (int i = 0; i < current_func_params->child_count; i++) {
        ASTNode* param = current_func_params->children[i];
        if (param->token.length == name_len &&
            strncmp(param->token.start, name, name_len) == 0 &&
            param->var_type.type == TOKEN_FUNC) {
            return true;
        }
    }
    return false;
}

bool is_func_typed_var(const char* name, int name_len) {
    if (find_closure_var(name)) return true;
    if (!program_node) return false;
    for (int i = 0; i < program_node->child_count; i++) {
        ASTNode* child = program_node->children[i];
        if (child->type == AST_MAIN && child->left) {
            for (int j = 0; j < child->left->child_count; j++) {
                ASTNode* stmt = child->left->children[j];
                if (stmt->type == AST_VAR_DECL &&
                    stmt->token.length == name_len &&
                    strncmp(stmt->token.start, name, name_len) == 0 &&
                    stmt->var_type.type == TOKEN_FUNC) {
                    return true;
                }
            }
        }
    }
    return false;
}
