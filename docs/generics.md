# Generics

## Generic Functions

Write functions that work with any type:

```cnext
func first<T>(T[] arr) -> T {
    return arr[0]
}

var numbers = {1, 2, 3}
var first_num = first(numbers)  // 1

var words = {"hello", "world"}
var first_word = first(words)  // "hello"
```

## Generic Classes

Create reusable data structures:

```cnext
class Box<T> {
    T value

    func new(T v) {
        self.value = v
    }

    func get() -> T {
        return self.value
    }

    func set(T v) {
        self.value = v
    }
}

var int_box = new Box<int>(42)
printin(int_box.get())  // 42

var str_box = new Box<str>("hello")
printin(str_box.get())  // "hello"
```

## Multiple Type Parameters

```cnext
class Pair<A, B> {
    A first
    B second

    func new(A a, B b) {
        self.first = a
        self.second = b
    }
}

var pair = new Pair<int, str>(1, "hello")
printin(pair.first)   // 1
printin(pair.second)  // "hello"
```

## Generic Functions with Multiple Type Parameters

```cnext
func swap<T>(T a, T b) -> (T, T) {
    return (b, a)
}

var (x, y) = swap(1, 2)
printin(x)  // 2
printin(y)  // 1
```

## Type Constraints

Generic types can be used with any type:

```cnext
func print_all<T>(T[] arr) {
    for var item in arr {
        printin(item)
    }
}

print_all({1, 2, 3})
print_all({"hello", "world"})
```

## Generic Interfaces

```cnext
interface Container<T> {
    func add(T item)
    func get(int index) -> T
    func size() -> int
}

class List<T> implements Container<T> {
    T[] items

    func new() {
        self.items = {}
    }

    override func add(T item) {
        // Add item
    }

    override func get(int index) -> T {
        return self.items[index]
    }

    override func size() -> int {
        return self.items.length
    }
}
```

## Generic Traits

```cnext
trait Serializable<T> {
    func serialize() -> str
    func deserialize(str data) -> T
}

class User implements Serializable<User> {
    str name

    func new(str n) {
        self.name = n
    }

    override func serialize() -> str {
        return '{"name": "' + self.name + '"}'
    }

    override func deserialize(str data) -> User {
        // Parse JSON and create User
        return new User("parsed")
    }
}
```

## Generic Enums

```cnext
enum Option<T> {
    Some(T),
    None
}

var some = Option<int>.Some(42)
var none = Option<int>.None
```

## Type Inference

The compiler often infers generic types:

```cnext
func identity<T>(T x) -> T {
    return x
}

// Type is inferred as int
var x = identity(42)

// Type is inferred as str
var y = identity("hello")
```

## Generic Specialization

The compiler generates specialized versions for each type used:

```cnext
func add<T>(T a, T b) -> T {
    return a + b
}

// Compiler generates:
// int add_int(int a, int b) { return a + b; }
// float add_float(float a, float b) { return a + b; }

var x = add(1, 2)      // Uses add_int
var y = add(1.5, 2.5)  // Uses add_float
```
