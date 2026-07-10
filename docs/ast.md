# Cnext AST Structure

The Parser translates tokens into an Abstract Syntax Tree (AST). Below is the conceptual structure.

## Base Node
Every node in the AST is an `ASTNode` with a specific type.

```c
typedef enum {
    AST_PROGRAM,
    AST_MAIN,
    AST_FUNC_DECL,
    AST_CLASS_DECL,
    AST_VAR_DECL,
    AST_BLOCK,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_RETURN,
    AST_ASSIGN,
    AST_BINOP,
    AST_UNOP,
    AST_CALL,
    AST_IDENTIFIER,
    AST_LITERAL_INT,
    AST_LITERAL_FLOAT,
    AST_LITERAL_STR,
    AST_LITERAL_BOOL
} ASTNodeType;
```

## Node Details

### Program
- `declarations`: List of top-level declarations (functions, classes, main, global vars).

### Main Block
- `body`: An AST_BLOCK node.

### Function Declaration
- `name`: Identifier.
- `params`: List of `AST_VAR_DECL` (acting as parameters).
- `returnType`: String representing type (or enum).
- `body`: An AST_BLOCK node.

### Class Declaration
- `name`: Identifier.
- `members`: List of `AST_VAR_DECL` or methods.

### Variable Declaration
- `name`: Identifier.
- `varType`: String or enum.
- `initializer`: Optional `ASTNode` (expression).

### Statements
- **Block**: Contains a list of `ASTNode` statements.
- **If**: Contains `condition` (expr), `trueBranch` (block), optional `falseBranch` (block).
- **While**: Contains `condition` (expr), `body` (block).
- **For**: Contains `init` (var_decl or assign), `condition` (expr), `increment` (assign), `body` (block).
- **Return**: Contains optional `expr`.
- **Assignment**: Contains `left` (identifier/field), `right` (expr), `operator` (e.g., `=`, `+=`).

### Expressions
- **Binary Operation**: `left` (expr), `operator`, `right` (expr).
- **Unary Operation**: `operator`, `operand` (expr).
- **Function Call**: `name` (identifier), `args` (list of exprs).
- **Literals**: Hold their specific parsed value (int, string, bool, etc.).
- **Identifiers**: Name of the variable.
