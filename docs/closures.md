# Closures

## Basic Closures

Functions that capture variables from their enclosing scope:

```cnext
var x = 10
var add_x = (int a) => a + x
printin(add_x(5))  // 15
```

## Capturing Variables

Closures can capture and modify variables:

```cnext
var counter = 0
var increment = () => {
    counter = counter + 1
    return counter
}

printin(increment())  // 1
printin(increment())  // 2
printin(increment())  // 3
```

## Multiple Captures

```cnext
var x = 10
var y = 20
var add_xy = (int a) => a + x + y
printin(add_xy(5))  // 35
```

## Closures as Arguments

Pass closures to functions:

```cnext
func apply(int x, func f) -> int {
    return f(x)
}

func double(int x) -> int => x * 2
func square(int x) -> int => x * x

printin(apply(5, double))  // 10
printin(apply(5, square))  // 25
```

## Array Operations

Use closures with array methods:

```cnext
int[] nums = {1, 2, 3, 4, 5}

// Filter
int[] evens = filter(nums, (int n) => n % 2 == 0)

// Map
int[] doubled = map(nums, (int n) => n * 2)
```

## Closure Variables

Store closures in variables:

```cnext
var operations = {
    "double": (int x) => x * 2,
    "square": (int x) => x * x
}

printin(operations["double"](5))  // 10
printin(operations["square"](5))  // 25
```

## Reference Capture

Closures capture variables by reference:

```cnext
var x = 10
var modify = () => {
    x = 20
}

modify()
printin(x)  // 20
```

## Closures in Loops

Be careful with closures in loops:

```cnext
var functions = {}
for int i = 0; i < 5; i = i + 1 {
    var val = i  // Create new variable for each iteration
    functions[i] = () => val
}

printin(functions[0]())  // 0
printin(functions[1]())  // 1
```

## Non-Capturing Closures

Closures that don't capture variables are more efficient:

```cnext
var add = (int a, int b) => a + b  // No capture
printin(add(3, 4))  // 7
```

## Closure Types

Closures are first-class values:

```cnext
func apply(func f, int x) -> int {
    return f(x)
}

var double = (int x) => x * 2
var result = apply(double, 5)
printin(result)  // 10
```

## Returning Closures

Functions can return closures:

```cnext
func make_adder(int x) -> func {
    return (int a) => a + x
}

var add5 = make_adder(5)
var add10 = make_adder(10)

printin(add5(3))   // 8
printin(add10(3))  // 13
```

## Closure Memory

Closures automatically manage captured variables:

```cnext
func create_counter() -> func {
    var count = 0
    return () => {
        count = count + 1
        return count
    }
}

var counter = create_counter()
printin(counter())  // 1
printin(counter())  // 2
```
