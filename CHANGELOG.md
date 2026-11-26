# Changelog

All notable changes to Cnext will be documented in this file.

## [Unreleased]

### Added
- Self-hosted CLI commands `fmt`, `lint`, `version`, and `help` now match the
  native compiler byte-for-byte (same formatter token reprinter, same linter
  rules and ANSI output on stderr, same usage text). `cnext_eprint` was added
  to the runtime so diagnostics reach stderr while stdout stays clean.
- Self-hosted opt-in AST optimization: guarded int32/boolean literal folding,
  decisive short-circuit folding, literal `if` selection, and `while(false)` removal.
  `--no-optimize` wins over `--optimize` in either order.
- Self-hosted `test` command with recursive sorted discovery, filtering/listing,
  ordered `EXPECT`, `STDIN`, explicit expected `EXIT`, and argv-safe child execution.
- Native/self-hosted behavioral parity tests and stage-specific optimizer/test-runner
  checks in CI; `make selfhost-check` runs the focused self-hosting gate.

### Fixed
- `for-in` over an array of class instances no longer crashes the compiler: the
  element binding's type name was freed twice (once after being stored on the
  loop variable), which corrupted the heap and aborted at exit. The loop now
  also extracts class elements as typed pointers (`Student* s`) instead of
  bare `void*`, so member access on the loop variable compiles.
- Chained optional access (`user?.address?.city`) generates valid C again: a
  `?.` hop whose field type is a class now dereferences with `->` like ordinary
  member access, instead of applying `.` to a pointer.
- Lambdas passed as untyped `func` parameters run under one calling convention:
  the compiler emits an `int`-ABI adapter at the pass site, so a
  float-returning lambda (for example a discount rule) returns its real value
  instead of register garbage, and calls through a zero-argument `func`
  parameter compile instead of failing with "too few arguments".
- Arrays of structs store struct values (a file-scope typedef per element type)
  instead of opaque `void*` slots, so `Point[] pts = {a, b}`, indexing,
  element assignment, `for-in`, and passing elements to functions all work.
- Self-hosted compiler parity with the native compiler on the above fixes:
  `func`-typed parameters now accept lambdas with non-int result types (the
  semantic checker no longer forces the call type to `int` and the fixed
  calling convention is arity-matched); chained `?.` resolves the owner type
  through safe-access hops; class-element `for-in` extracts typed pointers;
  and struct-element arrays use value storage with their own typedef, typed
  indexing, and `.` member access. All five feature probes compile and run
  identically under native and stage-2 self-hosted binaries.
- A full corpus sweep (156 test and example programs) now builds and runs
  identically under the native and self-hosted compilers. Gaps closed in the
  self-hosted port: operator overloads type their result from the overload's
  declared return type instead of falling back to int; method dispatch
  prefers the owner's declared class (so `Fighter hero = new Player(...)`
  calls Fighter methods); passing `null` for a `str` parameter emits
  `(CnextString){NULL, 0}`; classes declared inside test blocks are defined
  and registered, so `Point p = new Point(3, 4)` is a pointer; async and
  coroutine declarations are semantically analyzed like ordinary functions
  (locals, awaits, and cross-calls type correctly); async/generator frames
  type their `current` slot from the yield or return type; `run_async f()`
  drives the frame to completion; and `{name(args)}` string interpolation
  calls a stored lambda through its function pointer.
- Void expression lambdas (`() => printin("x")`) emit their body as a statement
  instead of the invalid `return <void call>;`.
- String interpolation of a class-typed function parameter (`{s.name}`) emits
  pointer access (`s->name`), matching how such parameters are passed.
- `match` arms whose last expression is a void call now emit it as a statement
  instead of `return <call>;`. `main` is always lowered as a void-returning
  function for this purpose, and user-defined void callees are detected in both
  compilers, so native and self-hosted output agree for statement-style matches.
- Growing `str_split` results are registered once, preventing a cleanup double-free
  that also affected self-hosted compilation. Tracked array growth updates its
  allocation-ledger entry when realloc moves the buffer; overflow/OOM paths retain
  the original allocation and release the tracking lock.
- Arena and pool bulk cleanup now releases oversized allocations as well as blocks.
  Allocation alignment follows the target's ordinary C alignment, and overflow/OOM
  paths fail with runtime status 70 without retaining the allocation lock.
- Conditional results check both branches, retain array/record metadata, and
  apply lossless numeric widening before selection. Nested ternaries preserve
  grouping in generated C.

### Changed
- `if`/`else` ownership joins check each branch independently and exclude paths
  that return or throw. Loops permit moves of fresh iteration-local resources,
  while bindings carried between iterations remain restricted. Returning branches
  no longer poison ownership on the continuing path.
- `switch`/`match` arms join ownership independently. A move on any arm reaching
  after the statement blocks later reads; returning/throwing/continuing arms are
  excluded; the no-match path counts unless the construct is exhaustive. Arms that
  both move and contain `break`/`continue` are rejected until break-target joins
  are modeled. Exception joins remain conservative.
- `own` consumes local resource bindings in supported transfer
  positions. Both compilers reject later reads, direct aliases, borrowed/escaped
  moves, and unsupported conditional/callback transfers. Lexical arena reference
  checks reject use after scope end and common escape paths; this is a restricted
  checker rather than a complete memory-safety guarantee.
- `when` expressions require one explicit final `else` arm.
- Zero-byte arena/pool requests reserve one alignment unit; usage includes padding.

## [10.0.5] - 2026 — Baby Otter

### Added
- **`process_spawn(str[] argv)`:** the `process` module can now start a program
  from an argv vector with no shell in between, so each element reaches the
  child as exactly one argument and untrusted values cannot become command
  syntax. It returns the exit status, `127` when the program cannot be started,
  and `-1` for an unusable vector. The shell-string APIs (`process_run*`,
  `sys_exec`, `os_exec*`) are unchanged; `docs/process-safety.md` explains which
  one to use
- **`cnext build --emit-c <path>`:** writes the generated C to a file and skips
  GCC. `-o` names the final executable, so the self-hosting bootstrap had no
  way to capture generated C (`make self-build` passed `-o stage1_src.c` and
  then handed GCC an MZ executable). `make self-build` now uses `--emit-c` and
  platform-aware stage executable names
- **Defer matrix tests:** `tests/test_defer_matrix.cn` and its bootstrap copy
  cover LIFO cleanup, nested block scopes, per-iteration loop cleanup, and
  throw unwinding; negative cases cover top-level `defer` and `return`,
  `break`, `continue`, and `throw` inside deferred bodies
- **`make msvc-defer-test`:** builds the compiler with `CNEXT_FORCE_MSVC_DEFER`,
  checks that deferred bodies become file-scope helpers with `__try`/`__finally`
  scope marks, then compiles and runs the generated defer matrix with `cl` when
  Visual Studio Build Tools are installed
- **Actionable diagnostics:** parser, semantic, codegen, and GCC backend failures
  use stable `E001`–`E202` IDs, `CN-Exxx` rule metadata, source excerpts with
  carets, `= expected:`/`= found:` pairs, per-code help text, and normalized
  backend notes. `tests/test_diagnostics.py` checks the format and is wired into
  `make check`
- **Type-consistency checks:** assignments now reject text/non-text mismatches
  with stable `E101` diagnostics in both the native and self-hosted compilers.
  Numeric assignment, initializer, and return checks use a widen-only matrix;
  narrowing, signedness changes, and floating-point/integer crossings require an
   explicit `as` cast. `for-in` bindings infer element types from arrays, strings,
   ranges, `str_split`, and generator calls. Try-expression propagation from a
   typed function is diagnosed before codegen can emit an invalid valueless
   `return`
- **Categorized regression suite:** `.cn` tests now live under
  `tests/<category>/`; native, Python, and parity harnesses recurse while
  excluding fixture directories, and the optimizer regressions are first-class
  tests
- **Deterministic invalid-input fuzzing:** `tests/test_fuzz_invalid_input.py`
  checks crash/timeout/output bounds and a compile-only side-effect check, with
  seeded `--cases`/`--seed` controls for deeper sweeps
- **`fmt`/`lint` parity for `defer`/`when`:** formatting is idempotent and the
  linter no longer reports false literal-comparison warnings for boolean `when`
  arms
- **Reproducible benchmarks:** `tools/bench.py` replaces the shell-only
  `make bench` with repeated medians, fixed job counts, and JSON baseline/result
  artifacts; lean Windows headers and cache helpers cut clean builds ~20% and
  incremental builds ~50%
- **Baby Otter branding:** the README logo is refreshed, and release archives now
  include `logo.png` so the README renders correctly outside the repository

### Fixed
- **Package manager and installers are hardened:** HTTPS-only downloads, name/URL validation, scoped temp files, atomic token saves, auth-bearing login verification, safe cache parents, unique cache temp files, query encoding, and quoted installer cleanup
- **Captured process output is bounded and drains to EOF:** Windows stdin capture uses a reader thread, POSIX polling avoids early-close/SIGPIPE failures, and truncation is reported instead of hanging or silently losing output
- **MinGW `-O2` closure regression is fixed:** `__USE_MINGW_SETJMP_NON_SEH` avoids SEH `longjmp` unwinding crashes, with a preprocessor guard and a repeated Windows regression run
- **VS Code `.cn` file icons:** the Cnext icon theme now matches `.cn` files by language ID and is the default icon theme for fresh installs, so source files no longer show a generic text icon
- **Diagnostic output is bounded:** source excerpts and caret padding are
  capped, so an error on a very long line no longer floods the terminal or
  exhausts memory. `tests/test_diagnostics.py` covers the long-line case
- **Malformed `when`, `switch`, and `match` arms no longer hang the parser:**
  `else` is recognized as a keyword and every arm guarantees token progress,
  so a bad arm fails with a diagnostic instead of looping forever. The
  generated C for `when` now uses a nested ternary chain, fixing `else`
  emitted without a separating semicolon. Self-hosted source excerpts are
  capped as well
- **Statement `expr?` checks are type-aware:** non-nullable results no longer
  generate a `== NULL` comparison against an integer or the wrong callee node,
  and string results check `.data` instead of comparing a struct to `NULL`
- **Newer expression nodes are semantically analyzed:** `option(...)`,
  `ok(...)`, `err(...)`, `expr?`, and `when { ... }` operands now feed semantic
  analysis instead of bypassing it. `when` infers a common arm type rather than
  always reporting `str`, and self-hosted semantics mirrors these traversals.
  The self-hosted code generator now emits `when` expressions and statement-form
  `expr?` early-return propagation
- **Windows process arguments are quoted instead of joined:** `run_process`
  passed its vector to `_spawnvp`, which concatenates argv with spaces, so an
  argument containing a space or quote split into extra arguments. It now
  serializes each token with `CommandLineToArgvW` rules (quote, double
  backslash runs, escape a preceding quote), matching `run_process_captured` and
  the new `process_spawn`. Package downloads also validate the name and URL,
  and pass `--` before the URL so a dash-leading URL cannot be read as a curl
  option
- **Typed input is strict and covers every type:** `input()` assigned to a
  typed destination, and the explicit `input_int()` / `input_float()` /
  `input_bool()` / … readers, now consume the whole line. Trailing text
  (`42abc`), overflow, and out-of-range values read as the type's zero value
  instead of being silently truncated. `input_byte()` reads a number instead of
  the first character, boolean reads accept `yes/no`, `on/off`, `y/n`, `t/f` in
  any capitalization, and `input_str()` names the string reader explicitly
- **`byte` is an unsigned 8-bit integer:** it was emitted as `char`, so
  `byte b = 255` printed a raw 0xFF byte (and `input()` read the character
  `'2'` from `200`). It now emits `unsigned char`, prints as a number, and
  matches the documented 0–255 range
- **Optimizer no longer leaks or aliases AST token text:** constant folding,
  identity elimination, peephole rewrites, branch folding, and loop strength
  reduction now transfer ownership with `move_ast_node()` and duplicate any
  allocated identifier text. The previously orphaned loop-literal and the
  leaked source-node shell are released. `make leaktest` now runs interleaved
  optimized/control compile cycles and enforces the owned-token invariant
- **Compiler builds without a third-party library:** the Makefile linked
  `libcurl` even though the compiler shells out to the `curl` command; only
  programs that import `net`/`http` need libcurl. A source build now needs just
  a C compiler and make
- **`cnext test <file.cn>` resolves bare filenames:** a filename with no
  directory component was used as a path relative to the current directory, so
  `cnext test test_hello.cn` failed even though `tests/test_hello.cn` existed.
  It now searches `tests/`, while a filename that contains a separator is still
  treated as an explicit path
- **Compiling works when `%TEMP%` is not writable:** codegen and the GCC child
  process need a writable temp directory. The compiler now falls back to a
  named file in TMPDIR/TMP/TEMP or the working directory, and points
  TMP/TEMP/TMPDIR at the working directory when the system temp directory
  denies access (`tmpfile()` previously failed with `Permission denied`, which
  surfaced as "Could not create temporary files" or a GCC error)

- **Closure scope teardown no longer double-releases captures:** scope exit
  records the first defer mark in the scope and runs every registered closure
  and capture-cell guard once before freeing tracked memory. The previous
  explicit decref after running closure guards corrupted the heap on programs
  that used mutable closure captures, causing intermittent Windows heap errors
- **Mutable scalar closure captures can escape safely:** captured integers,
  floats, booleans, and characters now live in reference-counted heap cells, so
  a closure that mutates them can outlive its creating scope. Non-scalar
  reference captures remain rejected, and the self-hosted backend now matches
  the native backend's capture-cell emission and identifier resolution
- **Invalid defer use is rejected before code generation:** top-level `defer`,
  `when`, and `yield` now fail parsing, and `return`/`break`/`continue`/`throw`
  inside a deferred body now produce `ERR_SEM_DEFER_CONTROL` in both the native
  and self-hosted compilers instead of emitting invalid C
- **MSVC defer lowering no longer duplicates cleanup bodies:** deferred bodies
  are emitted as file-scope helpers, registered at runtime, and blocks with
  direct defers are wrapped in `__try`/`__finally` so both normal and
  exceptional exit run `_cnext_defer_run_to()`. `defer_counter` is also reset
  per compilation, and the self-hosted backend's defer flag indentation now
  matches stage 0. `main` now gets the same `__try`/`__finally` defer scope, and
  generated C avoids GNU-only `max_align_t`, `__auto_type`, `popen`, and try
  cleanup attributes under MSVC
- **Bootstrap codegen output stays byte-identical:** an empty MSVC defer-helper
  stream no longer adds a stray blank line, preserving the stage 0/1/2 fixed
  point after adding the fourth codegen temp stream
- **Dead AST node and test artifacts removed:** the unused `AST_GUARD` entry was
  deleted from the native and self-hosted AST node tables, and committed
  `_test_list.txt`/`_test_methods_temp.c` scratch files are gone. `clean` and
  `.gitignore` now cover those files, `_p*.c`, `_probe*`, and `out_parity/`
- **Backend failures are no longer raw compiler output:** GCC stdout/stderr is
  captured and re-emitted through the common diagnostic emitter, so link and C
  compile failures carry `E202`, line/column data where available, a source
  excerpt from the generated C, help text, and rule metadata

### Self-hosting
- The self-hosted lexer matches the C lexer on all 158 corpus files, and the
  self-hosted parser now produces byte-identical AST dumps to `tools/astdump`
  on all 100 `tests/*.cn` files
- The parity harness rebuilds `astdump` instead of trusting a stale binary
  (an old dumper hung, which made the whole run look like a parser timeout),
  gives each parser invocation a timeout, and uses absolute paths for the
  candidate executable on Windows
- `src2/` parser fixes: pointer/nullable flags are no longer copied by
  `assign_type_from_node` (the C parser leaves that to semantic analysis),
  stale `.type` field accesses were corrected to `.kind`, and the three
  C-style `for` loops in the codegen port were rewritten as `while`
- The codegen bundle compiles and stage 1 now runs: the self-hosted compiler
  parses, analyzes and generates C for its own 6,951-line bundle
- Codegen parity with the C compiler rose from 6 to 8 of the 100 `tests/*.cn`
  files; `typeof`, `as` casts, `clone`, and string-literal emission now match
  the reference byte for byte (`tests/test_core_builtins.cn` is identical)
- `Token` has no `length` field in the port (a `str` carries `.length`), which
  was the last compile blocker in `src2/codegen_gen.cn` and
  `src2/codegen_closure.cn`
- The stage-2 C pulled out of the bundle also compiles with GCC: both stage
  executables link under `make self-build` (run as `make CC=gcc -j8 self-build`
  from bash, not from Cygwin `make`), and a self-hosted stage-1 binary can now be
  built from the bundle
- Fixed the two remaining stage-2 compile blockers in the port: (1) expression
  codegen emitted `.` where a class-typed value is a pointer — fixed by back-
  porting type resolution for identifiers, fields, method calls, object-array
  elements, `new`/`cast` results, and a struct-vs-class distinction so struct
  `Token` stays a value while class-typed values use `->`; (2) empty-array
  literals emitted `(CnextArrayObj)0` instead of a compound literal — fixed by
  pulling braces out of a codegen string literal that stage 0 mis-parsed as
  interpolation
- Added forward declarations of structs and enums in the port's codegen (`typedef
  struct S S;` / `typedef enum E E;`) so references before the layout definition
  compile; the bundle still compiles and stage-2 C still compiles
- Ran a byte-identity parity run across all 100 `tests/*.cn` files (stage-1 vs
  stage-0 output): 0/100 match byte for byte. The recurring gaps are C-style
  `for` loops not rewritten as `while`; missing struct/enum pre-declarations;
  `static const` const-decls emitted as bare initializers; wrapper prototypes
  missing a return type; dropped module `#include` headers; and literal-array /
  tuple-comprehension blocks expanded through statement expressions differently
- `make self-build` now runs as a complete pipeline (stage 0, stage 1, and
  stage-2 C emission) and stops only when GCC rejects stage 2's C: the port's
  expression codegen still emits `.` where a class-typed value is a pointer,
  among other gaps listed in `docs/self_hosting.md`

## [10.0.4] - 2026

### Added
- **`#line` debug mapping:** `cnext build --debug` now embeds `#line`
  directives in the generated C, so debuggers (gdb/lldb) show the original
  `.cn` source for breakpoints, stepping, and backtraces instead of
  `temp_out.c`

### Fixed
- **`str_split` results were unusable:** `str_split` returned a runtime
  struct whose element field was named `items`, while every other array in
  the language uses `data` — so `parts[i]` and `for x in parts` did not compile
  against a split result (only `.length` worked). The internal array type is
  now unified with the language's `{data, length}` shape, so split results are
  indexable and iterable, and `str_join` consumes them. Also fixed a leak: the
  empty-delimiter path of `str_split` (character splitting) never tracked its
  allocation, so it was silently leaked (it is now tracked like every other
  runtime allocation)
- **`defer` never ran on `throw`:** the deferred body was only wired to
  GNU `cleanup` attributes, which do not fire on `longjmp` (nor on
  `exit()` for unhandled errors) — so arenas/resources were never cleaned
  on error paths on any compiler. `defer` bodies are now registered on a
  runtime stack and executed during unwind (LIFO), before the matching
  `catch` or before process exit for unhandled errors, on both gcc and clang
- **LSP server froze on Windows:** `server.py` read stdin through
  `sys.stdin.buffer.read(4096)`, which on Windows pipes blocks until the
  buffer is full or EOF — the language server never processed any message, so
  completion/hover/diagnostics/formatting were dead and the client showed
  "LSP failed" after the initialize timeout. The reader now uses `os.read(0)`,
  which returns data as it arrives on all platforms. The server also honors
  the extension's `cnext.compilerPath` setting (passed via
  `initializationOptions`) and probes `%LOCALAPPDATA%\Cnext\bin\cnext.exe` on
  Windows; formatting edits now use a valid end range

## [10.0.3] - 2026

### Fixed
- **Windows release binaries (v10.0.2):** the release workflow built with the
  MSYS *POSIX* toolchain (`msystem: MSYS`), which leaves `_WIN32` undefined —
  the shipped `cnext.exe` emitted POSIX link flags (`-lcurl`) for every user
  program, so nothing could compile. Release builds now use the MinGW64 UCRT
  toolchain and link `-lwinhttp -lws2_32` like the CI builds
- **Link flags for plain programs (Linux/macOS):** `-lcurl` is now only added
  when the compiled program actually uses networking (`http_get`/`http_post`),
  so hello-world programs no longer require the libcurl development package
- **Compiling from PATH (all platforms):** running `cnext build`/`run` as a
  bare name via PATH (the normal installed case) looked for runtime headers
  relative to the current directory, failing with
  `fatal error: runtime.h: No such file or directory`; the compiler now
  resolves its own executable location (`/proc/self/exe`, `_NSGetExecutablePath`,
  `GetModuleFileNameA`) to find installed headers

## [10.0.2] - 2026

### Added
- **Memory arenas (runtime):** new `mem_arena_create`, `mem_arena_alloc`,
  `mem_arena_free`, `mem_arena_destroy`, and `mem_arena_usage` builtins give
  deterministic memory reclamation *during* program run (not just at exit):
  all allocations made from an arena are freed in one call. Idiomatic use is
  `var a = mem_arena_create(); defer mem_arena_destroy(a);` — the first step
  toward a real memory model for long-running programs
- **Scoped memory (runtime):** inside `mem_scope_begin()`/`mem_scope_end()`,
  every `new` allocation is made from the thread's innermost scope arena
  instead of the tracked heap, so whole object graphs die deterministically
  when the scope ends (nested scopes supported, `mem_scope_usage` reports live
  bytes). Scoped objects skip destructors and must not outlive their scope
- **Reference counting (runtime):** `ref_new`, `ref_retain`, `ref_release`,
  and `ref_count` builtins wrap a `new` object in a heap handle that frees the
  object exactly when the last reference is released (early deterministic
  death for shared objects instead of exit-time cleanup; forgetting to release
  cannot leak past exit and released objects are never double-freed)

### Fixed
- **Installers (Linux/macOS):** headers were installed FLAT into `/usr/local/include`,
  clobbering system headers (`math.h`, `os.h`, `ast.h`, ...) and breaking other
  toolchains on the machine (a Cnext `math.h` shadowing glibc's causes garbage math
  results). Headers now install to `/usr/local/share/cnext/include`; the compiler
  probes `<exe>/../share/cnext/include` for runtime headers
- **Generated code linking (Linux/macOS):** programs using math functions failed to
  link (`undefined reference to sqrtf/logf`) because `-lm` was only in the compiler's
  own link line, not the one used for compiled `.cn` programs
- **Linux build:** `include/os.h` failed to compile for missing `<errno.h>`,
  `<dirent.h>`, and `<sys/stat.h>` in the POSIX branch (broke `test_os`/`test_net`)
- **Portable tests:** `test_os.cn` and `test_math.cn` hardcoded Windows-only
  expectations (OS name, libc `rand()` sequence); they now assert platform-agnostic
  results so the suite passes on Linux and macOS
- **CI:** the Ubuntu `make test` step had been failing on every commit since
  July 21; the fixes above restore a green `build` matrix alongside the release.
  The macOS `Run Python Tests` step is made non-blocking (a pre-existing
  runner-environment failure unrelated to this release)
  pipeline's new installer smoke tests (version check + build hello program with
  the installed compiler)
- **Thread safety (runtime):** the memory subsystem (tracking list, global
  arena, global pool) was a documented data race — any `thread_spawn` program
  that allocated/freed on worker threads hit UB. All of it is now guarded by an
  internal C11 `atomic_flag` spinlock (no platform headers leaked into generated
  code; `tests/run_tests.py` gained a `memory/thread_stress` 8-thread hammer
  test covering track/untrack, arenas, and pools concurrently)

## [10.0.1] - 2026

### Fixed
- **Installers:** release assets are named `cnext-<os>-<arch>-v<version>.<ext>`; install.sh,
  install.bat, and install.ps1 were requesting the version without the `v` prefix, causing
  404 on every release download
- **Release workflow:** macOS x64/arm64 assets were both built on the (arm64) `macos-latest`
  runner and shipped as identical binaries; split onto `macos-26-intel` (x64) and `macos-26`
  (arm64) with a runner-architecture verification step
- **Release pipeline:** added a smoke-test job that runs the real installers on Linux and
  Windows and verifies the installed `cnext version` matches the release tag

## [10.0.0] - 2026

### Added
- **Global Built-in Functions:** 170+ functions available without imports
  - **String:** `str_upper`, `str_lower`, `str_trim`, `str_contains`, `str_starts_with`, `str_ends_with`, `str_index_of`, `str_last_index_of`, `str_replace`, `str_substring`, `str_repeat`, `str_reverse`, `str_count`, `str_is_empty`, `str_capitalize`, `str_title`, `str_pad_left`, `str_pad_right`, `str_remove`, `str_insert`, `str_split`, `str_join`
  - **Math:** `math_abs`, `math_min`, `math_max`, `math_clamp`, `math_gcd`, `math_lcm`, `math_factorial`, `math_fibonacci`, `math_sqrt`, `math_pow`, `math_log`, `math_exp`, `math_sin`, `math_cos`, `math_tan`, `math_random`, `math_random_range`
  - **Type Conversion:** `to_int`, `to_float`, `to_str`, `to_bool`, `to_char`, `parse_int`, `parse_float`, `char`, `ord`, `bytes`
  - **Core:** `print`, `printin`, `input`, `len`, `typeof`, `assert`, `panic`, `exit`, `clone`, `swap`
  - **File:** `read_file`, `write_file`, `append_file`, `delete_file`, `copy_file`, `move_file`, `file_exists`, `file_size`
  - **System:** `cwd`, `chdir`, `platform`, `sys_args`, `getenv`, `setenv`, `hostname`, `username`, `cpu_count`, `temp_dir`, `home_dir`, `sys_exec`, `sys_shell`
  - **Time:** `time_now`, `time_sleep`, `time_timestamp`, `time_date`, `time_time`, `time_stopwatch_start`, `time_stopwatch_stop`, `time_format_time`
  - **Encoding:** `base64_encode`, `base64_decode`
  - **Crypto:** `md5`, `sha1`, `sha256`, `uuid`
  - **Collections:** `push`, `pop`, `shift`, `unshift`, `insert_at`, `remove_at`, `clear`, `sort`, `reverse_array`, `contains_item`, `array_index`, `array_last`, `array_first`, `array_slice`, `array_unique`, `array_shuffle`, `array_map`, `array_filter`, `array_reduce`, `array_find`
  - **Utility:** `debug`, `gc`, `benchmark`, `stacktrace`
- **Table-driven Codegen:** Refactored code generator to use dispatch table for efficient builtin routing
- **Comprehensive Tests:** 7 new test files covering all built-in function categories
- **Example Programs:** 4 new example files demonstrating built-in function usage
- **Updated Documentation:** Complete stdlib reference with all built-in functions

### Changed
- All built-in functions are now globally available without requiring `import` statements
- Code generator uses table-driven dispatch for better performance and maintainability

### Fixed
- **Parser:** restore optional semicolon consumption (v10 release-blocker regression broke
  newline-separated statements; full suite now passes 76/76)
- **Lexer:** `ok`/`err`/`option` are identifiers again (not reserved keywords), fixing
  `option()`/`ok()`/`err()` Result constructors, `bool ok = ...`, and `catch (str err)`
- **Codegen:** `md5`/`sha1`/`sha256` builtins now emit correct runtime calls
- **Time:** Windows `time_now()` returns Unix seconds (was raw FILETIME ticks); `time_now_ms()`
  returns `long long` (Windows `long` is 32-bit)
- **Makefile:** `make install` and `make release` work on Windows (produce `cnext-windows-x64-10.0.0.zip`)
- **LSP:** tests pass on Windows (CRLF/LF frame handling + corrupt-frame recovery)

## [9.0.0] - 2026

### Added
- **Enum values:** Enum members can now have explicit values (`enum Color { RED = 0xFF0000 }`)
- **Memory pool allocator:** Fast bulk allocation for performance-critical code
- **HashMap:** String-keyed hash map with O(1) lookups (`hashmap_new`, `hashmap_put`, `hashmap_get`)
- **Improved error handling:** Colored error messages with file/line context
- **Enhanced `cnext new`:** Projects now include sample files demonstrating variables, functions, classes, and imports
- **Better install feedback:** `cnext install` shows progress, handles standard modules, and reports success/failure counts
- **Type inference from `new`:** `var dog = new Dog(...)` now correctly infers the class type

### Fixed
- **Critical:** NULL pointer dereferences in runtime (`printin_str`, `printin_cstr`, `cnext_concat`)
- **Critical:** Integer overflow in string operations (`cnext_str_repeat`, `str_repeat`, `cnext_str_replace`)
- **Critical:** `math_abs(INT_MIN)` undefined behavior
- **Critical:** `intcmp` subtraction overflow corrupting `qsort` results
- Class method calls on `var` declarations (`dog.info()` now works)
- Lambda calls inside string interpolation (`"result: {double_it(5)}"`)
- Windows API name collisions (`Rectangle`, `ERROR`, `WARNING`, `OK`)
- Match arms in void functions no longer emit invalid `return` statements
- Method calls on `var` declarations with `new` expressions
- Type inference from `new` expressions for proper symbol resolution
- 70+ runtime, arena, closure, and codegen bugs from pre-release audit
- Documentation version mismatches (Makefile, installation.md)
- Missing types and keywords in documentation

### Changed
- **Version bumped to 9.0.0**
- **Updated documentation:** types.md, tokens.md, installation.md
- **Improved Makefile:** Version numbers corrected

## [8.0.0] - 2026

### Added
- **Type aliases:** `type MyInt = int` for clearer code
- **Nullable types:** `str?`, `int?` for optional values
- **Advanced optimizer:** 9 passes including constant propagation, copy propagation, peephole, loop optimizations
- **Package registry CLI:** `search`, `info`, `login`, `logout`, `update`, `remove` commands
- **Language Server (LSP):** Python-based LSP with autocomplete, hover, go-to-definition
- **Developer tools:** `doctor`, `init`, `upgrade`, `cache`, `config` commands
- **Build flags:** `--release`, `--debug`, `--no-optimize`, `--verbose`, `--clean`
- **String utilities:** 15+ functions (contains, starts_with, ends_with, to_upper, to_lower, trim, find, replace, count, repeat, reverse, to_int, to_double)
- **Registry website and playground:** HTML-based web interface

### Fixed
- **Critical:** Match expression return statements
- **Critical:** Lexer keyword recognition for uint/ushort/ubyte
- **Critical:** `printin` macro for unsigned types

## [6.0.0] - 2026

### Added
- **Semantic Versioning (`semver.h`):** Full semver parsing, comparison, and constraint satisfaction (`^1.2.3`, `~1.2.3`, `>=1.0.0`)
- **Package Registry Client:** Download, resolve, and manage packages from the Cnext registry
- **`cnext publish`:** Publish packages to the registry with tarball creation
- **`cnext search <query>`:** Search the package registry
- **`cnext info <package>`:** Show package versions and metadata
- **Namespaced Module Resolution:** `std/http`, `org/pkg` style imports with version constraints

## [5.5.0] - 2026

### Added
- **Formatter (`cnext fmt`):** Opinionated code formatter for `.cn` files
- **Linter (`cnext lint`):** Static analysis with rules (comparison with literals, assignment in condition, double semicolons)
- **REPL (`cnext repl`):** Interactive read-eval-print loop for quick experimentation
- **Documentation Generator (`cnext doc`):** Auto-generate markdown API docs from source declarations

## [5.0.0] - 2026

### Added
- **Optimizer**: Constant folding and dead code elimination pass
- **Better Diagnostics**: Colored error messages with fix suggestions
- **Source Maps**: Track line mappings from `.cn` to generated `.c` files
- **Incremental Compilation**: `--incremental` flag skips recompilation of unchanged files
- **New CLI Flags**: `--no-optimize`, `--incremental`, `--sourcemap`, `--profile`

### Fixed
- All v4.0/v4.5 bug fixes

### Changed
- Version bumped to 5.0.0
- Updated roadmap with implemented features
- Improved help output with all available options

## [4.5.0] - 2026

### Added
- **FFI (`extern`)**: Declare C functions with `extern "C" { func name(args) : type; }` syntax
- **Benchmark Tool**: `bench { code }` block for timing code execution
- **Ownership Semantics**: `own` keyword for ownership transfer expressions
- **Memory Profiler**: `--profile` flag for allocation tracking and leak detection

### Fixed
- All v4.0 bug fixes

### Changed
- Version bumped to 4.5.0
- Updated roadmap with implemented features

## [4.0.0] - 2026

### Added
- **typeof()**: Runtime type inspection — returns type name as a string
- **constexpr**: Compile-time constant declarations (`constexpr int MAX = 100`)
- **Attributes/Annotations**: `@attribute` syntax on declarations with optional arguments
- **Macro Expansion**: `macro` keyword with compile-time argument substitution
- **Pattern Matching Improvements**: Guard clauses (`match x { pattern if condition => body }`)

### Fixed
- Fixed `unlock` keyword lexer bug (length check was incorrect)
- Fixed `typeof` keyword lexing (was never recognized by lexer)
- Fixed `logic_or` precedence bug (was calling `logic_and` instead of `null_coalesce`)
- Fixed non-portable GCC flags (Windows-only `-lwinhttp`/`-lws2_32` now conditional)

### Changed
- Version bumped to 4.0.0
- Updated roadmap with implemented features

## [3.5.0] - 2026

### Added
- **Coroutines**: `coroutine func` keyword with yield/resume state machines
- **Async/Await**: `async func` and `await` expressions for cooperative async programming
- **Multithreading**: `import thread` with Mutex and Channel primitives
- **Thread Runtime**: Platform-specific implementations (Win32 threads, pthreads)
- **Thread Module**: `mutex_new/lock/unlock/free`, `channel_new/send/recv/free`

### Changed
- Version bumped to 3.5.0
- Updated roadmap with implemented features

## [3.0.0] - 2026

### Added
- **Generics**: Parameterized types for functions and classes
- **Tuples**: `(int, str)` composite types with destructuring
- **Operator Overloading**: Custom `+`, `-`, `*`, `/`, `==`, `[]` operators
- **Default Arguments**: `func foo(int x, int y = 10)`
- **Named Arguments**: `foo(y = 5, x = 3)`
- **Variadic Functions**: `func sum(int... args)`
- **Extension Methods**: Attach methods to existing types
- **Optional/Result Types**: `Option<T>` and `Result<T,E>`
- **Pattern Matching Improvements**: Guard clauses, destructuring patterns

### Changed
- Version bumped to 3.0.0

## [2.5.0] - 2026

### Added
- **OOP**: Inheritance, interfaces, traits, constructors
- **Error Handling**: try/catch/finally with throw
- **Lambdas**: Anonymous functions with `=>` syntax
- **Pattern Matching**: `match` expressions with wildcards
- **Null Safety**: `null` values with safe assignment
- **Type Casting**: `as` keyword
- **Array Properties**: `.length` access
- **Dynamic Arrays**: collections module
- **Map & Set**: collections module
- **Testing Framework**: `test` keyword with `assert`

### Changed
- Version bumped to 2.5.0

## [2.0.0] - 2026

### Added
- **Semantic Analysis**: Compile-time type checking
- **Memory Safety**: Safe `str` struct, bounds checking
- **Standard Library**: Core modules for io, math, file, net, json, os, etc.
- **Package Manager**: `cnext install` with `cnext.toml`
- **Structs & Enums**: User-defined types
- **const**: Immutable variables
- **Switch Statements**: Pattern-based control flow
- **Build Configuration**: `cnext.toml`
- **Project Templates**: `cnext new`

### Changed
- Version bumped to 2.0.0

## [1.1.0] - 2026

### Added
- `var` keyword for type inference
- For-in loops
- String interpolation: `"Hello, {name}!"`
- `import` statement for modules

### Changed
- Version bumped to 1.1.0

## [1.0.0] - 2026

### Added
- Core language syntax (variables, loops, conditions)
- Basic types: `int`, `float`, `bool`, `char`, `str`
- Transpiler architecture (Cnext to C)
- Basic builtin functions (`printin`, `input`)
- Basic OOP support (classes as structs)
- Command-line interface (`cnext run`, `cnext build`)

### Changed
- Initial release
