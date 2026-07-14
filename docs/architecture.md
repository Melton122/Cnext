# Cnext Compiler Architecture

The Cnext compiler is a transcompiler (transpiler) that takes `.cn` source code and generates equivalent `.c` code, which is then passed to a C compiler (GCC/Clang) to produce the final executable.

## Pipeline Overview

```
Source (.cn) -> Lexer -> Tokens -> Parser -> AST -> Semantic Analyzer -> Optimizer -> Code Generator -> C Source (.c) -> GCC/Clang -> Executable
```

### 1. Lexer (Scanner)
- **Input:** Cnext source code as a stream of characters.
- **Output:** A stream of Tokens.
- **Role:** Reads characters sequentially, discarding whitespace and comments. Groups characters into meaningful lexical units (Tokens) like `KEYWORD_IF`, `IDENTIFIER`, `LITERAL_INT`, `OP_PLUS`.

### 2. Parser
- **Input:** Stream of Tokens.
- **Output:** Abstract Syntax Tree (AST).
- **Role:** Analyzes the token stream against the EBNF grammar to ensure syntactic correctness. It constructs a tree where nodes represent language constructs (blocks, loops, function calls). Supports optional semicolons, pattern matching, generics, and macros.

### 3. Semantic Analyzer
- **Input:** AST.
- **Output:** Annotated AST with type information.
- **Role:** Performs full type checking, scope resolution, and safety checks. Validates variable declarations, function signatures, type compatibility, trait implementations, and catches semantic errors before code generation.

### 4. Optimizer
- **Input:** AST.
- **Output:** Optimized AST.
- **Role:** Performs compile-time optimizations including:
  - Constant folding (evaluating expressions like `2 + 3` at compile time)
  - Dead code elimination (removing unreachable code after return/break/throw)
  - Dead branch elimination (`if (true)` / `while (false)`)

### 5. Code Generator
- **Input:** Optimized AST.
- **Output:** C Source Code string.
- **Role:** Traverses the AST recursively and generates equivalent C code.
  - Maps `main {}` to `int main(void) {}`.
  - Maps `printin()` to `printf()`.
  - Converts class declarations to `struct` definitions with vtables.
  - Generates closures as struct+function pointer pairs.
  - Generates iterator protocol for generators.
  - Handles async/await via coroutine emulation.
  - Produces source maps for debugging.

### 6. Backend (C Compiler)
- **Input:** Generated C code.
- **Output:** Executable binary.
- **Role:** The CLI tool (`cnext`) automatically invokes `gcc` or `clang` behind the scenes to compile the generated `.c` file into a native executable for the current platform (Windows/Linux/macOS).

## Project Structure

```
cnext/
├── src/                    # Compiler source code
│   ├── main.c              # CLI entry point
│   ├── main_utils.c        # File utilities
│   ├── main_packages.c     # Package manager
│   ├── main_compiler.c     # Compilation driver
│   ├── lexer.c             # Lexer/tokenizer
│   ├── parser.c            # Parser entry
│   ├── parser_core.c       # Parser core (token management)
│   ├── parser_expr.c       # Expression parsing
│   ├── parser_stmt.c       # Statement parsing
│   ├── parser_decl.c       # Declaration parsing
│   ├── parser_type.c       # Type parsing
│   ├── parser_macro.c      # Macro expansion
│   ├── ast.c               # AST node management
│   ├── semantics.c         # Semantic analysis entry
│   ├── semantics_scope.c   # Scope/symbol table
│   ├── semantics_types.c   # Type system
│   ├── semantics_analyze.c # Semantic checks
│   ├── semantics_expr.c    # Expression analysis
│   ├── optimizer.c         # Constant folding, DCE
│   ├── codegen.c           # Code generation entry
│   ├── codegen_node.c      # Node-level codegen
│   ├── codegen_expr.c      # Expression codegen
│   ├── codegen_closure.c   # Closure codegen
│   ├── codegen_gen.c       # Generator codegen
│   ├── codegen_generic.c   # Generic specialization
│   ├── codegen_classspec.c # Class specialization
│   ├── codegen_types.c     # Type codegen
│   ├── codegen_emit_utils.c# Emit utilities
│   ├── formatter.c         # Code formatter
│   ├── linter.c            # Linter
│   ├── repl.c              # REPL
│   ├── docgen.c            # Documentation generator
│   ├── registry.c          # Package registry
│   ├── semver.c            # Semantic versioning
│   ├── sourcemap.c         # Source map generation
│   └── moduleresolver.c    # Module resolution
├── include/                # Header files
├── tests/                  # Test files
├── examples/               # Example programs
├── docs/                   # Documentation
└── Makefile                # Build system
```

## Runtime

The generated C code links against a runtime library (`include/runtime.h`) that provides:
- Memory management (`cnext_malloc`, `cnext_free`, `_cnext_track`)
- String handling (`CnextString` type)
- Error handling (`cnext_throw`, `setjmp`/`longjmp` try/catch)
- Threading primitives (`cnext_mutex`, `cnext_channel`)
- Generator/iterator protocol
- I/O functions (`cnext_printin`, `cnext_println`, `cnext_input`)
