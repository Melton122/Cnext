# Cnext v11 — Audit & Stabilization Roadmap

> Status: **Phases 1–22 complete; v10.0.5 “Baby Otter” release prepared**. Test sources now live in `tests/<category>/`; native/Python/parity tools recurse while excluding the `tests/bootstrap/` and `tests/optimizer/` fixture dirs. Phase 18 adds deterministic invalid-input fuzzing with crash/timeout/output bounds and a compile-only side-effect check. Phase 19 teaches `fmt`/`lint` about `defer`/`when`, makes formatting idempotent, and removes false literal-comparison warnings for boolean `when` arms. Phase 20 closes the package-manager and cross-platform security pass: HTTPS/name/URL validation, temp-file finalization, bounded captured output, unique registry cache files, auth-bearing login verification, shell-free CLI helpers, and hardened installer scripts. Phase 21 replaces the shell-only build benchmark with a reproducible Python harness that records median timings and baseline comparisons, then removes unused Windows headers and moves cache-tree helpers out of `main.c` to cut clean and incremental build times. Full verification is green (native 128/128, parser parity 128/128, bootstrap fixed point, codegen parity 118/128); the earlier intermittent `tests/test_native_linking.py::test_mutable_capture_cells` `-O2` crash was traced to MinGW SEH `longjmp` unwinding and fixed with `__USE_MINGW_SETJMP_NON_SEH`. Phase 13 makes the remaining canonical modules callable, adds deterministic per-module edge tests, and fixes SHA-256 padding, absolute path normalization, NULL-valued hashmap membership, binary log files, and the `fmt.join` array contract. Phase 14 fixes optimizer constant-folding/peephole semantics, adds an `--optimize` vs `--no-optimize` regression harness, and removes unsafe loop strength reduction. Phase 15 audits `--emit-c`: native/selfhost now reject `-o` consistently, raw strings are escaped in generated C, `main` returns share one cleanup path, and runtime print macros no longer shadow user locals. Phase 16 hardens cross-platform process/file behavior: downloader options precede `--`, registry/REPL use the shared argv process helper, generated C and stdlib file writes are byte-exact, and failed `--emit-c` runs remove their temp source. `process_spawn(argv)` runs a program with no shell, and `docs/process-safety.md` describes the injectable vs argv-safe APIs.
> Head: v10.0.5 “Baby Otter” release: Phase 22 docs alignment, Phase 21 reproducible benchmarking, and Phase 20 package-manager/cross-platform security hardening (full verification green).

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
| `tests/` | 128 categorized `.cn` integration programs in `tests/<category>/` (`// EXPECT:` protocol), excluded `tests/bootstrap/` and `tests/optimizer/` fixtures, `run_tests.py` Python harness, `test_lsp.py`. |
| `examples/` | 24 runnable programs. |
| `docs/` | ~24 Markdown docs incl. 75 KB beginner book; **no package-manager or defer docs**. |
| `lsp/` `vscode-extension/` | Python LSP (`server.py`), VS Code extension (v10.0.3 VSIX). |
| Build | `Makefile` (gcc/clang, `-std=gnu11`); CI `ci.yml` (3-OS) + `release.yml`. |
| Hygiene | **Build artifacts `.o`/`.d` are committed in `src/`**; stray `_test_*.exe`/`_cnext_test_*` files accumulate in `tests/`; `git status` shows `test.cn`, `tools/`, `_guide_frag/` untracked. |

---

## 3. Phase 13–17 Changes

Phases 13–15 are committed as `f3b2714 V11 audit: harden optimizer, modules, and generated C` (44 files changed, 944 insertions, 180 deletions):

- Phase 13: canonical module call prefixes, stdlib bug fixes, 12 deterministic module/edge tests, docs updates.
- Phase 14: optimizer int/float/string/shadowing/branch fixes, unsafe loop strength reduction removal, `tests/test_optimizer_semantics.py`, and six `tests/optimizer/*.cn` fixtures.
- Phase 15: `--emit-c`/`-o` rejection in both compilers, raw-string C escaping, shared `main` cleanup path, collision-resistant print temporary, `tests/types/test_raw_string_edges.cn`, and `tests/test_native_linking.py`.

Phase 16 is committed:

- `src/main_packages.c`: `curl -fsSL -o <out> -- <url>` and `wget -q -O <out> -- <url>` keep output options before the argument terminator.
- `src/registry.c`: removed the duplicate raw `run_process_registry`; tarball creation uses shared `run_process("tar", ...)`.
- `src/repl.c`: includes `main_internal.h`, uses shared `run_process`/`executable_command`, and writes its temp C with `"wb"`.
- `src/codegen.c`: final generated-C output opens with `"wb"` so emitted bytes are identical across Windows and POSIX.
- `src2/main.cn`: removed Windows CRLF pre-normalization before `driver_write`; `src2/parser_bundle.cn` regenerated.
- `include/cnext_io.h`: `write_file`/`append_file` open with `"wb"`/`"ab"` to preserve exact newline bytes.
- `src/main.c`: failed `--emit-c` target open and gcc failures remove `CNEXT_TEMP_C`.
- `Makefile`: `check` now runs `tests/test_cross_platform.py`.
- `tests/test_cross_platform.py`: guards downloader argv order, sole `_spawnvp` caller, LF-only generated C, exact newline IO writes, and temp-source cleanup.

Phase 16 verification is green: 5 targeted cross-platform tests pass; native suite 128/128, optimizer semantics 6/6, parser parity 128/128, bootstrap fixed point, and codegen parity 118/128.

Phase 17 was applied in v10.0.5:

- `tests/*.cn` moved into `tests/core/`, `tests/types/`, `tests/oop/`, `tests/control/`, `tests/collections/`, `tests/stdlib/`, `tests/io/`, `tests/optimization/`, `tests/cli/`, and `tests/version_features/`. The three formerly ignored optimizer regressions were promoted into `tests/optimization/`.
- `src/main.c`: recursive `tests/**/*.cn` discovery, hidden/fixture-dir exclusions, relative test names, basename/substring filters, and explicit-path filters.
- `tests/run_tests.py`: recursive discovery, fixture/negative-exclusion sets, and scratch artifacts under `build/tmp`; Windows CI already runs it, and `make check` no longer swallows its failures.
- `tests/parity_parser.py`, `tools/parser_parity.py`, and `tools/parity_sweep.py`: recursive corpus discovery with fixture exclusions and relative-path output names.
- `Makefile`, `test_sys.c`, `tests/test_msvc_defer_codegen.py`, `README.md`, and `CONTRIBUTING.md`: updated paths/commands for the categorized layout.
- Phase 17 verification: `cnext test --list` reports 128 entries; native suite 128/128, optimizer semantics 6/6, Python suite and LSP 7/7, formatter/linter/memory regressions all pass, parser parity 128/128, bootstrap fixed point, and codegen parity 118/128 in the final full gate. An earlier full-run rerun failed in the unchanged `tests/test_native_linking.py::test_mutable_capture_cells` `-O2` path; focused `-O0` runs pass and the latest full gate is green.

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
- `main` returns now jump to a shared cleanup label before `_cnext_free_all()` (Phase 15), so early returns use the same tracked-memory cleanup path as fallthrough.
- Top-of-function depth cap `CNEXT_MAX_DEFER_DEPTH 512` → `exit(70)`.
- `defer_counter` is now reset in `reset_codegen_state()`, so REPL/LSP sessions no longer drift.
- Tests now include `tests/control/test_defer.cn`, `tests/control/test_defer_matrix.cn` (LIFO, nested blocks, loop scopes, throw unwinding), bootstrap defer matrix coverage, and negative cases for top-level `defer` and all four control transfers.

### 4.6 Optimizer — Implemented, memory ownership stabilized + semantics audited (Phase 14)
`src/optimizer.c`: constant propagation, copy propagation, identity elimination, peephole, constant folding, DCE, and branch simplification. Enabled via `--optimize` (default off; run by `compile_file` only when `run_optimizer`). **REPL does not run the optimizer.**
Ownership defects in §6.2 are fixed and guarded by `make leaktest`. Phase 14 fixed float/int literal preservation, non-finite/overflow folding, string-identity folding, shadowed-parameter propagation, 64-bit branch folding, and identifier-only `x * 2` peephole rewriting; unsafe loop strength reduction was removed. `tests/test_optimizer_semantics.py` compares `--no-optimize` and `--optimize` output across six fixtures. No self-hosted optimizer exists yet.

### 4.7 Codegen — Implemented, large surface
`codegen_expr.c` (55.7 KB, **128-entry builtin dispatch**) + `codegen_node.c` (49 KB). Debug-line emission (`codegen_set_debug_source`), main synthesis runs tests, `_cnext_free_all()` then return. GCC driver invokes `gcc -std=gnu11` (argv-array, deliberate no-shell per `main.c:572`).

### 4.8 Formatter / Linter — Implemented + basic
`formatter.c` (6.4 KB), `linter.c` (11.8 KB). No `defer`/`when` awareness (round-trips generically). Formatter tested via 4 inline snippets in `run_tests.py`; linter via 3. Phase 8 should verify both survive new syntax.

### 4.9 REPL — Implemented
`repl.c` (7.2 KB): bare-expression wrapping + `validate_repl_expr_ast` structural guard. No optimizer. Frees AST per iteration (good pattern to copy in the leak harness).

### 4.10 Diagnostics — Implemented + actionable
Parser, semantic, codegen, and backend diagnostics now use stable IDs (`E001`–`E018`, `E100`–`E120`, `E200`–`E202`) and a common `error [Exxx] [line:col]: message` shape with source excerpts, `= expected:`, `= found:`, `= help:`, `= note:`, and `= rule:` continuations where relevant. GCC output is captured and normalized through the same emitter; `tests/test_diagnostics.py` and `tests/test_native_linking.py` guard the format. CLI/linter/REPL messages outside the compiler pipeline remain plain text.

### 4.11 Standard library — Implemented, broad, under-tested → Phase 12
Canonical imported modules are `io`, `file`, `net`, `json`, `math`, `os`, `string_utils`, `time`, `regex`, `collections`, `crypto`, `path`, `encoding`, `process`, `random`, `thread`, `http`, `log`, `args`, `fmt` (`src/main_packages.c:4-5`, `src2/main.cn:15-36`); additional headers such as `csv`, `sort`, `buffer`, and `uuid` exist but are not in the canonical import table. **Shell APIs are string-based and unsafe with untrusted input** (§6.3); `process_spawn(argv)` provides the argv-safe path. Phase 12 corrected `docs/stdlib.md` to the real HTTP API (`http_get_full`, `http_post_full`, `http_put`, `http_delete`, `http_patch`, `*_with_headers`, `http_request`, `http_free_response`, `http_status_text`) and clarified that `http_get`/`http_post`/`download_file` are not HTTP builtins. Per-module edge coverage remains uneven.

### 4.12 Builtins — Implemented (128 encoded) → Phase 13
`codegen_expr.c:90-…` dispatch table encodes 128+ mappings; total global functions exceed that once language-level builtins (string ops, math, time, file, os, collections) are counted. Phase 12 made the documented core/array builtins real: `swap`, `stacktrace`, core array mutators/queries, `shuffle`/`array_shuffle`, int-callback higher-order helpers, and a benchmark loop that consumes `iterations`. Higher-order helpers are currently int-only; `stacktrace` is a portable empty-string placeholder. `tests/collections/test_array_builtins_contract.cn` and `tests/stdlib/test_comprehensive_builtins.cn` cover the repaired contracts, but per-builtin error behavior and ownership still need a full sweep.

### 4.13 Package manager — Implemented + hardened in Phase 20
`registry.c` + `main_packages.c` + `semver.c`. HTTPS-only enforcement, local `registry.txt`, cache at `%USERPROFILE%\.cnext\registry` (Win) / `~/.cnext/registry`, `cnext.toml` manifest, `Name = URL` registry format, `std/http@^1.2.3` imports via `moduleresolver.c`. Semver supports single constraints (EXACT/CARET/TILDE/GTE/GT/LTE/LT/ANY) — no multi-constraint ranges. Phase 20 validates dependency URLs and local file paths, writes downloads to scoped `.tmp` files before rename, caps registry bodies, percent-encodes queries, validates names/versions, creates cache parents, uses unique temp files, saves tokens atomically, and sends the token during login verification. Residual findings are noted in §6.3.

### 4.14 LSP & VS Code extension — Implemented + hardened
Windows stdin deadlock fixed (`os.read(0,4096)`), compiler path honored via `initializationOptions`, uint/range formatting, icon theme. `subprocess` argv-list only (no shell). Pushed via v10.0.3/v10.0.4. `tests/test_lsp.py` is the regression harness (7 scenarios).

### 4.15 Testing infra — Reorganized in Phase 17, hardened in Phases 18–20
- Native `cnext test`: recursive `tests/**/*.cn` scan in `src/main.c`, in-order `// EXPECT:` check, per-file timeout via subprocess, fixture-dir exclusions, relative names such as `core/test_simple.cn`, and filters and explicit paths. Currently **128 passed, 0 failed**.
- `run_tests.py`: recursive positive discovery + examples + negative compile/runtime tests + formatter/linter + memory/thread stress. Scratch files live in `build/tmp`; fixture dirs and path-sensitive negatives are excluded explicitly. Positive checks only require build success and `"OK"`, not `// EXPECT` verification. Subprocess output uses replacement-safe decoding so arbitrary program bytes cannot raise cp1252 decode/encode errors on Windows.
- `test_optimizer_semantics.py`: six `tests/optimizer/*.cn` fixtures compare `--no-optimize` and `--optimize` byte-for-byte for constant/branch/float/int-width/shadowing/strength-reduction/string-identity regressions.
- `parity_parser.py`, `parser_parity.py`, and `parity_sweep.py`: recursive corpus discovery with the same fixture-dir exclusions; sweep output names include the relative test path.
- Resolved flake: `tests/test_native_linking.py::test_mutable_capture_cells` intermittently crashed under native `-O2` during MinGW SEH `longjmp` unwinding of `tests/bootstrap/test_capture_cells.cn`. Phase 20 enabled `__USE_MINGW_SETJMP_NON_SEH` in `include/runtime.h`, added a preprocessor assertion, and repeats the `-O2` run 100 times on Windows.
- The committed `tests/_test_list.txt` and PE binary mislabeled as `tests/_test_methods_temp.c` were removed in Phase 8, and `clean` now purges those plus `_p*.c`, `_probe*`, and `out_parity/`.
- No lexer/parser/semantics/type unit tests and no `regression/` dir. `tests/test_fuzz_invalid_input.py` now fuzzes invalid sources with a fixed seed, bounded cases, timeouts, crash-code detection, bounded output, and a compile-only side-effect check; no ASan/MSan in CI.
- Phase 19 formatter/linter regressions cover `defer` bodies and `when` expressions: calls keep `name(args)` spacing, `when` arms are one per line, output is idempotent, and boolean arms no longer trigger `Comparison with literal` warnings.
- Phase 20 adds `tests/test_package_security.py` and cross-platform source guards for temp-file finalization, HTTPS-only registry access, auth-bearing login verification, bounded captured output, shell-free CLI call sites, quoting, and installer cleanup.
- Test discovery cap 512; `TestEntry[512]` stack arrays.

### 4.16 Cross-platform — Core paths audited in Phase 16
Windows paths are first-class (`CNEXT_PATH_SEP`). The WIN32 argv→cmdline quoting breakout is fixed: `run_process`, `run_process_captured` and `process_spawn` all serialize argv with `CommandLineToArgvW` rules (§6.3), and registry/REPL no longer bypass that helper with raw `_spawnvp`. Downloader output options now precede `--`, generated C and stdlib file writes are byte-exact, and failed `--emit-c` runs remove their temp source. `tests/test_cross_platform.py` guards these paths and runs from `make check`. Phase 20 closes the remaining cross-platform gaps: captured output drains to EOF with a bounded buffer and truncation warning, registry cache parents/temp names are safe and unique, `doctor`/`cache` use argv helpers instead of shell strings, and installer scripts quote temp paths and clean up on failure. POSIX paths are exercised in CI (Ubuntu/macOS), and `run_tests.py` now runs on all CI platforms without being skipped.

### 4.17 Generated C — Audited in Phase 15
`cnext build --verbose` prints generated C; `--emit-c <path>` writes generated C and is used by self-build/parity tooling. Native `--emit-c` now rejects explicit `-o`, matching selfhost behavior. Raw-string literals are escaped before emission in both compilers, and Phase 16 removed the Windows CRLF pre-normalization so emitted C uses LF bytes on every platform. Failed `--emit-c` runs remove their temp source. `main` returns store a result and jump to a shared cleanup label before `_cnext_free_all()`; the label is emitted only when a return needs it. Runtime print macros use a collision-resistant temporary. Debug info via `--debug`; sourcemap exists (`sourcemap.c`) but integration is minimal.

---

## 5. Feature status matrix

| Feature | Status |
|---|---|
| Lexer / parser / AST / codegen core | Implemented + tested (integration) |
| Type system (int..ulong, nullable, aliases, generics, classes, traits, enums, union, variants) | Implemented + under-tested (no unit tests; `tests/types/test_new_types.cn`, `tests/oop/test_generics_full.cn` weak) |
| Coroutines / async / await / generators | Implemented + tested (`tests/control/test_coroutines.cn` etc.), defer/thread interplay untested |
| Closures (by-value + new by-reference) | Implemented + tested (`tests/core/test_closures_full.cn`) — by-reference WIP uncommitted |
| `match` / `when` / ranges / `?` try-expr / option-result | Implemented + under-tested; dead `AST_GUARD` removed |
| Optimizer | Implemented + memory-ownership tested (`make leaktest`); output-equivalence guarded (`tests/test_optimizer_semantics.py`) |
| defer | Hardened in Phases 5–7; invalid use rejected, LIFO/throw matrix covered, MSVC compile/run verified |
| try/catch/throw | Implemented + tested | 
| Error diagnostics | Stable IDs, source excerpts, expected/found, help, and rule metadata across parser/semantics/codegen/backend |
| Standard library | Implemented + edge tests added for array/JSON/encoding; per-module coverage still uneven |
| Builtins (170+) | Implemented + uneven coverage |
| Formatter / linter | Implemented with `defer`/`when` parity, idempotent formatting, and boolean-arm lint fixes |
| REPL / doctor / new / upgrade / cache / config / doc | Implemented |
| Package manager | Implemented + hardened in Phase 20; no known open security gaps |
| LSP / VS Code ext | Implemented + hardened |
| `--emit-c`, `process_spawn(argv)` | Implemented and audited in Phase 15 (`--emit-c <path>`; `tests/test_native_linking.py`, `tests/io/test_process_spawn.cn`) |
| Fuzzing, ASan CI, regression dir | Deterministic invalid-input fuzzing and categorized regression dir implemented; ASan is not in default CI |
| Performance benchmarking | Reproducible Python harness in Phase 21 (`tools/bench.py`, JSON baseline/results, repeated medians); clean build ~4.35s and incremental build ~0.74s on the recorded Windows baseline |

---

## 6. Confirmed defects & findings

### 6.1 Test metadata vs in-order runner (fixed in this audit)
The uncommitted runner requires EXPECT lines to appear **in order**. Three legacy tests mismatched real output:
- `tests/version_features/test_v9.cn`, `tests/version_features/test_v8_comprehensive.cn`: had two `// EXPECT: 255`; the first came from `printin(byte)` which emitted a **raw 0xFF byte, not text “255”** at the time → removed the bogus line, keeping the real `255` (source of `ubyte`) at its correct position. `byte` now maps to `unsigned char`, so its `255` is printed as text and both expectations are back in the list.
- `tests/stdlib/test_system_builtins.cn`: expected `1,1,1,hello_env_val`; program prints `1,1,hello_env_val,true` → corrected.
Baseline after fix: **84/84**.

### 6.2 Optimizer ownership (Phase 1 — fixed)
The audit found one leak, one double-free, and three aliasing hazards:
- `optimize_loops` strength reduction stole `cond->left` and orphaned `cond->right` before `free_ast(cond)`.
- `eliminate_identities` freed the discarded operand while `optimize_node` still freed it through its pre-pass child guards.
- `propagate_constants` aliased the const table's token text; peephole and loop strength reduction created identifier nodes whose `token.start` aliased another node's text.
- Whole-node fold replacements freed the target's fields but not the source node shell, leaving one AST allocation behind per fold.

All paths now follow the rule: optimizer-allocated text is duplicated into `owned_token_text`, ownership transfers through `replace_node_token()` / `move_ast_node()`, and replacement nodes never borrow another node's owned buffer. `tools/optimizer_leak_harness.c` runs interleaved control/optimized compile cycles, checks heap slope parity, and asserts the owned-token invariant after every optimized cycle. `make leaktest` passes with optimizer growth at or below the control baseline.

### 6.3 Process execution & package-manager security (Phases 2–4 and Phase 20 — fixed)
- ~~**Highest-severity (WIN32)**: the PowerShell download fallback interpolated the package name verbatim~~ — fixed. `download_package` validates the name with `is_safe_package_name` (no traversal, no `\ : ; & | < > ( ) { } * ? !`, no control characters) and the URL with `is_safe_url`, and `ps_quote` emits both operands as PowerShell single-quoted literals with `'` doubled, so neither can end the script. The fallback still exists; Phase 20 kept it but routed all downloads through validated URLs and temp files.
- ~~**`run_process_captured` WIN32 quoting**: args flattened with `"%s"` and no `"` escaping~~ — fixed. `append_quoted_arg` implements the `CommandLineToArgvW` rules (quote each token, double backslash runs, add one backslash before a literal quote), and `run_process` now re-serializes its vector the same way instead of letting `_spawnvp` join it.
- ~~**`is_safe_url`/dash-leading URL**: curl parsed a dash-leading URL as options~~ — fixed. All `curl`/`wget` invocations pass `--` before the URL; Phase 16 corrected the option order so output flags precede the terminator (`curl -fsSL -o <out> -- <url>`, `wget -q -O <out> -- <url>`). `is_safe_url` still rejects a leading `-`, `"`, `'`, backtick, `$` and `\`.
- ~~No argv-based process API exists~~ — added: `process_spawn(str[] argv)` in `include/cnext_process.h` (fork/`execvp` on POSIX, quoted `CreateProcessA` on Windows). It returns the exit status, `127` when the program cannot start, `-1` for an unusable vector. Registered for the `process` module in `src/semantics_types.c` and covered by `tests/io/test_process_spawn.cn`. It waits for the child and does not wire stdin or detach — those stay open.
- Shell-string builtins `sys_exec/sys_shell/os_exec*/process_run*` remain shell APIs by design; `docs/process-safety.md` documents both families, the Windows quoting rules, and the `cmd.exe` re-parsing caveat.
- Compiler’s own gcc/curl/tar/test invocations are argv-based (good; `main.c:572` comment confirms deliberate hardening). Phase 16 removed duplicate raw `_spawnvp` helpers from `registry.c` and `repl.c`; `main_utils.c` is now the only `_spawnvp` caller, guarded by `tests/test_cross_platform.py`.
- Phase 20 fixed captured-process buffering: both captured helpers drain pipes to EOF, bound retained output, warn on truncation, and avoid POSIX early-close/SIGPIPE failures. Windows stdin capture uses a reader thread so large child output cannot deadlock the writer.
- Phase 20 hardened registry/package-manager paths: HTTPS-only requests, status parsing, 64 MiB response cap, strict reads/writes, unique cache temp files, query encoding, name/version validation, atomic token save, auth-bearing `/api/me` verification, scoped package parents, `.tmp` downloads, and rename finalization.
- Phase 20 removed remaining CLI shell strings from `doctor`/`cache` via `run_process_quiet`, added cache filesystem helpers, and hardened `install.sh`/`install.ps1`/`install.bat` quoting and cleanup.

### 6.4 Diagnostics (Phase 9 — fixed)
Parser, semantics, codegen, and GCC backend failures now share one emitter. Codes are explicit instead of enum ordinals, source excerpts and carets point at the reported token, expected/found pairs cover parser and type mismatches, each code has stable help text and `CN-Exxx` rule metadata, and GCC diagnostics are parsed back into line/column/note fields. `tests/test_diagnostics.py` checks the native format; `tests/test_native_linking.py` checks normalized backend failure output.

### 6.5 Type system & language consistency (Phases 10–11 — fixed)
Assignments now check text/non-text compatibility in both compilers, so `int x = 1; x = "hello"` fails with `E101` before C generation instead of producing accidental C conversions. Numeric assignment, initializer, and return checks use a widen-only matrix: same-type and lossless widening are implicit; narrowing, signedness changes, and floating-point/integer crossings require `as`. `option(...)`, `ok(...)`, `err(...)`, `expr?`, and `when { ... }` operands reach semantic analysis in both compilers, statement-form try propagation from a typed function is rejected, and `when` expressions infer the common arm type. `for (x in ...)` bindings now infer element types from arrays, strings, ranges, `str_split`, and generator calls in both compilers. The self-hosted code generator also emits `when` expressions and statement-form `expr?` propagation. Regression cases live in `tests/run_tests.py`, `tests/test_diagnostics.py`, and `tests/test_bootstrap_codegen.py` (native and stage 2).

### 6.6 Cross-platform / hygiene (Phase 16 — core paths fixed)
- Windows quoting issue §6.3 fixed; `DEV_NULL` handled. Phase 16 routed registry/REPL through the shared argv helper, corrected downloader option order, made generated C and stdlib file writes byte-exact, and cleaned temp C on `--emit-c` failure. Phase 20 removed the remaining `system()` call sites from `doctor`/`cache clear/status`.
- Phase 20 closed the deferred cross-platform items: captured output bounds, registry cache parents/temp names, static shell-string CLI call sites, and installer script quoting/cleanup.
- Committed `.o`/`.d` in `src/` remain historical noise; the PE binary `tests/_test_methods_temp.c`, `tests/_test_list.txt`, stray `_test_out*.exe`, `_p*.c`, `_probe*`, and `out_parity/` are now ignored and purged by `clean`.
- No ASan/UBSan/Valgrind automation; `make coverage` runs gcov on Linux only.

---

## 7. Quality-bar checklist (current)

- [x] Existing compiler builds cleanly (this audit; `-Wall -Wextra` clean)
- [x] Existing tests pass (128/128 native; Python/LSP verified in `make check`)
- [x] Optimizer leaks fixed / ownership audited / repeated-compile tested (`make leaktest`)
- [x] Optimizer semantics audited / output-equivalence guarded (`tests/test_optimizer_semantics.py`)
- [x] Process APIs audited / shell separated / `process_spawn(argv)` added + tested (`docs/process-safety.md`)
- [x] Defer fully tested + documented
- [x] Parser/semantics/codegen/backend diagnostics hardened (`tests/test_diagnostics.py`)
- [ ] Type system, runtime, generated C, builtins, stdlib audited
- [x] Cross-platform core paths reviewed (WIN32 quoting, shared argv helpers, byte-exact writes, bounded capture, installer cleanup)
- [x] Invalid input does not crash compiler (deterministic fuzzing)
- [x] Tooling functional (fmt/lint/repl/doctor/LSP/ext) with `defer`/`when` formatter and linter parity
- [x] Package manager security reviewed (Phase 20 fixes in §6.3)
- [ ] Docs match implementation (`docs/defer.md`, pkg-manager, memory model, process safety)
- [ ] Examples compile (`run_tests.py` covers a subset)

---

## 8. Work plan (order = priority ladder)

1. **Phase 1 — done** — optimizer ownership: applied ownership rule, fixed §6.2 leaks, and added the repeated-compile regression harness (`make leaktest`).
2. **Phases 2–4 — done** — process-execution sweep: §6.3 injection bugs fixed (PowerShell quoting + name/URL validation, WIN32 argv quoting in `run_process`/`run_process_captured`, curl/wget `--`), `process_spawn(argv)` added and tested, `docs/process-safety.md` written.
3. **Phases 5–7 — done (committed)** — defer: MSVC lowering rewired, invalid top-level and control-transfer cases rejected in both compilers, `docs/defer.md` updated, matrix tests added. `make msvc-defer-test` generates the MSVC form and compiles/runs the defer matrix with `cl` when Visual Studio Build Tools are installed.
4. **Phases 8–9 — done (committed)** — correctness cleanup removed the dead `AST_GUARD` node and committed test artifacts; diagnostics now use stable IDs, rule metadata, source excerpts, expected/found fields, help text, and normalized GCC output.
5. **Phases 10–11 — done (committed)** — assignment compatibility, widen-only numeric conversions, `for-in` element typing, expression-node semantic coverage, and `when`/try codegen are consistent across native and self-hosted compilers.
6. **Phases 12–13 — done (committed)** — stdlib + builtin audit (array contracts, JSON-array/encoding edges, runner expansion) and remaining per-module tests (canonical module call paths, stdlib bug regressions, deterministic edge coverage).
7. **Phase 14 — done (committed)** — optimizer semantic audit: literal/fold overflow guards, float and string-identity fixes, parameter-shadowing guards, removal of unsafe loop strength reduction, and an `--optimize` output-equivalence harness.
8. **Phase 15 — done (committed)** — generated-C audit: consistent `--emit-c`/`-o` rejection, raw-string C escaping in both compilers, shared cleanup path for `main` returns, and collision-resistant runtime print temporaries.
9. **Phase 16 — done (committed)** — cross-platform core paths: downloader argv order, registry/REPL shared process helper, LF-only generated C, byte-exact stdlib file writes, temp cleanup, and `tests/test_cross_platform.py` wired into `make check`.
10. **Phases 17–21 — done (v10.0.5)** — categorized `tests/<category>/` layout, recursive native/Python/parity discovery, fixture exclusions, accurate Windows Python CI docs, deterministic scratch artifacts, deterministic invalid-input fuzzing with bounded runtime, crash detection and a compile-only side-effect check, replacement-safe subprocess output decoding in `run_tests.py`, `fmt`/`lint` parity for `defer`/`when`, and the Phase 20 security pass.
11. **Phase 20 — done (v10.0.5)** — package-manager security fixes and remaining cross-platform gaps: HTTPS/name/URL validation, temp-file finalization, bounded captured output, unique cache temp files, auth-bearing login verification, shell-free CLI helpers, and installer quoting/cleanup.
12. **Phase 21 — done (v10.0.5)** — measure-then-optimize: replaced the shell-only `make bench` with repeated median timings, saved a machine-readable baseline, and reduced clean build time ~20% and incremental build time ~50% by removing unused Windows headers and moving cache-tree helpers out of `main.c`.
13. **Phase 22 — done (v10.0.5)** — docs match the implemented test layout, tooling parity, package security, fuzzing, benchmarking, and release status.
14. **Always:** per-change Understand → Design → Implement → Test → Measure → Document; every bug fix gets a regression test; run the full suite after every phase.

---

## 9. Open decisions

- **Phase 16 commit:** Phases 13–16 are committed (`f3b2714` plus the Phase 16 cross-platform audit).
- **Phase 17 reorg:** `.cn` files moved into `tests/<category>/`; native-runner recursion, `run_tests.py` discovery, parity recursion, fixture exclusions, and promoted optimizer regressions are in v10.0.5. The stale `_test_list.txt`/`_test_methods_temp.c` artifacts were already deleted in Phase 8.
- **Phase 18 fuzzing:** `tests/test_fuzz_invalid_input.py` is in v10.0.5; `make check` runs its default seeded cases, and `--cases`/`--seed` allow deeper sweeps.
- **Phase 19 tooling parity:** `src/formatter.c`, `src/linter.c`, and `tests/run_tests.py` are in v10.0.5; `cnext fmt` is idempotent for `defer`/`when`, and linter boolean-arm false positives are covered.
- **Phase 20 security parity:** `src/main_packages.c`, `src/registry.c`, `src/main.c`, `src/main_utils.c`, installer scripts, and `tests/test_package_security.py` are in v10.0.5; cross-platform guards cover captured output, shell-free CLI paths, and installer cleanup.
- **Phase 21 benchmarking:** `tools/bench.py` and `make bench` are in v10.0.5; `build/bench/baseline.json` records the post-optimization Windows baseline, and `tests/test_cross_platform.py` guards the reproducible harness and lean Windows-header layout.
- `run_tests.py` now runs on all CI platforms, and `make check` fails on Python test errors instead of skipping them.
- Keep the in-order EXPECT runner (set the 3 legacy files fixed already). Also consider expecting the raw byte case (`printin(byte)`) via a byte-aware EXPECT marker — out of scope for now; note it.
- Keep the MinGW non-SEH `longjmp` regression guard for `test_mutable_capture_cells`; the historical `-O2` crash is fixed but remains sensitive to runtime include changes.
