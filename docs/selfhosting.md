# Self-hosted compiler status

The compiler in `src2/` is written in CNext. It parses and checks programs, emits
C, and uses GCC and the shared C runtime to produce executables. The native C
compiler in `src/` remains the bootstrap and a behavior reference.

Build and verify it with:

```sh
make self-build
make selfhost-check
```

`self-build` produces stage-one and stage-two executables under `build/selfhost/`
and checks generated-C parity and the stage-two rebuild fixed point.
`selfhost-check` also runs optimizer, test-command, CLI, and behavioral parity
checks. CI runs additional language, ownership, native-manifest, and bootstrap
fixtures on both stages. The Windows executable suffix is `.exe`.

## Supported commands

```sh
build/selfhost/stage2_exe build app.cn -o app
build/selfhost/stage2_exe run app.cn -- argument
build/selfhost/stage2_exe build app.cn --emit-c app.c
build/selfhost/stage2_exe test
build/selfhost/stage2_exe test tests/core --list
build/selfhost/stage2_exe test tests/core/test_simple.cn --optimize
```

The legacy `compiler file.cn -o output.c` spelling still emits C. Native sources,
objects, headers, and libraries can be supplied through CLI options or the
[native manifest](native-builds.md). Runtime headers can be selected with
`--runtime-dir` or `CNEXT_INCLUDE`.

## Optimizer

`--optimize` enables the self-hosted AST optimizer after all semantic and lifetime
checks. It is off by default. `--no-optimize` overrides it in either option order,
including when passed to `test`. These options control CNext rewrites; generated
programs still use the driver's normal GCC settings.

The current pass folds checked signed-int32 literal arithmetic and comparisons,
boolean constants, decisive short-circuit expressions, literal-condition `if`
branches, and never-entered `while` loops. It preserves lexical blocks, semantic
metadata, binding identity, and required side effects. It leaves overflow,
ambiguous leading-zero integer spellings, floating-point arithmetic, wide/unsigned
arithmetic, propagation, and reassociation to the original expression/backend.
This is a conservative subset of native optimizer transformations, rather than a
claim of identical optimization coverage. Differential tests compare behavior;
dedicated emitted-C tests ensure that real rewrites occur.

## Test command

`test` recursively discovers `.cn` files under `tests` by default. It supports a
file, directory, or substring filter, and `--list` prints deterministic relative
paths. Hidden directories, `bootstrap`, and `optimizer` are excluded from recursive
discovery; those fixtures have separate harnesses. Explicit files can still be run.

Each test is built by the same self-hosted executable, then run using argv-based
process execution. Input and merged stdout/stderr go through temporary files;
paths and arguments are not shell commands. A failure does not stop later tests,
and any failed test makes the runner exit nonzero.

Source comments can declare:

```cnext
// STDIN: hello
// EXPECT: hello
// EXIT: 0
main { printin(input()) }
```

Each `STDIN` marker contributes a line, including an empty one. Without markers,
the test receives EOF. `EXPECT` values must appear in order in the captured output;
they are substrings rather than an exact-output assertion. A successful run also
requires exit status zero unless one explicit `EXIT` marker declares a value from
0 through 255. Matching output alone does not excuse a failed process. Compile
errors always fail. Malformed metadata is an error.

Limits are 512 files, 32 directory levels, 512 expectations per file, 16 KiB of
declared input, and 1 MiB of captured run output. There is currently no child
timeout or live output-size enforcement; CI/job limits are still needed for
nonterminating or excessively verbose programs.

## Remaining parity work

Package installation/update/resolution commands, formatter, linter, REPL, and
documentation-generation commands remain native-driver work. Debug/source-map,
incremental-build, and complete optimization-option parity also remain pending.
See the [production plan](production-plan.md) for ownership, ABI, packaging, and
embedded-profile requirements shared by both compilers.
