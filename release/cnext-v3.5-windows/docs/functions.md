# Functions

## Basic Functions

```cnext
func greet(str name) {
    printin("Hello, " + name + "!")
}

greet("World")
```

## Return Values

```cnext
func add(int a, int b) -> int {
    return a + b
}

var result = add(5, 3)
printin(result)  // 8
```

## Arrow Functions

For single-expression functions:

```cnext
func multiply(int a, int b) -> int => a * b
func square(int x) -> int => x * x
```

## Default Arguments

```cnext
func greet(str name, str greeting = "Hello") {
    printin(greeting + ", " + name + "!")
}

greet("World")                    // Hello, World!
greet("Alice", greeting = "Hi")   // Hi, Alice!
```

## Named Arguments

```cnext
func create_person(str name, int age, str city = "Unknown") {
    printin(name + " is " + age + " from " + city)
}

create_person(name = "Bob", age = 25, city = "NYC")
create_person(age = 30, name = "Charlie")
```

## Variadic Functions

Accept any number of arguments:

```cnext
func sum(int... args) -> int {
    var total = 0
    for var n in args {
        total = total + n
    }
    return total
}

printin(sum(1, 2, 3))      // 6
printin(sum(1, 2, 3, 4, 5))  // 15
```

## Higher-Order Functions

Functions can be passed as arguments:

```cnext
func apply(int x, func f) -> int {
    return f(x)
}

func double(int x) -> int => x * 2
func triple(int x) -> int => x * 3

printin(apply(5, double))  // 10
printin(apply(5, triple))  // 15
```

## Closures

Functions can capture variables:

```cnext
var x = 10
var add_x = (int a) => a + x
printin(add_x(5))  // 15
```

## Generators

Functions that yield values:

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

for var x in fibonacci() {
    if x > 100 { break }
    printin(x)
}
```

## Coroutines

Functions that can be resumed:

```cnext
coroutine func counter() -> iter<int> {
    var i = 0
    while true {
        yield i
        i = i + 1
    }
}
```

## Async Functions

Asynchronous functions:

```cnext
async func fetch_data(str url) -> str {
    // Async operations
    return "data from " + url
}

async func main_async() {
    var result = await fetch_data("https://example.com")
    printin(result)
}
```

## Extension Methods

Add methods to existing types:

```cnext
extend str {
    func shout() -> str {
        return self + "!!!"
    }
}

var msg = "Hello"
printin(msg.shout())  // Hello!!!
```

## Operator Overloading

Define custom operators:

```cnext
class Vec2 {
    int x
    int y

    func new(int x_val, int y_val) {
        self.x = x_val
        self.y = y_val
    }

    func operator+(Vec2 other) -> Vec2 {
        return new Vec2(self.x + other.x, self.y + other.y)
    }
}
```
