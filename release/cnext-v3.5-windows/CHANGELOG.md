# Changelog

All notable changes to Cnext will be documented in this file.

## [3.5.0] - 2026

### Added
- **Coroutines**: `coroutine func` keyword with yield/resume state machines
- **Async/Await**: `async func` and `await` expressions for cooperative async programming
- **Multithreading**: `import thread` with Mutex and Channel primitives
- **Thread Runtime**: Platform-specific implementations (Win32 threads, pthreads)
- **Thread Module**: `mutex_new/lock/unlock/free`, `channel_new/send/recv/free`

### Changed
- Version bumped to 3.5.0
- Updated roadmap with implemented features

## [3.0.0] - 2026

### Added
- **Generics**: Parameterized types for functions and classes
- **Tuples**: `(int, str)` composite types with destructuring
- **Operator Overloading**: Custom `+`, `-`, `*`, `/`, `==`, `[]` operators
- **Default Arguments**: `func foo(int x, int y = 10)`
- **Named Arguments**: `foo(y = 5, x = 3)`
- **Variadic Functions**: `func sum(int... args)`
- **Extension Methods**: Attach methods to existing types
- **Optional/Result Types**: `Option<T>` and `Result<T,E>`
- **Pattern Matching Improvements**: Guard clauses, destructuring patterns

### Changed
- Version bumped to 3.0.0

## [2.5.0] - 2026

### Added
- **OOP**: Inheritance, interfaces, traits, constructors
- **Error Handling**: try/catch/finally with throw
- **Lambdas**: Anonymous functions with `=>` syntax
- **Pattern Matching**: `match` expressions with wildcards
- **Null Safety**: `null` values with safe assignment
- **Type Casting**: `as` keyword
- **Array Properties**: `.length` access
- **Dynamic Arrays**: collections module
- **Map & Set**: collections module
- **Testing Framework**: `test` keyword with `assert`

### Changed
- Version bumped to 2.5.0

## [2.0.0] - 2026

### Added
- **Semantic Analysis**: Compile-time type checking
- **Memory Safety**: Safe `str` struct, bounds checking
- **Standard Library**: Core modules for io, math, file, net, json, os, etc.
- **Package Manager**: `cnext install` with `cnext.toml`
- **Structs & Enums**: User-defined types
- **const**: Immutable variables
- **Switch Statements**: Pattern-based control flow
- **Build Configuration**: `cnext.toml`
- **Project Templates**: `cnext new`

### Changed
- Version bumped to 2.0.0

## [1.1.0] - 2026

### Added
- `var` keyword for type inference
- For-in loops
- String interpolation: `"Hello, {name}!"`
- `import` statement for modules

### Changed
- Version bumped to 1.1.0

## [1.0.0] - 2026

### Added
- Core language syntax (variables, loops, conditions)
- Basic types: `int`, `float`, `bool`, `char`, `str`
- Transpiler architecture (Cnext to C)
- Basic builtin functions (`printin`, `input`)
- Basic OOP support (classes as structs)
- Command-line interface (`cnext run`, `cnext build`)

### Changed
- Initial release
