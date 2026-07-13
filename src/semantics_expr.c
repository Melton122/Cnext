#include "semantics_internal.h"

CnextTokenType analyze_expression(ASTNode* node) {
    if (!node) return TOKEN_EOF;

    CnextTokenType type = TOKEN_EOF;
    switch (node->type) {
        case AST_LITERAL:
            if (node->token.type == TOKEN_NUMBER) type = TOKEN_INT_TYPE;
            else if (node->token.type == TOKEN_FLOAT_LITERAL) type = TOKEN_FLOAT_TYPE;
            else if (node->token.type == TOKEN_STRING_LITERAL) type = TOKEN_STR_TYPE;
            else if (node->token.type == TOKEN_CHAR_LITERAL) type = TOKEN_CHAR_TYPE;
            else if (node->token.type == TOKEN_TRUE || node->token.type == TOKEN_FALSE) type = TOKEN_BOOL_TYPE;
            break;
        case AST_LITERAL_ARRAY:
            for (int i = 0; i < node->child_count; i++) {
                analyze_expression(node->children[i]);
            }
            break;
        case AST_IDENTIFIER: {
            Symbol* sym = resolve_symbol(node->token);
            if (!sym) {
                report_token_error(node->token, "Undeclared identifier:");
            } else {
                if (sym->type_name) {
                    free(node->type_name);
                    node->type_name = copy_cstring(sym->type_name);
                }
                // Propagate is_class from declaration for member access pointer detection
                if (sym->decl_node && sym->decl_node->is_class) {
                    node->is_class = true;
                }
                if (sym->decl_node && sym->decl_node->is_pointer) {
                    node->is_pointer = true;
                }
                type = sym->type;
            }
            break;
        }
        case AST_TYPE_PARAM: {
            Symbol* sym = resolve_symbol(node->token);
            if (!sym) {
                report_token_error(node->token, "Undeclared type parameter:");
            } else {
                type = TOKEN_IDENTIFIER;
            }
            break;
        }
        case AST_BINARY: {
            CnextTokenType left_type = analyze_expression(node->left);
            CnextTokenType right_type = analyze_expression(node->right);
            // Propagate type to null literal in binary comparisons
            if (node->right && node->right->type == AST_NULL_LITERAL && left_type == TOKEN_STR_TYPE) {
                node->right->expr_type = TOKEN_STR_TYPE;
                right_type = TOKEN_STR_TYPE;
            } else if (node->left && node->left->type == AST_NULL_LITERAL && right_type == TOKEN_STR_TYPE) {
                node->left->expr_type = TOKEN_STR_TYPE;
                left_type = TOKEN_STR_TYPE;
            }
            // Operator overload resolution: if left operand is a class type,
            // check if the class defines an operator overload for this binary operator
            if (node->left && node->left->type_name &&
                node->left->type_name[0] != '\0') {
                const char* tn = node->left->type_name;
                if (strncmp(tn, "str", 3) != 0 && strncmp(tn, "int", 3) != 0 &&
                    strncmp(tn, "float", 5) != 0 && strncmp(tn, "bool", 4) != 0 &&
                    strncmp(tn, "char", 4) != 0) {
                    Token class_token = {TOKEN_IDENTIFIER, tn, (int)strlen(tn), node->token.line};
                    Symbol* class_sym = resolve_symbol(class_token);
                    if (class_sym && class_sym->decl_node &&
                        (class_sym->decl_node->type == AST_CLASS_DECL ||
                         class_sym->decl_node->type == AST_STRUCT_DECL)) {
                        ASTNode* class_node = class_sym->decl_node;
                        for (int i = 0; i < class_node->child_count; i++) {
                            ASTNode* child = class_node->children[i];
                            if (child->type == AST_OPERATOR_DECL &&
                                child->token.type == node->token.type) {
                                // Found matching operator overload, use its return type
                                type = child->return_type.type;
                                free(node->type_name);
                                node->type_name = type_name_from_token(child->return_type);
                                break;
                            }
                        }
                    }
                }
            }
            // Standard type inference (only if operator overload didn't resolve)
            if (type == TOKEN_EOF) {
                if (node->token.type == TOKEN_EQ_EQ || node->token.type == TOKEN_BANG_EQ ||
                    node->token.type == TOKEN_LESS || node->token.type == TOKEN_LESS_EQ ||
                    node->token.type == TOKEN_GREATER || node->token.type == TOKEN_GREATER_EQ ||
                    node->token.type == TOKEN_AND_AND || node->token.type == TOKEN_OR_OR) {
                    type = TOKEN_BOOL_TYPE;
                } else if (node->token.type == TOKEN_PLUS &&
                    (left_type == TOKEN_STR_TYPE || right_type == TOKEN_STR_TYPE)) {
                    type = TOKEN_STR_TYPE;
                } else if (left_type == TOKEN_FLOAT_TYPE || right_type == TOKEN_FLOAT_TYPE) {
                    type = TOKEN_FLOAT_TYPE;
                } else if (left_type == TOKEN_INT_TYPE && right_type == TOKEN_INT_TYPE) {
                    type = TOKEN_INT_TYPE;
                }
            }
            break;
        }
        case AST_UNARY:
            type = analyze_expression(node->right);
            break;
        case AST_POSTFIX:
            analyze_postfix(node);
            break;
        case AST_CALL: {
            CnextTokenType callee_type = analyze_expression(node->left);
            if (node->left && node->left->type == AST_IDENTIFIER && callee_type != TOKEN_FUNC) {
                report_token_error(node->left->token, "Cannot call non-function:");
            }
            for (int i = 0; i < node->child_count; i++) {
                analyze_expression(node->children[i]);
            }
            // Propagate str type to null literals passed as function arguments
            if (node->left && node->left->type == AST_IDENTIFIER) {
                Symbol* func_sym = resolve_symbol(node->left->token);
                if (func_sym && func_sym->decl_node && func_sym->decl_node->type == AST_FUNC_DECL) {
                    ASTNode* func_decl = func_sym->decl_node;
                    int param_count = func_decl->child_count < node->child_count ? func_decl->child_count : node->child_count;
                    for (int i = 0; i < param_count; i++) {
                        if (node->children[i]->type == AST_NULL_LITERAL &&
                            func_decl->children[i]->var_type.type == TOKEN_STR_TYPE) {
                            node->children[i]->expr_type = TOKEN_STR_TYPE;
                        }
                    }
                    // Generic type inference: if callee has no explicit type args but function is generic,
                    // infer type parameters from argument types
                    if (func_decl->type_param_count > 0 && node->left->type_arg_count == 0) {
                        for (int tp = 0; tp < func_decl->type_param_count; tp++) {
                            ASTNode* type_param = func_decl->type_params[tp];
                            for (int pi = 0; pi < func_decl->child_count; pi++) {
                                ASTNode* param = func_decl->children[pi];
                                if (param->var_type.type == TOKEN_IDENTIFIER &&
                                    param->var_type.length == type_param->token.length &&
                                    strncmp(param->var_type.start, type_param->token.start, type_param->token.length) == 0) {
                                    if (pi < node->child_count) {
                                        ASTNode* arg = node->children[pi];
                                        Token concrete = {TOKEN_EOF, NULL, 0, 0};
                                        if (arg->type_name && arg->type_name[0] != '\0') {
                                            concrete.type = TOKEN_IDENTIFIER;
                                            concrete.start = arg->type_name;
                                            concrete.length = (int)strlen(arg->type_name);
                                        } else if (arg->expr_type != TOKEN_EOF) {
                                            concrete = arg->expr_type == TOKEN_STR_TYPE ?
                                                (Token){TOKEN_STR_TYPE, "str", 3, 0} :
                                                arg->expr_type == TOKEN_INT_TYPE ?
                                                (Token){TOKEN_INT_TYPE, "int", 3, 0} :
                                                arg->expr_type == TOKEN_FLOAT_TYPE ?
                                                (Token){TOKEN_FLOAT_TYPE, "float", 5, 0} :
                                                arg->expr_type == TOKEN_BOOL_TYPE ?
                                                (Token){TOKEN_BOOL_TYPE, "bool", 4, 0} :
                                                (Token){TOKEN_EOF, NULL, 0, 0};
                                        }
                                        if (concrete.type != TOKEN_EOF) {
                                            ASTNode* type_arg = create_node(AST_IDENTIFIER, concrete);
                                            add_type_arg(node->left, type_arg);
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    // Resolve return type (handle generic return type via type arg mapping)
                    if (func_decl->return_type.type != TOKEN_EOF) {
                        bool resolved_generic = false;
                        if (func_decl->type_param_count > 0 && node->left->type_arg_count > 0 &&
                            func_decl->return_type.type == TOKEN_IDENTIFIER) {
                            for (int tp = 0; tp < func_decl->type_param_count; tp++) {
                                ASTNode* tp_node = func_decl->type_params[tp];
                                if (tp_node->token.length == func_decl->return_type.length &&
                                    strncmp(tp_node->token.start, func_decl->return_type.start, tp_node->token.length) == 0) {
                                    if (tp < node->left->type_arg_count) {
                                        ASTNode* concrete_arg = node->left->type_args[tp];
                                        free(node->type_name);
                                        node->type_name = sem_copy_token_text(concrete_arg->token);
                                        type = concrete_arg->type_name ?
                                            (strncmp(concrete_arg->type_name, "str", 3) == 0 ? TOKEN_STR_TYPE :
                                             strncmp(concrete_arg->type_name, "int", 3) == 0 ? TOKEN_INT_TYPE :
                                             strncmp(concrete_arg->type_name, "float", 5) == 0 ? TOKEN_FLOAT_TYPE :
                                             strncmp(concrete_arg->type_name, "bool", 4) == 0 ? TOKEN_BOOL_TYPE :
                                             TOKEN_IDENTIFIER) :
                                            concrete_arg->token.type;
                                        resolved_generic = true;
                                    }
                                    break;
                                }
                            }
                        }
                        if (!resolved_generic) {
                            free(node->type_name);
                            node->type_name = type_name_from_token(func_decl->return_type);
                            type = func_decl->return_type.type;
                        }
                    }
                }
            } else if (node->left && node->left->type == AST_SAFE_ACCESS &&
                       node->left->left && node->left->left->type_name) {
                // Safe method call: p?.method(args)
                // Look up the method's return type from the class declaration
                const char* class_name = node->left->left->type_name;
                Token class_token = {TOKEN_IDENTIFIER, class_name, (int)strlen(class_name), node->token.line};
                Symbol* class_sym = resolve_symbol(class_token);
                if (class_sym && class_sym->decl_node) {
                    ASTNode* class_node = class_sym->decl_node;
                    for (int i = 0; i < class_node->child_count; i++) {
                        ASTNode* child = class_node->children[i];
                        if ((child->type == AST_FUNC_DECL || child->type == AST_OPERATOR_DECL) &&
                            child->token.length == node->left->token.length &&
                            strncmp(child->token.start, node->left->token.start, node->left->token.length) == 0) {
                            if (child->return_type.type != TOKEN_EOF) {
                                free(node->type_name);
                                node->type_name = type_name_from_token(child->return_type);
                                type = child->return_type.type;
                            }
                            break;
                        }
                    }
                }
            }
            break;
        }
        case AST_MEMBER_ACCESS:
            analyze_expression(node->left);
            if (node->left && node->left->type_name) {
                // Try to resolve the actual field type from the class/struct declaration
                bool resolved_field = false;
                int cn_len = (int)strlen(node->left->type_name);
                Token class_token = {TOKEN_IDENTIFIER, node->left->type_name, cn_len, node->token.line};
                Symbol* class_sym = resolve_symbol(class_token);
                if (class_sym && class_sym->decl_node) {
                    ASTNode* class_node = class_sym->decl_node;
                    for (int i = 0; i < class_node->child_count; i++) {
                        ASTNode* child = class_node->children[i];
                        if (child->type == AST_VAR_DECL &&
                            child->token.length == node->token.length &&
                            strncmp(child->token.start, node->token.start, node->token.length) == 0) {
                            char* field_type_name = type_name_from_token(child->var_type);
                            if (field_type_name) {
                                free(node->type_name);
                                node->type_name = field_type_name;
                                resolved_field = true;
                            }
                            break;
                        }
                    }
                }
                if (!resolved_field) {
                    free(node->type_name);
                    node->type_name = copy_cstring(node->left->type_name);
                }
                
                if (node->left && node->left->type == AST_IDENTIFIER) {
                    Symbol* value_sym = resolve_symbol(node->left->token);
                    if (value_sym && value_sym->decl_node &&
                        (value_sym->decl_node->is_class || value_sym->decl_node->is_pointer)) {
                        node->is_pointer_access = true;
                    }
                }
            }
            break;
        case AST_INDEX:
            analyze_expression(node->left);
            analyze_expression(node->right);
            break;
        case AST_ASSIGN:
            analyze_assignment(node);
            break;
        case AST_NEW_EXPR: {
            // Validate class name exists
            Symbol* sym = resolve_symbol(node->token);
            if (!sym || !is_named_type_symbol(sym->type)) {
                report_token_error(node->token, "Unknown class in 'new':");
            }
            for (int i = 0; i < node->child_count; i++) {
                analyze_expression(node->children[i]);
            }
            break;
        }
        case AST_SUPER_EXPR:
            // Just validate we're inside a class method (simplified check)
            break;
        case AST_TYPEOF: {
            analyze_expression(node->left);
            type = TOKEN_STR_TYPE;
            break;
        }
        case AST_NULL_LITERAL:
            type = TOKEN_NULL;
            break;
        case AST_CAST: {
            analyze_expression(node->left);
            validate_type_token(node->var_type);
            type = node->var_type.type;
            break;
        }
        case AST_LAMBDA: {
            sem_push_scope();
            for (int i = 0; i < node->child_count; i++) {
                ASTNode* param = node->children[i];
                validate_type_token(param->var_type);
                char* tn = type_name_from_token(param->var_type);
                define_symbol(param->token, param->var_type.type, false, tn, param);
                free(tn);
            }
            if (node->left->type == AST_BLOCK) {
                analyze_node(node->left);
            } else {
                analyze_expression(node->left);
            }
            sem_pop_scope();
            type = TOKEN_FUNC;
            break;
        }
        case AST_NULL_COALESCE: {
            CnextTokenType left_type = analyze_expression(node->left);
            CnextTokenType right_type = analyze_expression(node->right);
            (void)right_type;
            // Result type is the type of the left operand
            type = left_type;
            // Propagate type_name from left operand
            if (node->left && node->left->type_name) {
                free(node->type_name);
                node->type_name = copy_cstring(node->left->type_name);
            }
            break;
        }
        case AST_SAFE_ACCESS: {
            analyze_expression(node->left);
            // Same analysis as AST_MEMBER_ACCESS: resolve field type
            if (node->left && node->left->type_name) {
                bool resolved_field = false;
                int cn_len = (int)strlen(node->left->type_name);
                Token class_token = {TOKEN_IDENTIFIER, node->left->type_name, cn_len, node->token.line};
                Symbol* class_sym = resolve_symbol(class_token);
                if (class_sym && class_sym->decl_node) {
                    ASTNode* class_node = class_sym->decl_node;
                    for (int i = 0; i < class_node->child_count; i++) {
                        ASTNode* child = class_node->children[i];
                        if (child->type == AST_VAR_DECL &&
                            child->token.length == node->token.length &&
                            strncmp(child->token.start, node->token.start, node->token.length) == 0) {
                            char* field_type_name = type_name_from_token(child->var_type);
                            if (field_type_name) {
                                free(node->type_name);
                                node->type_name = field_type_name;
                                resolved_field = true;
                            }
                            break;
                        }
                    }
                }
                if (!resolved_field) {
                    free(node->type_name);
                    node->type_name = copy_cstring(node->left->type_name);
                }
                if (node->left && node->left->type == AST_IDENTIFIER) {
                    Symbol* value_sym = resolve_symbol(node->left->token);
                    if (value_sym && value_sym->decl_node &&
                        (value_sym->decl_node->is_class || value_sym->decl_node->is_pointer)) {
                        node->is_pointer_access = true;
                    }
                }
            }
            break;
        }
        default:
            break;
    }
    node->expr_type = type;
    return type;
}
