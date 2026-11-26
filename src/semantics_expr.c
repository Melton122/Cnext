#include "semantics_internal.h"

static bool sem_is_numeric_type(CnextTokenType type);

static bool is_input_call(ASTNode* node) {
    return node && node->type == AST_CALL && node->left &&
           node->left->type == AST_IDENTIFIER &&
           node->left->token.length == 5 &&
           strncmp(node->left->token.start, "input", 5) == 0;
}

/* The types a typed read can produce. A destination of any other type (a
   class, a generic parameter, an alias) has no reader, so the read stays
   text. */
static bool is_readable_scalar(CnextTokenType type) {
    switch (type) {
        case TOKEN_INT_TYPE:
        case TOKEN_UINT_TYPE:
        case TOKEN_LONG_TYPE:
        case TOKEN_ULONG_TYPE:
        case TOKEN_FLOAT_TYPE:
        case TOKEN_DOUBLE_TYPE:
        case TOKEN_BOOL_TYPE:
        case TOKEN_CHAR_TYPE:
        case TOKEN_BYTE_TYPE:
        case TOKEN_USHORT_TYPE:
        case TOKEN_UBYTE_TYPE:
            return true;
        default:
            return false;
    }
}

/* `input()` is context-sensitive: it reads text unless the context says which
   type is wanted. This applies that context to the call, which is what lets
   codegen pick the matching reader.

   The type has to be applied *before* the call is analyzed, because analyzing
   it is what gives an uncontextualized read its default `str` type. */
static bool apply_read_context(ASTNode* expr, CnextTokenType expected) {
    if (!expr || expected == TOKEN_EOF) return false;

    if (expr->type == AST_LITERAL && expr->expr_type == TOKEN_EOF &&
        sem_is_numeric_type(expected)) {
        bool integer_literal = expr->token.type == TOKEN_NUMBER ||
                               expr->token.type == TOKEN_CHAR_LITERAL;
        bool float_literal = expr->token.type == TOKEN_FLOAT_LITERAL;
        if (integer_literal ||
            (float_literal &&
             (expected == TOKEN_FLOAT_TYPE || expected == TOKEN_DOUBLE_TYPE))) {
            expr->expr_type = expected;
            return true;
        }
    }

    if (is_input_call(expr)) {
        if (expr->expr_type != TOKEN_EOF) return false;
        if (!is_readable_scalar(expected) && expected != TOKEN_STR_TYPE) return false;
        expr->expr_type = expected;
        free(expr->type_name);
        expr->type_name = type_name_from_token((Token){expected, NULL, 0, 0});
        return true;
    }

    /* A signed read: `-input()`. The sign applies to the number that was
       read, so the operand still needs the numeric reader. */
    if (expr->type == AST_UNARY && expr->right) {
        return apply_read_context(expr->right, expected);
    }

    /* A read inside an arithmetic expression: `input() + 1`. */
    if (expr->type == AST_BINARY) {
        bool left_is_read = is_input_call(expr->left);
        bool right_is_read = is_input_call(expr->right);
        if (left_is_read && !right_is_read) {
            return apply_read_context(expr->left, expected);
        }
        if (right_is_read && !left_is_read) {
            return apply_read_context(expr->right, expected);
        }
        return false;
    }

    /* A read in one arm of a conditional: `cond ? input() : 0`. */
    if (expr->type == AST_TERNARY) {
        bool left_is_read = is_input_call(expr->left);
        bool right_is_read = is_input_call(expr->right);
        if (left_is_read && !right_is_read) {
            return apply_read_context(expr->left, expected);
        }
        if (right_is_read && !left_is_read) {
            return apply_read_context(expr->right, expected);
        }
        return false;
    }

    return false;
}

/* The explicitly typed readers: `input_int()` and friends. Each names its
   own type, so it needs no destination context. */
static CnextTokenType explicit_input_type(Token token) {
    if (token.length == 9 && strncmp(token.start, "input_str", 9) == 0) return TOKEN_STR_TYPE;
    if (token.length == 9 && strncmp(token.start, "input_int", 9) == 0) return TOKEN_INT_TYPE;
    if (token.length == 10 && strncmp(token.start, "input_uint", 10) == 0) return TOKEN_UINT_TYPE;
    if (token.length == 10 && strncmp(token.start, "input_long", 10) == 0) return TOKEN_LONG_TYPE;
    if (token.length == 11 && strncmp(token.start, "input_ulong", 11) == 0) return TOKEN_ULONG_TYPE;
    if (token.length == 11 && strncmp(token.start, "input_float", 11) == 0) return TOKEN_FLOAT_TYPE;
    if (token.length == 12 && strncmp(token.start, "input_double", 12) == 0) return TOKEN_DOUBLE_TYPE;
    if (token.length == 10 && strncmp(token.start, "input_bool", 10) == 0) return TOKEN_BOOL_TYPE;
    if (token.length == 10 && strncmp(token.start, "input_char", 10) == 0) return TOKEN_CHAR_TYPE;
    if (token.length == 10 && strncmp(token.start, "input_byte", 10) == 0) return TOKEN_BYTE_TYPE;
    if (token.length == 12 && strncmp(token.start, "input_ushort", 12) == 0) return TOKEN_USHORT_TYPE;
    if (token.length == 11 && strncmp(token.start, "input_ubyte", 11) == 0) return TOKEN_UBYTE_TYPE;
    return TOKEN_EOF;
}

static bool sem_is_numeric_type(CnextTokenType type) {
    switch (type) {
        case TOKEN_INT_TYPE: case TOKEN_LONG_TYPE: case TOKEN_FLOAT_TYPE:
        case TOKEN_DOUBLE_TYPE: case TOKEN_CHAR_TYPE: case TOKEN_BYTE_TYPE:
        case TOKEN_UINT_TYPE: case TOKEN_ULONG_TYPE: case TOKEN_USHORT_TYPE:
        case TOKEN_UBYTE_TYPE:
            return true;
        default:
            return false;
    }
}

static CnextTokenType analyze_when_expression(ASTNode* node) {
    ASTNode* current = NULL;
    bool has_else = false;
    int count = 0;
    for (int i = 0; i < node->child_count; i++) {
        ASTNode* arm = node->children[i];
        if (!arm) continue;
        if (arm->condition) analyze_expression(arm->condition);
        if (!arm->left) continue;
        analyze_expression(arm->left);
        if (arm->is_const) {
            if (has_else || i != node->child_count - 1)
                report_token_error(ERR_SEM_TYPE_MISMATCH, arm->token, "The else arm must be unique and last:");
            has_else = true;
        }
        if (!count) current = arm->left;
        else { sem_join_branch_types(node, current, arm->left); current = node; }
        count++;
    }
    if (!has_else) report_token_error(ERR_SEM_TYPE_MISMATCH, node->token, "When expression requires an else arm:");
    if (!count) return TOKEN_EOF;
    if (count == 1) sem_join_branch_types(node, current, current);
    if (node->expr_type != TOKEN_EOF) {
        for (int i = 0; i < node->child_count; i++) if (node->children[i] && node->children[i]->left)
            sem_coerce_branch(node->children[i]->left, node->var_type, node->is_array);
    }
    return node->expr_type;
}

bool sem_apply_expected_type(ASTNode* expr, CnextTokenType expected) {
    return apply_read_context(expr, expected);
}

// The declaration of the function or method a call refers to, so the
// parameters can type the arguments.
static ASTNode* resolve_callee_decl(ASTNode* call) {
    if (!call || !call->left) return NULL;

    if (call->left->type == AST_IDENTIFIER) {
        Symbol* sym = resolve_symbol(call->left->token);
        if (sym && sym->decl_node &&
            (sym->decl_node->type == AST_FUNC_DECL ||
             sym->decl_node->type == AST_ASYNC_FUNC_DECL ||
             sym->decl_node->type == AST_COROUTINE_DECL)) {
            return sym->decl_node;
        }
        if (sym) return ast_closure_lambda(sym->decl_node);
        return NULL;
    }

    if (call->left->type != AST_MEMBER_ACCESS) return NULL;

    const char* class_name = NULL;
    if (call->left->left && call->left->left->type_name) {
        class_name = call->left->left->type_name;
    }
    if (!class_name) return NULL;
    int cn_len = (int)strlen(class_name);
    Token class_token = {TOKEN_IDENTIFIER, class_name, cn_len, call->token.line, 0};
    Symbol* class_sym = resolve_symbol(class_token);
    if (!class_sym || !class_sym->decl_node) return NULL;
    ASTNode* class_node = class_sym->decl_node;
    for (int i = 0; i < class_node->child_count; i++) {
        ASTNode* child = class_node->children[i];
        if ((child->type == AST_FUNC_DECL || child->type == AST_OPERATOR_DECL) &&
            child->token.length == call->left->token.length &&
            strncmp(child->token.start, call->left->token.start, call->token.length) == 0) {
            return child;
        }
    }
    return NULL;
}

static ASTNode* value_declaration(ASTNode* value) {
    if (value && value->type == AST_IDENTIFIER) {
        Symbol* symbol = resolve_symbol(value->token);
        return symbol ? symbol->decl_node : NULL;
    }
    return resolve_field_decl(value);
}

CnextTokenType analyze_expression(ASTNode* node) {
    if (!node) return TOKEN_EOF;

    CnextTokenType type = TOKEN_EOF;
    switch (node->type) {
        case AST_LITERAL:
            if (node->expr_type != TOKEN_EOF && node->expr_type != TOKEN_VAR &&
                sem_is_numeric_type(node->expr_type)) {
                type = node->expr_type;
            } else if (node->token.type == TOKEN_NUMBER) type = TOKEN_INT_TYPE;
            else if (node->token.type == TOKEN_FLOAT_LITERAL) type = TOKEN_FLOAT_TYPE;
            else if (node->token.type == TOKEN_STRING_LITERAL || node->token.type == TOKEN_RAW_STRING) type = TOKEN_STR_TYPE;
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
                report_token_error(ERR_SEM_UNDECLARED, node->token, "Undeclared identifier:");
            } else {
                node->binding = sym->decl_node;
                if (sym->decl_node && sym->decl_node->is_array) {
                    node->is_array = true;
                    node->var_type = sym->decl_node->var_type;
                }
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
                report_token_error(ERR_SEM_UNDECLARED, node->token, "Undeclared type parameter:");
            } else {
                type = TOKEN_IDENTIFIER;
            }
            break;
        }
        case AST_BINARY: {
            CnextTokenType left_type = analyze_expression(node->left);
            CnextTokenType right_type = analyze_expression(node->right);
            if (sem_expr_is_cstr(node->left) || sem_expr_is_cstr(node->right)) {
                bool equality = node->token.type == TOKEN_EQ_EQ || node->token.type == TOKEN_BANG_EQ;
                bool left_pointer = sem_expr_is_cstr(node->left) || node->left->type == AST_NULL_LITERAL;
                bool right_pointer = sem_expr_is_cstr(node->right) || node->right->type == AST_NULL_LITERAL;
                if (!equality || !left_pointer || !right_pointer) {
                    sem_report_cstr_error(node->token, "CStr supports pointer equality; convert to str for text operations.");
                }
                type = TOKEN_BOOL_TYPE;
                break;
            }
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
                } else if (left_type == TOKEN_DOUBLE_TYPE || right_type == TOKEN_DOUBLE_TYPE ||
                           left_type == TOKEN_FLOAT_TYPE || right_type == TOKEN_FLOAT_TYPE) {
                    // Float/double promotion: prefer double if either side is double
                    type = (left_type == TOKEN_DOUBLE_TYPE || right_type == TOKEN_DOUBLE_TYPE)
                        ? TOKEN_DOUBLE_TYPE : TOKEN_FLOAT_TYPE;
                } else if (left_type == TOKEN_LONG_TYPE || right_type == TOKEN_LONG_TYPE ||
                           left_type == TOKEN_ULONG_TYPE || right_type == TOKEN_ULONG_TYPE) {
                    // Long promotion: prefer unsigned long if either side is ulong
                    type = (left_type == TOKEN_ULONG_TYPE || right_type == TOKEN_ULONG_TYPE)
                        ? TOKEN_ULONG_TYPE : TOKEN_LONG_TYPE;
                } else if (left_type == TOKEN_INT_TYPE || right_type == TOKEN_INT_TYPE ||
                           left_type == TOKEN_UINT_TYPE || right_type == TOKEN_UINT_TYPE) {
                    type = TOKEN_INT_TYPE;
                } else if (left_type == TOKEN_CHAR_TYPE || right_type == TOKEN_CHAR_TYPE ||
                           left_type == TOKEN_BYTE_TYPE || right_type == TOKEN_BYTE_TYPE ||
                           left_type == TOKEN_USHORT_TYPE || right_type == TOKEN_USHORT_TYPE ||
                           left_type == TOKEN_UBYTE_TYPE || right_type == TOKEN_UBYTE_TYPE) {
                    type = TOKEN_INT_TYPE;
                }
            }
            break;
        }
        case AST_BIT_MATCH:
        case AST_BIT_EXTRACT: {
            ASTNode* value = node->left;
            CnextTokenType operand = analyze_expression(value);
            ASTNode* decl = value_declaration(value);
            if (value->type == AST_INDEX) {
                ASTNode* base = value_declaration(value->left);
                if (base && base->is_array) operand = base->var_type.type;
            }
            if (operand == TOKEN_FLOAT_TYPE || operand == TOKEN_DOUBLE_TYPE ||
                operand == TOKEN_STR_TYPE || operand == TOKEN_NULL || operand == TOKEN_FUNC ||
                value->is_class || value->type == AST_LITERAL_ARRAY || value->type == AST_SLICE ||
                (decl && decl->is_array)) {
                report_token_error(ERR_SEM_TYPE_MISMATCH, value->token,
                                   "Bit patterns require an integer operand:");
            }
            type = node->type == AST_BIT_MATCH ? TOKEN_BOOL_TYPE : TOKEN_ULONG_TYPE;
            break;
        }
        case AST_UNARY:
            type = analyze_expression(node->right);
            if (node->token.type == TOKEN_BANG) type = TOKEN_BOOL_TYPE;
            break;
        case AST_RANGE:
        case AST_RANGE_INCLUSIVE:
            analyze_expression(node->left);
            analyze_expression(node->right);
            type = TOKEN_INT_TYPE;
            break;
        case AST_POSTFIX:
            analyze_postfix(node);
            break;
        case AST_CALL: {
            CnextTokenType callee_type = analyze_expression(node->left);
            if (node->left && node->left->type == AST_IDENTIFIER && callee_type != TOKEN_FUNC) {
                report_token_error(ERR_SEM_CANT_CALL, node->left->token, "Cannot call non-function:");
            }
            // The parameters type the arguments: `takes_int(input())` reads an
            // integer, because the parameter says an integer is wanted. The
            // type has to be applied before the argument is analyzed, since
            // analyzing an argument is what gives an uncontextualized read
            // its default text type.
            ASTNode* callee_decl = resolve_callee_decl(node);
            for (int i = 0; i < node->child_count; i++) {
                ASTNode* arg = node->children[i];
                ASTNode* param = NULL;
                int offset = callee_decl && callee_decl->child_count > 0 &&
                    node->left && (node->left->type == AST_MEMBER_ACCESS || node->left->type == AST_SAFE_ACCESS) &&
                    token_matches_name(callee_decl->children[0]->token, "self") ? 1 : 0;
                if (callee_decl && arg->type == AST_NAMED_ARG) {
                    for (int p = offset; p < callee_decl->child_count; p++) {
                        Token name = callee_decl->children[p]->token;
                        if (name.length == arg->token.length && !strncmp(name.start, arg->token.start, name.length))
                            param = callee_decl->children[p];
                    }
                    arg = arg->right;
                } else if (callee_decl && i + offset < callee_decl->child_count) {
                    param = callee_decl->children[i + offset];
                }
                if (param) sem_apply_expected_type(arg, param->var_type.type);
                analyze_expression(arg);
                if (param) {
                    sem_check_cstr_conversion(param->var_type, arg);
                    sem_check_value_assignment(param->var_type, param->is_array, arg);
                }
            }
            if (callee_decl && callee_decl->type == AST_LAMBDA) {
                type = callee_decl->return_type.type;
            }
            if (callee_decl) node->closure_signature = callee_decl->closure_signature;
            if (node->left && node->left->type == AST_IDENTIFIER &&
                (token_matches_name(node->left->token, "to_cstr") ||
                 token_matches_name(node->left->token, "from_cstr") ||
                 token_matches_name(node->left->token, "cstr_free"))) {
                bool to_cstr = token_matches_name(node->left->token, "to_cstr");
                bool from_cstr = token_matches_name(node->left->token, "from_cstr");
                if (node->child_count != 1) {
                    sem_report_cstr_error(node->left->token,
                                       "C string conversion functions require exactly one argument.");
                } else {
                    ASTNode* arg = node->children[0];
                    ASTNode* arg_decl = value_declaration(arg);
                    bool array = arg->is_array || arg->type == AST_LITERAL_ARRAY ||
                        (arg_decl && arg_decl->is_array);
                    bool valid = to_cstr ? arg->expr_type == TOKEN_STR_TYPE :
                        (sem_expr_is_cstr(arg) || arg->type == AST_NULL_LITERAL);
                    if (!valid || array) {
                        sem_report_cstr_error(arg->token,
                            to_cstr ? "to_cstr expects str." : "from_cstr/cstr_free expects CStr.");
                    }
                }
                free(node->type_name);
                node->type_name = to_cstr ? copy_cstring("CStr") :
                    (from_cstr ? copy_cstring("str") : NULL);
                type = to_cstr ? TOKEN_IDENTIFIER : (from_cstr ? TOKEN_STR_TYPE : TOKEN_EOF);
                break;
            }
            if (!callee_decl && node->left && node->left->type == AST_IDENTIFIER &&
                (token_matches_name(node->left->token, "md5") ||
                 token_matches_name(node->left->token, "sha1") ||
                 token_matches_name(node->left->token, "sha256") ||
                 token_matches_name(node->left->token, "hash_md5") ||
                 token_matches_name(node->left->token, "hash_sha1") ||
                 token_matches_name(node->left->token, "hash_sha256"))) {
                type = TOKEN_STR_TYPE;
            } else if (is_input_call(node)) {
                // A declaration or assignment may seed expr_type with its
                // destination type. Without that context input() is text,
                // which keeps `var value = input()` intuitive.
                if (node->expr_type == TOKEN_EOF) {
                    node->expr_type = TOKEN_STR_TYPE;
                    free(node->type_name);
                    node->type_name = type_name_from_token((Token){
                        TOKEN_STR_TYPE, "str", 3, node->token.line});
                }
                type = node->expr_type;
            } else if (node->left && node->left->type == AST_IDENTIFIER &&
                       explicit_input_type(node->left->token) != TOKEN_EOF) {
                // `input_int()` and friends name their own type, so the call
                // has that type wherever it stands.
                CnextTokenType explicit_type = explicit_input_type(node->left->token);
                node->expr_type = explicit_type;
                free(node->type_name);
                node->type_name = type_name_from_token((Token){explicit_type, NULL, 0, 0});
                type = explicit_type;
            }
            // Propagate str type to null literals passed as function arguments
            if (node->left && node->left->type == AST_IDENTIFIER) {
                Symbol* func_sym = resolve_symbol(node->left->token);
                if (func_sym && func_sym->decl_node &&
                    (func_sym->decl_node->type == AST_FUNC_DECL ||
                     func_sym->decl_node->type == AST_ASYNC_FUNC_DECL ||
                     func_sym->decl_node->type == AST_COROUTINE_DECL)) {
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
            } else if (node->left && node->left->type == AST_MEMBER_ACCESS &&
                       node->left->left && node->left->left->type_name) {
                // Method call on an object: obj.method(args). The declared return
                // type has to be resolved from the receiver's class, otherwise the
                // call node keeps no expression type and `var x = obj.method()`
                // has nothing to infer from. (The identifier and safe-access
                // cases above already do this.)
                const char* class_name = node->left->left->type_name;
                Token class_token = {TOKEN_IDENTIFIER, class_name, (int)strlen(class_name), node->token.line};
                Symbol* class_sym = resolve_symbol(class_token);
                // A generic receiver's methods are declared in terms of the class
                // type parameters (`func get(): T`), which are only meaningful
                // once specialized. Recording `T` here would make later
                // resolution treat the type parameter as a class, so leave these
                // alone exactly as the other branches do.
                if (class_sym && class_sym->decl_node && class_sym->decl_node->type_param_count == 0) {
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
            if (callee_decl && callee_decl->return_is_array) {
                node->is_array = true;
                node->var_type = callee_decl->return_type;
                type = callee_decl->return_type.type;
            }
            if (!callee_decl && node->left && node->left->type == AST_IDENTIFIER &&
                token_matches_name(node->left->token, "str_split")) {
                node->is_array = true;
                node->var_type = (Token){TOKEN_STR_TYPE, "str", 3, node->token.line};
                type = TOKEN_STR_TYPE;
            }
            break;
        }
        case AST_MEMBER_ACCESS:
            analyze_expression(node->left);
            if (node->left && node->left->type_name) {
                // Try to resolve the actual field type from the class/struct declaration
                bool resolved_field = false;
                ASTNode* found_field = NULL;
                int cn_len = (int)strlen(node->left->type_name);
                Token class_token = {TOKEN_IDENTIFIER, node->left->type_name, cn_len, node->token.line, 0};
                Symbol* class_sym = resolve_symbol(class_token);
                if (class_sym && class_sym->decl_node) {
                    ASTNode* class_node = class_sym->decl_node;
                    for (int i = 0; i < class_node->child_count; i++) {
                        ASTNode* child = class_node->children[i];
                        if (child->type == AST_VAR_DECL &&
                            child->token.length == node->token.length &&
                            strncmp(child->token.start, node->token.start, node->token.length) == 0) {
                            found_field = child;
                            char* field_type_name = type_name_from_token(child->var_type);
                            if (field_type_name) {
                                free(node->type_name);
                                node->type_name = field_type_name;
                                resolved_field = true;
                            }
                            // Record the field's type as the type of the read
                            // expression. Codegen picks `==` vs `cnext_str_eq`
                            // and `+` vs `cnext_concat` from the operand types,
                            // so a `str` field had to fall through to raw C `==`
                            // on two CnextStrings. Array fields are left alone:
                            // their element type is what appears in type_name,
                            // and the element is not what the field evaluates to.
                            if (!child->is_array) type = child->var_type.type;
                            else { node->is_array = true; node->var_type = child->var_type; }
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

                // A field whose declared type is a class is itself stored as a
                // pointer, so reading it yields a reference. Mark the access so
                // codegen dereferences it with '->' instead of '.'.
                //
                // This only applies to a real, non-array field of a class. When
                // the lookup above failed the type name is merely inherited
                // from the base, and for an array that name is the *element*
                // type, which would wrongly turn `a.length` into `a->length`.
                if (found_field && !found_field->is_array && node->type_name && node->type_name[0] != '\0') {
                    int tlen = (int)strlen(node->type_name);
                    Token field_type_token = {TOKEN_IDENTIFIER, node->type_name, tlen, node->token.line, 0};
                    Symbol* field_type_sym = resolve_symbol(field_type_token);
                    if (field_type_sym && field_type_sym->type == TOKEN_CLASS &&
                        field_type_sym->decl_node &&
                        field_type_sym->decl_node->type != AST_TYPE_ALIAS) {
                        node->is_pointer_access = true;
                        node->is_class = true;
                    }
                }
            }
            break;
        case AST_SLICE: {
            CnextTokenType base = analyze_expression(node->left);
            analyze_expression(node->right);
            analyze_expression(node->condition);
            ASTNode* decl = value_declaration(node->left);
            bool array = node->left->is_array || (decl && decl->is_array);
            if (base == TOKEN_STR_TYPE && !array) {
                type = TOKEN_STR_TYPE;
            }
            break;
        }
        case AST_INDEX: {
            analyze_expression(node->left);
            analyze_expression(node->right);
            // Indexing an array of class instances yields a reference, so the
            // access has to be dereferenced with '->' and the element fetched
            // rather than its address taken.
            ASTNode* base_decl = value_declaration(node->left);
            if (!base_decl && node->left && node->left->is_array) base_decl = node->left;
            if (base_decl && base_decl->is_array) {
                type = base_decl->var_type.type;
                node->var_type = base_decl->var_type;
            }
            if (base_decl && base_decl->is_array && base_decl->var_type.type == TOKEN_IDENTIFIER) {
                char* elem = type_name_from_token(base_decl->var_type);
                if (elem) {
                    int elen = (int)strlen(elem);
                    Token elem_token = {TOKEN_IDENTIFIER, elem, elen, node->token.line, 0};
                    Symbol* elem_sym = resolve_symbol(elem_token);
                    if (elem_sym && elem_sym->type == TOKEN_CLASS &&
                        elem_sym->decl_node && elem_sym->decl_node->type != AST_TYPE_ALIAS) {
                        free(node->type_name);
                        node->type_name = elem;
                        node->is_class = true;
                        node->is_pointer_access = true;
                        // Record the element type as the type of the expression.
                        // `var x = arr[i]` reads expr_type to infer x's type; with
                        // it still unset the declaration stayed untyped and fell
                        // back to __auto_type, which C resolves to `Num*` from
                        // the cast. The declaration was then never marked as a
                        // class, so a later `x.field` emitted `x.field` on a
                        // pointer.
                        type = TOKEN_IDENTIFIER;
                    } else {
                        free(elem);
                    }
                }
            }
            break;
        }
        case AST_ASSIGN:
            analyze_assignment(node);
            break;
        case AST_OWN_EXPR:
            type = analyze_expression(node->left);
            if (node->left) {
                node->is_array = sem_value_is_array(node->left);
                node->var_type = node->left->var_type;
                node->is_class = node->left->is_class;
                free(node->type_name);
                node->type_name = copy_cstring(node->left->type_name);
            }
            break;
        case AST_NEW_EXPR: {
            free(node->type_name);
            node->type_name = sem_copy_token_text(node->token);
            // Validate class name exists
            Symbol* sym = resolve_symbol(node->token);
            if (!sym || !is_named_type_symbol(sym->type)) {
                report_token_error(ERR_SEM_UNKNOWN_CLASS, node->token, "Unknown class in 'new':");
            }
            // Cannot instantiate abstract classes
            if (sym && sym->decl_node && sym->decl_node->is_abstract) {
                report_token_error(ERR_SEM_ABSTRACT_INST, node->token, "Cannot instantiate abstract class:");
            }
            ASTNode* constructor = NULL;
            if (sym && sym->decl_node) {
                for (int i = 0; i < sym->decl_node->child_count; i++) {
                    ASTNode* member = sym->decl_node->children[i];
                    if (member->type == AST_CONSTRUCTOR) constructor = member;
                }
            }
            for (int i = 0; i < node->child_count; i++) {
                ASTNode* param = constructor && i < constructor->child_count ? constructor->children[i] : NULL;
                if (param) sem_apply_expected_type(node->children[i], param->var_type.type);
                analyze_expression(node->children[i]);
                if (param) {
                    sem_check_cstr_conversion(param->var_type, node->children[i]);
                    sem_check_value_assignment(param->var_type, param->is_array, node->children[i]);
                }
            }
            type = TOKEN_IDENTIFIER;
            break;
        }
        case AST_SUPER_EXPR:
            // super.expr — validated structurally by parser (must be inside class with extends)
            // Full validation would require tracking current class context
            break;
        case AST_OPTION_NONE:
            type = TOKEN_EOF;
            break;
        case AST_OPTION_SOME:
        case AST_RESULT_OK:
        case AST_RESULT_ERR:
            analyze_expression(node->left);
            type = TOKEN_EOF;
            break;
        case AST_TRY_EXPR:
            type = analyze_expression(node->left);
            break;
        case AST_WHEN:
            type = analyze_when_expression(node);
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
            sem_check_cstr_conversion(node->var_type, node->left);
            type = node->var_type.type;
            if (sem_is_cstr_type(node->var_type)) {
                free(node->type_name);
                node->type_name = type_name_from_token(node->var_type);
            }
            break;
        }
        case AST_LAMBDA: {
            node->lambda_depth = ++sem_lambda_depth;
            sem_push_scope();
            for (int i = 0; i < node->child_count; i++) {
                ASTNode* param = node->children[i];
                param->lambda_depth = sem_lambda_depth;
                validate_type_token(param->var_type);
                char* tn = type_name_from_token(param->var_type);
                define_symbol(param->token, param->var_type.type, false, tn, param);
                free(tn);
            }
            if (node->left->type == AST_BLOCK) {
                // A lambda body has no declared return type, so a read inside
                // it is not typed by the enclosing function.
                CnextTokenType saved_return_type = sem_current_return_type;
                Token saved_return_token = sem_current_return_token;
                sem_current_return_type = TOKEN_EOF;
                sem_current_return_token = (Token){0};
                analyze_node(node->left);
                sem_current_return_type = saved_return_type;
                sem_current_return_token = saved_return_token;
            } else {
                analyze_expression(node->left);
            }
            sem_promote_capture_cells(node->left, node, false);
            ASTNode* result = node->left->type == AST_BLOCK ? ast_return_value(node->left) : node->left;
            if (result) node->return_type.type = result->expr_type;
            node->closure_signature = result && result->expr_type == TOKEN_FUNC ? ast_closure_lambda(result) : NULL;
            sem_pop_scope();
            sem_lambda_depth--;
            type = TOKEN_FUNC;
            break;
        }
        case AST_NULL_COALESCE: {
            CnextTokenType left_type = analyze_expression(node->left);
            CnextTokenType right_type = analyze_expression(node->right);
            if (sem_expr_is_cstr(node->left) != sem_expr_is_cstr(node->right) &&
                node->right->type != AST_NULL_LITERAL) {
                sem_report_cstr_error(node->token, "CStr fallback requires another CStr.");
            }
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
        case AST_TERNARY: {
            analyze_expression(node->condition);
            analyze_expression(node->left);
            analyze_expression(node->right);
            type = sem_join_branch_types(node, node->left, node->right);
            break;
        }
        case AST_SAFE_ACCESS: {
            analyze_expression(node->left);
            // Same analysis as AST_MEMBER_ACCESS: resolve field type
            if (node->left && node->left->type_name) {
                bool resolved_field = false;
                ASTNode* found_field = NULL;
                int cn_len = (int)strlen(node->left->type_name);
                Token class_token = {TOKEN_IDENTIFIER, node->left->type_name, cn_len, node->token.line, 0};
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
                                found_field = child;
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

                // A field whose declared type is a class is stored as a
                // pointer, so reading it yields a reference. Mark the access
                // so codegen dereferences it with '->' instead of '.', which
                // is what lets `a?.b?.c` chains emit valid C: each hop that
                // yields a class instance is followed by '->'.
                if (resolved_field && found_field && !found_field->is_array &&
                    node->type_name && node->type_name[0] != '\0') {
                    int tlen = (int)strlen(node->type_name);
                    Token field_type_token = {TOKEN_IDENTIFIER, node->type_name, tlen, node->token.line, 0};
                    Symbol* field_type_sym = resolve_symbol(field_type_token);
                    if (field_type_sym && field_type_sym->type == TOKEN_CLASS &&
                        field_type_sym->decl_node &&
                        field_type_sym->decl_node->type != AST_TYPE_ALIAS) {
                        node->is_pointer_access = true;
                        node->is_class = true;
                    }
                }
            }
            break;
        }
        case AST_AWAIT_EXPR: {
            // await expr — type is the return type of the awaited call.
            CnextTokenType awaited = node->left ? analyze_expression(node->left) : TOKEN_EOF;
            if (awaited == TOKEN_EOF || awaited == TOKEN_VAR) awaited = TOKEN_INT_TYPE;
            if (awaited == TOKEN_IDENTIFIER) {
                free(node->type_name);
                node->type_name = node->left && node->left->type_name
                    ? copy_cstring(node->left->type_name) : NULL;
            }
            type = awaited;
            node->expr_type = type;
            break;
        }
        default:
            break;
    }
    node->expr_type = type;
    return type;
}
