#include "semantics_internal.h"

void predeclare_global(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case AST_IMPORT:
            register_import(node->token);
            break;
        case AST_FUNC_DECL:
            define_symbol(node->token, TOKEN_FUNC, true, NULL, node);
            break;
        case AST_STRUCT_DECL:
            define_type_symbol(node, TOKEN_STRUCT);
            break;
        case AST_CLASS_DECL: {
            define_type_symbol(node, TOKEN_CLASS);
            // Define generic type parameters as opaque types in scope
            for (int i = 0; i < node->type_param_count; i++) {
                ASTNode* tp = node->type_params[i];
                Symbol* sym = (Symbol*)checked_malloc(sizeof(Symbol));
                sym->name = strndup(tp->token.start, tp->token.length);
                sym->type = TOKEN_IDENTIFIER;
                sym->is_const = true;
                sym->type_name = strndup(tp->token.start, tp->token.length);
                sym->decl_node = tp;
                sym->next = sem_current_scope->symbols;
                sem_current_scope->symbols = sym;
            }
            // Inherit from parent class (extends)
            if (node->parent_name) {
                Token parent_tok = {TOKEN_IDENTIFIER, node->parent_name, (int)strlen(node->parent_name), node->token.line};
                Symbol* parent_sym = resolve_symbol(parent_tok);
                if (parent_sym && parent_sym->decl_node) {
                    ASTNode* parent_node = parent_sym->decl_node;
                    int to_add_count = 0;
                    for (int i = 0; i < parent_node->child_count; i++) {
                        ASTNode* pchild = parent_node->children[i];
                        if (pchild->type == AST_VAR_DECL) {
                            to_add_count++;
                        } else if (pchild->type == AST_FUNC_DECL) {
                            bool overridden = false;
                            for (int j = 0; j < node->child_count; j++) {
                                if (node->children[j]->type == AST_FUNC_DECL &&
                                    node->children[j]->token.length == pchild->token.length &&
                                    strncmp(node->children[j]->token.start, pchild->token.start, pchild->token.length) == 0) {
                                    overridden = true;
                                    break;
                                }
                            }
                            if (!overridden) to_add_count++;
                        }
                    }
                    if (to_add_count > 0) {
                        int old_count = node->child_count;
                        int new_count = old_count + to_add_count;
                        ASTNode** new_children = (ASTNode**)malloc(sizeof(ASTNode*) * new_count);
                        int idx = 0;
                        for (int i = 0; i < parent_node->child_count; i++) {
                            if (parent_node->children[i]->type == AST_VAR_DECL) new_children[idx++] = parent_node->children[i];
                        }
                        for (int i = 0; i < old_count; i++) {
                            if (node->children[i]->type == AST_VAR_DECL) new_children[idx++] = node->children[i];
                        }
                        for (int i = 0; i < parent_node->child_count; i++) {
                            if (parent_node->children[i]->type == AST_FUNC_DECL) {
                                bool overridden = false;
                                for (int j = 0; j < old_count; j++) {
                                    if (node->children[j]->type == AST_FUNC_DECL &&
                                        node->children[j]->token.length == parent_node->children[i]->token.length &&
                                        strncmp(node->children[j]->token.start, parent_node->children[i]->token.start, parent_node->children[i]->token.length) == 0) {
                                        overridden = true;
                                        break;
                                    }
                                }
                                if (!overridden) new_children[idx++] = parent_node->children[i];
                            }
                        }
                        for (int i = 0; i < old_count; i++) {
                            if (node->children[i]->type != AST_VAR_DECL) new_children[idx++] = node->children[i];
                        }
                        free(node->children);
                        node->children = new_children;
                        node->child_count = new_count;
                        node->child_capacity = new_count;
                    }
                }
            }
            // Inherit default methods from implemented traits
            if (node->implements_names) {
                char names_copy[1024];
                strncpy(names_copy, node->implements_names, sizeof(names_copy) - 1);
                names_copy[sizeof(names_copy) - 1] = '\0';
                char* name = names_copy;
                while (name && *name) {
                    char* comma = strchr(name, ',');
                    if (comma) *comma = '\0';
                    while (*name == ' ' || *name == '\t') name++;
                    if (*name) {
                        Token trait_tok = {TOKEN_IDENTIFIER, name, (int)strlen(name), node->token.line};
                        Symbol* trait_sym = resolve_symbol(trait_tok);
                        if (trait_sym && trait_sym->decl_node && trait_sym->decl_node->type == AST_TRAIT_DECL) {
                            ASTNode* trait_node = trait_sym->decl_node;
                            int to_add = 0;
                            for (int i = 0; i < trait_node->child_count; i++) {
                                ASTNode* tchild = trait_node->children[i];
                                if (tchild->type == AST_FUNC_DECL && tchild->left) {
                                    bool overridden = false;
                                    for (int j = 0; j < node->child_count; j++) {
                                        if (node->children[j]->type == AST_FUNC_DECL &&
                                            node->children[j]->token.length == tchild->token.length &&
                                            strncmp(node->children[j]->token.start, tchild->token.start, tchild->token.length) == 0) {
                                            overridden = true;
                                            break;
                                        }
                                    }
                                    if (!overridden) to_add++;
                                }
                            }
                            if (to_add > 0) {
                                int old_count = node->child_count;
                                int new_count = old_count + to_add;
                                ASTNode** new_children = (ASTNode**)malloc(sizeof(ASTNode*) * new_count);
                                int idx = 0;
                                for (int i = 0; i < old_count; i++) {
                                    new_children[idx++] = node->children[i];
                                }
                                for (int i = 0; i < trait_node->child_count; i++) {
                                    ASTNode* tchild = trait_node->children[i];
                                    if (tchild->type == AST_FUNC_DECL && tchild->left) {
                                        bool overridden = false;
                                        for (int j = 0; j < old_count; j++) {
                                            if (node->children[j]->type == AST_FUNC_DECL &&
                                                node->children[j]->token.length == tchild->token.length &&
                                                strncmp(node->children[j]->token.start, tchild->token.start, tchild->token.length) == 0) {
                                                overridden = true;
                                                break;
                                            }
                                        }
                                        if (!overridden) {
                                            new_children[idx++] = tchild;
                                        }
                                    }
                                }
                                free(node->children);
                                node->children = new_children;
                                node->child_count = new_count;
                                node->child_capacity = new_count;
                            }
                        }
                    }
                    if (comma) name = comma + 1;
                    else break;
                }
            }
            break;
        }
        case AST_INTERFACE_DECL:
        case AST_TRAIT_DECL:
            define_type_symbol(node, TOKEN_CLASS);
            break;
        case AST_ENUM_DECL:
            define_type_symbol(node, TOKEN_ENUM);
            for (int i = 0; i < node->child_count; i++) {
                define_symbol(node->children[i]->token, TOKEN_INT_TYPE, true, NULL, node->children[i]);
            }
            break;
        default:
            break;
    }
}
