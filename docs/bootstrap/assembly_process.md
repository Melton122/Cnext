# Cnext Bootstrap Assembly Process

## Current verification (2026-11-21)

`make self-build` assembles `src2`, builds stage 1 and stage 2, and uses stage 2
to generate stage 3's C. The generated C is identical across these stages
after line-ending normalization. Mismatches fail the command and leave a diff
in `build/selfhost`. Bootstrap compilation defaults to `-O0` for lower memory
use; `BOOTSTRAP_OPT=-O2` selects optimized binaries.

Both self-hosted stages pass 25 runtime bootstrap programs and reject escaping
local reference captures. Run the existing
stage binaries without rebuilding with:

```bash
python tests/test_bootstrap_codegen.py --stage1 build/selfhost/stage1_exe
python tests/test_bootstrap_codegen.py --stage1 build/selfhost/stage2_exe
python tests/test_selfhost_cli.py --compiler build/selfhost/stage2_exe
```

Append `.exe` on Windows. The wider source comparison is 100/109 identical;
full language equivalence and the complete development CLI remain separate
work. See [the current self-hosting status](../self_hosting.md).

The remaining sections are historical records of earlier milestones.

## Verified Facts

### Stage 0 (Trusted Bootstrap) - WORKING
- Build: `make` in repo root produces `cnext.exe` (Windows) from `src/*.c`
- All tests pass: `./cnext.exe test` → 106 passed, 0 failed
- Python tests: `python tests/run_tests.py` all pass
- LSP tests: `python tests/test_lsp.py` all pass
- Lexer parity: passes (157 files)

### Self-hosted Components
- **Lexer** (`src2/lexer.cn`): Fully self-hosted, passes parity tests. Can be compiled by Stage 0.
  - Build: `./cnext.exe build src2/lexer.cn -o lexer2.exe` or via bundles
  - Test: `./lexer2.exe <file.cn>` dumps tokens
- **Parser** (`src2/parser*.cn`): Ported, compiles as bundle. `src2/parser_bundle.cn` compiles cleanly with Stage 0.
  - Build: `./cnext.exe build src2/parser_bundle.cn -o parser_selfhost.exe`
  - Test: `./parser_selfhost.exe <file.cn>` dumps AST
- **Semantics/Codegen**: Modules exist in src2/ but not integrated into a full working compiler bundle yet.

## Assembly Methods

### Method 1: Use pre-built bundles (parser)
```bash
./cnext.exe build src2/parser_bundle.cn -o build/bootstrap/parser1.exe
./build/bootstrap/parser1.exe tests/bootstrap/test_add.cn
```

### Method 2: Build bundles with build_bundle.py
```bash
python tools/build_bundle.py parser -o /tmp/parser_bundle.cn
./cnext.exe build /tmp/parser_bundle.cn -o parser_from_bundle.exe
```

Note: build_bundle.py strips mains from lexer.cn and parser.cn. Custom mains may need to be added.

## Testing Bootstrap Components

Bootstrap tests in `tests/bootstrap/` cover:
- test_hello.cn - hello world
- test_add.cn - function calls
- test_vars.cn - variables and types
- test_if.cn - conditionals
- test_while.cn - loops
- test_recursive.cn - recursion
- test_array.cn - arrays

All pass with Stage 0 compiler.

## Blockers for Full Compiler Bootstrap

1. **No complete compiler bundle** - Need lexer + parser + semantics + codegen + driver all integrated
2. **API mismatches** - src2/main.cn expects different APIs than modules provide
3. **Missing components** - formatter, linter, repl, optimizer not ported
4. **Integration issues** - Large bundles cause compilation issues
5. **Parser correctness** - Some edge cases may have issues

## Comparison Method (non-binary)
When comparing stages, do NOT require byte-identical binaries. Instead compare:
- Exit codes
- Program stdout/stderr
- Functional behavior
- AST/token parity where applicable
- Test pass/fail rates

## Progress Update (2025-10-28)

### Stage 1 Achieved (Partial, 2025-10-28 snapshot)
Successfully built self-hosted parser + semantics using Stage 0 compiler:
- Built: `build/bootstrap/cnext1_parser_sem2.exe` from assembled src2 modules
- Compilation: Stage 0 (cnext.exe from src/) compiled the self-hosted bundle
- Functionality: Parses and performs semantic analysis on Cnext programs
- Tested on all bootstrap tests (test_hello, test_add, test_vars, test_if, test_while, test_recursive, test_array) - all pass with output "OK"

This proves:
1. Stage 0 can compile substantial self-hosted compiler components from src2/
2. Self-hosted lexer, parser, and semantics work correctly together
3. The bootstrap path is viable

### Next Steps
- Add codegen to complete full compilation pipeline
- Create proper driver with full CLI
- Test Stage 2 (cnext1 compiling src2 to produce cnext2)

## Stage Comparison (2025-10-28)

| Test | Stage 0 (C compiler) | Stage 1 (self-hosted parser+sem) | Status |
|------|---------------------|-----------------------------------|--------|
| test_hello.cn | OK (prints "hello") | OK (parses+analyzes) | ✓ Both succeed |
| test_add.cn | OK (prints "5") | OK | ✓ Both succeed |
| test_vars.cn | OK (prints "30\ntest") | OK | ✓ Both succeed |
| test_if.cn | OK (prints "yes") | OK | ✓ Both succeed |
| test_while.cn | OK (prints "0\n1\n2") | OK | ✓ Both succeed |
| test_recursive.cn | OK (prints "8") | OK | ✓ Both succeed |
| test_array.cn | OK (prints "1\n2\n3") | OK | ✓ Both succeed |

Note: Stage 1 only does parsing and semantic analysis (no codegen), so it outputs "OK" rather than running the program. Both stages correctly validate all test programs.

At this point, Stage 1 only parsed and analyzed these programs; they were not compiled and run by Stage 1.

## Progress Update (2026-11-02)

### Stage 1 code generation
- `python tools/build_bundle.py codegen -o build/bootstrap/codegen.cn` assembles the compiler bundle and driver.
- Stage 0 builds the bundle into `build/bootstrap/cnext1.exe`.
- Stage 1 accepts `cnext <file.cn> [-o <output.c>]`; without `-o`, it writes generated C to stdout.
- Parse, semantic, usage, and output-file failures return nonzero exit statuses.
- `make bootstrap-test` checks generated-file output and compiles/runs all seven programs in `tests/bootstrap/`; all pass.

### Stage 2 attempt
Stage 1 can generate C for the full compiler bundle, but that C does not yet compile. The current simplified code generator does not lower core language/runtime features used by the compiler itself, including string operations and built-in helpers (`to_str`, `str_char_at`, `len`), and it does not emit correct C types for all declarations. This is the current blocker for Stage 2; passing the seven bootstrap programs only validates the small tested subset.

Next, expand expression and declaration code generation for the constructs used in `src2/`, then add a Stage 2 build-and-test gate once the generated compiler C compiles.
