# Changelog

All notable changes to Cnext will be documented in this file.

## [9.0.0] - 2026

### Added
- **Enum values:** Enum members can now have explicit values (`enum Color { RED = 0xFF0000 }`)
- **Memory pool allocator:** Fast bulk allocation for performance-critical code
- **HashMap:** String-keyed hash map with O(1) lookups (`hashmap_new`, `hashmap_put`, `hashmap_get`)
- **Improved error handling:** Colored error messages with file/line context
- **Enhanced `cnext new`:** Projects now include sample files demonstrating variables, functions, classes, and imports
- **Better install feedback:** `cnext install` shows progress, handles standard modules, and reports success/failure counts
- **Type inference from `new`:** `var dog = new Dog(...)` now correctly infers the class type

### Fixed
- **Critical:** NULL pointer dereferences in runtime (`printin_str`, `printin_cstr`, `cnext_concat`)
- **Critical:** Integer overflow in string operations (`cnext_str_repeat`, `str_repeat`, `cnext_str_replace`)
- **Critical:** `math_abs(INT_MIN)` undefined behavior
- **Critical:** `intcmp` subtraction overflow corrupting `qsort` results
- Class method calls on `var` declarations (`dog.info()` now works)
- Lambda calls inside string interpolation (`"result: {double_it(5)}"`)
- Windows API name collisions (`Rectangle`, `ERROR`, `WARNING`, `OK`)
- Match arms in void functions no longer emit invalid `return` statements
- Method calls on `var` declarations with `new` expressions
- Type inference from `new` expressions for proper symbol resolution
- 70+ runtime, arena, closure, and codegen bugs from pre-release audit
- Documentation version mismatches (Makefile, installation.md)
- Missing types and keywords in documentation

### Changed
- **Version bumped to 9.0.0**
- **Updated documentation:** types.md, tokens.md, installation.md
- **Improved Makefile:** Version numbers corrected

## [8.0.0] - 2026

### Added
- **Type aliases:** `type MyInt = int` for clearer code
- **Nullable types:** `str?`, `int?` for optional values
- **Advanced optimizer:** 9 passes including constant propagation, copy propagation, peephole, loop optimizations
- **Package registry CLI:** `search`, `info`, `login`, `logout`, `update`, `remove` commands
- **Language Server (LSP):** Python-based LSP with autocomplete, hover, go-to-definition
- **Developer tools:** `doctor`, `init`, `upgrade`, `cache`, `config` commands
- **Build flags:** `--release`, `--debug`, `--no-optimize`, `--verbose`, `--clean`
- **String utilities:** 15+ functions (contains, starts_with, ends_with, to_upper, to_lower, trim, find, replace, count, repeat, reverse, to_int, to_double)
- **Registry website and playground:** HTML-based web interface

### Fixed
- **Critical:** Match expression return statements
- **Critical:** Lexer keyword recognition for uint/ushort/ubyte
- **Critical:** `printin` macro for unsigned types

## [6.0.0] - 2026

### Added
- **Semantic Versioning (`semver.h`):** Full semver parsing, comparison, and constraint satisfaction (`^1.2.3`, `~1.2.3`, `>=1.0.0`)
- **Package Registry Client:** Download, resolve, and manage packages from the Cnext registry
- **`cnext publish`:** Publish packages to the registry with tarball creation
- **`cnext search <query>`:** Search the package registry
- **`cnext info <package>`:** Show package versions and metadata
- **Namespaced Module Resolution:** `std/http`, `org/pkg` style imports with version constraints

## [5.5.0] - 2026

### Added
- **Formatter (`cnext fmt`):** Opinionated code formatter for `.cn` files
- **Linter (`cnext lint`):** Static analysis with rules (comparison with literals, assignment in condition, double semicolons)
- **REPL (`cnext repl`):** Interactive read-eval-print loop for quick experimentation
- **Documentation Generator (`cnext doc`):** Auto-generate markdown API docs from source declarations

## [5.0.0] - 2026

### Added
- **Optimizer**: Constant folding and dead code elimination pass
- **Better Diagnostics**: Colored error messages with fix suggestions
- **Source Maps**: Track line mappings from `.cn` to generated `.c` files
- **Incremental Compilation**: `--incremental` flag skips recompilation of unchanged files
- **New CLI Flags**: `--no-optimize`, `--incremental`, `--sourcemap`, `--profile`

### Fixed
- All v4.0/v4.5 bug fixes

### Changed
- Version bumped to 5.0.0
- Updated roadmap with implemented features
- Improved help output with all available options

## [4.5.0] - 2026

### Added
- **FFI (`extern`)**: Declare C functions with `extern "C" { func name(args) : type; }` syntax
- **Benchmark Tool**: `bench { code }` block for timing code execution
- **Ownership Semantics**: `own` keyword for ownership transfer expressions
- **Memory Profiler**: `--profile` flag for allocation tracking and leak detection

### Fixed
- All v4.0 bug fixes

### Changed
- Version bumped to 4.5.0
- Updated roadmap with implemented features

## [4.0.0] - 2026

### Added
- **typeof()**: Runtime type inspection — returns type name as a string
- **constexpr**: Compile-time constant declarations (`constexpr int MAX = 100`)
- **Attributes/Annotations**: `@attribute` syntax on declarations with optional arguments
- **Macro Expansion**: `macro` keyword with compile-time argument substitution
- **Pattern Matching Improvements**: Guard clauses (`match x { pattern if condition => body }`)

### Fixed
- Fixed `unlock` keyword lexer bug (length check was incorrect)
- Fixed `typeof` keyword lexing (was never recognized by lexer)
- Fixed `logic_or` precedence bug (was calling `logic_and` instead of `null_coalesce`)
- Fixed non-portable GCC flags (Windows-only `-lwinhttp`/`-lws2_32` now conditional)

### Changed
- Version bumped to 4.0.0
- Updated roadmap with implemented features

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
