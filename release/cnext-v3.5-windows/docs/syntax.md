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
func add(int a, int b) -> int {
    return a + b
}
```

### Arrow Function

```cnext
func multiply(int a, int b) -> int => a * b
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
func sum(int... args) -> int {
    var total = 0
    for var n in args {
        total = total + n
    }
    return total
}

printin(sum(1, 2, 3))  // 6
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
