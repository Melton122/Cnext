#include "semantics_internal.h"

void analyze_function(ASTNode* node, bool define_name) {
    if (define_name && !resolve_current_symbol(node->token)) {
        define_symbol(node->token, TOKEN_FUNC, true, NULL, node);
    }

    // Detect generator functions (return type is iter<T>)
    bool was_generator = false;
    if (node->return_type.type == TOKEN_ITER) {
        node->is_generator = true;
        was_generator = true;
        generator_depth++;
    }

    sem_push_scope();
    // Define generic type parameters in scope as valid types BEFORE any type validation
    for (int i = 0; i < node->type_param_count; i++) {
        ASTNode* tp = node->type_params[i];
        Symbol* sym = (Symbol*)checked_malloc(sizeof(Symbol));
        sym->name = checked_strndup(tp->token.start, tp->token.length);
        sym->type = TOKEN_IDENTIFIER; // Opaque type
        sym->is_const = true;
        sym->type_name = checked_strndup(tp->token.start, tp->token.length);
        if (!sym->name || !sym->type_name) { free(sym->name); free(sym->type_name); free(sym); continue; }
        sym->decl_node = tp;
        sym->next = sem_current_scope->symbols;
        sem_current_scope->symbols = sym;
    }

    validate_type_token(node->return_type);

    for (int i = 0; i < node->child_count; i++) {
        ASTNode* param = node->children[i];
        validate_type_token(param->var_type);
        char* type_name = type_name_from_token(param->var_type);
        define_symbol(param->token, param->var_type.type, false, type_name, param);
        // Mark class-type parameters as pointer (class instances are heap-allocated)
        if (param->var_type.type == TOKEN_IDENTIFIER) {
            Symbol* type_sym = resolve_symbol(param->var_type);
            if (type_sym && type_sym->type == TOKEN_CLASS) {
                param->is_class = true;
                param->is_pointer = true;
            }
        }
        free(type_name);
        
        // Analyze default value if present
        if (param->default_value) {
            analyze_expression(param->default_value);
        }
    }
    analyze_node(node->left);
    sem_pop_scope();
    if (was_generator) generator_depth--;
}

void analyze_assignment(ASTNode* node) {
    CnextTokenType left_type = analyze_expression(node->left);
    analyze_expression(node->right);

    if (node->right && node->right->type == AST_NULL_LITERAL) {
        node->right->expr_type = left_type;
        if (node->left->type_name) {
            node->right->type_name = copy_cstring(node->left->type_name);
        } else if (node->left->type == AST_IDENTIFIER) {
            Symbol* sym = resolve_symbol(node->left->token);
            if (sym && sym->type_name) {
                node->right->type_name = copy_cstring(sym->type_name);
            }
        }
    }

    if (!node->left) {
        report_error(node->token.line, "Invalid assignment target.", NULL);
        return;
    }

    if (node->left->type == AST_IDENTIFIER) {
        Symbol* sym = resolve_symbol(node->left->token);
        if (!sym) {
            // Already reported by analyze_expression
        } else if (sym->is_const) {
            report_error(node->left->token.line, "Cannot reassign to const variable:", sym->name);
        }
        return;
    }

    if (node->left->type == AST_MEMBER_ACCESS || node->left->type == AST_INDEX) {
        return;
    }

    report_error(node->token.line, "Invalid assignment target.", NULL);
}

void analyze_postfix(ASTNode* node) {
    if (!node->left) {
        report_error(node->token.line, "Invalid postfix target.", NULL);
        return;
    }

    if (node->left->type == AST_IDENTIFIER) {
        Symbol* sym = resolve_symbol(node->left->token);
        if (!sym) {
            report_token_error(node->left->token, "Undeclared variable:");
        } else if (sym->is_const) {
            report_error(node->left->token.line, "Cannot mutate const variable:", sym->name);
        }
        return;
    }

    if (node->left->type == AST_MEMBER_ACCESS || node->left->type == AST_INDEX) {
        analyze_expression(node->left);
        return;
    }

    analyze_expression(node->left);
    report_error(node->token.line, "Invalid postfix target.", NULL);
}

void analyze_node(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
        case AST_BLOCK:
            sem_push_scope();
            for (int i = 0; i < node->child_count; i++) analyze_node(node->children[i]);
            sem_pop_scope();
            break;
        case AST_MAIN:
            analyze_node(node->left);
            break;
        case AST_VAR_DECL:
            analyze_var_declaration(node);
            break;
        case AST_FUNC_DECL:
            analyze_function(node, true);
            break;
        case AST_STRUCT_DECL:
            for (int i = 0; i < node->child_count; i++) {
                analyze_field_declaration(node->children[i]);
            }
            break;
        case AST_CLASS_DECL:
            define_type_symbol(node, TOKEN_CLASS);
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_FUNC_DECL) {
                    analyze_function(node->children[i], false);
                } else if (node->children[i]->type == AST_OPERATOR_DECL) {
                    analyze_node(node->children[i]);
                } else {
                    analyze_field_declaration(node->children[i]);
                }
            }
            break;
        case AST_ENUM_DECL:
            break;
        case AST_IMPORT:
            register_import(node->token);
            break;
        case AST_ASSIGN:
            analyze_assignment(node);
            break;
        case AST_EXPR_STMT:
            analyze_expression(node->left);
            break;
        case AST_IF:
            analyze_expression(node->condition);
            analyze_node(node->left);
            if (node->right) analyze_node(node->right);
            break;
        case AST_WHILE:
            loop_depth++;
            analyze_expression(node->condition);
            analyze_node(node->left);
            loop_depth--;
            break;
        case AST_FOR:
            loop_depth++;
            sem_push_scope();
            analyze_node(node->init);
            analyze_expression(node->condition);
            analyze_node(node->increment);
            analyze_node(node->left);
            sem_pop_scope();
            loop_depth--;
            break;
        case AST_FOR_IN:
            loop_depth++;
            sem_push_scope();
            analyze_expression(node->condition);
            if (node->init) define_symbol(node->init->token, TOKEN_VAR, false, NULL, node->init);
            analyze_node(node->left);
            sem_pop_scope();
            loop_depth--;
            break;
        case AST_SWITCH:
            analyze_expression(node->condition);
            switch_depth++;
            for (int i = 0; i < node->child_count; i++) {
                analyze_node(node->children[i]);
            }
            switch_depth--;
            break;
        case AST_CASE:
            analyze_expression(node->condition);
            analyze_node(node->left);
            break;
        case AST_DEFAULT:
            analyze_node(node->left);
            break;
        case AST_BREAK:
            if (loop_depth == 0 && switch_depth == 0) {
                report_error(node->token.line, "Cannot use 'break' outside of a loop or switch.", NULL);
            }
            break;
        case AST_CONTINUE:
            if (loop_depth == 0) {
                report_error(node->token.line, "Cannot use 'continue' outside of a loop.", NULL);
            }
            break;
        case AST_YIELD:
            if (generator_depth == 0) {
                report_error(node->token.line, "Cannot use 'yield' outside of a generator function.", NULL);
            }
            if (node->left) analyze_expression(node->left);
            break;
        case AST_COROUTINE_DECL:
        case AST_ASYNC_FUNC_DECL:
            // Treat like FUNC_DECL for semantic analysis
            analyze_function(node, true);
            break;
        case AST_RESUME_EXPR:
            // resume co [with val]
            analyze_expression(node->left);
            if (node->right) analyze_expression(node->right);
            break;
        case AST_AWAIT_EXPR:
            // await expr — should be inside async function (validated by generator_depth)
            analyze_expression(node->left);
            break;
        case AST_RUN_ASYNC:
            // run_async func()
            if (node->left) analyze_expression(node->left);
            break;
        case AST_INDEX:
            analyze_expression(node->left);
            analyze_expression(node->right);
            break;
        case AST_SLICE:
            analyze_expression(node->left);
            if (node->init) analyze_expression(node->init);
            if (node->right) analyze_expression(node->right);
            break;
        case AST_POSTFIX:
            analyze_postfix(node);
            break;
        case AST_RETURN:
            if (node->left) analyze_expression(node->left);
            break;
        case AST_TRY:
            analyze_node(node->left); 
            if (node->right) { 
                sem_push_scope();
                if (node->right->token.start && node->right->token.length > 0) {
                    define_symbol(node->right->token, node->right->var_type.type, false, NULL, node->right);
                }
                analyze_node(node->right->left); 
                sem_pop_scope();
            }
            if (node->condition) { 
                analyze_node(node->condition);
            }
            break;
        case AST_THROW:
            if (node->left) analyze_expression(node->left);
            break;
        case AST_MATCH:
            analyze_expression(node->condition);
            for (int i = 0; i < node->child_count; i++) {
                ASTNode* arm = node->children[i];
                for (int j = 0; j < arm->child_count; j++) {
                    analyze_expression(arm->children[j]);
                }
                // Guard clause: pattern if condition => body
                if (arm->condition) {
                    analyze_expression(arm->condition);
                }
                if (arm->left) analyze_node(arm->left);
            }
            break;
        case AST_INTERFACE_DECL:
        case AST_TRAIT_DECL:
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_FUNC_DECL) {
                    validate_type_token(node->children[i]->return_type);
                }
            }
            if (node->type == AST_TRAIT_DECL) {
                // Traits can have default implementations - analyze their bodies
                for (int i = 0; i < node->child_count; i++) {
                    if (node->children[i]->type == AST_FUNC_DECL && node->children[i]->left) {
                        analyze_function(node->children[i], true);
                    }
                }
            }
            break;
        case AST_TEST:
            analyze_node(node->left);
            break;
        case AST_ASSERT:
            analyze_expression(node->left);
            break;
        case AST_CONSTRUCTOR:
            sem_push_scope();
            for (int i = 0; i < node->child_count; i++) {
                ASTNode* param = node->children[i];
                validate_type_token(param->var_type);
                char* tn = type_name_from_token(param->var_type);
                define_symbol(param->token, param->var_type.type, false, tn, param);
                free(tn);
            }
            analyze_node(node->left);
            sem_pop_scope();
            break;
        case AST_OPERATOR_DECL: {
            // Analyze operator declaration
            // Register as a function
            define_symbol(node->token, TOKEN_FUNC, true, NULL, node);
            sem_push_scope();
            for (int i = 0; i < node->child_count; i++) {
                ASTNode* param = node->children[i];
                validate_type_token(param->var_type);
                char* type_name = type_name_from_token(param->var_type);
                define_symbol(param->token, param->var_type.type, false, type_name, param);
                free(type_name);
            }
            analyze_node(node->left);
            sem_pop_scope();
            break;
        }
        case AST_EXTEND_DECL: {
            // Analyze extension methods
            // For now, allow extending any type (built-in or user-defined)
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_FUNC_DECL) {
                    analyze_function(node->children[i], false);
                }
            }
            break;
        }
        case AST_DESTRUCTURE: {
            // Analyze destructuring
            analyze_expression(node->right);
            for (int i = 0; i < node->child_count; i++) {
                ASTNode* var = node->children[i];
                validate_type_token(var->var_type);
                char* type_name = type_name_from_token(var->var_type);
                define_symbol(var->token, var->var_type.type, false, type_name, var);
                free(type_name);
            }
            break;
        }
        case AST_TUPLE:
            // Analyze tuple elements
            for (int i = 0; i < node->child_count; i++) {
                analyze_expression(node->children[i]);
            }
            break;
        case AST_OPTION_SOME:
        case AST_OPTION_NONE:
        case AST_RESULT_OK:
        case AST_RESULT_ERR:
            if (node->left) analyze_expression(node->left);
            break;
        case AST_NAMED_ARG:
            analyze_expression(node->right);
            break;
        case AST_MACRO_DECL:
            // Macro declarations are compile-time only and do not emit runtime code.
            if (node->left) analyze_node(node->left);
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]) analyze_node(node->children[i]);
            }
            break;
        case AST_CONSTEXPR_DECL: {
            // constexpr — analyze like const but validate initializer is a constant expression
            validate_type_token(node->var_type);
            char* tn = type_name_from_token(node->var_type);
            define_symbol(node->token, node->var_type.type, true, tn, node);
            free(tn);
            if (node->left) {
                analyze_expression(node->left);
            }
            break;
        }
        case AST_ATTRIBUTE:
            // Attributes are metadata annotations; they don't need runtime analysis.
            break;
        case AST_MACRO_INVOCATION:
            // Macro invocations should have been expanded during parsing.
            // If we see one here, it wasn't expanded — analyze as a regular call.
            if (node->left) analyze_expression(node->left);
            break;
        case AST_EXTERN_DECL:
            // Extern declarations are C function prototypes — no analysis needed.
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]) analyze_node(node->children[i]);
            }
            break;
        case AST_BENCH_DECL:
            // bench block — analyze the body
            if (node->left) analyze_node(node->left);
            break;
        case AST_OWN_EXPR:
            // own expr — analyze the expression, ownership is a semantic hint
            if (node->left) analyze_expression(node->left);
            break;
        default:
            break;
    }
}
