/* Reference AST dumper for self-host parity testing (Milestone B).
 *
 * Builds against the hand-written C parser (src/parser*.c, src/ast.c) and
 * prints a deterministic AST serialization; the Cnext port (src2/parser.cn)
 * must produce byte-identical output.
 *
 * Output:
 *   <path>                                (first line, like tools/lexdump.c)
 *   <indent>[<slot> ]KIND@<line>:<col>[ token="..."][ var_type="..."]...
 *
 * indent: two spaces per depth. `slot` is printed only for the non-children
 * slots (type_param, type_arg, left, right, condition, init, increment,
 * default_value); entries of `children` print without a slot label.
 * Attribute order is fixed: token, var_type, return_type, flags, type_name,
 * parent, implements, attribute, named_arg, operator. String values use the
 * same escaping as tools/lexdump.c. A file the parser rejects prints
 * !PARSE_ERROR.
 */
#include "parser.h"
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* kind_names[] = {
    "PROGRAM", "MAIN", "BLOCK", "VAR_DECL", "FUNC_DECL", "MACRO_DECL",
    "CLASS_DECL", "STRUCT_DECL", "ENUM_DECL", "EXPR_STMT", "IF", "WHILE",
    "FOR", "FOR_IN", "SWITCH", "CASE", "DEFAULT", "IMPORT", "RETURN",
    "ASSIGN", "BINARY", "UNARY", "POSTFIX", "CALL", "IDENTIFIER", "LITERAL",
    "LITERAL_ARRAY", "MEMBER_ACCESS", "INDEX", "SLICE", "BREAK", "CONTINUE",
    "TRY", "CATCH", "THROW", "INTERFACE_DECL", "TRAIT_DECL", "CONSTRUCTOR",
    "NEW_EXPR", "SUPER_EXPR", "TYPEOF", "MATCH", "MATCH_ARM", "LAMBDA",
    "CAST", "NULL_LITERAL", "TEST", "ASSERT", "TYPE_PARAM", "TUPLE",
    "TUPLE_ACCESS", "NAMED_ARG", "VARIADIC", "OPERATOR_DECL", "EXTEND_DECL",
    "DESTRUCTURE", "OPTION_SOME", "OPTION_NONE", "RESULT_OK", "RESULT_ERR",
    "NULL_COALESCE", "TERNARY", "SAFE_ACCESS", "YIELD", "COROUTINE_DECL",
    "RESUME_EXPR", "ASYNC_FUNC_DECL", "AWAIT_EXPR", "RUN_ASYNC", "ATTRIBUTE",
    "CONSTEXPR_DECL", "MACRO_INVOCATION", "GUARD", "EXTERN_DECL", "BENCH_DECL",
    "OWN_EXPR", "TYPE_ALIAS", "DEFER", "RANGE", "UNION_TYPE", "STATIC_METHOD",
    "ABSTRACT_METHOD", "VARIANT", "TRY_EXPR", "RANGE_INCLUSIVE", "WHEN",
    "WHEN_ARM"
};

static const int kind_count = (int)(sizeof(kind_names) / sizeof(kind_names[0]));

static void print_escaped(const char* s, int len) {
    putchar('"');
    if (s) {
        for (int i = 0; i < len; i++) {
            unsigned char c = (unsigned char)s[i];
            switch (c) {
                case '\n': fputs("\\n", stdout); break;
                case '\r': fputs("\\r", stdout); break;
                case '\t': fputs("\\t", stdout); break;
                case '\\': fputs("\\\\", stdout); break;
                case '"':  fputs("\\\"", stdout); break;
                default:
                    if (c >= 32 && c <= 126) {
                        putchar(c);
                    } else {
                        printf("\\x%02x", c);
                    }
            }
        }
    }
    putchar('"');
}

static void print_token_attr(const char* name, Token token) {
    printf(" %s=", name);
    print_escaped(token.start, token.length);
}


static void print_attrs(const ASTNode* n) {
    print_token_attr("token", n->token);
    if (n->var_type.type != TOKEN_EOF) print_token_attr("var_type", n->var_type);
    if (n->return_type.type != TOKEN_EOF) print_token_attr("return_type", n->return_type);

    struct { const char* name; bool value; } flags[] = {
        {"const", n->is_const}, {"array", n->is_array}, {"pointer", n->is_pointer},
        {"class", n->is_class}, {"ptraccess", n->is_pointer_access},
        {"union", n->is_union_type}, {"inferred", n->type_is_inferred},
        {"variadic", n->is_variadic}, {"generator", n->is_generator},
        {"tailcall", n->is_tail_call_optimized}, {"static", n->is_static},
        {"abstract", n->is_abstract}, {"final", n->is_final},
    };
    const char* sep = "";
    bool any = false;
    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
        if (flags[i].value) any = true;
    }
    if (any) {
        fputs(" flags=", stdout);
        for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
            if (!flags[i].value) continue;
            fputs(sep, stdout);
            fputs(flags[i].name, stdout);
            sep = ",";
        }
    }

    if (n->type_name) { fputs(" type_name=", stdout); print_escaped(n->type_name, (int)strlen(n->type_name)); }
    if (n->parent_name) { fputs(" parent=", stdout); print_escaped(n->parent_name, (int)strlen(n->parent_name)); }
    if (n->implements_names) { fputs(" implements=", stdout); print_escaped(n->implements_names, (int)strlen(n->implements_names)); }
    if (n->attribute_name) { fputs(" attribute=", stdout); print_escaped(n->attribute_name, (int)strlen(n->attribute_name)); }
    if (n->named_arg_name.type != TOKEN_EOF) print_token_attr("named_arg", n->named_arg_name);
    if (n->operator_token.type != TOKEN_EOF) print_token_attr("operator", n->operator_token);
}

static void print_indent(int depth) {
    for (int i = 0; i < depth; i++) fputs("  ", stdout);
}

static void print_node(const ASTNode* node, int depth, const char* slot) {
    if (!node) return;
    print_indent(depth);
    if (slot) printf("%s ", slot);
    int kind = (int)node->type;
    fputs(kind >= 0 && kind < kind_count ? kind_names[kind] : "UNKNOWN", stdout);
    printf("@%d:%d", node->token.line, node->token.column);
    print_attrs(node);
    putchar('\n');

    for (int i = 0; i < node->child_count; i++) print_node(node->children[i], depth + 1, NULL);
    for (int i = 0; i < node->type_param_count; i++) print_node(node->type_params[i], depth + 1, "type_param");
    for (int i = 0; i < node->type_arg_count; i++) print_node(node->type_args[i], depth + 1, "type_arg");
    print_node(node->left, depth + 1, "left");
    print_node(node->right, depth + 1, "right");
    print_node(node->condition, depth + 1, "condition");
    print_node(node->init, depth + 1, "init");
    print_node(node->increment, depth + 1, "increment");
    print_node(node->default_value, depth + 1, "default_value");
}

static void dump_ast(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        exit(2);
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char* src = (char*)malloc((size_t)size + 1);
    if (!src) {
        fprintf(stderr, "OOM reading %s\n", path);
        exit(2);
    }
    size_t got = fread(src, 1, (size_t)size, f);
    fclose(f);
    src[got] = '\0';

    printf("%s\n", path);
    ASTNode* program = parse_program(src);
    if (!program) {
        fputs("!PARSE_ERROR\n", stdout);
    } else {
        print_node(program, 0, NULL);
        free_ast(program);
    }
    free(src);
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) dump_ast(argv[i]);
    return 0;
}
