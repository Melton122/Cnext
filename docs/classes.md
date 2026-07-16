# Classes

## Basic Classes

```cnext
class Point {
    int x
    int y

    func new(int x_val, int y_val) {
        self.x = x_val
        self.y = y_val
    }
}

Point p = new Point(3, 4)
printin(p.x)  // 3
printin(p.y)  // 4
```

## Methods

```cnext
class Circle {
    float radius

    func new(float r) {
        self.radius = r
    }

    func area() -> float {
        return 3.14159 * self.radius * self.radius
    }

    func circumference() -> float {
        return 2 * 3.14159 * self.radius
    }
}

Circle c = new Circle(5.0)
printin(c.area())           // 78.53975
printin(c.circumference())  // 31.4159
```

## Inheritance

```cnext
class Animal {
    str name

    func new(str n) {
        self.name = n
    }

    func speak() {
        printin("...")
    }
}

class Dog extends Animal {
    func new(str n) {
        super.new(n)
    }

    override func speak() {
        printin("Woof!")
    }
}

Dog d = new Dog("Rex")
d.speak()  // Woof!
```

## Interfaces

```cnext
interface Drawable {
    func draw()
}

class Circle implements Drawable {
    float radius

    func new(float r) {
        self.radius = r
    }

    override func draw() {
        printin("Drawing circle")
    }
}
```

## Traits

```cnext
trait Loggable {
    func log() {
        printin("Logging: " + self.to_string())
    }
}

class User implements Loggable {
    str name

    func new(str n) {
        self.name = n
    }

    func to_string() -> str {
        return "User(" + self.name + ")"
    }
}
```

## Null Safety

```cnext
class Person {
    str name

    func new(str n) {
        self.name = n
    }
}

Person p = null
bool is_null = (p == null)  // true

// Safe access
str name = p?.name ?? "Unknown"
```

## Generic Classes

```cnext
class Box<T> {
    T value

    func new(T v) {
        self.value = v
    }

    func get() -> T {
        return self.value
    }
}

var int_box = new Box<int>(42)
var str_box = new Box<str>("hello")
```

## Operator Overloading

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

    func operator==(Vec2 other) -> bool {
        return self.x == other.x && self.y == other.y
    }

    func operator[](int index) -> int {
        if index == 0 { return self.x }
        return self.y
    }
}
```

## Pattern Matching

```cnext
enum Shape {
    CIRCLE,
    RECTANGLE,
    TRIANGLE
}

func describe(Shape s) -> str {
    match s {
        CIRCLE => "Circle"
        RECTANGLE => "Rectangle"
        TRIANGLE => "Triangle"
        _ => "Unknown"
    }
}
```

## Destructuring

```cnext
var (x, y) = (1, 2)
printin(x)  // 1
printin(y)  // 2
```

## Structs

For simpler data types:

```cnext
struct Point {
    int x
    int y
}

Point p
p.x = 10
p.y = 20
printin(p.x)  // 10
```

## Enums

```cnext
enum Color {
    RED,
    GREEN,
    BLUE
}

Color c = RED
printin(c == RED)  // true
```

### Enum with Explicit Values

```cnext
enum HttpStatus {
    OK = 200,
    NOT_FOUND = 404,
    ERROR = 500
}

HttpStatus code = OK
match code {
    200 => printin("OK")
    404 => printin("Not Found")
    _ => printin("Error")
}
```
