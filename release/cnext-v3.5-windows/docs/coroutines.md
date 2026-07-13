# Coroutines

## Basic Coroutines

Functions that can be paused and resumed:

```cnext
coroutine func counter() -> iter<int> {
    var i = 0
    while true {
        yield i
        i = i + 1
    }
}
```

## Coroutine vs Generator

Coroutines are like generators but support bidirectional communication:

```cnext
// Generator: yields values
func gen() -> iter<int> {
    yield 1
    yield 2
}

// Coroutine: can receive values
coroutine func coro() -> iter<int> {
    var received = 0
    while true {
        yield received
        received = received + 1
    }
}
```

## Using Coroutines

```cnext
var co = counter()
var val1 = resume co      // 0
var val2 = resume co      // 1
var val3 = resume co      // 2
```

## Coroutine with Input

```cnext
coroutine func doubler() -> iter<int> {
    var val = 1
    while true {
        yield val
        val = val * 2
    }
}

var co = doubler()
var v1 = resume co          // 1
var v2 = resume co          // 2
var v3 = resume co          // 4
```

## Sending Values to Coroutines

```cnext
coroutine func accumulator() -> iter<int> {
    var total = 0
    while true {
        var received = yield total
        total = total + received
    }
}

var co = accumulator()
resume co with 10    // total = 10, returns 10
resume co with 20    // total = 30, returns 30
resume co with 5     // total = 35, returns 35
```

## Coroutine State

Coroutines maintain their state between calls:

```cnext
coroutine func stateful() -> iter<int> {
    var count = 0
    while true {
        count = count + 1
        yield count
    }
}

var co = stateful()
printin(resume co)  // 1
printin(resume co)  // 2
printin(resume co)  // 3
```

## Coroutine with Parameters

```cnext
coroutine func range_coro(int start, int end) -> iter<int> {
    var i = start
    while i < end {
        yield i
        i = i + 1
    }
}

var co = range_coro(3, 7)
printin(resume co)  // 3
printin(resume co)  // 4
printin(resume co)  // 5
printin(resume co)  // 6
```

## Coroutine as Iterator

Coroutines can be used with for-in loops:

```cnext
coroutine func filter_positive(int[] nums) -> iter<int> {
    for var n in nums {
        if n > 0 {
            yield n
        }
    }
}

int[] data = {1, -2, 3, -4, 5}
for var x in filter_positive(data) {
    printin(x)  // 1, 3, 5
}
```

## Nested Coroutines

```cnext
coroutine func inner() -> iter<int> {
    yield 1
    yield 2
    yield 3
}

coroutine func outer() -> iter<int> {
    for var x in inner() {
        yield x * 10
    }
}

for var x in outer() {
    printin(x)  // 10, 20, 30
}
```

## Coroutine Communication

Coroutines can communicate via yield and resume:

```cnext
coroutine func echo() -> iter<str> {
    while true {
        var msg = yield ""
        printin("Echo: " + msg)
    }
}

var co = echo()
resume co with "hello"  // Echo: hello
resume co with "world"  // Echo: world
```

## Coroutine Memory

Coroutines are memory efficient:

```cnext
coroutine func fibonacci_coro() -> iter<int> {
    var a = 0, b = 1
    while true {
        yield a
        var temp = a + b
        a = b
        b = temp
    }
}

// Only stores current state, not all values
var co = fibonacci_coro()
for int i = 0; i < 10; i = i + 1 {
    printin(resume co)
}
```
