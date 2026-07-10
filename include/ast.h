#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include "lexer.h"

typedef enum {
    AST_PROGRAM,
    AST_MAIN,
    AST_BLOCK,
    AST_VAR_DECL,
    AST_FUNC_DECL,
    AST_CLASS_DECL,
    AST_STRUCT_DECL,
    AST_ENUM_DECL,
    AST_EXPR_STMT,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_FOR_IN,
    AST_SWITCH,
    AST_CASE,
    AST_DEFAULT,
    AST_IMPORT,
    AST_RETURN,
    AST_ASSIGN,
    AST_BINARY,
    AST_UNARY,
    AST_POSTFIX,
    AST_CALL,
    AST_IDENTIFIER,
    AST_LITERAL,
    AST_LITERAL_ARRAY,
    AST_MEMBER_ACCESS,
    AST_INDEX,
    AST_SLICE,
    AST_BREAK,
    AST_CONTINUE,
    // Error handling
    AST_TRY,
    AST_CATCH,
    AST_THROW,
    // OOP
    AST_INTERFACE_DECL,
    AST_TRAIT_DECL,
    AST_CONSTRUCTOR,
    AST_NEW_EXPR,
    AST_SUPER_EXPR,
    // Pattern matching
    AST_MATCH,
    AST_MATCH_ARM,
    // Lambda
    AST_LAMBDA,
    // Cast and Null
    AST_CAST,
    AST_NULL_LITERAL,
    // Testing
    AST_TEST,
    AST_ASSERT,
    // Generics
    AST_TYPE_PARAM,
    // v3.0: Tuples
    AST_TUPLE,
    AST_TUPLE_ACCESS,
    // v3.0: Default/Named Arguments
    AST_NAMED_ARG,
    // v3.0: Variadic
    AST_VARIADIC,
    // v3.0: Operator Overloading
    AST_OPERATOR_DECL,
    // v3.0: Extension Methods
    AST_EXTEND_DECL,
    // v3.0: Destructuring
    AST_DESTRUCTURE,
    // v3.0: Optional/Result
    AST_OPTION_SOME,
    AST_OPTION_NONE,
    AST_RESULT_OK,
    AST_RESULT_ERR
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    Token token; // For identifying specific info, e.g., variable name
    bool is_const;
    bool is_array;
    bool is_pointer;
    bool is_class;
    bool is_pointer_access;
    char* type_name; // For resolved type (e.g., class name for member access)
    char* parent_name; // For class inheritance (parent class name)
    char* implements_names; // Comma-separated list of implemented interface/trait names
    
    // For lists (e.g. block statements, function args, program declarations)
    struct ASTNode** children;
    int child_count;
    int child_capacity;
    
    // Generics: type parameters (for generic declarations: func foo<T, U>)
    struct ASTNode** type_params;
    int type_param_count;
    int type_param_capacity;
    
    // Generics: type arguments (for instantiations: foo<int, str>)
    struct ASTNode** type_args;
    int type_arg_count;
    int type_arg_capacity;
    
    // Additional info based on node type
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode* condition;
    struct ASTNode* init;
    struct ASTNode* increment;
    
    // Types
    Token var_type; // the token representing the type
    Token return_type;
    TokenType expr_type;
    
    // v3.0: Default argument value
    struct ASTNode* default_value;
    // v3.0: Named argument name
    Token named_arg_name;
    // v3.0: Variadic flag
    bool is_variadic;
    // v3.0: Operator token (for operator overloading)
    Token operator_token;
} ASTNode;

ASTNode* create_node(ASTNodeType type, Token token);
void add_child(ASTNode* parent, ASTNode* child);
void add_type_param(ASTNode* parent, ASTNode* param);
void add_type_arg(ASTNode* parent, ASTNode* arg);
void free_ast(ASTNode* node);

#endif // AST_H
