# Cnext Language Specification

## Overview
Cnext is a modern, beginner-friendly evolution of C. It maintains C's high performance and familiar syntax while removing unnecessary complexity like mandatory semicolons, manual memory management for strings (by default), and verbose loop syntax.

## File Extensions
- Cnext source files end with `.cn`

## Entry Point
The program starts executing from the `main` block.
```cnext
main {
    printin("Hello Cnext!");
}
```

## Basic Types
Cnext is strongly typed.
- `int`: Standard 32-bit integer.
- `float`: Standard 32-bit floating point.
- `str`: String type (mapped to C-strings or struct depending on implementation).
- `bool`: Boolean type (`true` or `false`).
- `char`: Single ASCII character.

## Variables
Variables must be declared with their types. Semicolons are optional.
```cnext
int age = 19
str name = "Melton"
bool student = true
```

## Input and Output
Built-in functions for I/O operations.
```cnext
printin("Hello World")
printin(age)
str name = input("Enter name: ")
```

## Control Flow
### Conditional Statements
Parentheses around conditions are optional, but curly braces are mandatory.
```cnext
if age >= 18 {
    printin("Adult")
} else {
    printin("Minor")
}
```

### Loops
`while` loops execute as long as the condition is true.
```cnext
while health > 0 {
    health -= 10
}
```

`for` loops follow the traditional C-style `init; condition; increment` but parentheses are optional.
```cnext
for int i = 0; i < 10; i += 1 {
    printin(i)
}
```

## Functions
Functions are declared using the `func` keyword. Return types are indicated with `->`.
```cnext
func greet(str name) {
    printin("Hello, " + name)
}

func add(int a, int b) -> int {
    return a + b
}
```

## Arrays
Arrays are initialized with curly braces.
```cnext
int[] nums = {1, 2, 3, 4, 5}
```

## Classes (OOP Support)
Cnext provides basic Object-Oriented support through classes. Under the hood, these map to structs and methods.
```cnext
class Student {
    str name
    int age
}
```

## Comments
```cnext
// This is a single line comment

/*
This is a
multi-line comment
*/
```
