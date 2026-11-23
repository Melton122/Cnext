# Cnext v11 — Audit & Stabilization Roadmap

> Status: **Phases 1–11 first pass done locally — optimizer, process execution, defer, diagnostics, and a first type-consistency sweep**. Native suite **109 passed, 0 failed**, parser parity **109/109**, `make leaktest` passes with matched optimizer/control heap slopes, and `make self-build` reaches a fixed point. Self-hosted closure capture-cell emission is now covered by stage 1/2 bootstrap tests. `process_spawn(argv)` runs a program with no shell, and `docs/process-safety.md` describes the injectable vs argv-safe APIs.
> Head: `1386554` (v10.0.4). **The working tree contains uncommitted WIP** (test-runner refactor, optimizer ownership fixes, closure capture cells, `os.h` NUL-termination fix) — see §“Uncommitted Working-Tree Changes”.

---

## 1. Purpose

Make Cnext a stable, reliable, secure, well-tested production language **before** AI/domain expansion. Priority ladder: Correctness → Memory safety → Security → Consistency → Reliability → Testing → DX → Performance → New features.

This document is the audit+plan. It classifies the state of every major subsystem (implemented/tested/buggy/partial/missing), records confirmed defects with file:line evidence, and drives what each phase must touch. It must describe what Cnext **actually** does — no aspirational claims.

---

## 2. Repository topology

| Area | Contents |
|---|---|
| `src/` (C, ~600 KB) | Hand-written compiler: lexer, hand-rolled recursive-descent parser, AST, semantics, codegen, optimizer, formatter, linter, REPL, docgen, registry (package manager), semver, sourcemaps, module resolver. Compiles to C. |
| `include/` (C headers, header-only stdlib) | `runtime.h` (78 KB core runtime + string/mem/refcount), `json.h`, `net.h`, `collections.h`, `os.h`, `http.h` (WinHTTP/libcurl), `crypto.h`, `regex.h`, `csv.h`, `string_utils.h`, `encoding.h`, `fmt.h`, `cnext_time.h`, `path.h`, `cnext_process.h`, `sort.h`, `buffer.h`, `log.h`, `math.h`, `args.h`, `uuid.h`, `thread.h`, `random.h`, `checked_alloc.h`, `file.h`, `cnext_io.h`, `semver.h`, … Highly capable, header-inline, mostly living in ONE TU (per-generated-file). |
| `tests/` | ~90 flat `.cn` integration programs (`// EXPECT:` protocol), `run_tests.py` Python harness, `test_lsp.py`. |
| `examples/` | 24 runnable programs. |
| `docs/` | ~24 Markdown docs incl. 75 KB beginner book; **no package-manager or defer docs**. |
| `lsp/` `vscode-extension/` | Python LSP (`server.py`), VS Code extension (v10.0.3 VSIX). |
| Build | `Makefile` (gcc/clang, `-std=gnu11`); CI `ci.yml` (3-OS) + `release.yml`. |
| Hygiene | **Build artifacts `.o`/`.d` are committed in `src/`**; stray `_test_*.exe`/`_cnext_test_*` files accumulate in `tests/`; `git status` shows `test.cn`, `tools/`, `_guide_frag/` untracked. |

---

## 3. Uncommitted Working-Tree Changes

These exist on top of `1386554` and must be reconciled before/while Phase 1:

- `src/main.c` (+219): test runner refactored into `run_tests()`; adds `cnext test [filter|file.cn] [--list]`, **in-order** EXPECT verification (`output_has_expectations_in_order`, `main.c:385`). **Breaks 3 legacy tests whose EXPECT lists assumed unordered matching; fixed in this audit** (see §6.1).
- `src/optimizer.c`, `src/ast.h`, `src/ast.c`: optimizer token-text ownership converted in Phase 1 — `ASTNode::owned_token_text`, `make_int_literal`/`make_float_literal` own their text, `replace_node_token()`, `move_ast_node()`, and `make_owned_identifier()`. Regression harness: `tools/optimizer_leak_harness.c` (`make leaktest`); details in §6.2.
- `include/codegen_internal.h`, `src/codegen_closure.c`, `src/codegen_expr.c`, `src2/codegen_closure.cn`: mutable scalar captures use heap-backed capture cells, so closures can escape their creating scope safely. Non-scalar reference escapes remain rejected; native and self-hosted backends now match.
- `include/os.h`: `os_exec`/`os_exec_output` now copy-and-NUL-terminate `CnextString` commands (fixes shell misuse of non-NUL-terminated slices).
- `src/main_utils.c`: `run_process` re-serializes its argv with `append_quoted_arg` before `_spawnvp`, so the Win32 spawn path quotes exactly like `run_process_captured` does (`include/cnext_process.h` got the same treatment for `process_spawn`).
- Docs: `README.md`, `docs/stdlib.md`, guide HTML modified; `docs/cnext-beginner-book.md`, `docs/self_hosting.md` untracked new.

**Decision needed:** commit these as roll-up (with regression coverage) once Phase 1 proves them, or split. Do not discard — they are load-bearing for v11.

---

## 4. Phase-by-phase audit (current state)

### 4.1 Lexer — Implemented + tested (indirect)
`src/lexer.c` (10.7 KB): keyword table (`defer` at `lexer.c:141`), tokens incl. v10 keywords, range `..`/`..=`, string interpolation lexing. No dedicated lexer unit tests (covered via integration). Phase 8/17 should add negative lexer cases. `TOKEN_*` enum is large but consistent.

### 4.2 Parser — Implemented + under-tested
Hand-written recursive descent (`parser_*.c`). Supports classes, traits, interfaces, generics, closures, coroutines, async/await, macros/attributes, constexpr, match+guard, `when`, union types, variants, `extern "C"`, benches, `own`, option/result/try-`?`, ranges, defer.

**Fixed in Phase 5:** the top-level reject-list now includes **`AST_DEFER`**, **`AST_WHEN`**, and **`AST_YIELD`** in both parsers, so `defer`/`when`/`yield` at top level fail before code generation. Negative tests cover top-level `defer`.

### 4.3 AST — Implemented
`include/ast.h`/`src/ast.c`: full node family; `free_ast` owns `children`, `type_params`, `type_args`, `left/right/condition/init/increment/default_value`, `type_name` etc., and `owned_token_text`. **Ownership rule enforced in Phase 1:** a node’s `token.start` either borrows the source buffer (lexer, non-owning) or points into `node->owned_token_text` (owning); optimizer-allocated text is adopted once by `replace_node_token()`, and whole-node replacements transfer ownership with `move_ast_node()`. The dead `AST_GUARD` enum entry was removed from both AST node tables in Phase 8.

### 4.4 Semantic analysis — Implemented + under-tested
`semantics_*.c`: scopes, undeclared-id, type checking, classes/inheritance/interfaces/traits, generics, nullability, union/variant, constexpr const-checking. `analyze_semantics` has `has_semantic_error` global (resettable). No type-system unit tests. **Fixed in Phase 5:** both native and self-hosted semantics reject `return`/`break`/`continue`/`throw` inside deferred bodies with `ERR_SEM_DEFER_CONTROL` before code generation; nested lambdas reset the defer context so only direct control transfers are rejected.

### 4.5 Defer — Hardened in Phases 5–7
Ownership and cleanup rules are documented in `docs/memory.md`, with defer-specific behavior in `docs/defer.md`. Confirmed findings (`include/runtime.h:412-499`, `src/codegen_node.c:786-824`):
- Lowered via GNU `cleanup` attribute nested function + runtime LIFO registry. **LIFO** on both normal exit and throw-unwind. Correct on the live path (MinGW/clang `-std=gnu11`).
- **MSVC path rewired:** deferred bodies are emitted as file-scope helpers with reference-capture contexts, registered at runtime, and blocks containing direct defers are wrapped in `__try`/`__finally` with `_cnext_defer_run_to()`. Nested helpers precede their callers. `make msvc-defer-test` checks the generated form and compiles/runs the matrix and capture fixtures with `cl` when available.
- **Invalid defer use is rejected:** top-level `defer`, `when`, and `yield` fail parsing; `return`/`break`/`continue`/`throw` inside a deferred body fail semantic analysis.
- **Thread-local stacks (fixed)**: `_cnext_defer_stack/_count/_unwinding` and `_cnext_try_stack` are declared `_Thread_local` (`runtime.h:432-435`), so defer/try/throw across OS threads no longer corrupt each other's marks. Refcount handle counters are still non-atomic and need a user mutex. See `docs/memory.md`.
- **Generator/coroutine frames + defer** untested and hazardous (global registry + retained `void*` into reused frames).
- Defer is **block-scoped** (unlike Go), body evaluated at run time (capture-by-reference).
- `main` emits `_cnext_free_all()` *before* scope teardown → a defer freeing tracked memory double-frees (`codegen_node.c:515-549`).
- Top-of-function depth cap `CNEXT_MAX_DEFER_DEPTH 512` → `exit(70)`.
- `defer_counter` is now reset in `reset_codegen_state()`, so REPL/LSP sessions no longer drift.
- Tests now include `tests/test_defer.cn`, `tests/test_defer_matrix.cn` (LIFO, nested blocks, loop scopes, throw unwinding), bootstrap defer matrix coverage, and negative cases for top-level `defer` and all four control transfers.

### 4.6 Optimizer — Implemented, memory ownership stabilized → Phase 14 still open
`src/optimizer.c`: constant propagation, copy propagation, identity elimination, peephole, constant folding, DCE, branch simplification, loop strength reduction. Enabled via `--optimize` (default off; run by `compile_file` only when `run_optimizer`). **REPL does not run the optimizer.**
Ownership defects in §6.2 are fixed and guarded by `make leaktest`; output-semantics audit remains a Phase 14 item.

### 4.7 Codegen — Implemented, large surface
`codegen_expr.c` (55.7 KB, **128-entry builtin dispatch**) + `codegen_node.c` (49 KB). Debug-line emission (`codegen_set_debug_source`), main synthesis runs tests, `_cnext_free_all()` then return. GCC driver invokes `gcc -std=gnu11` (argv-array, deliberate no-shell per `main.c:572`).

### 4.8 Formatter / Linter — Implemented + basic
`formatter.c` (6.4 KB), `linter.c` (11.8 KB). No `defer`/`when` awareness (round-trips generically). Formatter tested via 4 inline snippets in `run_tests.py`; linter via 3. Phase 8 should verify both survive new syntax.

### 4.9 REPL — Implemented
`repl.c` (7.2 KB): bare-expression wrapping + `validate_repl_expr_ast` structural guard. No optimizer. Frees AST per iteration (good pattern to copy in the leak harness).

### 4.10 Diagnostics — Implemented + actionable
Parser, semantic, codegen, and backend diagnostics now use stable IDs (`E001`–`E018`, `E100`–`E120`, `E200`–`E202`) and a common `error [Exxx] [line:col]: message` shape with source excerpts, `= expected:`, `= found:`, `= help:`, `= note:`, and `= rule:` continuations where relevant. GCC output is captured and normalized through the same emitter; `tests/test_diagnostics.py` and `tests/test_native_linking.py` guard the format. CLI/linter/REPL messages outside the compiler pipeline remain plain text.

### 4.11 Standard library — Implemented, broad, under-tested → Phase 12
Header-inline modules: io, math, json, net, os, file, string_utils, time, regex, csv, collections, crypto, path, encoding, process, random, thread, sort, buffer, log, fmt, args, uuid, math, http. Runtime builtins (`runtime.h:2056-2074`): assert/panic/exit. **Shell APIs are string-based and unsafe with untrusted input** (§6.3). No `process.spawn(argv)` direct API yet → Phase 3.

### 4.12 Builtins — Implemented (128 encoded) → Phase 13
`codegen_expr.c:33-…` dispatch table encodes 128+ mappings; total global functions exceed that once language-level builtins (string ops, math, time, file, os, collections) are counted. `test_comprehensive_builtins.cn` exists but coverage is uneven; needs per-builtin audit (esp. error behavior + memory ownership + Windows/POSIX).

### 4.13 Package manager — Implemented + security gaps → Phase 20
`registry.c` (23.7 KB) + `main_packages.c` (11.8 KB) + `semver.c`. HTTPS-only enforcement, local `registry.txt`, cache at `%USERPROFILE%\.cnext\registry` (Win) / `~/.cnext/registry`, `cnext.toml` manifest, `Name = URL` registry format, `std/http@^1.2.3` imports via `moduleresolver.c`. Semver supports single constraints (EXACT/CARET/TILDE/GTE/GT/LTE/LT/ANY) — no multi-constraint ranges. **Security findings in §6.3.**

### 4.14 LSP & VS Code extension — Implemented + hardened
Windows stdin deadlock fixed (`os.read(0,4096)`), compiler path honored via `initializationOptions`, uint/range formatting, icon theme. `subprocess` argv-list only (no shell). Pushed via v10.0.3/v10.0.4. `tests/test_lsp.py` is the regression harness (7 scenarios).

### 4.15 Testing infra — Functional, needs reorg → Phase 17
- Native `cnext test`: flat `tests/` scan (`main.c:421-434`), `// EXPECT:` substring-in-order check, per-file timeout via subprocess. **Does not recurse subdirectories.**
- `run_tests.py`: hardcoded list (~70) + negative compile tests + runtime-negative + formatter/linter + memory/thread stress. **Duplicate of compile coverage; does not verify EXPECT.** Not run on Windows CI.
- The committed `tests/_test_list.txt` and PE binary mislabeled as `tests/_test_methods_temp.c` were removed in Phase 8, and `clean` now purges those plus `_p*.c`, `_probe*`, and `out_parity/`.
- No lexer/parser/semantics/type unit tests, **no fuzzing**, **no regression/ dir**. No ASan/MSan in CI.
- Test discovery cap 512; `TestEntry[512]` stack arrays.

### 4.16 Cross-platform — Partial → Phase 16
Windows paths are first-class (`CNEXT_PATH_SEP`). The WIN32 argv→cmdline quoting breakout is fixed: `run_process`, `run_process_captured` and `process_spawn` all serialize argv with `CommandLineToArgvW` rules (§6.3). POSIX paths exercised in CI (Ubuntu/macOS); macOS `continue-on-error` for Python tests.

### 4.17 Generated C — Implemented → Phase 15
`cnext build --verbose` prints generated C; `--emit-c` **does not exist** (Phase 15 should add it). Debug info via `--debug`; sourcemap exists (`sourcemap.c`) but integration is minimal.

---

## 5. Feature status matrix

| Feature | Status |
|---|---|
| Lexer / parser / AST / codegen core | Implemented + tested (integration) |
| Type system (int..ulong, nullable, aliases, generics, classes, traits, enums, union, variants) | Implemented + under-tested (no unit tests; `test_new_types.cn`, `test_generics_full.cn` weak) |
| Coroutines / async / await / generators | Implemented + tested (`test_coroutines.cn` etc.), defer/thread interplay untested |
| Closures (by-value + new by-reference) | Implemented + tested (`test_closures_full.cn`) — by-reference WIP uncommitted |
| `match` / `when` / ranges / `?` try-expr / option-result | Implemented + under-tested; dead `AST_GUARD` removed |
| Optimizer | Implemented + memory-ownership tested (`make leaktest`); output-semantics only lightly tested |
| defer | Hardened in Phases 5–7; invalid use rejected, LIFO/throw matrix covered, MSVC compile/run verified |
| try/catch/throw | Implemented + tested | 
| Error diagnostics | Stable IDs, source excerpts, expected/found, help, and rule metadata across parser/semantics/codegen/backend |
| Standard library | Implemented + under-tested per-module |
| Builtins (170+) | Implemented + uneven coverage |
| Formatter / linter | Implemented + basic; no defer/when awareness |
| REPL / doctor / new / upgrade / cache / config / doc | Implemented |
| Package manager | Implemented + security gaps |
| LSP / VS Code ext | Implemented + hardened |
| `--emit-c`, `process_spawn(argv)` | Implemented (`--emit-c <path>`; `tests/test_process_spawn.cn`) |
| Fuzzing, ASan CI, regression dir | **Missing** |
| Performance benchmarking | `make bench` exists; no rigor (Phase 21) |

---

## 6. Confirmed defects & findings

### 6.1 Test metadata vs in-order runner (fixed in this audit)
The uncommitted runner requires EXPECT lines to appear **in order**. Three legacy tests mismatched real output:
- `test_v9.cn`, `test_v8_comprehensive.cn`: had two `// EXPECT: 255`; the first came from `printin(byte)` which emitted a **raw 0xFF byte, not text “255”** at the time → removed the bogus line, keeping the real `255` (source of `ubyte`) at its correct position. `byte` now maps to `unsigned char`, so its `255` is printed as text and both expectations are back in the list.
- `test_system_builtins.cn`: expected `1,1,1,hello_env_val`; program prints `1,1,hello_env_val,true` → corrected.
Baseline after fix: **84/84**.

### 6.2 Optimizer ownership (Phase 1 — fixed)
The audit found one leak, one double-free, and three aliasing hazards:
- `optimize_loops` strength reduction stole `cond->left` and orphaned `cond->right` before `free_ast(cond)`.
- `eliminate_identities` freed the discarded operand while `optimize_node` still freed it through its pre-pass child guards.
- `propagate_constants` aliased the const table's token text; peephole and loop strength reduction created identifier nodes whose `token.start` aliased another node's text.
- Whole-node fold replacements freed the target's fields but not the source node shell, leaving one AST allocation behind per fold.

All paths now follow the rule: optimizer-allocated text is duplicated into `owned_token_text`, ownership transfers through `replace_node_token()` / `move_ast_node()`, and replacement nodes never borrow another node's owned buffer. `tools/optimizer_leak_harness.c` runs interleaved control/optimized compile cycles, checks heap slope parity, and asserts the owned-token invariant after every optimized cycle. `make leaktest` passes with optimizer growth at or below the control baseline.

### 6.3 Process execution & package-manager security (Phases 2–4 — fixed; Phase 20 keeps the review open)
- ~~**Highest-severity (WIN32)**: the PowerShell download fallback interpolated the package name verbatim~~ — fixed. `download_package` validates the name with `is_safe_package_name` (no traversal, no `\ : ; & | < > ( ) { } * ? !`, no control characters) and the URL with `is_safe_url`, and `ps_quote` emits both operands as PowerShell single-quoted literals with `'` doubled, so neither can end the script. The fallback still exists (Phases 2–4 hardened it rather than dropping it); removing it entirely remains an option for Phase 20.
- ~~**`run_process_captured` WIN32 quoting**: args flattened with `"%s"` and no `"` escaping~~ — fixed. `append_quoted_arg` implements the `CommandLineToArgvW` rules (quote each token, double backslash runs, add one backslash before a literal quote), and `run_process` now re-serializes its vector the same way instead of letting `_spawnvp` join it.
- ~~**`is_safe_url`/dash-leading URL**: curl parsed a dash-leading URL as options~~ — fixed. All `curl`/`wget` invocations pass `--` before the URL, and `is_safe_url` still rejects a leading `-`, `"`, `'`, backtick, `$` and `\`.
- ~~No argv-based process API exists~~ — added: `process_spawn(str[] argv)` in `include/cnext_process.h` (fork/`execvp` on POSIX, quoted `CreateProcessA` on Windows). It returns the exit status, `127` when the program cannot start, `-1` for an unusable vector. Registered for the `process` module in `src/semantics_types.c` and covered by `tests/test_process_spawn.cn`. It waits for the child and does not wire stdin or detach — those stay open.
- Shell-string builtins `sys_exec/sys_shell/os_exec*/process_run*` remain shell APIs by design; `docs/process-safety.md` documents both families, the Windows quoting rules, and the `cmd.exe` re-parsing caveat.
- Compiler’s own gcc/curl/tar/test invocations are argv-based (good; `main.c:572` comment confirms deliberate hardening).

### 6.4 Diagnostics (Phase 9 — fixed)
Parser, semantics, codegen, and GCC backend failures now share one emitter. Codes are explicit instead of enum ordinals, source excerpts and carets point at the reported token, expected/found pairs cover parser and type mismatches, each code has stable help text and `CN-Exxx` rule metadata, and GCC diagnostics are parsed back into line/column/note fields. `tests/test_diagnostics.py` checks the native format; `tests/test_native_linking.py` checks normalized backend failure output.

### 6.5 Type system & language consistency (Phases 10–11 — fixed)
Assignments now check text/non-text compatibility in both compilers, so `int x = 1; x = "hello"` fails with `E101` before C generation instead of producing accidental C conversions. Numeric assignment, initializer, and return checks use a widen-only matrix: same-type and lossless widening are implicit; narrowing, signedness changes, and floating-point/integer crossings require `as`. `option(...)`, `ok(...)`, `err(...)`, `expr?`, and `when { ... }` operands reach semantic analysis in both compilers, statement-form try propagation from a typed function is rejected, and `when` expressions infer the common arm type. `for (x in ...)` bindings now infer element types from arrays, strings, ranges, `str_split`, and generator calls in both compilers. The self-hosted code generator also emits `when` expressions and statement-form `expr?` propagation. Regression cases live in `tests/run_tests.py`, `tests/test_diagnostics.py`, and `tests/test_bootstrap_codegen.py` (native and stage 2).

### 6.6 Cross-platform / hygiene (Phase 16)
- Windows quoting issue §6.3. `DEV_NULL` handled. `system()` static-command uses in `doctor`/`cache clear/status` are literal-only today (safe, fragile).
- Committed `.o`/`.d` in `src/` remain historical noise; the PE binary `tests/_test_methods_temp.c`, `tests/_test_list.txt`, stray `_test_out*.exe`, `_p*.c`, `_probe*`, and `out_parity/` are now ignored and purged by `clean`.
- No ASan/UBSan/Valgrind automation; `make coverage` runs gcov on Linux only.

---

## 7. Quality-bar checklist (current)

- [x] Existing compiler builds cleanly (this audit; `-Wall -Wextra` clean)
- [x] Existing tests pass (84/84 native; Python/LSP to re-verify in Phase 17)
- [x] Optimizer leaks fixed / ownership audited / repeated-compile tested (`make leaktest`)
- [x] Process APIs audited / shell separated / `process_spawn(argv)` added + tested (`docs/process-safety.md`)
- [x] Defer fully tested + documented
- [x] Parser/semantics/codegen/backend diagnostics hardened (`tests/test_diagnostics.py`)
- [ ] Type system, runtime, generated C, builtins, stdlib audited
- [ ] Cross-platform reviewed (WIN32 quoting fix)
- [ ] Invalid input does not crash compiler (fuzzing)
- [ ] Tooling functional (fmt/lint/repl/doctor/LSP/ext) — working, needs feature-parity checks
- [ ] Package manager security reviewed (§6.3 findings to fix)
- [ ] Docs match implementation (`docs/defer.md`, pkg-manager, memory model, process safety)
- [ ] Examples compile (`run_tests.py` covers a subset)

---

## 8. Work plan (order = priority ladder)

1. **Phase 1 — done** — optimizer ownership: applied ownership rule, fixed §6.2 leaks, and added the repeated-compile regression harness (`make leaktest`).
2. **Phases 2–4 — done** — process-execution sweep: §6.3 injection bugs fixed (PowerShell quoting + name/URL validation, WIN32 argv quoting in `run_process`/`run_process_captured`, curl/wget `--`), `process_spawn(argv)` added and tested, `docs/process-safety.md` written.
3. **Phases 5–7 — done locally** — defer: MSVC lowering rewired, invalid top-level and control-transfer cases rejected in both compilers, `docs/defer.md` updated, matrix tests added. `make msvc-defer-test` generates the MSVC form and compiles/runs the defer matrix with `cl` when Visual Studio Build Tools are installed.
4. **Phases 8–9 — done locally** — correctness cleanup removed the dead `AST_GUARD` node and committed test artifacts; diagnostics now use stable IDs, rule metadata, source excerpts, expected/found fields, help text, and normalized GCC output.
5. **Phases 10–11 — done locally** — assignment compatibility, widen-only numeric conversions, `for-in` element typing, expression-node semantic coverage, and `when`/try codegen are consistent across native and self-hosted compilers.
6. **Phases 12–13** — stdlib + builtin audit (add missing per-module tests).
7. **Phases 14–18** — optimizer semantic audit, generated C (`--emit-c`), cross-platform (WIN32 quoting), test reorg to `tests/<category>/` (+ native runner subdir support; stale artifact removal already done), fuzz for invalid input.
8. **Phases 19–20** — tooling parity (fmt/lint know defer/when), package-manager security fixes.
9. **Phases 21–22** — measure-then-optimize (`make bench` baseline vs reorg), docs that match the code.
10. **Always:** per-change Understand → Design → Implement → Test → Measure → Document; every bug fix gets a regression test; run the full suite after every phase.

---

## 9. Open decisions

- **Commit the current WIP** (runner, optimizer ownership fixes, closure capture cells, os.h) up-front, or fold into phase commits?
- Test reorg: move `.cn` files into `tests/<category>/` requires native-runner recursion (`main.c`) + `run_tests.py` list rewrite — do it in Phase 17. The stale `_test_list.txt`/`_test_methods_temp.c` artifacts were already deleted in Phase 8.
- `run_tests.py` runs only on non-Windows CI today; Phase 17 should enable it on Windows too.
- Keep the in-order EXPECT runner (set the 3 legacy files fixed already). Also consider expecting the raw byte case (`printin(byte)`) via a byte-aware EXPECT marker — out of scope for now; note it.
