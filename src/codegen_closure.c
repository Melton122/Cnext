#include "codegen_internal.h"

// Closure support: track captured variables per closure

void push_scope(void) {
    Scope* s = (Scope*)malloc(sizeof(Scope));
    s->closures = NULL;
    s->parent = current_scope;
    current_scope = s;
}

void pop_scope(void) {
    if (!current_scope) return;
    for (ScopeClosureVar* cv = current_scope->closures; cv; cv = cv->next) {
        write_indent();
        fprintf(out, "cnext_closure_decref(&%s);\n", cv->name);
    }
    ScopeClosureVar* cv = current_scope->closures;
    while (cv) {
        ScopeClosureVar* next = cv->next;
        free(cv->name);
        free(cv);
        cv = next;
    }
    Scope* parent = current_scope->parent;
    free(current_scope);
    current_scope = parent;
}

void register_scope_closure(const char* name, int len) {
    if (!current_scope) return;
    ScopeClosureVar* cv = (ScopeClosureVar*)malloc(sizeof(ScopeClosureVar));
    cv->name = strndup(name, len);
    cv->next = current_scope->closures;
    current_scope->closures = cv;
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

void detect_captures_walk(ASTNode* node, CapturedVar** list, ASTNode* lambda, ASTNode* prog) {
    if (!node) return;
    if (node->type == AST_IDENTIFIER) {
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
                for (int pi = 0; pi < prog->child_count && !found; pi++) {
                    ASTNode* prog_child = prog->children[pi];
                    ASTNode* block = NULL;
                    if (prog_child->type == AST_MAIN) block = prog_child->left;
                    else if (prog_child->type == AST_FUNC_DECL) block = prog_child->left;
                    if (!block) continue;
                    found = find_var_type_in_block(block, name, nlen, &cap_type, &cap_is_pointer, &cap_is_class);
                }
                CapturedVar* cv = (CapturedVar*)malloc(sizeof(CapturedVar));
                cv->name = strdup(name);
                cv->type = cap_type;
                cv->is_pointer = cap_is_pointer;
                cv->is_class = cap_is_class;
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
