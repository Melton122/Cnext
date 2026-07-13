# Generators

## Basic Generators

Functions that yield values lazily:

```cnext
func count_to(int n) -> iter<int> {
    var i = 1
    while i <= n {
        yield i
        i = i + 1
    }
}

for var x in count_to(5) {
    printin(x)  // 1, 2, 3, 4, 5
}
```

## Fibonacci Sequence

```cnext
func fibonacci() -> iter<int> {
    var a = 0, b = 1
    while true {
        yield a
        var temp = a + b
        a = b
        b = temp
    }
}

var sum = 0
for var x in fibonacci() {
    if x > 100 { break }
    sum = sum + x
}
printin(sum)  // Sum of fibonacci numbers <= 100
```

## Single Yield

```cnext
func always_42() -> iter<int> {
    yield 42
}

for var x in always_42() {
    printin(x)  // 42
}
```

## Conditional Yield

```cnext
func positive_only(int a, int b) -> iter<int> {
    if a > 0 {
        yield a
    }
    if b > 0 {
        yield b
    }
}

for var x in positive_only(5, -1) {
    printin(x)  // 5
}
```

## Generator with Parameters

```cnext
func range_gen(int start, int end) -> iter<int> {
    var i = start
    while i < end {
        yield i
        i = i + 1
    }
}

for var x in range_gen(3, 7) {
    printin(x)  // 3, 4, 5, 6
}
```

## Multiple Local Variables

```cnext
func fibonacci_gen(int count) -> iter<int> {
    var a = 0, b = 1
    var i = 0
    while i < count {
        yield a
        var temp = a + b
        a = b
        b = temp
        i = i + 1
    }
}

for var x in fibonacci_gen(8) {
    printin(x)  // First 8 fibonacci numbers
}
```

## Nested Generators

```cnext
func inner() -> iter<int> {
    yield 1
    yield 2
}

func outer() -> iter<int> {
    for var x in inner() {
        yield x
    }
    yield 3
}

for var x in outer() {
    printin(x)  // 1, 2, 3
}
```

## Generator Expressions

Create generators inline:

```cnext
var nums = {1, 2, 3, 4, 5}
for var x in nums {
    if x % 2 == 0 {
        printin(x)  // 2, 4
    }
}
```

## Generator State

Generators maintain their state between yields:

```cnext
func counter() -> iter<int> {
    var i = 0
    while true {
        yield i
        i = i + 1
    }
}

var gen = counter()
printin(gen.next())  // 0
printin(gen.next())  // 1
printin(gen.next())  // 2
```

## Generator Memory

Generators are memory efficient - they don't store all values:

```cnext
func infinite() -> iter<int> {
    var i = 0
    while true {
        yield i
        i = i + 1
    }
}

// Only processes values as needed
for var x in infinite() {
    if x > 1000 { break }
}
```

## Combining Generators

```cnext
func concat<T>(iter<T> a, iter<T> b) -> iter<T> {
    for var x in a {
        yield x
    }
    for var x in b {
        yield x
    }
}

var gen1 = count_to(3)
var gen2 = count_to(3)
for var x in concat(gen1, gen2) {
    printin(x)  // 1, 2, 3, 1, 2, 3
}
```
