# Cnext Token Definitions

The Lexer breaks down a `.cn` file into the following tokens.

## Keywords
- `main`
- `if`
- `else`
- `while`
- `for`
- `func`
- `return`
- `class`
- `new`
- `self`
- `var`
- `let`

## Types
- `int`
- `float`
- `str`
- `bool`
- `char`

## Literals
- `NUMBER`: Integer numbers (e.g., `42`)
- `FLOAT_LITERAL`: Floating-point numbers (e.g., `3.14`)
- `STRING_LITERAL`: Characters enclosed in double quotes (e.g., `"Hello"`)
- `CHAR_LITERAL`: Single character enclosed in single quotes (e.g., `'a'`)
- `TRUE_LITERAL`: Boolean `true`
- `FALSE_LITERAL`: Boolean `false`

## Identifiers
- `IDENTIFIER`: Alphanumeric string starting with a letter or underscore (e.g., `age`, `greet_user`).

## Operators
- `+` (Plus)
- `-` (Minus)
- `*` (Multiply)
- `/` (Divide)
- `=` (Assign)
- `+=`, `-=`, `*=`, `/=` (Compound Assign)
- `==` (Equal)
- `!=` (Not Equal)
- `<` (Less Than)
- `<=` (Less Than or Equal)
- `>` (Greater Than)
- `>=` (Greater Than or Equal)
- `&&` (Logical AND)
- `||` (Logical OR)
- `!` (Logical NOT)
- `->` (Return type arrow)

## Punctuation
- `{` `}` (Curly braces for blocks)
- `(` `)` (Parentheses for grouping and parameters)
- `[` `]` (Brackets for arrays)
- `,` (Comma)
- `;` (Optional Semicolon)

## Special
- `EOF`: End of file
- `ERROR`: Invalid character or unclosed literal
