<p align="center">
  <img src="logo.png" alt="Cnext Logo" width="400">
</p>

<h1 align="center">Cnext</h1>

<p align="center">
  <strong>A modern programming language that compiles to C</strong>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#quick-start">Quick Start</a> •
  <a href="#documentation">Documentation</a> •
  <a href="#examples">Examples</a> •
  <a href="#contributing">Contributing</a> •
  <a href="#testing">Testing</a>
</p>

---

Cnext is a high-level, statically-typed language with a familiar syntax that transpiles to C. It combines modern language features like generics, closures, pattern matching, and async/await with the performance and portability of C.

## Features

- **Modern Syntax** — Clean, readable code with type inference (`var`), string interpolation, and optional semicolons
- **Rich Type System** — `int`, `long`, `float`, `double`, `str`, `bool`, `char`, `byte`, `uint`, `ulong`, `ushort`, `ubyte`
- **Type Aliases** — `type MyInt = int` for clearer code
- **Nullable Types** — `str?`, `int?` for optional values
- **Generics** — Write reusable code with parameterized types
- **Closures** — First-class functions with captured variables
- **Pattern Matching** — Expressive `match` expressions with wildcards
- **Classes & Traits** — OOP with inheritance, interfaces, and composable traits
- **Error Handling** — `try/catch/finally` with `throw` statements
- **Generators & Coroutines** — Lazy iteration with `yield` and cooperative multitasking
- **Async/Await** — Asynchronous programming with `async func` and `await`
- **Multithreading** — Thread-safe programming with `Mutex` and `Channel`
- **Standard Library** — Modules for I/O, math, JSON, networking, crypto, and more
- **String Utilities** — `contains`, `starts_with`, `ends_with`, `to_upper`, `to_lower`, `trim`, `find`, `replace`, and more
- **Package Manager** — Built-in dependency management with `cnext.toml`
- **Code Formatter** — `cnext fmt` for consistent code style
- **Linter** — `cnext lint` with rules for unreachable code, comparisons, and more
- **REPL** — `cnext repl` for interactive experimentation
- **Optimizer** — 9 passes: constant propagation, copy propagation, peephole, identity elimination, constant folding, dead code elimination, branch simplification, tail-call detection, loop optimizations
- **Package Registry** — `cnext search/install/publish/login/logout/update/remove`
- **Language Server** — LSP for VS Code, Neovim, Helix, Sublime, Emacs
- **Cross-Platform** — Windows, Linux, macOS support with conditional compilation
- **Developer Tools** — `cnext doctor`, `cnext init`, `cnext upgrade`, `cnext cache`, `cnext config`

## Quick Start

### Installation

**From Source (Windows/Linux/macOS):**

```bash
git clone https://github.com/Melton122/cnext.git
cd cnext
make
make install
```

**Prerequisites:**
- GCC or Clang compiler (C11 support)
- Make (Linux/macOS) or MinGW (Windows)
- libcurl (Linux/macOS, for networking)

### Create a New Project

```bash
cnext new my_app
cd my_app
cnext run src/main.cn
```

This creates a project with sample files demonstrating variables, functions, classes, and modules.

### Hello World

Create a file `hello.cn`:

```cnext
main {
    printin("Hello, World!")
}
```

Run it:

```bash
cnext run hello.cn
```

### Build Options

```bash
cnext build file.cn --release    # Optimized build
cnext build file.cn --debug      # Debug build with symbols
cnext build file.cn --verbose    # Show generated C code
cnext build file.cn --clean      # Remove build artifacts
```

## Language Overview

### Variables and Types

```cnext
int x = 42
long big = 9999999999
float pi = 3.14
double precise = 2.718281828
str name = "Cnext"
bool flag = true
char letter = 'A'
byte small = 255
uint unsigned_val = 4294967295

// Type inference
var auto_detected = 100  // int
var greeting = "Hi"      // str

// Type aliases
type MyInt = int
type Name = str
MyInt my_num = 100

// Nullable types
str? optional_str = null
int? optional_int = 42
```

### Functions

```cnext
func add(int a, int b) -> int {
    return a + b
}

// Arrow syntax for simple functions
func multiply(int a, int b) -> int => a * b

// Default and named arguments
func greet(str name, str greeting = "Hello") {
    printin(greeting + ", " + name + "!")
}

greet("World")                    // Hello, World!
greet("Alice", greeting = "Hi")   // Hi, Alice!
```

### Classes and Inheritance

```cnext
class Animal {
    str name

    func new(str n) {
        self.name = n
    }

    func speak() -> str {
        return "..."
    }
}

class Dog extends Animal {
    func new(str n) {
        super.new(n)
    }

    override func speak() -> str {
        return "Woof!"
    }
}
```

### Generics

```cnext
func first<T>(T[] arr) -> T {
    return arr[0]
}

class Box<T> {
    T value

    func new(T v) {
        self.value = v
    }
}
```

### Closures

```cnext
var x = 10
var add_x = (int a) => a + x
printin(add_x(5))  // 15

// Captures by reference
var counter = 0
var increment = () => {
    counter = counter + 1
    return counter
}
increment()  // 1
increment()  // 2
```

### Pattern Matching

```cnext
enum Color { RED, GREEN, BLUE }

func color_name(Color c) -> str {
    match c {
        RED => "red"
        GREEN => "green"
        BLUE => "blue"
        _ => "unknown"
    }
}
```

### Enum with Values

```cnext
enum HttpStatus {
    OK = 200,
    NOT_FOUND = 404,
    ERROR = 500
}

func describe(HttpStatus code) -> str {
    match code {
        200 => "OK"
        404 => "Not Found"
        _ => "Error"
    }
}
```

### String Utilities

```cnext
import string_utils

str s = "Hello, World!"
printin(len(s))                           // 13
printin(string_utils.to_upper(s))         // HELLO, WORLD!
printin(string_utils.contains(s, "World")) // true
printin(string_utils.replace(s, "World", "Cnext"))  // Hello, Cnext!
```

## Standard Library

| Module | Description |
|--------|-------------|
| `io` | File I/O operations |
| `math` | Mathematical functions |
| `json` | JSON parsing and generation |
| `net` | HTTP and TCP networking |
| `os` | System operations |
| `file` | File system operations |
| `string_utils` | String manipulation |
| `time` | Date and time functions |
| `regex` | Regular expressions |
| `collections` | Lists, maps, and sets |
| `crypto` | Cryptographic functions |
| `path` | Path manipulation |
| `encoding` | Base64, hex, URL encoding |
| `process` | Process management |
| `random` | Random number generation |
| `thread` | Threading and synchronization |

## Package Manager

Create a `cnext.toml` in your project root:

```toml
[package]
name = "my-project"
version = "0.1.0"

[dependencies]
mylib = "https://github.com/user/mylib/raw/main/mylib.cn"
```

Install dependencies:

```bash
cnext install
```

## Building

```bash
# Build the compiler
make

# Build with debug symbols
make DEBUG=1

# Run tests
make test

# Run full test suite (compiler + Python tests)
make check

# Install to /usr/local/bin (Linux/macOS)
make install

# Install to %LOCALAPPDATA%\Cnext\bin (Windows)
mingw32-make install

# Clean build artifacts
make clean

# Format source code
make format

# Show build benchmarks
make bench
```

## Project Structure

```
cnext/
├── src/                    # Compiler source code
│   ├── main.c              # CLI entry point
│   ├── lexer.c             # Tokenizer
│   ├── parser*.c           # Parser (6 files)
│   ├── ast.c               # AST nodes
│   ├── codegen*.c          # Code generation (8 files)
│   ├── semantics*.c        # Semantic analysis (5 files)
│   ├── optimizer.c         # Optimizer
│   ├── formatter.c         # Code formatter
│   ├── linter.c            # Linter
│   ├── repl.c              # REPL
│   ├── registry.c          # Package registry
│   └── semver.c            # Semantic versioning
├── include/                # Header files and runtime
│   ├── runtime.h           # Runtime library (all functions)
│   ├── collections.h       # Data structures
│   ├── string_utils.h      # String utilities
│   └── diagnostics.h       # Error diagnostics
├── tests/                  # Test files (50+ tests)
├── examples/               # Example programs
├── docs/                   # Documentation
├── lsp/                    # Language Server Protocol
├── website/                # Registry website and playground
└── Makefile                # Build system
```

## Contributing

We welcome contributions! Here's how to get started:

### Development Setup

1. Fork the repository
2. Clone your fork:
   ```bash
   git clone https://github.com/YOUR_USERNAME/cnext.git
   cd cnext
   ```
3. Build the compiler:
   ```bash
   make
   ```
4. Run tests:
   ```bash
   make test
   ```

### Code Style

- **Language:** C (GNU C11 standard)
- **Indentation:** 4 spaces (no tabs)
- **Braces:** Allman style (opening brace on new line)
- **Naming:** `snake_case` for functions/variables, `UPPER_CASE` for macros
- **Header guards:** `#ifndef CNEXT_NAME_H` / `#define CNEXT_NAME_H`
- **No comments** unless the intent is non-obvious
- **Use `checked_alloc.h`** for all allocations

### Making Changes

1. Create a feature branch:
   ```bash
   git checkout -b feature/my-feature
   ```
2. Make your changes
3. Run tests:
   ```bash
   make test
   make check  # Full test suite
   ```
4. Commit with a clear message:
   ```bash
   git commit -m "Add feature: description of change"
   ```
5. Push and create a pull request

### Reporting Issues

Found a bug? Please report it:

1. Check if the issue already exists in [GitHub Issues](https://github.com/Melton122/cnext/issues)
2. If not, create a new issue with:
   - **Title:** Clear, concise description
   - **Environment:** OS, compiler version (`cnext version`)
   - **Steps to reproduce:** Minimal code example
   - **Expected behavior:** What should happen
   - **Actual behavior:** What actually happens

### What to Work On

**Good first issues:**
- Fix typos in documentation
- Add test cases for edge cases
- Improve error messages
- Add examples

**Larger contributions:**
- New standard library modules
- Optimizer improvements
- Cross-platform enhancements
- Language features

## Testing

### Running Tests

```bash
# Run compiler built-in tests
make test

# Run full test suite
make check

# Run specific test
cnext build tests/test_v9.cn
./out.exe
```

### Writing Tests

Create a test file in `tests/`:

```cnext
// EXPECT: expected output line 1
// EXPECT: expected output line 2

main {
    // Your test code
    printin("Hello, World!")
}
```

### Test Categories

- **Lexer tests:** Token recognition
- **Parser tests:** Syntax parsing
- **Semantic tests:** Type checking
- **Codegen tests:** Code generation
- **Runtime tests:** Runtime behavior
- **Optimizer tests:** Optimization passes
- **Integration tests:** Full program tests

## Examples

See the `examples/` directory for complete programs:
- `hello.cn` - Basic hello world
- `classes.cn` - OOP with classes
- `closures.cn` - Closure examples
- `generators.cn` - Generator functions
- `http_server.cn` - HTTP server
- `calculator.cn` - Calculator
- `todo_cli.cn` - CLI todo app

## Documentation

- [Language Overview](docs/syntax.md)
- [Type System](docs/types.md)
- [Standard Library](docs/stdlib.md)
- [Architecture](docs/architecture.md)
- [Installation](docs/installation.md)

## License

MIT License - see [LICENSE](LICENSE) for details.

## Acknowledgments

Inspired by modern languages like Rust, Go, Swift, and TypeScript, while maintaining compatibility with C through transpilation.
