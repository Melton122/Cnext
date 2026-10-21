# Cnext v11 — Audit & Stabilization Roadmap

> Status: **Phase 0 — audit in progress**. Baseline verified: clean build (`-Wall -Wextra`, no warnings), native suite **84 passed, 0 failed** (after fixing 3 EXPECT mismatches exposed by the stricter, still-uncommitted in-order test runner).
> Head: `1386554` (v10.0.4). **The working tree contains uncommitted WIP** (test-runner refactor, optimizer ownership groundwork, closure by-reference capture, `os.h` NUL-termination fix) — see §“Uncommitted Working-Tree Changes”.

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
- `src/optimizer.c`, `src/ast.h`, `src/ast.c`: optimizer token-text ownership groundwork — `ASTNode::owned_token_text`, `make_int_literal`/`make_float_literal` own their text, `replace_node_token()`. **This is exactly the Phase-1 area, only partially converted** (gaps in §6.2.1).
- `include/codegen_internal.h`, `src/codegen_closure.c`, `src/codegen_expr.c`: closure captures made **by-reference when mutated** (`by_reference` → `#define name (*(_env->name))`). Regression risk for closure semantics; covered by `test_closures_full.cn` but worth dedicated tests.
- `include/os.h`: `os_exec`/`os_exec_output` now copy-and-NUL-terminate `CnextString` commands (fixes shell misuse of non-NUL-terminated slices).
- Docs: `README.md`, `docs/stdlib.md`, guide HTML modified; `docs/cnext-beginner-book.md`, `docs/self_hosting.md` untracked new.

**Decision needed:** commit these as roll-up (with regression coverage) once Phase 1 proves them, or split. Do not discard — they are load-bearing for v11.

---

## 4. Phase-by-phase audit (current state)

### 4.1 Lexer — Implemented + tested (indirect)
`src/lexer.c` (10.7 KB): keyword table (`defer` at `lexer.c:141`), tokens incl. v10 keywords, range `..`/`..=`, string interpolation lexing. No dedicated lexer unit tests (covered via integration). Phase 8/17 should add negative lexer cases. `TOKEN_*` enum is large but consistent.

### 4.2 Parser — Implemented + under-tested
Hand-written recursive descent (`parser_*.c`). Supports classes, traits, interfaces, generics, closures, coroutines, async/await, macros/attributes, constexpr, match+guard, `when`, union types, variants, `extern "C"`, benches, `own`, option/result/try-`?`, ranges, defer.

**Confirmed defect — top-level statement gap** (`src/parser.c:17-24`): the top-level reject-list omits **`AST_DEFER`**, **`AST_WHEN`**, and **`AST_YIELD`**; `defer`/`when`/`yield` at top level pass parse+semantics and then emit invalid C (file-scope `__attribute__((cleanup))`, etc.). Fix: add all statement-y node types to the reject list; add negative tests.

### 4.3 AST — Implemented
`include/ast.h`/`src/ast.c`: full node family; `free_ast` owns `children`, `type_params`, `type_args`, `left/right/condition/init/increment/default_value`, `type_name` etc., and now `owned_token_text` (WIP). **Ownership rule to lock down in Phase 1:** a node’s `token.start` either borrows the source buffer (lexer, non-Owning) or points into `node->owned_token_text` (owning); `replace_node_token()` is the ONLY authorized way to swap a token and its ownership; never set `token.start` to optimizer-allocated text except via it. Dead enum entry: `AST_GUARD` (`ast.h:100`) — declared, zero implementations.

### 4.4 Semantic analysis — Implemented + under-tested
`semantics_*.c`: scopes, undeclared-id, type checking, classes/inheritance/interfaces/traits, generics, nullability, union/variant, constexpr const-checking, `defer` just scopes the block (see §4.5). `analyze_semantics` has `has_semantic_error` global (resettable). No type-system unit tests. **No rejection of `return`/`break`/`continue`/`throw` inside a deferred block** (evaluated in the wrong C context → invalid generated C).

### 4.5 Defer — Implemented + buggy, under-tested → Phases 5–7
Full detail in `docs/defer.md` (to be written). Confirmed findings (`include/runtime.h:393-478`, `src/codegen_node.c:786-824`):
- Lowered via GNU `cleanup` attribute nested function + runtime LIFO registry. **LIFO** on both normal exit and throw-unwind. Correct on the live path (MinGW/clang `-std=gnu11`).
- **MSVC path is broken for throw**: duplicates the body, no registry push/release, relies on SEH `__finally` that `longjmp` bypasses (`codegen_node.c:802-816`). Dead today, latent defect.
- **Top-level `defer` not rejected** (→ invalid C). **`return/break/continue` inside defer body** not rejected (→ invalid C).
- **Thread-unsafe**: `_cnext_defer_stack/_count/_unwinding` and `_cnext_try_stack` are process-global (`runtime.h:403+`); defer/try/throw across OS threads corrupt marks. Under-tested.
- **Generator/coroutine frames + defer** untested and hazardous (global registry + retained `void*` into reused frames).
- Defer is **block-scoped** (unlike Go), body evaluated at run time (capture-by-reference).
- `main` emits `_cnext_free_all()` *before* scope teardown → a defer freeing tracked memory double-frees (`codegen_node.c:515-549`).
- Top-of-function depth cap `CNEXT_MAX_DEFER_DEPTH 512` → `exit(70)`.
- `defer_counter` (global, `codegen_node.c:7`) not reset in `reset_codegen_state()` (`codegen.c:69-118`) → drifts in REPL/LSP sessions.
- Tests: only `tests/test_defer.cn` (return + throw ordering) + a `defer mem_arena_destroy` in `test_memory.cn:38`. Phase 6 adds the full matrix.

### 4.6 Optimizer — Implemented + buggy (memory) → Phase 1 & 14
`src/optimizer.c` (34 KB): constant propagation, copy propagation, identity elimination, peephole, constant folding, DCE, branch simplification, loop strength reduction. Enabled via `--optimize` (default off; run by `compile_file` only when `run_optimizer`). **REPL does not run the optimizer.**
Confirmed ownership/concurrency defects — §6.2.

### 4.7 Codegen — Implemented, large surface
`codegen_expr.c` (55.7 KB, **128-entry builtin dispatch**) + `codegen_node.c` (49 KB). Debug-line emission (`codegen_set_debug_source`), main synthesis runs tests, `_cnext_free_all()` then return. GCC driver invokes `gcc -std=gnu11` (argv-array, deliberate no-shell per `main.c:572`).

### 4.8 Formatter / Linter — Implemented + basic
`formatter.c` (6.4 KB), `linter.c` (11.8 KB). No `defer`/`when` awareness (round-trips generically). Formatter tested via 4 inline snippets in `run_tests.py`; linter via 3. Phase 8 should verify both survive new syntax.

### 4.9 REPL — Implemented
`repl.c` (7.2 KB): bare-expression wrapping + `validate_repl_expr_ast` structural guard. No optimizer. Frees AST per iteration (good pattern to copy in the leak harness).

### 4.10 Diagnostics — Implemented + verbose
Compiler emits `error [E0xx] [line:col]: message` + optional `= hint:` continuation (used by LSP/VS Code, cnext-ide). Phase 9 wants richer (source excerpt, expected/found, help). Note: **gcc backend failures** are surfaced raw (`temp_out.c:…: error: …`) with mixed filtering in the test harness.

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
- `tests/_test_list.txt` & `_test_methods_temp.c` (a PE binary mislabeled `.c`) are committed dead artifacts.
- No lexer/parser/semantics/type unit tests, **no fuzzing**, **no regression/ dir**. No ASan/MSan in CI.
- Test discovery cap 512; `TestEntry[512]` stack arrays.

### 4.16 Cross-platform — Partial → Phase 16
Windows paths are first-class (`CNEXT_PATH_SEP`, `run_process_captured` WIN32 quoting). **Confirmed WIN32 argv→cmdline quoting breakout** (§6.3). POSIX paths exercised in CI (Ubuntu/macOS); macOS `continue-on-error` for Python tests.

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
| `match` / `when` / ranges / `?` try-expr / option-result | Implemented + under-tested; `AST_GUARD` dead |
| Optimizer | Implemented + **memory-leaky**; output-semantics only lightly tested |
| defer | Implemented + **buggy edges, under-tested** |
| try/catch/throw | Implemented + tested | 
| Error diagnostics | Implemented + verbose; desired Phase-9 upgrade |
| Standard library | Implemented + under-tested per-module |
| Builtins (170+) | Implemented + uneven coverage |
| Formatter / linter | Implemented + basic; no defer/when awareness |
| REPL / doctor / new / upgrade / cache / config / doc | Implemented |
| Package manager | Implemented + security gaps |
| LSP / VS Code ext | Implemented + hardened |
| `--emit-c`, `process.spawn(argv)`, fuzzing, ASan CI, regression dir | **Missing** |
| Performance benchmarking | `make bench` exists; no rigor (Phase 21) |

---

## 6. Confirmed defects & findings

### 6.1 Test metadata vs in-order runner (fixed in this audit)
The uncommitted runner requires EXPECT lines to appear **in order**. Three legacy tests mismatched real output:
- `test_v9.cn`, `test_v8_comprehensive.cn`: had two `// EXPECT: 255`; the first came from `printin(byte)` which emits a **raw 0xFF byte, not text “255”** → removed the bogus line, keeping the real `255` (source of `ubyte`) at its correct position.
- `test_system_builtins.cn`: expected `1,1,1,hello_env_val`; program prints `1,1,hello_env_val,true` → corrected.
Baseline after fix: **84/84**.

### 6.2 Optimizer ownership (Phase 1)
Trace confirmed (`optimizer.c`):
- `make_int_literal`/`make_float_literal` set `owned_token_text` (OK, WIP already). `replace_node_token()` frees any prior owned text and adopts the new one or `NULL` for borrowed text (OK pattern).
- **Leak (read-confirmed) in `optimize_loops` strength reduction (`optimizer.c:948-952`)**: `x*2 → x+x` sets `cond->left=NULL`, `cond->right=NULL`, then `free_ast(cond)` — the literal `2` (`cond->right`) and its owned text are orphaned (left is stolen to `add->left`; right is *leaked*).
- **Aliasing without ownership**: `add->right = create_node(AST_IDENTIFIER, left->token)` (`optimizer.c:567`, `:949`) creates nodes whose `token.start` aliases another node’s (possibly owned) text. Safe only because the owner survives to teardown; fragile if a future pass frees the owner first.
- **`propagate_constants` identifier → const-literal aliasing** (`optimizer.c:301-305`): the identifier node becomes `AST_LITERAL` aliasing the const-table value's text. Safe while the registering `VAR_DECL`’s `init` survives, but any path that frees that init (DCE of the var decl, folding that replaces init) leaves dangling aliases in the tree → future UAF risk.
- The ownership **rule** to enforce: “every optimizer-allocated `char*` text lives under exactly one `owned_token_text`, transferred only via `replace_node_token`, and never aliased by a second node; all folding replacement paths detach children before `free_ast`.”
- Regression test required: loop `parse → analyze → optimize → free_ast` with a memory baseline (see Phase 1).

### 6.3 Process execution & package-manager security (Phases 2–4, 20)
- **Highest-severity (WIN32)**: `src/main_packages.c:100-111` PowerShell download fallback interpolates the **package name verbatim** into a PowerShell `-Command` script → **command injection + path traversal** (`packages\<name>.cn`). Reachable when curl/wget are absent. Both `name` and `url` must be strictly validated; better, drop PowerShell entirely.
- **`run_process_captured` WIN32 quoting (`main_utils.c:315-335`)**: args flattened to a cmdline via `"%s"` with **no `"` escaping**; registry search query/`CNEXT_REGISTRY_URL`/token with `"` can inject extra curl options (file read/write). Add proper argv→cmdline quoting.
- **`is_safe_url`/dash-leading URL**: curl invoked with the URL as last arg (`registry.c:83,154`); a URL starting with `-` is parsed as curl options. Use `--` terminator.
- Language shell builtins `sys_exec/sys_shell/os_exec*/process_run*` are shell-string APIs (docs explicitly warn). **No direct `process.spawn(argv)` equivalent exists** → Phase 3 adds it cleanly (name it `process.spawn`/`process.exec`), keeping shell APIs clearly named.
- Compiler’s own gcc/curl/tar/test invocations are argv-based (good; `main.c:572` comment confirms deliberate hardening).

### 6.4 Diagnostics (Phase 9)
`error [E0xx] [l:c]: message` is standardized; missing source excerpt/“expected vs found”/help affordances. gcc backend errors pass through raw. Phase 9 should reuse the same emitted format everywhere (parser, semantics, codegen, backend), add `-`-rule metadata, and make every error actionable without verbosity.

### 6.5 Cross-platform / hygiene (Phase 16)
- Windows quoting issue §6.3. `DEV_NULL` handled. `system()` static-command uses in `doctor`/`cache clear/status` are literal-only today (safe, fragile).
- Committed `.o`/`.d` in `src/`, PE binary `tests/_test_methods_temp.c`, stray `_test_out*.exe` — clean target should purge and gitignore them.
- No ASan/UBSan/Valgrind automation; `make coverage` runs gcov on Linux only.

---

## 7. Quality-bar checklist (current)

- [x] Existing compiler builds cleanly (this audit; `-Wall -Wextra` clean)
- [x] Existing tests pass (84/84 native; Python/LSP to re-verify in Phase 17)
- [ ] Optimizer leaks fixed / ownership audited / repeated-compile tested
- [ ] Process APIs audited (§6.3 done) / shell separated / `process.spawn(argv)` added
- [ ] Defer fully tested + documented
- [ ] Type system, runtime, generated C, builtins, stdlib audited
- [ ] Cross-platform reviewed (WIN32 quoting fix)
- [ ] Invalid input does not crash compiler (fuzzing)
- [ ] Tooling functional (fmt/lint/repl/doctor/LSP/ext) — working, needs feature-parity checks
- [ ] Package manager security reviewed (§6.3 findings to fix)
- [ ] Docs match implementation (`docs/defer.md`, pkg-manager, memory model, process safety)
- [ ] Examples compile (`run_tests.py` covers a subset)

---

## 8. Work plan (order = priority ladder)

1. **Phase 1** — optimizer ownership: apply ownership rule, fix §6.2 leaks, add repeated-compile regression harness (build on the existing uncommitted `owned_token_text` WIP).
2. **Phases 2–4** — process-execution sweep: fix §6.3 injection bugs (PowerShell fallback, WIN32 quoting, curl `--`), add `process.spawn(argv)`, write `docs/process-safety.md`.
3. **Phases 5–7** — defer: fix MSVC path, reject invalid defer bodies, thread-safety notes, `tests/defer/` matrix, `docs/defer.md`.
4. **Phases 8–9** — correctness audit + diagnostics, starting from §6 findings (top-level statement gap, `AST_GUARD`, binary-artifacts hygiene), standardized actionable diagnostics.
5. **Phases 10–11** — type-system + language-consistency audit.
6. **Phases 12–13** — stdlib + builtin audit (add missing per-module tests).
7. **Phases 14–18** — optimizer semantic audit, generated C (`--emit-c`), cross-platform (WIN32 quoting), test reorg to `tests/<category>/` (+ native runner subdir support + `_test_list.txt` removal), fuzz for invalid input.
8. **Phases 19–20** — tooling parity (fmt/lint know defer/when), package-manager security fixes.
9. **Phases 21–22** — measure-then-optimize (`make bench` baseline vs reorg), docs that match the code.
10. **Always:** per-change Understand → Design → Implement → Test → Measure → Document; every bug fix gets a regression test; run the full suite after every phase.

---

## 9. Open decisions

- **Commit the current WIP** (runner, optimizer ownership groundwork, closure by-ref, os.h) up-front, or fold into phase commits?
- Test reorg: move `.cn` files into `tests/<category>/` requires native-runner recursion (`main.c`) + `run_tests.py` list rewrite — do it in Phase 17, and delete `_test_list.txt`/`_test_methods_temp.c`.
- `run_tests.py` runs only on non-Windows CI today; Phase 17 should enable it on Windows too.
- Keep the in-order EXPECT runner (set the 3 legacy files fixed already). Also consider expecting the raw byte case (`printin(byte)`) via a byte-aware EXPECT marker — out of scope for now; note it.