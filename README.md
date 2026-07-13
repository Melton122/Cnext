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
  <a href="#contributing">Contributing</a>
</p>

---

Cnext is a high-level, statically-typed language with a familiar syntax that transpiles to C. It combines modern language features like generics, closures, pattern matching, and async/await with the performance and portability of C.

## Features

- **Modern Syntax** — Clean, readable code with type inference (`var`), string interpolation, and optional semicolons
- **Static Typing** — Catch errors at compile time with a powerful type system
- **Generics** — Write reusable code with parameterized types
- **Closures** — First-class functions with captured variables
- **Pattern Matching** — Expressive `match` expressions with wildcards
- **Classes & Traits** — OOP with inheritance, interfaces, and composable traits
- **Error Handling** — `try/catch/finally` with `throw` statements
- **Generators & Coroutines** — Lazy iteration with `yield` and cooperative multitasking
- **Async/Await** — Asynchronous programming with `async func` and `await`
- **Multithreading** — Thread-safe programming with `Mutex` and `Channel`
- **Standard Library** — Modules for I/O, math, JSON, networking, crypto, and more
- **Package Manager** — Built-in dependency management with `cnext.toml`

## Quick Start

### Installation

**From Source (Windows/Linux/macOS):**

```bash
git clone https://github.com/Melton122/cnext.git
cd cnext
make
```

**Prerequisites:**
- GCC or Clang compiler
- Make (Linux/macOS) or MinGW (Windows)

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

## Language Overview

### Variables and Types

```cnext
int x = 42
float pi = 3.14
str name = "Cnext"
bool flag = true

// Type inference
var auto_detected = 100  // int
var greeting = "Hi"      // str
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

    func speak() {
        printin("...")
    }
}

class Dog extends Animal {
    func new(str n) {
        super.new(n)
    }

    override func speak() {
        printin("Woof!")
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

var numbers = {1, 2, 3}
var first_num = first(numbers)  // 1

var box = new Box<int>(42)
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

### Generators and Coroutines

```cnext
// Generator with yield
func fibonacci() -> iter<int> {
    var a = 0, b = 1
    while true {
        yield a
        var temp = a + b
        a = b
        b = temp
    }
}

for var x in fibonacci() {
    if x > 100 { break }
    printin(x)
}

// Coroutine with resume
coroutine func counter() -> iter<int> {
    var i = 0
    while true {
        yield i
        i = i + 1
    }
}
```

### Async/Await

```cnext
async func fetch_data(str url) -> str {
    // Async operations
    return "data from " + url
}

async func main_async() {
    var result = await fetch_data("https://example.com")
    printin(result)
}

main {
    run_async main_async()
}
```

### Threading

```cnext
import thread

main {
    var m = mutex_new()
    var ch = channel_new(8)

    // Mutex for thread-safe access
    mutex_lock(m)
    // critical section
    mutex_unlock(m)

    // Channel for communication
    channel_send(ch, "hello")
    var msg = channel_recv(ch)
}
```

### Error Handling

```cnext
func divide(int a, int b) -> int {
    if b == 0 {
        throw "Division by zero"
    }
    return a / b
}

try {
    var result = divide(10, 0)
} catch (str err) {
    printin("Error: " + err)
} finally {
    printin("Cleanup")
}
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

# Run tests
make test

# Clean build artifacts
make clean
```

## Project Structure

```
cnext/
├── src/           # Compiler source code
├── include/       # Header files and runtime
├── tests/         # Test files
├── examples/      # Example programs
├── docs/          # Documentation
└── packages/      # Installed packages
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## License

MIT License - see [LICENSE](LICENSE) for details.

## Acknowledgments

Inspired by modern languages like Rust, Go, Swift, and TypeScript, while maintaining compatibility with C through transpilation.
