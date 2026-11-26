# Cnext production plan

The intended direction is one language for application code and systems components,
with explicit C boundaries. Keep the C backend while strengthening the language
contracts above it. Cnext is not yet a memory-safe or fully specified systems language.

Target order: native applications and C libraries first, followed by an embedded
profile. Both are intended targets; embedded support must be explicit about which
runtime facilities it requires or omits.

## Stabilization changes

The current stabilization work adds:

- Rejection of unrelated concrete classes/structs at initialization, assignment,
  argument, field, and return boundaries, including named arguments. Verified
  derived-to-base conversions emit explicit C casts.
- A diagnostic for unsupported primitive nullable types and nullable arrays,
  instead of silently dropping `?`. Strings and class references remain nullable;
  non-null reference enforcement is still pending.
- Real MD5, SHA-1, and SHA-256 global builtins. MD5 and SHA-256 share the module
  implementations; digest tests compare against independent known results at
  padding and block boundaries. These names no longer refer to 32-bit toy hashes.
- A harmless `gc()` compatibility hint. There is no tracing collector. Explicit
  resource cleanup and arenas remain necessary for bounded runtime memory use.
- Real package refresh through `update`, nonzero exits for manifest installation
  failures, malformed dependency diagnostics, and fresh scoped-package installation.
- Contract tests for the native and self-hosted compilers in CI.

These changes do not establish full type soundness. Generic specialization,
array shape compatibility, conditional-expression joins, and all foreign-boundary
signatures still need systematic checks. Ownership checking remains a restricted subset.

The next type-system increment checks array/scalar shape and invariant element
types at initialization, assignment, argument, and return boundaries. Ordinary
functions preserve typed array results, including heap-backed literal results.
String splitting uses the same string-array runtime type as `str[]`. Binary
expressions preserve their AST grouping, and cyclic aliases receive bounded
diagnostics. See [the implemented language contract](language-contract.md).

The memory/branch-typing increment reclaims oversized arena and pool allocations,
aligns allocations for the target's ordinary C types, and checks size/accounting
overflow and allocation-failure cleanup. Allocator tests observe actual allocation
and free calls. Conditional branches now compute a common checked result type,
preserve numeric widening before selection, and reject incompatible known branches.
`when` requires an explicit final `else`.

The ownership increment checks straight-line `own` transfers, use after move,
direct local aliases, and lexical arena reference lifetimes in both compilers.
Escapes through calls, captures, fields, globals, returns, and arrays are rejected
for scoped objects. Deferred scope cleanup covers explicit early exits. `if`/`else`
now merges ownership across continuing paths, loops allow fresh iteration-local
moves while rejecting moves of loop-carried values, and `switch`/`match` arms join
across continuing paths (an arm that both moves and contains `break`/`continue` is
rejected until break-target joins are modeled). Full loop ownership proofs,
exception joins, explicit-release tracking, foreign/container alias analysis, and
thread/interprocedural ownership remain pending. See
[memory rules](memory.md#checked-transfers-and-scoped-references).

## 1. Define the core language contract

Write normative rules, with positive and negative executable examples, for:

- Exact numeric widths and signedness; literal typing; widening, narrowing, casts,
  overflow, division, and evaluation order. Make C backend behavior match those rules.
- Classes versus value structs, aliases versus distinct types, generic type identity,
  trait conformance, inheritance, and the visibility of imported declarations.
- A tagged optional representation for scalar and array values. Missing and present
  zero/false/empty values must remain distinct. Specify assignment, calls, returns,
  comparisons, and unwrapping before accepting their syntax.
- Errors and cleanup on every exit path. Choose how typed recoverable errors interact
  with exceptions, suspended generators, threads, and foreign code.
- Ownership of strings, arrays, objects, closures, arena values, and foreign handles.
  Specify aliasing and escape checks before describing `own` as an enforced move.

Exit gate: both compilers reject the same invalid programs, run the same valid
programs, and preserve behavior with optimization enabled and disabled.

## 2. Make the C ABI a supported product interface

Use fixed-width scalars, explicit pointer/length buffers, opaque handles, and status
returns at public boundaries. Keep Cnext runtime structs private unless explicitly
declared ABI-stable. `CStr` is read-only; writable and binary buffers need separate types.

Implement and specify:

- Typed raw pointers, constness, nullability, borrowed buffers, and explicit unsafe
  operations. Foreign code must document retention and release responsibilities.
- Target C types such as `size_t`, alignment, struct layout, and calling conventions.
  Verify declarations against C headers rather than maintaining unchecked duplicates.
- Callbacks with context pointers and explicit lifetimes. Exceptions must not unwind
  through foreign frames; translate them to the declared error protocol.
- Static/shared library outputs, exported symbols, generated C headers, and controlled
  runtime initialization/shutdown for embedding.

Exit gate: a C library called by Cnext, a C host calling an exported Cnext library,
callback round trips, and ownership/error tests on Windows, Linux, and macOS.
Use this same ABI for other languages' C FFI; C++ can use a C wrapper.

## 3. Provide predictable systems facilities

Add allocator selection and explicit resource ownership before extending pointer
syntax. Specify atomics, data races, thread transfer, and shared ownership. Existing
refcount handles require external synchronization.

Introduce a documented runtime profile with explicit capabilities and dependencies.
A future constrained/freestanding profile must avoid implicit heap allocation,
exceptions, threads, and host I/O when those facilities are disabled. Test object
size, alignment, allocation failure, cleanup, and generated-code portability.

## 4. Finish reproducible packages and native builds

The current install path uses URLs or a local name-to-URL registry. A separate semver
resolver exists, but manifest installation does not yet enforce version constraints.
`update` refreshes declared sources; it is not a version solver. Do not treat a
successful download as proof that a requested version was selected.

Implement a dependency graph with exact resolved versions and a committed lockfile
containing source identities and content digests. Add conflict reporting, transitive
dependencies, cache integrity checks, and frozen/offline installation.

Define declarative native build inputs in the manifest: C sources, headers, include
directories, libraries, and per-target settings. Preserve library order and track all
inputs in build-cache keys. Prefer declarative settings to executable install hooks.

Implemented increment: `[native]` plus host Windows/Linux/macOS sections declare
C sources, objects/archives, include directories, library directories, and ordered
libraries. Both drivers share the parser and merge manifest settings with CLI
flags. See [native builds](native-builds.md). Dependency locking, cross-target
profiles, transitive native package graphs, and cache keys remain pending.

Exit gate: a mixed C/Cnext package installs and builds from a lockfile on all supported
platforms, works offline from a verified cache, and fails clearly for conflicts,
missing inputs, or modified artifacts.

## Release requirements

The self-hosted driver supports build/run/C emission, declarative native linking,
an opt-in conservative AST optimizer, and a recursive test command. Both compiler
stages run dedicated rewrite, behavior, ownership, and CLI contracts in CI.
Cross-compiler parity tests include normal build/emission cleanup as well as
program execution; this exposed and fixed duplicate tracking of grown string-split
buffers. Array growth now preserves existing allocation-ledger entries when
reallocation moves a buffer. See [self-hosting status](selfhosting.md) for exact
coverage and the remaining package/tooling gaps.

Require native/self-hosted contract parity, bootstrap fixed-point verification,
optimizer differential tests, invalid-input tests, diagnostics tests, and mixed-language
integration tests. Add supported sanitizer runs and long-running allocation tests.
Publish the tested compiler/runtime/target matrix and an explicit compatibility policy.
Historical feature completion or a passing happy-path suite is not a production guarantee.
