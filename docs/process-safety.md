# Process execution safety

Cnext has two ways to start a program, and they have very different safety
properties. Pick deliberately.

| API | Input | Shell involved | Safe with untrusted input |
|---|---|---|---|
| `process_spawn(str[] argv)` | argv vector | no | yes |
| `process_run`, `process_run_status` | one command string | yes (`system`/`popen`/`cmd.exe`) | **no** |
| `sys_exec`, `sys_shell` | one command string | yes | **no** |
| `os_exec`, `os_exec_output`, `os_exec_status` | one command string | yes | **no** |

## `process_spawn` — argv, no shell

```cnext
import process

main {
    str[] args = {"git", "log", "--oneline", "-n", "5"}
    int status = process_spawn(args)
    printin(status)
}
```

- `argv[0]` is the program, resolved through `PATH`; every later element is
  passed to the child as exactly one argument.
- No shell is started, so `;`, `&`, `|`, `>`, `$()`, backticks, wildcards and
  quotes are ordinary characters rather than syntax.
- Returns the child's exit status, `127` when the program could not be started,
  and `-1` when the array is empty or an element is unusable.
- Cnext strings may be slices, so the length (not a trailing NUL) decides where
  each argument ends; the call copies them into NUL-terminated buffers.
- The call waits for the child. There is no detach or stdin wiring yet.

On Windows the argument vector is rebuilt into a command line with the
`CommandLineToArgvW` rules: each token is quoted, runs of backslashes are
doubled, and a run that precedes a literal `"` gains one extra backslash. Any
argument containing spaces, quotes or trailing backslashes therefore arrives
intact instead of splitting into extra arguments or closing the quoting context.

`cmd.exe` is a special case: it re-parses the remainder of its own command line,
so `process_spawn({"cmd", "/c", "echo", "hello"})` sees the embedded quotes and
mis-parses. When you genuinely need `cmd`, pass the whole command as one
argument (`{"cmd", "/c", "echo hello"}`) or use a real executable.

## Shell-string APIs are not a parsing boundary

`process_run*`, `sys_*` and `os_exec*` hand their input to the host shell
(`system`, `popen`, `cmd.exe /c`). Cnext does not escape, quote or validate it.
Any value that reaches these functions from a file, a network response, a CLI
argument or a package must be treated as an injection risk — the caller is
responsible for producing a safe command line, or for switching to
`process_spawn`.

The shell variants exist for interactive one-liners and for running user-typed
commands (the REPL, `cnext run`), where the input is already trusted.

## Compiler-internal process use

The compiler itself never builds shell strings for its own work:

- `run_process` / `run_process_captured` (`src/main_utils.c`) take an argv array.
  On Windows both serialize the vector with the same quoting rules described
  above before calling `_spawnvp` / `CreateProcessA`; on POSIX they `execvp` the
  array directly.
- Tool invocations (GCC, `curl`, `tar`, the test runner) pass fixed argv arrays.

Package installation is the only place where names and URLs come from outside:

- **Package names** are checked by `is_safe_package_name` before they are used
  in a path or a command: no leading `-`/`.`/`/`, no `..`, no empty or doubled
  `/`, no control characters, and no `\ : " ' ` $ ; & | < > ( ) { } * ? !`.
- **URLs** are checked by `is_safe_url`: non-empty, not starting with `-` (so a
  URL cannot turn into a curl option), and free of `"`, `'`, backtick, `$` and
  backslash.
- Downloads invoke `curl` and `wget` with an argv array and the `--` terminator
  in front of the URL, so a dash-leading or option-looking URL is an operand.
- The Windows `powershell -Command` fallback emits both the URL and the output
  path as PowerShell **single-quoted literals** with `'` doubled, so neither
  operand can terminate the script and inject commands.
- Registry HTTP uses HTTPS only; plain `http://` URLs are refused. The auth
  token is passed as an `Authorization` header built in the compiler, never
  through a shell.

## Checklist for new process code

1. Default to `process_spawn` (Cnext) / `run_process` (compiler C).
2. If a shell is unavoidable, keep untrusted values out of the command string,
   or validate them against a strict allowlist first.
3. Never interpolate a value into a Windows shell command line without quoting
   it for that shell; `CommandLineToArgvW` quoting is not `cmd.exe` quoting.
4. Put `--` before operands in external tools that accept options.
5. Validate names used to build filesystem paths, not just command arguments.
