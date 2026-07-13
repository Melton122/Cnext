# Async/Await

## Basic Async Functions

Functions that can be paused and resumed:

```cnext
async func fetch_data(str url) -> str {
    // Simulate async work
    return "data from " + url
}
```

## Using Await

Pause until an async function completes:

```cnext
async func main_async() {
    var result = await fetch_data("https://example.com")
    printin(result)
}

main {
    run_async main_async()
}
```

## Multiple Awaits

```cnext
async func step1() -> str {
    return "step1"
}

async func step2() -> str {
    return "step2"
}

async func workflow() {
    var r1 = await step1()
    var r2 = await step2()
    printin(r1 + " " + r2)
}
```

## Async with Parameters

```cnext
async func compute(int x) -> int {
    return x * 2
}

async func main_async() {
    var result = await compute(5)
    printin(result)  // 10
}
```

## Nested Async Calls

```cnext
async func inner(int x) -> int {
    return x + 1
}

async func outer(int x) -> int {
    var temp = await inner(x)
    var result = await inner(temp)
    return result
}

async func main_async() {
    var result = await outer(5)
    printin(result)  // 7
}
```

## Async Error Handling

```cnext
async func risky_operation() -> int {
    throw "Something went wrong"
}

async func safe_operation() {
    try {
        var result = await risky_operation()
    } catch (str err) {
        printin("Error: " + err)
    }
}
```

## Async Loops

```cnext
async func process_items(int[] items) {
    for var item in items {
        var result = await process(item)
        printin(result)
    }
}
```

## Async with Closures

```cnext
async func apply_async(func f, int x) -> int {
    return await f(x)
}

var double = async (int x) => x * 2
async func main_async() {
    var result = await apply_async(double, 5)
    printin(result)  // 10
}
```

## Async Generators

```cnext
async func fetch_all(str[] urls) -> iter<str> {
    for var url in urls {
        var data = await fetch(url)
        yield data
    }
}
```

## Async/Await Model

Cnext uses a synchronous cooperative model:

- `async` marks a function as asynchronous
- `await` pauses until the async function completes
- `run_async` starts the async execution

## Best Practices

1. Use async for I/O operations
2. Keep async functions small and focused
3. Handle errors with try/catch
4. Use `run_async` at the top level

## Example: Data Processing

```cnext
async func read_file(str path) -> str {
    // Simulate file reading
    return "contents of " + path
}

async func process_files(str[] paths) {
    for var path in paths {
        var content = await read_file(path)
        printin("Processed: " + path)
    }
}

main {
    str[] files = {"file1.txt", "file2.txt", "file3.txt"}
    run_async process_files(files)
}
```
