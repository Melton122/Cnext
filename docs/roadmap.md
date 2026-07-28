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

## Version 7.0 — Module System (Implemented)
- **Import system:** `import module` with package support
- **Modules:** File-based module organization
- **Package manager improvements:** `cnext install` for dependency management
- **Namespaces:** `namespace MyLib { }` syntax
- **Visibility modifiers:** `public`, `private`, `protected`

## Version 8.0 — Type System & Developer Experience (Implemented)
- **Type aliases:** `type MyInt = int` for clearer code
- **Nullable types:** `str?`, `int?` for optional values
- **Optional chaining:** `?.` operator for safe member access
- **Null coalescing:** `??` operator for default values
- **Match expressions:** Enhanced pattern matching with guards
- **When expressions:** Multi-branch conditional expressions
- **Comprehensions:** List/dict/set comprehension syntax
- **Ranges:** `1..10`, `1..=10` range types
- **String interpolation:** `"Hello ${name}"` syntax
- **Raw strings:** `r"no \escape here"` syntax
- **9-pass optimizer:** Constant propagation, copy propagation, peephole, identity elimination, constant folding, dead code elimination, branch simplification, tail-call detection, loop optimizations
- **LSP support:** Language Server Protocol for VS Code, Neovim, Helix, Sublime, Emacs

## Version 9.0 — Performance & Safety (Implemented)
- **Enum values:** `enum Color { RED = 0xFF0000, GREEN = 0x00FF00 }`
- **Memory pool allocator:** `POOL_ALLOC`, `POOL_FREE_ALL` for fast bulk allocation
- **Arena allocator:** `ARENA_ALLOC`, `ARENA_FREE_ALL` for bump allocation
- **HashMap:** String-keyed hash map with O(1) lookups (`hashmap_new`, `hashmap_put`, `hashmap_get`)
- **Colored error messages:** File/line context with colored output
- **Critical bug fixes:** NULL pointer handling, integer overflow protection, `math_abs(INT_MIN)`, `intcmp` overflow in qsort
- **`func` type:** Use `func` as a type for function pointers and closures

## Version 10.0 — Global Built-in Functions (Implemented)
- **All built-in functions globally available:** No `import` required for core, string, math, type conversion, file, system, encoding, and crypto functions
- **String builtins:** `str_upper`, `str_lower`, `str_trim`, `str_contains`, `str_starts_with`, `str_ends_with`, `str_index_of`, `str_last_index_of`, `str_replace`, `str_substring`, `str_repeat`, `str_reverse`, `str_count`, `str_is_empty`, `str_capitalize`, `str_title`, `str_pad_left`, `str_pad_right`, `str_remove`, `str_insert`, `str_split`, `str_join`
- **Math builtins:** `math_abs`, `math_min`, `math_max`, `math_clamp`, `math_gcd`, `math_lcm`, `math_factorial`, `math_fibonacci`, `math_sqrt`, `math_pow`, `math_log`, `math_exp`, `math_sin`, `math_cos`, `math_tan`, `math_random`, `math_random_range`
- **Type conversion builtins:** `to_int`, `to_float`, `to_str`, `to_bool`, `to_char`, `parse_int`, `parse_float`, `char`, `ord`, `bytes`
- **Core builtins:** `print`, `printin`, `input`, `len`, `typeof`, `assert`, `panic`, `exit`, `clone`, `swap`
- **File builtins:** `read_file`, `write_file`, `append_file`, `delete_file`, `copy_file`, `move_file`, `file_exists`, `file_size`
- **System builtins:** `cwd`, `chdir`, `platform`, `sys_args`, `getenv`, `setenv`, `hostname`, `username`, `cpu_count`, `temp_dir`, `home_dir`, `sys_exec`, `sys_shell`
- **Time builtins:** `time_now`, `time_sleep`, `time_timestamp`, `time_date`, `time_time`, `time_stopwatch_start`, `time_stopwatch_stop`, `time_format_time`
- **Encoding builtins:** `base64_encode`, `base64_decode`
- **Crypto builtins:** `md5`, `sha1`, `sha256`, `uuid`
- **Collections builtins:** `push`, `pop`, `shift`, `unshift`, `insert_at`, `remove_at`, `clear`, `sort`, `reverse_array`, `contains_item`, `array_index`, `array_last`, `array_first`, `array_slice`, `array_unique`, `array_shuffle`, `array_map`, `array_filter`, `array_reduce`, `array_find`
- **Utility builtins:** `debug`, `gc`, `benchmark`, `stacktrace`
- **Table-driven codegen:** Refactored code generator to use dispatch table for efficient builtin routing
- **Comprehensive tests:** 7 new test files covering all built-in function categories
- **Example programs:** 4 new example files demonstrating built-in function usage
