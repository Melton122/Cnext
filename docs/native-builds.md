# Mixed Cnext and C projects

`cnext build` and `cnext run` read native build inputs from `cnext.toml` in the
current working directory. This uses the same native linking support as the CLI
flags, without running scripts or install hooks.

```toml
[native]
c_sources = ["native/calibration.c"]
objects = []
include_dirs = ["native/include"]
library_dirs = ["native/lib"]
libraries = ["calibration_support"]

[native.windows]
libraries = ["ws2_32"]

[native.linux]
libraries = ["m"]

[native.macos]
libraries = ["m"]
```

The native and self-hosted compilers use one C parser for these settings.

| Setting | Equivalent CLI flag | Value |
|---|---|---|
| `c_sources` | `--c-source` | C source file paths |
| `objects` | `--object` | Precompiled objects or static archives |
| `include_dirs` | `--include-dir` | C header search directories |
| `library_dirs` | `--library-dir` | Library search directories |
| `libraries` | `--link-lib` | Library names without the platform prefix or extension |

All settings are arrays of strings. Relative paths resolve against the directory
containing the manifest. Absolute paths remain absolute. Sources and objects are
passed before named libraries. Library order is preserved: list a library before
the libraries it depends on.

Common `[native]` settings are applied first, then the current host's platform
table, then CLI native flags. CLI flags append; they do not replace manifest
settings. Duplicate source files are not deduplicated. A shared library must also
be discoverable by the operating system when the executable runs.

## Selecting settings

```powershell
cnext run src/main.cn
cnext build src/main.cn --manifest config/cnext.toml
cnext run src/main.cn --no-native-config --c-source alternate.c
```

`--manifest` requires the selected file to exist. Without it, a missing
`cnext.toml` is allowed. `--no-native-config` disables automatic loading and
cannot be combined with `--manifest`.

An explicit manifest selects native settings only: it does not select an entry
point, change the working directory, or relocate package resolution. Input,
output, and CLI paths retain their normal working-directory interpretation.

`--emit-c` and the self-hosted compiler's legacy C-emission mode do not load or
execute native build settings. Compile and link the generated C separately with
the required native inputs. Explicit native CLI flags still cannot be combined
with C emission.

## Supported manifest syntax

Native tables use the exact names shown above. Keys are unquoted. Values may use
single-quoted literal strings or double-quoted strings with escaped quotes and
backslashes. Arrays may span lines, contain comments, and have a trailing comma.
On Windows, single-quoted strings are convenient for literal backslashes:

```toml
[native]
c_sources = [
    'native\helper.c',
]
include_dirs = ["native\\include"]
```

Unknown native tables/keys, duplicate native tables/keys, empty values, unsupported
escapes, control characters, zero bytes, and malformed arrays fail before linking.
Inactive platform tables are syntax checked but contribute no build inputs.
Other project and dependency sections are left to their existing consumers. This
is a native-settings parser, not a complete TOML implementation: dotted assignments,
quoted table/key spellings, Unicode escape sequences, and inline tables are not
supported native syntax.

There is a 1 MiB manifest limit, a 4095-byte limit for an individual value or
manifest path, and a 128-argument limit for each native/library group, including
CLI additions. Search flags and named libraries each consume two arguments;
sources and object paths consume one. Drive-relative paths such as `C:helper.c`
are rejected; use project-relative paths or an absolute drive path.

## Runnable example

From the repository root:

```powershell
.\cnext.exe run examples/c_interop/main.cn --manifest examples/c_interop/cnext.toml
```

The example combines Cnext code with a C component and prints
`Calibrated reading: 42`.

Version resolution, lockfiles, transitive native package graphs, cross-compilation
targets, and native build-cache invalidation remain separate production milestones.
The current platform tables describe the host used by the build driver.
