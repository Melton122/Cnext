# Implemented language contract

This document records the checked subset used to stabilize native applications
and C libraries. It distinguishes current behavior from guarantees that still
need implementation. It is not a claim that the entire language is type-sound
or memory-safe.

## Values and references

Primitive numbers and booleans are values. Structs are values. Classes are
references to objects. Strings are pointer-and-length values whose copies share
the underlying bytes. Arrays are pointer-and-length values whose copies share
mutable element storage. See [memory rules](memory.md) for allocation and lifetime.

Record compatibility is nominal: unrelated concrete classes and structs cannot
be interchanged merely because they have the same fields. A derived class
reference may be converted to its base class. The backend emits the required
conversion after checking the relationship.

Aliases do not intentionally introduce a distinct type. Alias cycles are invalid;
alias traversal is bounded at 64 links. Full alias and generic type identity,
including aliases of composite types, remains part of type-system completion.

## Arrays

`T` and `T[]` have different shapes. Checked assignment, field, argument, and
return boundaries reject a known array where a scalar is required and vice versa.
Mutable array views require exactly matching element types. Scalar numeric
widening and class inheritance do not make `int[]` compatible with `long[]`, or
`Derived[]` compatible with `Base[]`.

A contextual array literal checks its elements against the destination element
type. An ordinary non-generic function can return a typed array:

```cnext
func readings(): int[] {
    return {10, 20}
}

func total(int[] values): int {
    return values[0] + values[1]
}

main {
    var values = readings()
    printin(total(values))
    printin(total({3, 4}))
}
```

Literal backing storage is copied to tracked heap storage, so it does not borrow
the returning function's C stack. Copies still alias that storage. Indexing is
bounds checked. `str_split` returns the canonical string-array representation.

Generic array results are rejected explicitly until specialization is supported.
Nested arrays, composite aliases, and untyped builtin results
still need systematic type resolution. Arrays returned
through suspended async/coroutine/generator frames are not supported by this increment.

## Expression grouping

Generated C preserves the parsed grouping of binary expressions. For example,
`a - (b - c)` and `a / (b * c)` retain those groupings. This rule concerns the
expression tree; it does not establish left-to-right evaluation of function
arguments or other operands with side effects. Evaluation order remains a
specification and lowering task.

## Conditional result types

Known `condition ? left : right` and `when` result branches are checked together.
They must agree on array/scalar shape. Mutable array element types remain invariant.
A typed array operand can supply context for an array literal in the other branch.
Unrelated concrete records and string/scalar mixtures are rejected before C emission.
A base-class/derived-class pair produces the base reference type; sibling-class
common-ancestor inference is not yet implemented.

Numeric branches use an existing lossless conversion direction when one exists.
Otherwise they may use `double` if both types convert to it losslessly. The backend
casts operands before selection, so the C conditional operator does not first
round an integer through `float`. Pairs without a supported lossless common type
require explicit conversion. A null branch can pair with a typed nullable reference;
two untyped null branches do not determine a result type.

`when` expressions require one explicit final `else` arm. Earlier/duplicate `else`
arms are rejected. Known branch metadata propagates through variable inference,
field access, array indexing, and function returns. Unknown builtin results and
full generic/composite-alias resolution remain outside this checked subset.

## Nullability and ownership

Strings and class references can be null. Their `?` marker documents nullable
intent; the compiler does not yet enforce non-null reference use. Nullable
primitive scalars and arrays are rejected pending a tagged optional representation.
Literal `null` is rejected for scalar and value-struct destinations at checked
declaration, assignment, argument, field, and return boundaries. Aliases are
followed when deciding whether a destination is a reference. `CStr` may be null.

`own` enforces a restricted local move: subsequent reads are rejected, direct
aliases prevent a move, and borrowed, escaped, or arena-backed values cannot move.
Transfers require supported initializer/assignment/return/call positions.
`if`/`else` joins reject later reads when any continuing path moved the binding;
returning/throwing paths are excluded. Fresh iteration-local resources can move in
loops, while values carried between iterations cannot. `switch`/`match` arms join
like `if`/`else`: a move on any arm reaching after the statement blocks later reads,
returning/throwing/continuing arms are excluded, and the no-match path counts unless
the construct is exhaustive. An arm that both moves and contains `break`/`continue`
is rejected until break-target joins are modeled. Moves in conditional expressions,
try execution, callbacks, defers, or suspended functions remain rejected. Scoped
object aliases cannot escape or be read after their scope ends.
See [the checked subset and its limits](memory.md#checked-transfers-and-scoped-references).
`gc()` is a no-op;
it never frees live tracked values. Explicit frees, scoped arenas, refcounts, and
defers have the limitations described in the memory rules. Borrowed values must
not outlive their owners. Scope escape and use-after-free checking is incomplete.

## Native boundaries

An `extern "C"` declaration must match the actual C function and target ABI.
Cnext `long` is emitted as C `long long`; it is not a portable spelling for C's
target-dependent `long`. `str` is not `char*`. Use explicit `CStr` conversion for
read-only zero-terminated text. Arrays and objects are not automatically valid
foreign representations.

Native libraries must use compatible target objects and calling conventions.
Do not unwind exceptions through foreign frames or retain a borrowed conversion
past its documented lifetime. Public layout controls, binary/writable buffers,
callbacks, generated headers, and exported-library lifecycle support remain
milestones in the production plan.

## Remaining numeric and concurrency rules

The compiler checks a set of lossless numeric assignments and requires explicit
casts for narrowing. A complete contract for overflow, shifts, division edge
cases, target type widths, atomics, data races, and thread transfer is still
required. Do not infer those guarantees from successful C compilation.

Both compiler implementations must run the contract tests. The native and
self-hosted results, optimizer differential tests, and bootstrap fixed point
are release evidence; they do not replace the remaining specification work.
