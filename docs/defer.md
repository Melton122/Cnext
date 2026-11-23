# Defer

`defer` schedules a block of code to run when the current block exits. It is a
Cnext statement, not a Go-style deferred call: the deferred block is lowered
into a helper and registered with the runtime.

```cnext
func returns_early(): int {
    defer printin("cleanup")
    return 7
}
```

The cleanup runs on normal scope exit and on `return`, `break`, and exception
unwinding. Cleanups are **LIFO**: the last `defer` registered in a scope runs
first.

```cnext
func example() {
    defer printin("first")
    defer printin("second")
    // prints "second" then "first" when the function exits
}
```

## Try and Throw

A `defer` registered inside a `try` block is recorded in that frame's
`defer_mark`. When `throw` unwinds the stack, defers down to that mark run
before a matching `catch` handler is entered.

```cnext
func throws_error() {
    defer printin("unwind-cleanup")
    throw "expected error"
}

main {
    try {
        throws_error()
    } catch (str err) {
        printin("caught")
    }
}
```

## Scope and Captures

- `defer` is block-scoped. Leaving any block runs the defers registered in that
  block.
- The deferred body is evaluated at run time and captures variables by
  reference. Pointers captured by a defer must stay valid until it runs.
- Register cleanup immediately after acquiring a resource so an early return
  or throw cannot skip it.

## Restrictions

A deferred body must not contain `return`, `break`, `continue`, or `throw`. The
body is lowered into a helper function and cannot transfer control out of the
deferred block. The compiler rejects these statements with a semantic error
before code generation.

Top-level `defer` is not valid: it has no enclosing function scope. The parser
rejects it, along with top-level `when` and `yield`, before code generation.

## Threading and Runtime Notes

- The defer and try/catch stacks are `_Thread_local`; each OS thread has its
  own independent cleanup stack.
- A defer freeing a pointer from the tracked heap can double-free, because
  `main` calls `_cnext_free_all()` before scope teardown. Arena and refcount
  cleanups are safe because they are not on the tracked heap.
- Defer inside generator or coroutine frames is currently untested and should
  be avoided.
- The live GNU/Clang path uses a `cleanup` attribute helper plus a runtime LIFO
  registry. MSVC now emits file-scope defer helpers and wraps blocks containing
  defers in `__try`/`__finally` so normal and exceptional exit both run the
  registry cleanup. Captured locals and parameters are passed by reference in
  explicit context structs. Nested deferred helpers are emitted before their
  callers, and shadowed locals remain separate bindings.
  `make msvc-defer-test` generates the MSVC form, compiles it
  with `cl` when Visual Studio Build Tools are installed, and runs the defer
  matrix and capture tests to verify LIFO, early returns, loop exits, nested
  defers, and throw unwinding.
- The runtime caps depth at `CNEXT_MAX_DEFER_DEPTH` (512) and exits with status
  `70` if the limit is exceeded.

See [Memory, ownership, and cleanup](memory.md) for the broader ownership and
cleanup model.
