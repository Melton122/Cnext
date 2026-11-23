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
func apply(int x, func f): int {
    return f(x)
}

func double(int x): int => x * 2
func square(int x): int => x * x

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

Variables assigned or incremented by a closure are captured by reference:

```cnext
var x = 10
var modify = () => {
    x = 20
}

modify()
printin(x)  // 20
```

Read-only captures copy the value when the closure is created. For example,
`var read = () => x` continues to read the copied value if the enclosing code
later reassigns `x`.

## Local Aliases and Cleanup

Local aliases keep the original closure's call signature, including through
multiple aliases and nested scopes:

```cnext
int base = 10
var add = (int value) => base + value
var alias = add
printin(alias(2))  // 12
```

Closure variables share a reference-counted capture environment. Creating an
alias retains it; releasing one variable leaves other copies valid. Returning
a value-capturing closure keeps its environment alive after the creating
function returns. Reassignment retains the replacement before releasing the
previous value, including self-assignment. Captured closure values retain
their own environments too.

Local owners release their references on normal block exit, `return`, `break`,
`continue`, and exception unwinding. Cleanup uses the defer registry, so a
deferred callback runs while its closure is still alive. The environment is
untracked and freed after its last owner releases it.

Temporary closure arguments release their references after the call, including
when the called function throws. Discarded closure results also release their
environments without requiring a local variable.

Mutable scalar captures (integers, floats, booleans, and characters) use
reference-counted heap cells, so a closure that mutates such a local can escape
its creating scope safely. The cell outlives the scope while the closure owns a
reference to it.

This applies to scalar locals and parameters in ordinary functions. Generator
frames and variables declared in loop headers retain the previous reference
lifetime restrictions. Read-only captures still copy a snapshot, even when
another closure mutates the same variable through a cell.

Non-scalar locals such as strings, arrays, classes, and pointers still require
their storage to remain alive. Returning a closure that mutates one of those
locals is rejected. Runtime checks also reject indirect reference escapes
through wrapper closures and reference-capturing replacements in closure
assignments. Keep those closures in their creating scope.

Value captures follow the captured type's existing rules. In particular,
copying a `str` copies its pointer and length; closure ownership does not make
an independent copy of that string's bytes. Do not explicitly free backing
storage while a capture still uses it. These checks do not constitute a
general borrow checker for arbitrary object or collection storage.

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
func apply(func f, int x): int {
    return f(x)
}

var double = (int x) => x * 2
var result = apply(double, 5)
printin(result)  // 10
```

## Returning Closures

Functions can return closures:

```cnext
func make_adder(int x): func {
    return (int a) => a + x
}

var add5 = make_adder(5)
var add10 = make_adder(10)

printin(add5(3))   // 8
printin(add10(3))  // 13
```

## Nested Closure Factories

A closure can create and return another closure. Mutable scalar captures stay
shared across those closures, even after the original function returns:

```cnext
func make_builder(int total): func {
    return (int start) => {
        total = total + start
        return (int step) => {
            total = total + step
            return total
        }
    }
}

main {
    var builder = make_builder(10)
    var first = builder(1)
    var second = builder(2)
    printin(first(3))   // 16
    printin(second(4))  // 20
}
```

Arrow-form factories are also supported. Returning an existing callback retains
its environment, allowing that callback to outlive the factory closure.

## Closure Memory

This counter keeps its reference capture in the variable's scope:

```cnext
main {
    int count = 0
    var counter = () => {
        count = count + 1
        return count
    }
    printin(counter())  // 1
    printin(counter())  // 2
}
```

Returning this mutable counter from a factory is supported: its `int` capture
lives in a heap cell. Returning a closure that mutates a non-scalar local is
still rejected. The value-capturing `make_adder` example above is also
supported. Existing opaque `func` parameters retain their `int(int)` call
convention; this work does not introduce a new syntax for fully typed function
parameters.
