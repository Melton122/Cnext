# Language Syntax

## Basics

### Comments

```cnext
// Single line comment

/* Multi
   line
   comment */
```

### Semicolons

Semicolons are optional in Cnext:

```cnext
// Both are valid
var x = 42
var y = 42;
```

### Blocks

Code blocks are delimited by curly braces:

```cnext
if condition {
    // code
}
```

## Variables

### Declaration

```cnext
// Explicit type
int x = 42
float pi = 3.14
str name = "Cnext"
bool flag = true

// Type inference with var
var auto = 100        // int
var text = "hello"    // str
var decimal = 3.14    // float
var boolean = false   // bool
```

### Constants

```cnext
const int MAX_SIZE = 100
const str PI = "3.14159"
```

### Scope

Variables are block-scoped:

```cnext
var x = 10
if true {
    var y = 20  // y only exists here
}
// y is not accessible here
```

## Types

### Primitive Types

| Type | Description | Example |
|------|-------------|---------|
| `int` | Integer | `42` |
| `float` | Floating point | `3.14` |
| `str` | String | `"hello"` |
| `bool` | Boolean | `true` / `false` |
| `char` | Character | `'a'` |

### Composite Types

```cnext
// Arrays
int[] numbers = {1, 2, 3}
str[] words = {"hello", "world"}

// Tuples
var pair = (1, "hello")
var triple = (1, 2.0, true)
```

### Type Casting

```cnext
float f = 7.8
int i = f as int      // 7

int n = 5
float fn = n as float  // 5.0
```

## Operators

### Arithmetic

```cnext
var a = 10 + 5   // 15
var b = 10 - 5   // 5
var c = 10 * 5   // 50
var d = 10 / 5   // 2
var e = 10 % 3   // 1
```

### Comparison

```cnext
10 == 10   // true
10 != 5    // true
10 > 5     // true
10 < 5     // false
10 >= 10   // true
10 <= 5    // false
```

### Logical

```cnext
true && false   // false
true || false   // true
!true           // false
```

### Assignment

```cnext
var x = 10
x = 20
x += 5    // x = 25
x -= 3    // x = 22
x *= 2    // x = 44
x /= 4    // x = 11
```

### String Operations

```cnext
str a = "hello"
str b = " world"
str c = a + b      // "hello world"
bool d = a == b    // false
int e = a.length   // 5
```

### Null Safety

```cnext
str s = null
bool is_null = (s == null)     // true
bool not_null = (s != null)   // false

// Null coalescing
str result = s ?? "default"
```

## Control Flow

### If/Else

```cnext
if condition {
    // code
} else if other_condition {
    // code
} else {
    // code
}
```

### While Loop

```cnext
var i = 0
while i < 10 {
    printin(i)
    i = i + 1
}
```

### For Loop

```cnext
for int i = 0; i < 10; i = i + 1 {
    printin(i)
}
```

### For-In Loop

```cnext
int[] nums = {1, 2, 3, 4, 5}
for var n in nums {
    printin(n)
}
```

### Break and Continue

```cnext
for int i = 0; i < 10; i = i + 1 {
    if i == 3 {
        continue  // skip 3
    }
    if i == 7 {
        break     // stop at 7
    }
    printin(i)
}
```

### Switch

```cnext
var x = 2
switch x {
    case 1:
        printin("one")
    case 2:
        printin("two")
    case 3:
        printin("three")
    default:
        printin("other")
}
```

### Testing bits with `~=`

Use `~=` to test selected bits of an integer. The pattern reads from most to
least significant bit, with its rightmost bit aligned to bit 0. Each `0` or
`1` tests that position, `?` accepts either bit, and `_` is a visual separator.
Higher bits outside the pattern are ignored. Signed values are interpreted
modulo 2^64.

```cnext
ubyte status = 165
bool ready = status ~= "10??_???1"
```

Here 165 is binary `1010_0101`, so `ready` is true. The compiler checks the
quoted pattern (1 to 64 bits) and emits a mask comparison. The left operand
is evaluated once. The operator has the same precedence as `==`.

### Reading a packed field with `~>`

```cnext
ubyte status = 165
ulong channel_id = status ~> "??##_#???"
```

In binary, 165 is `1010_0101`. The three `#` marks select bits 5 through 3,
which read `100`, so `channel_id` is 4. Use one continuous group of `#`
marks to select a field. Each `?` skips a bit and `_` adds spacing. Layouts
contain 1 to 64 bits, with bit 0 at the right edge.

The result is a `ulong` with the field shifted down to bit 0. For signed
inputs the operator reads the low 64 bits. It evaluates its input once and
binds more tightly than multiplication, so `status ~> "##" * 2` doubles
the value of the low two bits.

### Match

```cnext
enum Color { RED, GREEN, BLUE }

Color c = RED
match c {
    RED => printin("red")
    GREEN => printin("green")
    BLUE => printin("blue")
    _ => printin("other")
}
```

## Functions

### Basic Function

```cnext
func add(int a, int b): int {
    return a + b
}
```

### Arrow Function

```cnext
func multiply(int a, int b): int => a * b
```

### Default Arguments

```cnext
func greet(str name, str greeting = "Hello") {
    printin(greeting + ", " + name + "!")
}

greet("World")                    // Hello, World!
greet("Alice", greeting = "Hi")   // Hi, Alice!
```

### Named Arguments

```cnext
func create_person(str name, int age, str city = "Unknown") {
    printin(name + " is " + age + " from " + city)
}

create_person(name = "Bob", age = 25, city = "NYC")
create_person(age = 30, name = "Charlie")
```

### Variadic Functions

```cnext
func sum(int... args): int {
    var total = 0
    for var n in args {
        total = total + n
    }
    return total
}

printin(sum(1, 2, 3))  // 6
```

### Built-in Functions

Cnext provides many built-in functions globally available without imports:

**String Functions:**
```cnext
str_upper("hello")           // "HELLO"
str_lower("HELLO")           // "hello"
str_trim("  hi  ")           // "hi"
str_contains("hello", "ell") // true
str_index_of("hello", "ll")  // 2
str_replace("hello", "l", "r") // "herro"
str_substring("hello", 1, 3) // "el"
str_repeat("ab", 3)          // "ababab"
str_reverse("hello")         // "olleh"
str_capitalize("hello")      // "Hello"
str_title("hello world")     // "Hello World"
```

**Math Functions:**
```cnext
math_abs(-5)                 // 5
math_min(5, 3)               // 3
math_max(5, 3)               // 5
math_clamp(10, 0, 5)         // 5
math_gcd(12, 8)              // 4
math_lcm(4, 6)               // 12
math_factorial(5)            // 120
math_fibonacci(10)           // 55
```

**Type Conversion:**
```cnext
to_int(3.14)                 // 3
to_float(42)                 // 42.0
to_str(42)                   // "42"
to_bool(1)                   // true
ord("A")                     // 65
```

**File Operations:**
```cnext
write_file("test.txt", "hello")
str content = read_file("test.txt")
bool exists = file_exists("test.txt")
long size = file_size("test.txt")
```

**System Info:**
```cnext
str os = platform()          // "windows"
int cpus = cpu_count()       // 8
str temp = temp_dir()        // temp directory
str home = home_dir()        // home directory
```

## Arrays

### Declaration

```cnext
int[] nums = {1, 2, 3, 4, 5}
str[] words = {"hello", "world"}
```

### Access

```cnext
var first = nums[0]    // 1
var last = nums[4]     // 5
```

### Length

```cnext
printin(nums.length)   // 5
```

### Slicing

```cnext
int[] slice = nums[1:3]    // {2, 3}
int[] from_two = nums[2:]  // {3, 4, 5}
int[] to_three = nums[:3]  // {1, 2, 3}
```

### Bounds Checking

```cnext
int[] nums = {1, 2, 3}
// nums[5] would cause a runtime error
```

## Strings

### Declaration

```cnext
str s1 = "hello"
str s2 = "world"
```

### Interpolation

```cnext
var name = "Alice"
var age = 25
var msg = "I am {name} and I am {age} years old"
```

### Concatenation

```cnext
str result = s1 + " " + s2  // "hello world"
```

### Length

```cnext
printin(s1.length)  // 5
```

### Comparison

```cnext
bool equal = (s1 == s2)    // false
bool not_equal = (s1 != s2)  // true
```

## Additional Keywords

### `own` — Ownership Transfer

```cnext
own str data = get_string()
```

### `constexpr` — Compile-Time Constants

```cnext
constexpr int MAX_SIZE = 1024
```

### `typeof` — Type Inspection

```cnext
str type_name = typeof(42)  // "int"
```

### `extern` — C FFI

```cnext
extern "C" func abs(int value): int

main {
    printin(abs(-42))  // 42, calculated by the C standard library
}
```

You can also group declarations in a block:

```cnext
extern "C" {
    func abs(int value): int
    func rand(): int
}
```

An `extern` declaration describes an existing C function; it does not implement
the function or automatically link an external library. The declarations above
use the C standard library, which the normal `cnext build` workflow already links.
See [the runnable example](../examples/c_interop.cn).

The declaration must match the C function's actual signature and target ABI.
Cnext `str` is a pointer-and-length value, not C's `char*`; declaring
`printf(str fmt, ...)` does not convert it into a C string. Arrays and class
instances also require an explicit, verified representation at a C boundary.
Use `printin` for ordinary Cnext output.

#### Explicit C string conversion

`CStr` represents a read-only, zero-terminated C string (`const char*`). Use
explicit conversion when a C function expects text:

```cnext
extern "C" func strcmp(CStr left, CStr right): int

main {
    CStr first = to_cstr("hello")
    var second = to_cstr("hello")
    printin(strcmp(first, second))  // 0
    str copied = from_cstr(first)
    cstr_free(first)
    cstr_free(second)
    printin(copied)                // hello
}
```

- `to_cstr(str)` allocates an independent copy and appends a zero byte. Empty
  strings are supported. Null strings and strings containing embedded zero
  bytes throw an error instead of silently losing text.
- `from_cstr(CStr)` copies bytes up to the first zero into an ordinary `str`.
  It does not free or take ownership of the C pointer. Null pointers throw.
- `cstr_free(CStr)` releases a live `to_cstr` allocation. It accepts null as a
  no-op and rejects borrowed pointers or already released allocations.

Conversions preserve bytes without changing the encoding. A C function may
return borrowed `CStr` text; copy it with `from_cstr` while the pointer is still
valid. Follow that C library's rules for the borrowed pointer's lifetime and
release. `CStr` does not represent a writable C buffer or a binary buffer.
Use an API that carries a separate length for binary data.

Owned conversions are tracked until `cstr_free`, `gc()`, or process cleanup.
Every alias becomes invalid when its allocation is released. A C function must
not write to, free, or retain an owned conversion beyond that lifetime.
The compiler does not check pointer lifetimes. For repeated calls, release or
reuse the converted string rather than accumulating allocations.

See [the runnable example](../examples/c_strings.cn) and
[memory rules](memory.md#c-string-interoperability).

For custom C sources, use `--c-source` with `build` or `run`. From the repository
root, run the [mixed Cnext/C example](../examples/c_interop/main.cn):

```bash
cnext run examples/c_interop/main.cn --c-source examples/c_interop/calibration.c --include-dir examples/c_interop/include
```

This prints `Calibrated reading: 42`. Cnext generates C, compiles both sources,
and links the executable in one command. The C code includes `calibration.h`;
the Cnext declaration must be kept consistent with that header.

| Build option | Purpose |
|---|---|
| `--c-source <path>` | Compile an additional C source file |
| `--object <path>` | Link an existing compatible object file or static archive |
| `--include-dir <path>` | Add a directory to the C header search path |
| `--library-dir <path>` | Add a directory to the library search path |
| `--link-lib <name>` | Link a library by name, such as `calibration` for `libcalibration.a` |

All five options are repeatable. Paths are relative to the working directory;
quote paths that contain spaces. Native sources and objects are passed before
named libraries. Named libraries retain their supplied order, so list a library
before other libraries it depends on. Objects and libraries must be built for
the same target and a compatible C ABI. Shared libraries also need to be
available to the operating system when the executable runs.

For example, a prebuilt library can be used with:

```bash
cnext build program.cn --library-dir native/lib --link-lib calibration
```

The CLI currently invokes GCC. For a manual build,
`cnext build program.cn --emit-c program.c` generates C without invoking GCC.
Compile that file with your C sources, the Cnext runtime include directory
(`-iquote include` in this repository), and the required platform libraries.
`--emit-c` cannot be combined with native build options because it does not
compile or link. Cnext does not yet generate a public C header for calling
Cnext functions from C.

### `macro` — Compile-Time Code Generation

```cnext
macro swap(a, b) {
    var temp = a
    a = b
    b = temp
}
```

### `bench` — Performance Measurement

```cnext
bench {
    // code to benchmark
}
```

### `@attribute` — Annotations

```cnext
@deprecated
func old_function() { }
```

### Null Coalescing `??`

```cnext
str name = get_name() ?? "default"
```

### Optional Chaining `?.`

```cnext
str val = obj?.field?.subfield
```
