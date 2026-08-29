# Cnext Self-Hosting — Proposal & Plan

Status: scoped, awaiting go-ahead on milestones. Milestone A (lexer parity) is in progress.

## 1. Goal

The Cnext compiler compiles its own source. The milestone property is a
**fixpoint**: a stage-N compiler must produce byte-identical generated C to
stage-(N−1) for the same input; then the hand-written C can be retired.

## 2. Why it is feasible now

- Language surface is rich: `class`/`struct`, `enum`, `generics`,
  `closures`, `macros`, `match`, `func` pointers, `extern "C"` FFI
  (docs/syntax.md), and the 10.0.2 memory work (arenas, scoped `new`,
  refcounting) is exactly what a compiler needs.
- The C core is ~13k lines across 38 files; the lexer (~340 lines) is a
  self-contained module and a good first parity target.
- Harness infrastructure exists: `cnext test` (81 tests), `tests/run_tests.py`,
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
| Milestone A | `src2/lexer.cn` Cnext lexer, token-level parity vs `src/lexer.c` | `tests/parity_lexer.py` diffs token streams to zero over the corpus |
| Milestone B | Parser port (`src2/parser*.cn`) | parity vs C parser over corpus |
| Milestone C | Semantics port | parity |
| Milestone D | Codegen port | parity + `cnext test` = 81/81 |
| 1 | Driver/registry port → complete `src2/`; `cnext1` = C compiler compiling `src2/` | full suite |
| 2 | `cnext1` compiles `src2/*.cn` → `cnext2` | `cnext2 test` = 81/81 |
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

- (a) Minimal `ptr`/buffer type in Cnext vs. pure-FFI escape hatches
  (recommended: reassess at Milestone C; avoid preemptively)
- (b) Runtime stays C (recommended yes)
- (c) Shipping: land inside 11.x cycle as experimental (`--selfbuild`) vs.
  dedicated 12.0 release

## 8. Milestone A acceptance

- `tests/parity_lexer.py`: zero diff on `tests/*.cn`, `examples/*.cn`,
  `src2/*.cn` for:
  - token type, line, column, text
  - error tokens (unterminated string/char/comment, unexpected char)
- Runs on Windows (MSYS) and Linux (WSL clang) toolchains in CI.