# Cnext Self-Hosting — Proposal & Plan

Status: scoped, awaiting go-ahead on milestones. Milestone A (lexer parity) is
**verified on Windows**: `src2/lexer.cn` compiles with the v10.0.4 compiler and
`tests/parity_lexer.py` reports zero diff against the C lexer over all 112
corpus files (`tests/*.cn`, `examples/*.cn`, `src2/*.cn`). The port needed five
fixes to reach parity: type-first parameter declaration, avoiding the reserved
word `type` as an identifier, the missing `advance()` method, `str_substring`
taking an end index (not a length), and echoing the input path like
`tools/lexdump.c` does.

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

Covered by `tests/test_class_field_arrays.cn`.

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

Covered by `tests/test_class_selfref.cn`, which asserts nullable assignment,
null checks, and recursive traversal.

**Input typing:** uncontextualized `input()` returns `str`; when assigned to an
explicitly typed variable or existing typed variable, it uses the corresponding
numeric or boolean reader. `var num: int = input()` therefore reads an integer,
while `var text = input()` reads a string. Covered by `tests/test_input_inference.cn`.

`docs/generics.md` documents `class List<T> { T[] items }` with
`self.items = {}` and `self.items[index]`. That example now compiles and runs;
it previously failed for two separate reasons, both fixed:

- Constructor bodies were never semantically analyzed (see above), so the
  array literal in `self.items = {}` was emitted untyped.
- `generate_node_type` switched on the *unsubstituted* element type, so `T[]`
  in a specialization was emitted as a bare `T`/`int` rather than the matching
  array struct. It now resolves the element type through the substitution map
  first.

Covered by `tests/test_generic_class_field_array.cn`.

What *does* work, and is what `src2/lexer.cn` relies on: `str` plus
`str_char_at` / `str_substring`, classes with mutable fields and methods,
`new Klass(...)` construction, locals, and **local** arrays (`int[] a = {1,2,3}`,
`a[i]`, `a[i] = v`, `push(a, v)`) and array parameters. Class field arrays are
now supported too and are exercised by `tests/test_class_field_arrays.cn` and
`tests/test_class_selfref.cn`.

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
rewrite their own signature. `tests/test_forward_calls.cn` pins this down with
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

These came out of writing `tests/test_parser_shape.cn` and are genuine bugs
regardless of the forward-reference question. Each has a regression test:

- Instances are zeroed before the constructor runs. Previously an unset `str`
  or array field held garbage, so `Node[]` fields crashed on `push` and
  `field == ""` was false. (`tests/test_class_zero_init.cn`)
- Classes with no declared constructor now get an empty one, plain and generic.
  (`tests/test_class_zero_init.cn`)
- Class returns from top-level functions *and* methods are emitted as
  `Node*`, and `var` infers the type from the initializer's class name.
  (`tests/test_class_return.cn`)
- Constructor and operator parameters of class type are emitted as pointers.
  (`tests/test_class_ctor_params.cn`)

`tests/test_parser_shape.cn` remains useful as the parser's class/array layout
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
  (`tests/test_class_ctor_params.cn`)
- **An array parameter whose element type is a class was declared as a pointer.**
  The class check keyed off the element type, so `ASTNode[] params` became
  `CnextArrayObj*` while every caller passed the struct. `define_parameter` now
  skips `is_array`. This alone cleared 6 errors, including every
  `CNEXT_ARRAY_IDX_OBJ` expansion inside `substitute_params`.
- **Reading a `str` field did not produce a string-typed expression.** Codegen
  picks `==` vs `cnext_str_eq` from the operand types, and member access recorded
  only the field's name, so `a.text == b.text` emitted raw C `==` on two
  `CnextString` structs. Comparing against a literal had worked by accident
  because the literal side carried the type. (`tests/test_str_field_compare.cn`)
- **`<`, `<=`, `>`, `>=` did not exist for `str`.** Only `==`/`!=` were defined,
  so `c >= "A" && c <= "Z"` failed. Added `cnext_str_cmp` and the four
  comparators to `include/runtime.h` and routed string relationals to them. A
  single-character range check now behaves the way it does for `char`.
- **Indexing an object array recorded no expression type.** `var cur = arr[i]`
  reads the initializer's `expr_type` to infer `cur`'s type. Index expressions
  filled in `type_name` but left `expr_type` unset, so the declaration stayed
  untyped and fell back to `__auto_type`; C then resolved it to `Num*` from the
  cast, but the declaration was never marked as a class, so `cur.v` emitted
  `cur.v` on a pointer. (`tests/test_obj_array_element_var.cn`)
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
  has to contain an expression. (`tests/test_interp_blank_placeholder.cn`)
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
  `tests/test_class_return.cn`.
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
- Harness infrastructure exists: `cnext test` (86 tests), `tests/run_tests.py`,
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
| Milestone A | `src2/lexer.cn` Cnext lexer, token-level parity vs `src/lexer.c` — **done**, 112/112 files zero-diff | `tests/parity_lexer.py` diffs token streams to zero over the corpus; run by `make parity` and in CI |
| Milestone B | Parser port (`src2/parser*.cn`) — **blocked on §1a**, needs object-ref + class-array support first | parity vs C parser over corpus (`tests/parity_parser.py` + `tools/astdump.c` already exist) |
| Milestone C | Semantics port | parity |
| Milestone D | Codegen port | parity + `cnext test` = 86/86 |
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
  choices are now exercised by `tests/test_class_selfref.cn`.
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

- `tests/parity_lexer.py`: zero diff on `tests/*.cn`, `examples/*.cn`,
  `src2/*.cn` for:
  - token type, line, column, text
  - error tokens (unterminated string/char/comment, unexpected char)
- Runs on Windows (MSYS) and Linux (WSL clang) toolchains in CI.
