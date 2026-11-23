# Memory, ownership, and cleanup

Cnext has no garbage collector and no borrow checker. Every value belongs to
exactly one of the ownership regimes below. The regime decides who may free it,
when cleanup runs, and whether it may cross a thread boundary.

## 1. Values (strings and arrays)

`str`, `byte[]`, and object arrays are value structs: a pointer plus a length.
Copying the struct copies the pointer, not the bytes.

- A literal owns static storage. Never free it.
- A value returned by a builtin (for example `to_str`, `read_file`, or string
  concatenation) owns heap memory that is **tracked** by the runtime.
- `len(s)` reads `.length`; indexing and slicing are bounds-checked.
- Passing a value to a function does not transfer ownership. The callee may
  read it, and may keep it only if it retains it explicitly (see §4).

Tracked allocations are reclaimed by `_cnext_free_all()` at process exit, so
forgetting to free a value cannot leak past shutdown. Calling `free(s)` on a
tracked string untracks it and releases it immediately; never use `s` again.

## C string interoperability

`CStr` is a read-only C string pointer, separate from the `str` value type.
`to_cstr(text)` makes an independent, tracked, zero-terminated allocation.
Only this allocation may be released with `cstr_free`; null is a no-op.
Borrowed C pointers and already released pointers are rejected. Copies of a
`CStr` share the pointer, so releasing one invalidates every alias. Remaining
owned conversions are reclaimed by `gc()` or process cleanup.

`from_cstr(pointer)` copies the C text into a new tracked `str`. It neither
retains nor releases the original pointer. The returned string remains valid
after the original C text changes or is released. C pointers must be valid and
zero-terminated while copying; the runtime cannot validate arbitrary foreign
addresses. Null pointers throw an error.

`to_cstr` rejects embedded zero bytes and null `str` values. For binary data,
use an interface with a pointer and a separate length instead of a C string.
Do not pass `CStr` to APIs that mutate the buffer or take ownership of it.

## Closure environments

Local closure variables own references to a shared capture environment. Copies,
returned values, and captured closures retain that environment. Reassignment
releases the old reference, and scope guards release owners on normal exit,
return, break/continue, and exception unwinding. The last release untracks and
frees the environment, including references held by captured closure values.
Function arguments borrow their incoming closure for the duration of the call;
storing or returning it as a local closure value retains it.

Mutable scalar captures are stored in reference-counted heap cells, so their
lifetime follows the closure environment rather than the creating stack frame.
Environment ownership does not extend the lifetime of other referenced stack
variables or independently own the bytes behind copied string/array values.
Escaping non-scalar local reference captures and reference-capturing
replacements are rejected. See
[closure lifetime rules](closures.md#local-aliases-and-cleanup).

## 2. Arena ownership

An arena is a bulk allocator with one owner: the code that created it.

```cnext
var arena = mem_arena_create()
defer mem_arena_destroy(arena)
var scratch = mem_arena_alloc(arena, 1024)
```

- `mem_arena_alloc` returns memory owned by the arena. There is no per-pointer
  free; individual allocations die when the arena does.
- `mem_arena_free` releases the arena's blocks but keeps the arena handle alive.
  `mem_arena_destroy` releases both.
- `mem_arena_usage` reports currently live bytes.
- Values allocated from an arena must not outlive the arena.

## 3. Scoped ownership

`mem_scope_begin()` / `mem_scope_end()` push a thread-local arena. While a scope
is active, every `new` allocates from that arena instead of the tracked heap.

```cnext
mem_scope_begin()
var node = new Node()
mem_scope_end()
```

- All objects created in the scope die together when `mem_scope_end()` runs.
  There is no destructor call, so scoped objects must not hold tracked handles
  or refcount handles that require release.
- Scopes nest. `mem_scope_usage()` reports bytes in the innermost scope.
- A scoped object must not be returned or stored somewhere that outlives its
  scope.

## 4. Reference-counted ownership

Use a refcount handle when ownership is genuinely shared.

```cnext
var handle = ref_new(new Node())
ref_retain(handle)   // another owner
ref_release(handle)  // this owner is done
```

- `ref_new` starts the count at 1 and tracks both handle and object.
- `ref_retain` adds an owner; `ref_release` removes one. The last release runs
  the destructor (if any), frees the object, and invalidates the handle.
- Never use a handle after its count reaches zero.
- Retain/release on the **same handle** from two threads needs a user mutex.
  The counter itself is not atomic; the runtime's allocation lock only protects
  the tracking list.

## 5. Cleanup order: `defer`, `return`, and `throw`

`defer` runs registered cleanup in LIFO order on normal scope exit and during
exception unwinding. The runtime keeps the defer and try/catch stacks in
`_Thread_local` storage, so each thread has its own independent stack.

- A `defer` registered at the point a `try` is entered is recorded in that
  frame's `defer_mark`. A `throw` runs defers down to that mark before jumping
  to the handler.
- A `return` or `break` from a normal scope releases defers registered in that
  scope only.
- `defer` bodies must not contain `return`, `break`, `continue`, or `throw`:
  they are lowered into helper functions and cannot transfer control out of
  the deferred block.
- A defer's captured pointers must remain valid until it runs. Register the
  cleanup immediately after acquiring the resource.
- `main` frees tracked allocations at exit; do not also free the same pointer
  from a defer. Arena and refcount cleanups are safe because they are not on
  the tracked heap.

## 6. Compiler AST token ownership

Inside the compiler, an `ASTNode` token either borrows a slice of the source
buffer or owns a private copy in `owned_token_text`. The optimizer may only
install allocated text through `replace_node_token()`, which frees the old
buffer and adopts the new one. Folding replacements use `move_ast_node()` to
transfer every owned field—including the source node's token text—and then free
the now-empty source node, so no alias or shell allocation survives.

`make leaktest` enforces this invariant after each optimized compile cycle and
compares optimized heap growth against an unoptimized control pipeline. A node
that borrows another node's owned buffer fails the harness even if the AST
happens to be valid at teardown.

## 7. Thread rules summary

| State | Storage | Safe across threads? |
|---|---|---|
| Tracked allocation list | shared, spinlocked | yes |
| Global arena / pool | shared, spinlocked | yes |
| Scoped arena stack | thread-local | per thread only |
| Try/catch stack | thread-local | per thread only |
| Defer stack | thread-local | per thread only |
| Refcount handle counter | shared, **not atomic** | with user mutex only |

Objects created in one thread's scope or defer stack must not be touched by
another thread. Share data through tracked values, arenas, or `Channel`, and
synchronize mutable state with `Mutex`.
