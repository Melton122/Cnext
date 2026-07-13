# Cnext Roadmap

## Version 1.0 — Core (Implemented)
- Core language syntax (variables, loops, conditions)
- Basic types: `int`, `float`, `bool`, `char`, `str`
- Transpiler architecture (Cnext to C)
- Basic builtin functions (`printin`, `input`)
- Basic OOP support (classes as structs)
- Command-line interface (`cnext run`, `cnext build`)

## Version 1.1 — Modern Touches (Implemented)
- `var` keyword for type inference
- For-in loops for array iteration
- String interpolation: `"Hello, {name}!"`
- `import` statement for standard library modules

## Version 2.0 — Safety & Standard Library (Implemented)
- **Semantic Analysis:** Strict compile-time type checking and error reporting before generating C code
- **Memory Safety:** Safe `str` struct to prevent buffer overflows, bounds checking for arrays
- **Standard Library:** Core modules for `io`, `math`, `file`, `net`, `json`, `os`, `string_utils`, `time`, `regex`, `collections`, `crypto`, `path`, `encoding`, `process`, `random`
- **Package Manager:** `cnext install <pkg>` with `cnext.toml` parsing
- **Structs & Enums:** User-defined data types
- **`const`:** Immutable variable declarations
- **Switch Statements:** Pattern-based control flow with `case`/`default`
- **Build configuration file:** `cnext.toml` with project metadata and dependencies
- **Project templates:** `cnext new <ProjectName>` scaffolding

## Version 2.5 — Modern Language Features (Implemented)
- **OOP:** Inheritance (`extends`), interfaces (`implements`), `override`, `super`, constructors with `new`
- **Traits:** Composable behavior with default method implementations (`trait`/`implements`)
- **Error Handling:** `try`/`catch`/`finally` with `throw` (setjmp/longjmp)
- **Lambdas:** Anonymous functions with `=>` syntax
- **Pattern Matching:** `match` expressions with wildcard `_` arms
- **Null Safety:** `null` values with safe assignment to classes and strings
- **Type Casting:** `as` keyword for explicit type conversion
- **Array Properties:** `.length` access on arrays
- **Dynamic Arrays:** `collections` module — `list_new/add/get/set/remove/size/contains/sort/free`
- **Map & Set:** `collections` module — `map_new/set/get/has/remove/size/free`, `set_new/add/has/remove/size/free`
- **Testing Framework:** Built-in `test` keyword with `assert` for unit/integration tests

## Version 3.0 — Generics & Type System Expansion (Implemented)
- **Generics:** Parameterized types `List<T>`, `Map<K,V>`, generic functions and classes
- **Optional/Result Types:** `Option<T>` and `Result<T,E>` for safe error handling without exceptions
- **Tuples:** `(int, str)` composite types
- **Destructuring:** `int a, str b = tuple` and pattern destructuring
- **Operator Overloading:** Custom `+`, `-`, `*`, `/`, `==`, `[]`, etc. for user types
- **Default Arguments:** `func foo(int x, int y = 10)`
- **Named Arguments:** `foo(y = 5, x = 3)`
- **Variadic Functions:** `func sum(int... args)`
- **Extension Methods:** Attach methods to existing types

## Version 3.5 — Closures & Async (Implemented)
- **Closures with Captured Variables:** Lambdas that capture enclosing scope by reference/value
- **Async/Await:** `async func`, `await` expressions (synchronous cooperative model)
- **Coroutines:** `coroutine func` with yield/resume state machines
- **Multithreading:** `thread` keyword, `Mutex`, `Channel`, thread pools (Win32 + pthreads)
- **Iterators:** Custom iterator protocol (`next()`, `iter()`), `yield` generator syntax

## Version 4.0 — Metaprogramming & Reflection (Implemented)
- **Macros:** `macro` keyword for compile-time code generation with argument substitution
- **Attributes/Annotations:** `@attribute` syntax for metadata on declarations
- **Reflection:** Runtime type inspection via `typeof()` returning type name as string
- **Compile-Time Evaluation (constexpr):** `constexpr` declarations for compile-time constants
- **Pattern Matching Improvements:** Guard clauses (`match x { pattern if condition => body }`)

## Version 4.5 — Memory Model & FFI (Implemented)
- **FFI Improvements:** `extern "C"` declarations for seamless C function interop
- **Ownership Semantics:** `own` keyword for ownership transfer expressions
- **Memory Profiler:** `--profile` flag for heap profiling, leak detection, allocation tracking
- **Benchmark Tool:** Built-in `bench { }` block for performance measurement

## Version 5.0 — Backends & Compiler Infrastructure (Implemented)
- **Optimizer:** Constant folding, dead code elimination in AST
- **Incremental Compilation:** `--incremental` flag to skip recompilation of unchanged files
- **Better Compiler Diagnostics:** Colored terminal output, error suggestions, context-aware hints
- **Source Maps:** `--sourcemap` flag for line mapping from `.cn` to generated `.c`

## Version 5.5 — Tooling (Implemented)
- **Formatter (`cnext fmt`):** Opinionated code formatter
- **Linter (`cnext lint`):** Static analysis with rules and auto-fixes
- **REPL (`cnext repl`):** Interactive read-eval-print loop
- **Documentation Generator (`cnext doc`):** Auto-generate docs from source declarations

## Version 6.0 — Ecosystem (Implemented)
- **Package Registry:** Central registry for publishing and discovering Cnext packages
- **Package Publishing (`cnext publish`):** Upload packages to registry with versioning
- **Dependency Versioning:** Semantic versioning support (`^1.2.3`, `>=2.0`)
- **Module Registry:** Namespaced modules (`std/http`, `org/pkg`)
- **Official Documentation Website:** Full language docs, tutorial, API reference
- **Interactive Playground:** Try Cnext in the browser (Wasm-based)
- **Performance Benchmarking Suite:** Comprehensive benchmarks for language perf
- **Cross-platform CI:** Automated testing on Windows, Linux, macOS
