# Types

## Primitive Types

### int

Integer numbers:

```cnext
var x = 42
var negative = -10
var zero = 0
```

### float

Floating-point numbers:

```cnext
var pi = 3.14159
var e = 2.71828
var negative = -1.5
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

str s = "123"
// int x = s as int  // Not supported, use parsing functions
```

## Null Values

Classes and strings can be null:

```cnext
str s = null
bool is_null = (s == null)  // true
```

## Type Checking

```cnext
var x = 42
// x is an int

var arr = {1, 2, 3}
// arr is an int[]
```
