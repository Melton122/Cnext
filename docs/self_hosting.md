# Cnext Self-Hosting — Proposal & Plan

## Status 2026-11-21

The compiler bundle reaches a generated-C bootstrap fixed point on Windows.
Stage 0 builds stage 1; stage 1 builds stage 2; stage 2 emits stage 3's C.
All three generated C files are identical after normalizing line endings.
`make self-build` now checks this and fails on a mismatch, writing a `.diff`
file under `build/selfhost` for investigation. Executable byte identity is
not required: build filenames and platform metadata can differ.

Stage 1 and stage 2 each pass 25 bootstrap programs, covering functions,
recursion, arrays, classes, C strings, extern declarations, test-local objects,
defer timing/order and reference captures during normal exit, return, loop
exits, nested defers, and exception unwinding,
generator loops with nested locals, nested generic function calls, closure
aliases, capture-environment cleanup across repeated loop iterations and early
exits, returned value-capturing closures, temporary call arguments, discarded
closure results, escaping mutable scalar captures backed by heap cells, and
nested closure factories in block and arrow form, and string literals passed
to array `push`.
Both stages also reject escaping non-scalar local reference captures.
The code generator now emits extern prototypes and defers, and it treats
objects inside test wrappers as local values.

Generator `for` counters are lifted into the saved frame in both compiler
implementations, and assignment-based initializers/increments are emitted as
expressions. This fixes invalid C and preserves loop state between yields.
The self-hosted generator now collects nested-block locals and shares the
native compiler's helper placement and variable ordering. Generic prototypes
and specialization definitions also share the native compiler's order.

The wider code-generation comparison is **118/128 byte-identical**. The 10
remaining differences involve closure-heavy programs
(`tests/core/test_closures_full.cn`, `tests/core/test_lambdas.cn`,
`tests/optimization/test_opt_closure_mutate.cn`,
`tests/optimization/test_opt_minimal_closure.cn`) and programs combining closures
with other features. All 10 compile with both stages; sampled programs pass their
existing runtime expectations with stage 2, though native `-O2` runs of
`test_capture_cells.cn` currently crash intermittently during `longjmp` unwinding.
A fixed point
does not establish full language equivalence or complete semantic diagnostics.
The self-hosted driver supports native build/run commands and C linking, while
the complete development CLI remains unfinished.

Closure aliases now resolve to the original lambda signature, including alias
chains and nested lexical scopes. The native backend discards alias metadata
when its scope ends. Both backends use shared environment ownership and defer-
registered cleanup guards. Owners release their references on normal exit,
return, break/continue, and exception unwinding, after user defers can run.
Runtime probes verify allocation counts return to baseline after loop scopes,
early exits, reassignment, returned aliases, closures capturing closures, and
temporary arguments, including exception unwinding from the called function.
Direct non-scalar local-reference escapes are rejected during semantic
analysis; runtime checks cover indirect wrapper escapes and reference-capturing
replacements. Mutable scalar captures use heap-backed cells and may escape; see
[closure lifetime rules](closures.md#local-aliases-and-cleanup).

Nested lambda helpers are emitted before their enclosing helpers rather than
inside an expression in the outer body. Both compilers preserve the returned
closure signature across factory calls, distinguish it from the factory's own
signature, and retain callback values returned by arrow lambdas. Capture writes
are matched by declaration, so a shadowing local does not change an outer
read-only capture into a reference capture.

```bash
make self-build
python tests/test_bootstrap_codegen.py --stage1 build/selfhost/stage1_exe
python tests/test_bootstrap_codegen.py --stage1 build/selfhost/stage2_exe
python tools/parity_sweep.py --out build/selfhost/parity
```

On Windows, use `mingw32-make` and append `.exe` to the two compiler paths.
Add `--check` to the parity sweep to fail when any comparison differs.
CI verifies the fixed point, bootstrap programs, and build/run CLI in both stages.

## Building and running with the self-hosted compiler

```bash
build/selfhost/stage2_exe build program.cn -o program
build/selfhost/stage2_exe run program.cn -- "an argument with spaces"
build/selfhost/stage2_exe build program.cn --emit-c program.c
# Legacy bootstrap invocation still emits C:
build/selfhost/stage2_exe program.cn -o program.c
```

`build -o` names the executable. `run -o` keeps that executable; a run without
`-o` cleans up its temporary executable. The program inherits stdin/stdout and
its exit status is returned by the compiler. `build` without `-o` produces
`a.exe` on Windows or `a.out` elsewhere.

The driver supports repeatable `--c-source`, `--object`, `--include-dir`,
`--library-dir`, and `--link-lib` options. Libraries follow the objects that
consume them. Compiler and program arguments are passed as vectors without a
shell. `--cc` selects one C compiler executable, defaulting to `gcc`. Native
builds through this driver currently use `-O0`; optimization-level options
remain part of the CLI work still to be ported.

Runtime headers are located from the checkout or relative to the compiler's
installation when launched by path. Use `--runtime-dir <directory>` or set
`CNEXT_INCLUDE` when launching through `PATH` outside a checkout, or when the
compiler is moved elsewhere. Generated C and GCC intermediates use the writable
working directory; the original temporary-directory environment is restored
before running the user's program. Generated C is removed after compilation.
Source emission remains available without requiring an installed C compiler.

## Development build memory

`make self-build` compiles the generated stage-1 and stage-2 C with `-O0` by
default to reduce GCC memory use and build time. For optimized compiler
binaries, use `make self-build BOOTSTRAP_OPT=-O2`. This setting affects only
bootstrap binaries; ordinary Cnext program builds retain their existing
optimization settings.

The self-hosted code generator tracks output length separately from its text
chunks. Querying a function's insertion position no longer joins and retains
the entire generated prefix. It still joins the chunks when actual output is
needed, including closure and generator snapshots.

The sections below retain earlier milestone snapshots. Milestone A (lexer parity) is
**verified on Windows**: `src2/lexer.cn` compiles with the v10.0.4 compiler and
`tests/parity_lexer.py` reports zero diff against the C lexer over all 158
corpus files (`tests/**/*.cn`, `examples/*.cn`, `src2/*.cn`). The port needed five
fixes to reach parity: type-first parameter declaration, avoiding the reserved
word `type` as an identifier, the missing `advance()` method, `str_substring`
taking an end index (not a length), and echoing the input path like
`tools/lexdump.c` does.

## Status 2026-11-07

- **Milestone D compiles.** `python tools/build_bundle.py codegen` produces a
  6,951-line bundle; `cnext build build/selfhost/stage1_bundle.cn --emit-c ...`
  succeeds and GCC links `stage1_exe`. The last compile blockers were in the
  port: `Token` has no `length` field (a `str` carries `.length`), so
  `stmt.token.length` in `src2/codegen_gen.cn` and
  `src2/codegen_closure.cn` became `stmt.token.text.length`.
- **`cnext build --emit-c <path>` exists.** `-o` names the final executable, so
  the bootstrap previously could not obtain generated C at all: it passed
  `-o stage1_src.c` and GCC was handed an MZ executable. `make self-build` now
  uses `--emit-c` and platform-aware `stage1_exe`/`stage2_exe` names.
- **Stage 1 runs, self-compiles, and the stage-2 C now compiles.** `make
  self-build` (with `CC=gcc` from bash, not Cygwin `make`) produces
  `stage1_exe.exe` (315 KB) and `stage2_exe.exe` (283 KB). The stage-2 C used
  to fail because the port's expression codegen emitted `.` on class-typed values
  that C types as pointers, and it emitted `(CnextArrayObj)0` for empty array
  literals — a stage-0 string-interpolation bug that mistook `{0}` in a codegen
  string literal for an interpolated expression. Both were fixed in the port.
  The stage-2 binary is not yet identical to stage-1, so the bootstrap fixpoint
  is not reached.
- **Codegen parity across `tests/**/*.cn` is 0/100 byte-identical now.** The bundle
  compiles and the stage-2 C it produces also compiles, but when the stage-1
  binary personally rewrites each `tests/**/*.cn` file, none of the 100 outputs
  match the stage-0 output byte for byte. Comparing the documents directly,
  the failures fall into a small number of recurring patterns: `for` loops
  written with C-style syntax that stage 0 turns into `while` and stage 1 keeps
  as `for`; structs and enums emitted without a pre-declaration `typedef struct
  S S;`/`typedef enum E E;` that stage 0 emits; `static const` const-decls
  turned into bare initializer statements; trailing-wrapper lines missing a
  return type; `#include` module headers dropped; and literal-array and
  tuple-comprehension blocks rewritten through statement expressions that stage
  1 expands differently. That is tracked under Milestone D.
- **Build note (Windows):** the MSYS2 toolchain's Cygwin `make` cannot spawn
  children in this setup (`Cannot create temporary file in C:\WINDOWS\`), so
  use `mingw32-make` for the make targets there.
- **Remaining codegen gaps, roughly by frequency in the diffs:** string
  interpolation in literals, the `builtin_c_name` table (~20 of the 170+
  builtins), aggregate definition ordering / forward declarations, default and
  named arguments, variadics, generics, closures, generators, `defer`/`try`,
  and class method call forms.

## Status 2026-11-04

- **Milestone B (parser parity) is done.** `python tools/parser_parity.py`
  produces byte-identical AST dumps to `tools/astdump` for all 100
  `tests/**/*.cn` files. The last two diffs were a self-hosted-only copy of
  `is_pointer` in `assign_type_from_node` (the C parser leaves the nullable
  flag to semantic analysis) and a stale reference `astdump.exe` that hung;
  the harness now rebuilds it every run and times out each parser process.
- **The codegen port does not compile yet.** `tools/build_bundle.py codegen`
  reaches parse errors in `src2/codegen_closure.cn` and
  `src2/codegen_generic.cn`: the port uses C out-parameters (`int* out_type`,
  `CapturedVar** list`, `&cap_type`, `*list`) and the language has no raw
  pointers or address-of operator. Those functions need to return a small
  class or tuple instead. Several referenced classes are also still undefined
  (`CapturedVar`, `ScopeClosureVar`, `ClosureVar`, `TokenArray`, `BoolArray`,
  `SpecWork`). This is the next concrete blocker for Milestone D.
- The C compiler remains canonical; no bootstrap binary is produced yet.

## 1. Goal

The Cnext compiler compiles its own source. The milestone property is a
**fixpoint**: a stage-N compiler must produce byte-identical generated C to
stage-(N−1) for the same input; then the hand-written C can be retired.

## 1a. Blocker found 2026-09-25: language gaps that gate Milestone B

A capability probe was run against the v10.0.4 compiler to test whether the
`ASTNode` shape can be expressed in Cnext at all. **It cannot.** The AST is a
tree whose nodes each hold a dynamic array of children plus nullable
left/right/etc. references — exactly the two features that are missing.

Confirmed broken (each reproduced against a freshly built compiler):

| Construct | Example | Failure |
|---|---|---|
| Class forward references | class used before its definition | *unknown type name* in C — struct typedefs are emitted in source order. Top-level function forward references already work; see below. |

**Resolved: arrays of class instances.** Arrays whose elements are class
instances now work, as locals and as class fields:

- `CnextArrayObj` (`void** data`) was added to `include/runtime.h` for object
  element arrays, alongside `CNEXT_ARRAY_IDX_OBJ` (fetches the element) and
  `CNEXT_ARRAY_PUSH` (appends and returns the grown struct).
- `generate_node_type` maps a non-primitive element type to `CnextArrayObj`
  instead of emitting the bare type name, which had produced a struct where an
  array struct was required.
- `push(arr, v)` is now generated as `arr = CNEXT_ARRAY_PUSH(arr, v)`. It was
  previously not generated at all, so `push` failed for *every* array type, not
  just class element arrays.
- Indexing an object array yields the element as a reference, so
  `b.vals[0].v` reads through `->`.
- Array literals now heap-allocate their backing store. They were compound
  literals in automatic storage, so growing one with `realloc` was undefined
  behaviour.
- `arr = { ... }` builds a whole array value rather than a bare braced list,
  which is not an expression in C.

Covered by `tests/oop/test_class_field_arrays.cn`.

**Resolved: constructor bodies are semantically analyzed.** Assigning a brace
literal to a field from inside a constructor emitted a bare `{}`, which is not
an expression in C. The parse was always correct; the cause was that
`case AST_CLASS_DECL` routed constructors to `analyze_field_declaration`, which
only validates the declared type and never visits the body. Constructor bodies
were therefore never type-checked at all, and `self` was never bound, so the
array literal went unannotated. Simple statements like `self.v = x` happened to
work only because codegen does not need type information for a plain field
assignment. Constructors now get their own branch, and `self` is bound in their
scope; the parser records the enclosing class on the constructor node so this
works without adding a `self` parameter, which codegen already supplies itself.

**Resolved: self-referential classes and class references.** A class can now
name its own type and hold instances of it:

```
class Node { int kind; str text; Node? left; Node? right }
```

was fixed by three coordinated changes:

- `src/codegen_node.c` emits `typedef struct Node Node;` ahead of the body, so
  the struct may reference its own tag.
- `src/semantics_types.c` marks a field whose declared type is a class with
  `is_class`, and `src/codegen_node.c` then emits that field as a pointer, so a
  class-typed field is a reference rather than an embedded value. This also
  makes infinite-size types like the tree node well-formed.
- `src/semantics_expr.c` marks a member access as `is_pointer_access` when the
  field it resolves to is class-typed, so `self.left.kind` emits `->` rather
  than `.`. `x == null` is likewise a plain pointer test and no longer routes
  through the (usually nonexistent) `Node_operator_eq` overload.

Covered by `tests/oop/test_class_selfref.cn`, which asserts nullable assignment,
null checks, and recursive traversal.

**Input typing:** uncontextualized `input()` returns `str`; when assigned to an
explicitly typed variable or existing typed variable, it uses the corresponding
numeric or boolean reader. `var num: int = input()` therefore reads an integer,
while `var text = input()` reads a string. Covered by `tests/core/test_input_inference.cn`.

`docs/generics.md` documents `class List<T> { T[] items }` with
`self.items = {}` and `self.items[index]`. That example now compiles and runs;
it previously failed for two separate reasons, both fixed:

- Constructor bodies were never semantically analyzed (see above), so the
  array literal in `self.items = {}` was emitted untyped.
- `generate_node_type` switched on the *unsubstituted* element type, so `T[]`
  in a specialization was emitted as a bare `T`/`int` rather than the matching
  array struct. It now resolves the element type through the substitution map
  first.

Covered by `tests/oop/test_generic_class_field_array.cn`.

What *does* work, and is what `src2/lexer.cn` relies on: `str` plus
`str_char_at` / `str_substring`, classes with mutable fields and methods,
`new Klass(...)` construction, locals, and **local** arrays (`int[] a = {1,2,3}`,
`a[i]`, `a[i] = v`, `push(a, v)`) and array parameters. Class field arrays are
now supported too and are exercised by `tests/oop/test_class_field_arrays.cn` and
`tests/oop/test_class_selfref.cn`.

### One fix already landed

`src/parser_expr.c` had no postfix loop: the chain after an identifier was a
flat series of `if (match_token(...)) { return X; }`, so `obj.field[i]` stopped
after `field` and the caller then failed with *Expect ')' after arguments*.
Chained indexing on an identifier could not be expressed at all. The chain is
now a real loop that rebinds `node` and continues, which is what made
`obj.field[i]` parse. All 86 tests and the 112-file lexer parity pass with it.
### Consequence for the plan

The object-reference and class-array gaps listed above are now closed, so
Milestone B is no longer language-blocked. The next section covers the
forward-reference investigation and the remaining known gaps. The parser port
can begin, provided it keeps to the definition-before-use rule that the
investigation concludes is sufficient.

### Forward references: functions DO need them (corrected 2026-09-30)

An earlier draft of this section claimed top-level function forward references
already worked. **That was wrong**, and the mistake was in the test, not the
reasoning: the probes only ever called *downward* into already-defined
functions, which C accepts without help. Calling upward does not compile:

```cnext
func caller(): int { return callee() }   // error: implicit declaration of 'callee'
func callee(): int { return 7 }
```

`predeclare_global` does register every top-level function before analysis, so
*semantics* resolve the call and the Cnext compiler reports no diagnostic of its
own. The failure happens later, in codegen: functions are emitted in source
order with no C prototypes, so C never sees a declaration before first use.

So both constructs fail, for the same underlying reason and at the same layer:

| Construct used before definition | Semantics | Codegen / C |
| --- | --- | --- |
| Top-level function | resolves | **fixed** — was *implicit declaration* / *conflicting types* |
| Class or struct | resolves | forward typedef emitted; still needs the complete type inside a body |

This is the one item that genuinely gates Milestone B, because a recursive
descent parser is *defined* by mutual recursion written upward.
`src2/parser_bundle.cn` is the proof: `parse_primary` (L1883) calls
`parse_expression` (L2503), `parse_block` (L2621), `parse_statement` (L2996),
and `parse_declaration` (L3678) — all defined roughly 600–1800 lines later.
Every one of them reports `implicit declaration of function ... followed by
initialization of 'ASTNode *' from 'int'`. Reordering the functions is not a
fix: the recursion is cyclic, so no ordering satisfies it.

The smallest correct fix is to emit a C prototype for every top-level function
and a forward typedef for every aggregate *before* the definitions, keeping the
existing bodies as-is. That needs no user-facing syntax and no change to the
resolution that already works.

**Landed.** `emit_forward_declarations` in `src/codegen.c` writes, ahead of the
bodies: a `typedef struct X X;` per class and struct, `typedef enum X X;` per
plain enum (or `typedef struct X X;` per variant-carrying enum, which is emitted
as a tagged struct), the alias's own `typedef` for each type alias, and then one
prototype per top-level function via `generate_function_prototype`
(`src/codegen_emit_utils.c`), which mirrors `generate_function`'s signature.
Structs, plain enums, and variant enums are now emitted in the same tagged shape
classes already used, which is what makes the duplicate typedefs legal.

Generics and generators are excluded: generic declarations are instantiated into
specialized functions that already get prototypes alongside them, and generators
rewrite their own signature. `tests/core/test_forward_calls.cn` pins this down with
upward calls, mutual recursion, and a `main` that calls later definitions.

Effect on the port: the forward-declaration errors in `src2/parser_bundle.cn`
went from 25+ to **0**. `parse_primary` → `parse_expression` and friends now
compile.

### Still definition-before-use: aggregate *types*

A forward typedef is only enough for a prototype. Naming an aggregate inside a
function *body* still needs the complete type, so these remain
definition-before-use:

- a by-value parameter of a class or struct declared later
  (`incomplete type`),
- dereferencing a pointer to a class declared later
  (`invalid use of incomplete typedef`).

The port does not need either — `Token`, `ASTNode`, `Lexer`, and the rest are all
declared before use, and the forward *calls* return pointers to already-complete
types. Fixing this properly would mean emitting all aggregate definitions before
any function body, which is a larger change to codegen ordering than this
investigation warrants.

### Independent fixes made during this investigation

These came out of writing `tests/oop/test_parser_shape.cn` and are genuine bugs
regardless of the forward-reference question. Each has a regression test:

- Instances are zeroed before the constructor runs. Previously an unset `str`
  or array field held garbage, so `Node[]` fields crashed on `push` and
  `field == ""` was false. (`tests/oop/test_class_zero_init.cn`)
- Classes with no declared constructor now get an empty one, plain and generic.
  (`tests/oop/test_class_zero_init.cn`)
- Class returns from top-level functions *and* methods are emitted as
  `Node*`, and `var` infers the type from the initializer's class name.
  (`tests/oop/test_class_return.cn`)
- Constructor and operator parameters of class type are emitted as pointers.
  (`tests/oop/test_class_ctor_params.cn`)

`tests/oop/test_parser_shape.cn` remains useful as the parser's class/array layout
regression, but it does **not** test forward calls — its chain runs downward.

### The compiler now builds `src2/parser_bundle.cn`

`cnext.exe build src2/parser_bundle.cn -o build/selfhost/parser_selfhost.exe`
compiles clean. Getting there took the forward-declaration work above plus these
compiler fixes. None of them were forward references; every one was a place where
the compiler emitted C that disagreed with itself or with the runtime.

Fixed along the way:

- **Class-typed constructor parameters were emitted by value**, in both the plain
  and the generic-specialization path (`generate_node_type(param, false)`), and
  semantics never marked constructor or operator parameters the way it marked
  ordinary function parameters. `func new(Node n)` disagreed with its own body
  and call sites. Both paths now consult `param->is_pointer`, via a shared
  `define_parameter` in `src/semantics_analyze.c`.
  (`tests/oop/test_class_ctor_params.cn`)
- **An array parameter whose element type is a class was declared as a pointer.**
  The class check keyed off the element type, so `ASTNode[] params` became
  `CnextArrayObj*` while every caller passed the struct. `define_parameter` now
  skips `is_array`. This alone cleared 6 errors, including every
  `CNEXT_ARRAY_IDX_OBJ` expansion inside `substitute_params`.
- **Reading a `str` field did not produce a string-typed expression.** Codegen
  picks `==` vs `cnext_str_eq` from the operand types, and member access recorded
  only the field's name, so `a.text == b.text` emitted raw C `==` on two
  `CnextString` structs. Comparing against a literal had worked by accident
  because the literal side carried the type. (`tests/types/test_str_field_compare.cn`)
- **`<`, `<=`, `>`, `>=` did not exist for `str`.** Only `==`/`!=` were defined,
  so `c >= "A" && c <= "Z"` failed. Added `cnext_str_cmp` and the four
  comparators to `include/runtime.h` and routed string relationals to them. A
  single-character range check now behaves the way it does for `char`.
- **Indexing an object array recorded no expression type.** `var cur = arr[i]`
  reads the initializer's `expr_type` to infer `cur`'s type. Index expressions
  filled in `type_name` but left `expr_type` unset, so the declaration stayed
  untyped and fell back to `__auto_type`; C then resolved it to `Num*` from the
  cast, but the declaration was never marked as a class, so `cur.v` emitted
  `cur.v` on a pointer. (`tests/types/test_obj_array_element_var.cn`)
- **Assigning to an object-array element was not a modifiable lvalue.** A read
  yields the stored element pointer so a following `->field` works, and that
  pointer cannot be assigned through. Reaching the slot was not an option either:
  `CNEXT_ARRAY_IDX_OBJ` is a statement expression, and GCC does not preserve
  lvalue-ness through one, so its result cannot be addressed. Added
  `CNEXT_ARRAY_IDX_OBJ_ASSIGN`, which performs the store inside the statement
  expression. Both the expression and statement assignment paths go through
  `emit_object_array_element_assignment` so they cannot drift apart. Note the
  tempting `*(T**)CNEXT_ARRAY_IDX_OBJ(a, i)` compiles and is wrong: the macro
  already returned the element, so that dereferences the instance, not the slot,
  and silently corrupts memory rather than failing.
- **A blank `{ }` in a string literal was treated as an interpolation.** The
  scanner accepted a whitespace-only placeholder, so `{ }` generated
  `cnext_to_string()` with no argument, which does not compile. A placeholder now
  has to contain an expression. (`tests/core/test_interp_blank_placeholder.cn`)
- **Globals could not be initialized with `new`.** The allocation is a GCC
  statement expression, which is not a constant expression and is not legal at
  file scope at all. Such globals are now declared uninitialized and filled in
  from a `__attribute__((constructor))` function, which runs before `main`.

One port bug remained and was fixed in the port, not the compiler:

- `g_lexer.pos` — the ported `Lexer` names that field `index`, as the port itself
  does 300 lines later. Renamed to match.

The bundle builds and runs, so the next phase is port correctness rather than
compiler gaps: it parses `func add(int a, int b): int` but reports
`Expect variable name. at '('` on the call `add(2, 3)`, so call arguments are the
first thing to look at. `tests/parity_parser.py` becomes usable once
`--cand-cmd build/selfhost/parser_selfhost.exe` is passed.

### Known gaps left over (not blockers for the port)

- **Member access on a call result.** `obj.bump().kind` emits `.` against a
  pointer. Bind the result to a local first. Found while writing
  `tests/oop/test_class_return.cn`.
- **Generic specializations using a class type argument.** `identity<Node>(n)`
  emits its prototype before any class definition, so C reports *unknown type
  name*, and the specialization path does not add the class-return `*`.
  `T = int`/`str` works fine. `T[]` fields inside generic classes work.
- **Generic field access from outside the class.** `genericObj.field.push(v)`
  emits `T_push` because the type-parameter substitution map only exists while
  emitting the class's own methods. Pre-existing and unrelated to this work.
- **Generic self-reference.** `class Tree<T> { Tree<T> left }` emits a bare
  `Tree` and fails. The parser needs none of this; a base class works if ever
  required.
- **Null class fields are not guarded.** Reading a field through an unset class
  field segfaults rather than reporting an error. This cost some bisection
  because it looks exactly like a compiler crash.
- **The compiler can loop forever on a malformed generic function signature.**
  `func identity<T>(x: T)` is a parse error (the parameter type goes *before* the
  name: `func identity<T>(T x)`), and the error path never exits.

## 2. Why it is feasible now

- Language surface is rich: `class`/`struct`, `enum`, `generics`,
  `closures`, `macros`, `match`, `func` pointers, `extern "C"` FFI
  (docs/syntax.md), and the 10.0.2 memory work (arenas, scoped `new`,
  refcounting) is exactly what a compiler needs.
- The C core is ~12.5k lines of compiler C across 38 files, plus ~7k lines of headers; the lexer (~340 lines) is a
  self-contained module and a good first parity target.
- Harness infrastructure exists: `cnext test` (128 tests), `tests/run_tests.py`,
  `make bench`, `--verbose` generated-C dump.
- The `include/runtime.h` C runtime stays C. It is the generated-code
  harness, not part of the transpiler; every transpiled language keeps its
  runtime in the host language.

## 3. Known gaps

- **No raw pointers in the language.** The compiler relies on `char*`
  slicing, `Token*` arrays, and arena-style buffers. Today's escape hatches:
  `str_char_at`, `str_substring`, `str_find` builtins, `collections`, and
  `extern "C"` for anything lower-level (gcc spawn, `GetModuleFileNameA`,
  `readlink`). If the parser/codegen port hits ergonomic walls, a minimal
  `ptr`/buffer type may be the decisive investment.
- Performance of generated C is lower than hand-written C (structure
  copying, bounds checks). `--optimize` and arenas mitigate; `make bench`
  is the gate.

## 4. Bootstrap stages

| Stage | What | Verifier |
|---|---|---|
| 0 | Existing C compiler (unchanged) | today's green CI |
| Milestone A | `src2/lexer.cn` Cnext lexer, token-level parity vs `src/lexer.c` — **done**, 158/158 files zero-diff | `tests/parity_lexer.py` diffs token streams to zero over the corpus; run by `make parity` and in CI |
| Milestone B | Parser port (`src2/parser*.cn`) — **done**, 100/100 files zero-diff | `python tools/parser_parity.py` (`tests/parity_parser.py` + `tools/astdump.c`) |
| Milestone C | Semantics port | parity |
| Milestone D | Codegen port — compiles, runs, and stage-2 C now compiles (fixpoint + byte-parity not reached) — see Status | parity + `cnext test` = 100/100 |
| 1 | Driver/registry port → complete `src2/`; `cnext1` = C compiler compiling `src2/` | full suite |
| 2 | `cnext1` compiles `src2/*.cn` → `cnext2` | `cnext2 test` = 86/86 |
| Fixpoint | `cnext1` vs `cnext2`: byte-identical generated C, identical binaries | CI `self-host` job |
| 3 | Freeze: `src2/` canonical, `src/*.c` orphaned; CI bootstraps from `src2/` only | green matrix + bench gate |

Incremental rule: every module port lands behind a `--selfbuild` flag; the C
compiler stays canonical until the full suite passes under `cnext1`.

## 5. Migration order

1. Foundations: strings/slices, diagnostics, token structs (in Cnext)
2. Lexer (Milestone A, in progress)
3. Parser (`parser*.c` ≈ 2,200 lines — largest chunk)
4. Semantics (`semantics*.c` ≈ 1,900 lines; symbol tables on `Map<K,V>`)
5. Codegen (`codegen*.c` ≈ 3,900 lines; string builders + `func` dispatch)
6. Driver + registry (`main*.c` ≈ 2,300 lines; CLI, gcc spawn via FFI)

## 6. Rough sizing

Phases 1–3 ≈ 50–60% of total effort. Overall 2–4 focused dev-weeks assuming
no blocker from §3. Codegen ports fastest once parser/semantics exist.

## 7. Decisions needed

- (a) ~~Object-reference model for class fields~~ **Decided and implemented:**
  class-typed fields are pointers, and a class can name its own type. Both
  choices are now exercised by `tests/oop/test_class_selfref.cn`.
- (b) ~~Class-element arrays~~ **Decided and implemented:** they reuse the same
  array layout as local arrays, so `length`, indexing, `push` and literal init
  behave identically on fields and locals. This also fixed the
  `docs/generics.md` `List<T>` example.
- (c) Runtime stays C (recommended yes)
- (d) Shipping: land inside 11.x cycle as experimental (`--selfbuild`) vs.
  dedicated 12.0 release
- (e) Forward declarations: **decided — not added.** Top-level function forward
  references already work, and the parser's class graph can avoid mutual
  dependencies, so `src2/` keeps the definition-before-use rule the lexer port
  was written against. See "Forward references: investigated, not required"
  above.


## 8. Milestone A acceptance

- `tests/parity_lexer.py`: zero diff on `tests/**/*.cn`, `examples/*.cn`,
  `src2/*.cn` for:
  - token type, line, column, text
  - error tokens (unterminated string/char/comment, unexpected char)
- Runs on Windows (MSYS) and Linux (WSL clang) toolchains in CI.
