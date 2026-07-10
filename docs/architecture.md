# Cnext Compiler Architecture

The Cnext compiler is a transcompiler (transpiler) that takes `.cn` source code and generates equivalent `.c` code, which is then passed to a C compiler (GCC/Clang) to produce the final executable.

## Pipeline Overview

```
Source (.cn) -> Lexer -> Tokens -> Parser -> AST -> Code Generator -> C Source (.c) -> GCC/Clang -> Executable
```

### 1. Lexer (Scanner)
- **Input:** Cnext source code as a stream of characters.
- **Output:** A stream of Tokens.
- **Role:** Reads characters sequentially, discarding whitespace and comments. Groups characters into meaningful lexical units (Tokens) like `KEYWORD_IF`, `IDENTIFIER`, `LITERAL_INT`, `OP_PLUS`.

### 2. Parser
- **Input:** Stream of Tokens.
- **Output:** Abstract Syntax Tree (AST).
- **Role:** Analyases the token stream against the EBNF grammar to ensure syntactic correctness. It constructs a tree where nodes represent language constructs (blocks, loops, function calls). Optional semicolons and parenthesis are resolved here.

### 3. Semantic Analyzer (Future/Lightweight)
- **Input:** AST.
- **Output:** Annotated AST / Symbol Table.
- **Role:** In Cnext 1.0, type checking and semantic analysis will primarily be offloaded to the target C compiler. Future versions will perform strict type checking, scope resolution, and safety checks at this phase.

### 4. Code Generator
- **Input:** AST.
- **Output:** C Source Code string.
- **Role:** Traverses the AST recursively and generates equivalent C code.
  - Maps `main {}` to `int main(int argc, char** argv) {}`.
  - Maps `printin()` to `printf()`.
  - Converts class declarations to `struct` definitions.
  - Generates necessary `#include` directives (e.g., `<stdio.h>`, `<stdlib.h>`, `<stdbool.h>`, `<string.h>`).

### 5. Backend (C Compiler)
- **Input:** Generated C code.
- **Output:** Executable binary.
- **Role:** The CLI tool (`cnext`) automatically invokes `gcc` or `clang` behind the scenes to compile the generated `.c` file into a native executable for the current platform (Windows/Linux/macOS).
