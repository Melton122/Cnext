# Cnext Modernization Plan

## Goal
Fix critical bugs and modernize the entire Cnext project for cross-platform support, safety, maintainability, and future development.

---

## Phase 1: Critical Cross-Platform Fixes

### 1.1 Makefile Overhaul (`Makefile`)
- Add platform detection (`uname -s` / `OS` variable)
- Set `EXEC` conditionally: `cnext.exe` on Windows, `cnext` on Linux/macOS
- Add `-MMD -MP` for automatic header dependency tracking (generate `.d` files)
- Add `DEBUG=1` variable for `-O0 -g` vs `-O2` release builds
- Add `install` target with `DESTDIR`/`PREFIX` support
- Add `uninstall` target
- Add `format` target (clang-format)
- Add `check` target that runs both `cnext test` and Python test runner
- Add `-lwinhttp -lws2_32` conditionally for Windows only
- Add `-lpthread -lcurl` for Linux/macOS networking (once net.h is ported)
- Remove stale `release/cnext-v3.5-windows/Makefile`

### 1.2 Network Module POSIX Support (`include/net.h`)
- Wrap all Windows includes (`winsock2.h`, `windows.h`, `winhttp.h`, `ws2tcpip.h`) in `#ifdef _WIN32`
- Add POSIX implementation using `libcurl` for HTTP and POSIX sockets for TCP
- Provide stub implementations for platforms where networking is not available
- Add `#ifdef` guards for all functions: `http_get`, `http_post`, `tcp_connect`, etc.

### 1.3 GCC Args Platform Fix (`src/main.c:118-129`)
- Wrap `-lwinhttp -lws2_32` in `#ifdef _WIN32` block
- Add `-lpthread -lcurl` for Linux/macOS when networking is used
- Make the linker flags array dynamic based on platform

### 1.4 Output Extension Fix (`include/main_internal.h`)
- Already has `CNEXT_DEFAULT_EXE` with platform-conditional value -- verify it works
- Ensure the Makefile and VS Code extension respect this

---

## Phase 2: Source Code Safety & Quality

### 2.1 Memory Safety Fixes (HIGH priority)
- **`src/linter.c:27`**: Replace raw `realloc` with `checked_realloc` or add NULL check
- **`src/sourcemap.c:20`**: Same -- add NULL check on `realloc`
- **`src/formatter.c:24`**: Same -- add NULL check on `realloc`
- **`include/json.h:52,62,71,166,172,180,191,197,209,221,222,231,232,249,261,269`**: Add NULL checks for all `malloc`/`realloc` calls, or use `checked_malloc`/`checked_realloc`
- **`src/optimizer.c:35,49`**: Add NULL check for `malloc`
- **`src/codegen_types.c:7,44,70`**: Add NULL checks for `strdup`/`strndup` (or use `checked_strdup`)
- **`src/sourcemap.c:11-12`**: Add NULL checks for `strdup`
- **`src/main_packages.c:163`**: Add NULL check for `strdup`

### 2.2 Command Injection Fix (`src/main_packages.c:76,84,95`)
- Replace `system()` calls with safe process execution (`run_process` or `popen`)
- Sanitize URLs before passing to shell commands

### 2.3 Buffer Safety Improvements
- **`src/repl.c:30`**: Increase `last_generated_c` buffer or use dynamic allocation
- **`src/repl.c:144`**: Use dynamic `snprintf` with size checking
- **`src/codegen_expr.c:131,479`**: Use `snprintf` instead of `strncpy` for safer string construction
- **`src/codegen.c:310,321,335`**: Increase line buffer from 4096 to 8192 or use dynamic reading

### 2.4 Error Handling Improvements
- **`src/repl.c:186`**: Check `fread` return value
- **`src/linter.c:120`**: Check `fread` return value
- **`src/formatter.c:164`**: Check `fread` return value
- **`src/linter.c:116-118`**: Check `fseek`/`ftell` return values
- **`src/formatter.c:160-162`**: Same
- **`src/repl.c:182-184`**: Same

### 2.5 Static Counter Reset (`src/codegen_node.c`)
- Add reset for `bench_counter` (line 328), `loop_counter` (line 463), `try_counter` (line 580), `match_counter` (line 629) in `reset_codegen_state()`

### 2.6 Coding Style Consistency
- Replace `()` with `(void)` in no-parameter function declarations:
  - `include/lexer.h:162`: `Token next_token()` -> `Token next_token(void)`
  - `src/lexer.c:22`: `is_at_end()` -> `is_at_end(void)`
  - `src/parser_core.c:86,113`: Add `(void)` to empty-parameter functions
- Use `checked_strdup`/`checked_strndup` consistently instead of raw `strdup`/`strndup`
- Use `diagnostics.h` color macros instead of hardcoded ANSI escape codes in `parser_core.c`

### 2.7 Deprecated API Replacement
- **`include/json.h:169`**: Replace `atof()` with `strtod()`

---

## Phase 3: Build System & CI/CD

### 3.1 CI Workflow Modernization (`.github/workflows/ci.yml`)
- Add `macos-latest` to the OS matrix
- Add Python test runner step: `python tests/run_tests.py` (or `python3`)
- Add caching for build artifacts
- Add `concurrency` group to cancel stale runs
- Upload build artifacts with `actions/upload-artifact`
- Add `-Werror` build variant in CI

### 3.2 Release Automation (new `.github/workflows/release.yml`)
- Trigger on version tags (`v*`)
- Build binaries for Linux, macOS, Windows
- Create GitHub release with artifacts
- Generate SHA-256 checksums

### 3.3 Version Embedding
- Add `-DCNEXT_VERSION` flag in Makefile derived from a `VERSION` file or git tag
- Remove hardcoded `#define CNEXT_VERSION "6.0"` from `main_internal.h` or make it fallback

---

## Phase 4: VS Code Extension Modernization

### 4.1 Replace `execSync` with Async Execution (`vscode-extension/extension.js`)
- Replace all `execSync` calls with `child_process.exec` (callback-based) or `util.promisify(exec)`
- Use VS Code `Task` API for build/run commands to get output panel integration
- Add proper error handling with user feedback

### 4.2 Add Output Channel
- Create `vscode.window.createOutputChannel('Cnext')` for compiler output
- Show build/run results in output channel instead of toast notifications

### 4.3 Platform-Aware Build Output
- **`extension.js:63`**: Replace hardcoded `.exe` with platform detection
- Use `process.platform === 'win32'` to conditionally append `.exe`

### 4.4 Package.json Updates (`vscode-extension/package.json`)
- Bump version to match language version (or use independent scheme)
- Remove deprecated `activationEvents` field (auto-inferred from `contributes.languages`)
- Add `keybindings` section for `Ctrl+Shift+R`, `Ctrl+Shift+B`, `Ctrl+Shift+T`
- Add `license` field
- Update `engines.vscode` to current minimum

### 4.5 Cleanup
- Remove dead TypeScript entries from `.vscodeignore`
- Fix README version references

---

## Phase 5: Documentation Updates

### 5.1 Architecture Doc (`docs/architecture.md`)
- Update to reflect v6.0 reality: semantic analyzer is implemented, not "Future"
- Update pipeline description to include all current stages

### 5.2 Installation Doc (`docs/installation.md`)
- Replace hardcoded "v3.5" with current version
- Add Linux and macOS installation instructions

### 5.3 Specification Doc (`docs/specification.md`)
- Mark as "v1.x base specification" or update with current features
- Add sections for generics, closures, pattern matching, async/await, threading

### 5.4 Roadmap (`docs/roadmap.md`)
- Mark unimplemented items as "Planned" not "Implemented"
- Add items for cross-platform CI, documentation website, etc.

### 5.5 Getting Started (`docs/getting-started.md`)
- Add community links (GitHub Discussions, etc.)

### 5.6 New Documentation Files
- Create `docs/README.md` as documentation index/table of contents
- Add `docs/async.md` (async/await)
- Add `docs/packaging.md` (package manager)
- Add `docs/tools.md` (fmt, lint, repl, doc commands)

---

## Phase 6: Repository Hygiene

### 6.1 `.gitignore` Updates
- Add: `*.zip`, `*.tar.gz`, `node_modules/`, `.mimocode/`, `registry.txt`
- Add: `*.d`, `*.dSYM` (dependency files, macOS debug symbols)

### 6.2 Remove Stale Artifacts
- Remove tracked `.o` files from git index (they should be gitignored)
- Remove `test_temp_20940.c` from repo root
- Remove `release/cnext-v3.5-windows/` (stale v3.5 snapshot)
- Remove `release/cnext-v3.5-windows.zip` (stale)
- Remove `registry.txt` from tracked files

### 6.3 License Year
- Update `LICENSE` copyright from `2024` to `2024-2026`

### 6.4 CHANGELOG
- Add month/day granularity to dates
- Add comparison links at bottom

### 6.5 CONTRIBUTING.md
- Add Cnext-specific code style guidelines
- Add build system documentation
- Add test running instructions
- Add commit message conventions

---

## Verification

After implementation, verify by:
1. `make clean && make` -- builds successfully on Windows
2. `make DEBUG=1` -- debug build works
3. `make test` -- all tests pass
4. `python tests/run_tests.py` -- Python test suite passes
5. VS Code extension loads without errors in Extension Development Host
6. `cnext run examples/hello.cn` -- produces expected output
7. `cnext build examples/hello.cn` -- produces working executable
8. `cnext fmt`, `cnext lint`, `cnext repl` -- all subcommands work

---

## Execution Order

Phases 1-2 are highest priority (critical bugs and safety). Phase 3 enables future development. Phase 4-6 improve DX and maintainability. Execute in order, committing after each phase.
