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

Release archives include the native CLI as `cnext` and stage two as `cnext-selfhost`.
Both can run from the same installation.

To install the stage-two compiler and its tools into a separate prefix, run
`make install-selfhost INSTALL_DIR="/path/to/prefix"` and add the prefix's
`bin` directory to `PATH`. The installed compiler finds the runtime headers
relative to its executable when invoked from a project directory.

## Supported commands

```sh
build/selfhost/stage2_exe new MyProject
build/selfhost/stage2_exe init MyProject
build/selfhost/stage2_exe build app.cn -o app
build/selfhost/stage2_exe run app.cn -- argument
build/selfhost/stage2_exe build app.cn --emit-c app.c
build/selfhost/stage2_exe test
build/selfhost/stage2_exe test tests/core --list
build/selfhost/stage2_exe test tests/core/test_simple.cn --optimize
```

The bootstrap puts a native tools companion (`cnext-tools` or
`cnext-tools.exe`) and its `registry.txt` next to the stage executables.
Keep these files together when moving a build. Both stages use the companion
for `doctor`, `repl`, `upgrade`, `cache`, `config`, `doc`, and package commands.
The self-hosted driver compiles programs directly for `build`, `run`, and `test`.

With `CNEXT_REGISTRY_URL` set to an HTTPS registry, `publish` uploads a gzip
archive containing cnext.toml and src/lib.cn to `/api/publish`. A consumer can declare a dependency
such as `answer = "^0.1.0"` under `[dependencies]` in `cnext.toml` and run
`install`. The registry's `/api/packages/answer` response lists one
`version|description|url` row per release. The URL must serve the importable
CNext source over HTTPS. The installer selects the highest matching version
and writes it to `packages/answer.cn` for use with `import answer`.

For a team registry, set `CNEXT_REGISTRY_URL` in each publisher's and
consumer's environment. A publisher runs `cnext login`, enters the registry
token at the prompt, then runs `cnext publish` from the package directory.
Increment the package version in `cnext.toml` before publishing another release.
A consumer can use `cnext search answer`, `cnext info answer`, and
`cnext update` to discover releases and install the latest version allowed
by its manifest. `cnext remove answer` removes the dependency entry and its
installed module. `cnext logout` removes the local token.

After `make self-build`, run `python3 tests/test_package_workflow.py` to exercise
both projects against a temporary loopback HTTPS registry. The test uses
OpenSSL to create its certificate and keeps credentials in a temporary profile.

### Maintainer review

The reference service in `tools/package_registry.py` requires Python 3.11+
and persistent SQLite storage. To serve it behind an HTTPS reverse proxy:

```sh
python3 tools/package_registry.py --db registry.db serve --public-url https://packages.example.org
```

Forward HTTPS to its loopback listener (port 8080 by default). Configure request
limits on public registration and submission at the proxy. Anyone can obtain a
publisher token with `curl -fsS -X POST "$CNEXT_REGISTRY_URL/api/register"`.
Use that private token at the `cnext login` prompt. Login credentials are scoped
to the exact configured registry URL. Log in again after upgrading an older
unscoped token file.

Submit a manifest and a UTF-8 `src/lib.cn` with `cnext publish`. The reference
registry accepts major.minor.patch versions. Pending content is visible only
to the database operator. Inspect a submission in a new directory, read both
files, then approve or reject that ID:

```sh
python3 tools/package_registry.py --db registry.db pending
python3 tools/package_registry.py --db registry.db inspect 1 review-1
python3 tools/package_registry.py --db registry.db approve 1
```

Use `reject` in place of `approve` to decline. The first approved release assigns
the name to its publisher. Approved versions are immutable.

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

The stage driver implements formatting and linting. Package commands, the REPL,
and documentation generation still use the native companion. Debug/source-map,
incremental-build, and complete optimization-option parity also remain pending.
See the [production plan](production-plan.md) for ownership, ABI, packaging, and
embedded-profile requirements shared by both compilers.
