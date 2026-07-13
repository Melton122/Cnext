#include "semantics_internal.h"

void reset_semantics_state(void) {
    semantics_source = NULL;
    while (sem_current_scope) {
        sem_pop_scope();
    }
    has_semantic_error = false;
    loop_depth = 0;
    switch_depth = 0;
    generator_depth = 0;
}

bool analyze_semantics(ASTNode* program, bool require_main) {
    has_semantic_error = false;
    sem_current_scope = NULL;
    loop_depth = 0;
    switch_depth = 0;
    generator_depth = 0;
    sem_push_scope();

    Token printin_token = {TOKEN_IDENTIFIER, "printin", 7, 0};
    Token input_token = {TOKEN_IDENTIFIER, "input", 5, 0};
    Token free_token = {TOKEN_IDENTIFIER, "free", 4, 0};
    define_symbol_if_missing(printin_token, TOKEN_FUNC, true, NULL);
    define_symbol_if_missing(input_token, TOKEN_FUNC, true, NULL);
    define_symbol_if_missing(free_token, TOKEN_FUNC, true, NULL);

    for (int i = 0; i < program->child_count; i++) {
        predeclare_global(program->children[i]);
    }

    bool has_main = false;
    bool has_test = false;
    for (int i = 0; i < program->child_count; i++) {
        if (program->children[i]->type == AST_MAIN) {
            has_main = true;
        }
        if (program->children[i]->type == AST_TEST) {
            has_test = true;
        }
        analyze_node(program->children[i]);
    }
    if (require_main && !has_main && !has_test) {
        report_error(0, "Program must have a 'main' block.", NULL);
    }

    sem_pop_scope();
    return !has_semantic_error;
}
