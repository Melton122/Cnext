# Getting Started with Cnext

Welcome to Cnext! This guide will help you get up and running quickly.

## Installation

### From Source

**Prerequisites:**
- GCC or Clang compiler
- Make (Linux/macOS) or MinGW (Windows)
- Git

**Build Steps:**

```bash
git clone https://github.com/Melton122/cnext.git
cd cnext
make
```

This will create the `cnext` executable in the project root.

### Adding to PATH

**Linux/macOS:**
```bash
export PATH=$PATH:/path/to/cnext
```

**Windows:**
Add the Cnext directory to your system PATH.

## Your First Program

Create a file called `hello.cn`:

```cnext
main {
    printin("Hello, World!")
}
```

Run it:

```bash
cnext run hello.cn
```

Output:
```
Hello, World!
```

## Basic Concepts

### Variables

```cnext
// Explicit types
int x = 42
float pi = 3.14
str name = "Cnext"
bool flag = true

// Type inference with var
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

// Call functions
var result = add(5, 3)
printin(result)  // 8
```

### Control Flow

```cnext
// If/else
if x > 10 {
    printin("big")
} else {
    printin("small")
}

// While loop
var i = 0
while i < 5 {
    printin(i)
    i = i + 1
}

// For loop
for int i = 0; i < 5; i = i + 1 {
    printin(i)
}

// For-in loop
int[] nums = {1, 2, 3, 4, 5}
for var n in nums {
    printin(n)
}
```

### Arrays

```cnext
// Array literal
int[] nums = {1, 2, 3, 4, 5}

// Access elements
printin(nums[0])  // 1

// Array length
printin(nums.length)  // 5

// Slicing
int[] slice = nums[1:3]  // {2, 3}
```

### Classes

```cnext
class Point {
    int x
    int y

    func new(int x_val, int y_val) {
        self.x = x_val
        self.y = y_val
    }

    func distance() -> int {
        return self.x * self.x + self.y * self.y
    }
}

Point p = new Point(3, 4)
printin(p.distance())  // 25
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
}
```

## Next Steps

- Read the [Language Syntax](syntax.md) guide
- Explore the [Standard Library](stdlib.md)
- Check out [Examples](../examples/)
- Read about [Classes](classes.md) and [OOP](classes.md)
- Learn about [Generics](generics.md)
- Explore [Closures](closures.md) and [Generators](generators.md)

## Getting Help

- Check the [Documentation](../docs/)
- Open an issue on GitHub
- Join the community
