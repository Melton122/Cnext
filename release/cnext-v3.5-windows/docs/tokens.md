# Cnext Token Definitions

The Lexer breaks down a `.cn` file into the following tokens.

## Keywords
- `main` - Entry point
- `if` / `else` - Conditional statements
- `while` / `for` - Loops
- `func` - Function declaration
- `return` - Return from function
- `class` / `struct` / `enum` - Type declarations
- `new` / `self` / `super` - Object creation and access
- `var` / `const` - Variable declarations
- `import` - Module imports
- `break` / `continue` - Loop control
- `try` / `catch` / `finally` / `throw` - Error handling
- `match` / `case` / `default` - Pattern matching
- `extends` / `implements` / `override` - Inheritance
- `interface` / `trait` - Type contracts
- `async` / `await` / `yield` - Asynchronous code
- `coroutine` - Coroutine functions
- `type` - Type aliases
- `macro` - Macro declarations
- `constexpr` - Compile-time constants
- `extern` - External declarations
- `typeof` - Type reflection
- `operator` - Operator overloading
- `test` / `assert` - Testing

## Types
- `int` - 32-bit integer
- `long` - 64-bit integer
- `float` - 32-bit float
- `double` - 64-bit float
- `str` - String
- `bool` - Boolean
- `char` - Character
- `byte` - 8-bit unsigned
- `uint` - 32-bit unsigned
- `ulong` - 64-bit unsigned
- `ushort` - 16-bit unsigned
- `ubyte` - 8-bit unsigned

## Literals
- `NUMBER`: Integer numbers (e.g., `42`)
- `FLOAT_LITERAL`: Floating-point numbers (e.g., `3.14`)
- `STRING_LITERAL`: Characters enclosed in double quotes (e.g., `"Hello"`)
- `CHAR_LITERAL`: Single character enclosed in single quotes (e.g., `'a'`)

## Operators
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logical: `&&`, `||`, `!`
- Assignment: `=`, `+=`, `-=`, `*=`, `/=`
- Increment/Decrement: `++`, `--`
- Arrow: `->`, `=>`
- Range: `..`

## Punctuation
- `{` `}` - Blocks
- `(` `)` - Grouping / parameters
- `[` `]` - Arrays / indexing
- `;` - Statement separator
- `,` - List separator
- `.` - Member access
- `:` - Type annotation
- `?` - Nullable type
- `@` - Attributes
- `...` - Variadic arguments
