#include "semantics_internal.h"

// Defines a parameter in the current scope and marks class-typed parameters as
// pointers. Class instances are heap-allocated, so a parameter of class type is
// passed as `Klass*`; without this the declaration is emitted by value and every
// use inside the body mismatches it. `analyze_function` has always done this, and
// constructors and operator declarations need the same treatment or a
// `func new(Node n)` signature disagrees with its own call sites.
static void define_parameter(ASTNode* param) {
    validate_type_token(param->var_type);
    char* type_name = type_name_from_token(param->var_type);
    define_symbol(param->token, param->var_type.type, false, type_name, param);
    if (param->var_type.type == TOKEN_IDENTIFIER) {
        Symbol* type_sym = resolve_symbol(param->var_type);
        // An array parameter is a value struct (`CnextArrayObj` for class
        // elements), not a pointer, even when its element type is a class. The
        // element type is what resolves to a class here, so the is_array guard
        // is what keeps `Node[] items` from being declared `CnextArrayObj*`
        // while every caller passes the struct itself.
        if (type_sym && type_sym->type == TOKEN_CLASS && !param->is_array) {
            param->is_class = true;
            param->is_pointer = true;
        }
    }
    free(type_name);
}

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

    // A class-typed return value is a pointer, exactly like a class-typed local,
    // field or parameter. Record it so codegen emits `Klass*` rather than
    // `Klass` for the return type. Without this, a function returning a class
    // instance declares a by-value return type and fails to compile.
    if (node->return_type.type == TOKEN_IDENTIFIER) {
        Symbol* rt_sym = resolve_symbol(node->return_type);
        if (rt_sym && rt_sym->type == TOKEN_CLASS &&
            rt_sym->decl_node && rt_sym->decl_node->type != AST_TYPE_ALIAS) {
            node->is_class = true;
        }
    }

    for (int i = 0; i < node->child_count; i++) {
        ASTNode* param = node->children[i];
        define_parameter(param);

        // Analyze default value if present
        if (param->default_value) {
            analyze_expression(param->default_value);
        }
    }
    // The return type types a `return input()` in the body.
    CnextTokenType saved_return_type = sem_current_return_type;
    sem_current_return_type = node->return_type.type;
    analyze_node(node->left);
    sem_current_return_type = saved_return_type;
    sem_pop_scope();
    if (was_generator) generator_depth--;
}

// Resolve the declaration of a `self.field` / `obj.field` access so the field's
// real type (notably whether it is an array) can be inspected. The enclosing
// class is taken from the base expression: `self` is an ordinary parameter
// typed as its class (bound explicitly for constructors, which have no `self`
// parameter), and a nested field carries its class in type_name.
ASTNode* resolve_field_decl(ASTNode* access) {
    if (!access || access->type != AST_MEMBER_ACCESS || !access->left) return NULL;
    const char* class_name = NULL;
    if (access->left->type == AST_IDENTIFIER) {
        Symbol* base_sym = resolve_symbol(access->left->token);
        if (base_sym) class_name = base_sym->type_name;
    } else if (access->left->type == AST_MEMBER_ACCESS && access->left->type_name) {
        class_name = access->left->type_name;
    }
    if (!class_name) return NULL;
    int cn_len = (int)strlen(class_name);
    Token class_token = {TOKEN_IDENTIFIER, class_name, cn_len, access->token.line, 0};
    Symbol* class_sym = resolve_symbol(class_token);
    if (!class_sym || !class_sym->decl_node) return NULL;
    ASTNode* class_node = class_sym->decl_node;
    for (int i = 0; i < class_node->child_count; i++) {
        ASTNode* child = class_node->children[i];
        if (child->type == AST_VAR_DECL &&
            child->token.length == access->token.length &&
            strncmp(child->token.start, access->token.start, access->token.length) == 0) {
            return child;
        }
    }
    return NULL;
}

// `arr = { ... }` needs the element type so codegen can build the right array
// struct. Record it on the literal itself.
static void annotate_array_literal_assignment(ASTNode* node, ASTNode* decl) {
    if (!node->right || node->right->type != AST_LITERAL_ARRAY) return;
    if (!decl || !decl->is_array) return;
    node->right->var_type = decl->var_type;
    node->right->is_array = true;
    node->right->is_class = decl->is_class || false;
    if (decl->var_type.type == TOKEN_IDENTIFIER) {
        char* elem = type_name_from_token(decl->var_type);
        if (elem) {
            int elen = (int)strlen(elem);
            Token elem_token = {TOKEN_IDENTIFIER, elem, elen, node->token.line, 0};
            Symbol* elem_sym = resolve_symbol(elem_token);
            if (elem_sym && elem_sym->type == TOKEN_CLASS) node->right->is_class = true;
            free(elem);
        }
    }
}

// The type a destination expects. `input()` is context-sensitive, so the
// destination decides which reader the read uses: an element of an `int[]`
// wants an integer, a field of type `float` wants a float.
static CnextTokenType assignment_target_type(ASTNode* left) {
    if (!left) return TOKEN_EOF;

    if (left->type == AST_IDENTIFIER) {
        Symbol* sym = resolve_symbol(left->token);
        return sym ? sym->type : TOKEN_EOF;
    }

    if (left->type == AST_MEMBER_ACCESS) {
        ASTNode* field = resolve_field_decl(left);
        return field ? field->var_type.type : TOKEN_EOF;
    }

    if (left->type == AST_INDEX) {
        ASTNode* base = left->left;
        ASTNode* base_decl = NULL;
        if (base && base->type == AST_IDENTIFIER) {
            Symbol* base_sym = resolve_symbol(base->token);
            if (base_sym) base_decl = base_sym->decl_node;
        } else if (base && base->type == AST_MEMBER_ACCESS) {
            base_decl = resolve_field_decl(base);
        }
        // An array's declared type is its element type.
        if (base_decl && base_decl->is_array) return base_decl->var_type.type;
        return TOKEN_EOF;
    }

    return TOKEN_EOF;
}

void analyze_assignment(ASTNode* node) {
    CnextTokenType left_type = analyze_expression(node->left);
    // `input()` is context-sensitive: assigning it to a typed destination
    // selects the corresponding runtime reader before analyzing the RHS.
    CnextTokenType target_type = left_type;
    if (target_type == TOKEN_EOF || target_type == TOKEN_VAR) {
        target_type = assignment_target_type(node->left);
    }
    if (node->right) {
        sem_apply_expected_type(node->right, target_type);
    }
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
        report_error(ERR_SEM_INVALID_ASSIGN, node->token.line, "Invalid assignment target.", NULL);
        return;
    }

    if (node->left->type == AST_IDENTIFIER) {
        Symbol* sym = resolve_symbol(node->left->token);
        if (!sym) {
            // Already reported by analyze_expression
        } else if (sym->is_const) {
            report_error(ERR_SEM_CONST_MUTATE, node->left->token.line, "Cannot reassign to const variable:", sym->name);
        }
        annotate_array_literal_assignment(node, sym ? sym->decl_node : NULL);
        return;
    }

    if (node->left->type == AST_MEMBER_ACCESS) {
        annotate_array_literal_assignment(node, resolve_field_decl(node->left));
        return;
    }

    if (node->left->type == AST_INDEX) {
        return;
    }

    report_error(ERR_SEM_INVALID_ASSIGN, node->token.line, "Invalid assignment target.", NULL);
}

void analyze_postfix(ASTNode* node) {
    if (!node->left) {
        report_error(ERR_SEM_INVALID_POSTFIX, node->token.line, "Invalid postfix target.", NULL);
        return;
    }

    if (node->left->type == AST_IDENTIFIER) {
        Symbol* sym = resolve_symbol(node->left->token);
        if (!sym) {
            report_token_error(ERR_SEM_UNDECLARED, node->left->token, "Undeclared variable:");
        } else if (sym->is_const) {
            report_error(ERR_SEM_CONST_MUTATE, node->left->token.line, "Cannot mutate const variable:", sym->name);
        }
        return;
    }

    if (node->left->type == AST_MEMBER_ACCESS || node->left->type == AST_INDEX) {
        analyze_expression(node->left);
        return;
    }

    analyze_expression(node->left);
    report_error(ERR_SEM_INVALID_POSTFIX, node->token.line, "Invalid postfix target.", NULL);
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
                } else if (node->children[i]->type == AST_CONSTRUCTOR) {
                    // A constructor is not a field: its children are parameters
                    // and its `left` is a body that must be analyzed. Routing it
                    // to analyze_field_declaration would silently skip the body.
                    analyze_node(node->children[i]);
                } else if (node->children[i]->type == AST_OPERATOR_DECL) {
                    analyze_node(node->children[i]);
                } else {
                    analyze_field_declaration(node->children[i]);
                }
            }
            // Check trait/interface conformance
            if (node->implements_names) {
                char names_copy[1024];
                snprintf(names_copy, sizeof(names_copy), "%s", node->implements_names);
                char* name = names_copy;
                while (name && *name) {
                    char* comma = strchr(name, ',');
                    if (comma) *comma = '\0';
                    while (*name == ' ' || *name == '\t') name++;
                    if (*name) {
                        Token trait_tok = {TOKEN_IDENTIFIER, name, (int)strlen(name), node->token.line};
                        Symbol* trait_sym = resolve_symbol(trait_tok);
                        if (trait_sym && trait_sym->decl_node &&
                            (trait_sym->decl_node->type == AST_TRAIT_DECL ||
                             trait_sym->decl_node->type == AST_INTERFACE_DECL)) {
                            ASTNode* trait_node = trait_sym->decl_node;
                            for (int t = 0; t < trait_node->child_count; t++) {
                                ASTNode* tmethod = trait_node->children[t];
                                if (tmethod->type != AST_FUNC_DECL) continue;
                                // Interface methods have no body; trait abstract methods have no body
                                if (tmethod->left != NULL) continue;
                                // Check if class implements this method
                                bool found = false;
                                for (int c = 0; c < node->child_count; c++) {
                                    ASTNode* cmethod = node->children[c];
                                    if (cmethod->type == AST_FUNC_DECL &&
                                        cmethod->token.length == tmethod->token.length &&
                                        strncmp(cmethod->token.start, tmethod->token.start, tmethod->token.length) == 0) {
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found) {
                                    char msg[512];
                                    snprintf(msg, sizeof(msg), "Class '%.*s' does not implement required method '%.*s' from '%s'.",
                                             node->token.length, node->token.start,
                                             tmethod->token.length, tmethod->token.start, name);
                                     report_error(ERR_SEM_MISSING_METHOD, node->token.line, msg, NULL);
                                }
                            }
                        }
                    }
                    if (comma) name = comma + 1;
                    else break;
                }
            }
            break;
        case AST_ENUM_DECL: {
            // Validate variant payload types exist
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_VARIANT) {
                    ASTNode* variant = node->children[i];
                    for (int j = 0; j < variant->child_count; j++) {
                        if (variant->children[j]->type == AST_IDENTIFIER) {
                            validate_type_token(variant->children[j]->token);
                        } else if (variant->children[j]->type == AST_UNION_TYPE) {
                            // Validate each type in the union
                            for (int k = 0; k < variant->children[j]->child_count; k++) {
                                validate_type_token(variant->children[j]->children[k]->token);
                            }
                        }
                    }
                }
            }
            break;
        }
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
            if (node->init) {
                if (node->init->type == AST_VAR_DECL && node->init->var_type.type != TOKEN_VAR) {
                    define_symbol(node->init->token, node->init->var_type.type, false, NULL, node->init);
                } else {
                    define_symbol(node->init->token, TOKEN_VAR, false, NULL, node->init);
                }
            }
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
                report_error(ERR_SEM_BREAK_OUTSIDE, node->token.line, "Cannot use 'break' outside of a loop or switch.", NULL);
            }
            break;
        case AST_CONTINUE:
            if (loop_depth == 0) {
                report_error(ERR_SEM_CONTINUE_OUTSIDE, node->token.line, "Cannot use 'continue' outside of a loop.", NULL);
            }
            break;
        case AST_DEFER:
            if (node->left) {
                if (node->left->type == AST_BLOCK) {
                    sem_push_scope();
                    analyze_node(node->left);
                    sem_pop_scope();
                } else {
                    analyze_expression(node->left);
                }
            }
            break;
        case AST_YIELD:
            if (generator_depth == 0) {
                report_error(ERR_SEM_YIELD_OUTSIDE, node->token.line, "Cannot use 'yield' outside of a generator function.", NULL);
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
            if (node->left) {
                // `return input()` in `func ask(): int` reads an integer: the
                // return type is the context that types the read.
                sem_apply_expected_type(node->left, sem_current_return_type);
                analyze_expression(node->left);
            }
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
            // Constructors carry no explicit `self` parameter, because codegen
            // emits the receiver itself. Bind it here so that `self.field`
            // resolves the same way it does inside a method.
            if (node->var_type.type == TOKEN_IDENTIFIER) {
                Token self_token = {TOKEN_IDENTIFIER, "self", 4, node->token.line, 0};
                char* class_name = type_name_from_token(node->var_type);
                define_symbol(self_token, TOKEN_IDENTIFIER, false, class_name, node);
                free(class_name);
            }
            for (int i = 0; i < node->child_count; i++) {
                define_parameter(node->children[i]);
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
                define_parameter(node->children[i]);
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
        case AST_TYPE_ALIAS:
            // type MyType = OtherType — validate the target type exists and register the alias
            if (node->left) validate_type_token(node->left->token);
            define_type_symbol(node, TOKEN_CLASS);
            break;
        default:
            break;
    }
}
