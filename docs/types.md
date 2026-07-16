# Types

## Primitive Types

### int

32-bit integer:

```cnext
var x = 42
var negative = -10
var zero = 0
```

### long

64-bit integer:

```cnext
long big = 9999999999
long negative = -9999999999
```

### float

32-bit floating-point:

```cnext
var pi = 3.14159
var e = 2.71828
var negative = -1.5
```

### double

64-bit floating-point:

```cnext
double precise = 3.14159265358979
double e = 2.718281828459045
```

### str

Strings:

```cnext
var name = "Cnext"
var greeting = "Hello, World!"
var empty = ""
```

### bool

Boolean values:

```cnext
var flag = true
var other = false
```

### char

Single characters:

```cnext
var c = 'a'
var digit = '9'
var newline = '\n'
```

### byte

8-bit unsigned integer (0-255):

```cnext
byte small = 255
byte tiny = 0
```

### uint

32-bit unsigned integer:

```cnext
uint positive = 4294967295
uint zero = 0
```

### ulong

64-bit unsigned integer:

```cnext
ulong big_positive = 18446744073709551615
```

### ushort

16-bit unsigned integer:

```cnext
ushort medium = 65535
```

### ubyte

8-bit unsigned integer:

```cnext
ubyte tiny = 255
```

## Type Aliases

Create custom type names:

```cnext
type MyInt = int
type Name = str

MyInt x = 100
Name name = "Test"
```

## Nullable Types

Add `?` to make types nullable:

```cnext
str? optional_str = null
int? optional_int = 42

if optional_str != null {
    printin(optional_str)
}
```

## Composite Types

### Arrays

Fixed-size collections:

```cnext
int[] numbers = {1, 2, 3, 4, 5}
str[] words = {"hello", "world"}
float[] decimals = {1.1, 2.2, 3.3}
```

### Tuples

Heterogeneous collections:

```cnext
var pair = (1, "hello")
var triple = (1, 2.0, true)
var mixed = ("name", 42, true)
```

## Type Inference

Use `var` for automatic type inference:

```cnext
var x = 42        // int
var pi = 3.14     // float
var name = "Cnext" // str
var flag = true   // bool
```

## Type Casting

Convert between types with `as`:

```cnext
float f = 7.8
int i = f as int      // 7

int n = 5
float fn = n as float  // 5.0
```

## Null Values

Classes and strings can be null:

```cnext
str s = null
bool is_null = (s == null)  // true
```

## Struct Types

```cnext
struct Point {
    int x
    int y
}

Point p = Point { x: 10, y: 20 }
printin(p.x)  // 10
```

## Function Types

Use `func` as a type for function pointers and closures:

```cnext
// Function parameter typed as func
func apply(int x, func f) -> int {
    return f(x)
}

var dbl = (int x) => x * 2
printin(apply(5, dbl))  // 10

// Return type as func
func make_adder(int x) -> func {
    return (int a) => a + x
}

var add5 = make_adder(5)
printin(add5(3))  // 8
```

## Iterator Types

```cnext
// Iterator type for generators
coroutine func count_to(int n) -> iter<int> {
    for int i = 1; i <= n; i = i + 1 {
        yield i
    }
}

var counter = count_to(5)
for int val in counter {
    printin(val)  // 1, 2, 3, 4, 5
}
```
