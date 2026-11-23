#!/usr/bin/env python3
"""Validate stable, actionable compiler diagnostics."""

import argparse
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;]*m")


def compile_source(compiler, source):
    with tempfile.TemporaryDirectory(prefix="cnext-diagnostics-", dir=ROOT / "build") as temp_dir:
        temp = Path(temp_dir)
        source_path = temp / "case.cn"
        output_path = temp / ("case.exe" if os.name == "nt" else "case")
        source_path.write_text(source, encoding="utf-8")
        result = subprocess.run(
            [str(compiler), "build", str(source_path), "-o", str(output_path)],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=30,
        )
    return result.returncode, ANSI_ESCAPE.sub("", result.stdout + result.stderr)


def require(label, output, needles):
    missing = [needle for needle in needles if needle not in output]
    if missing:
        raise RuntimeError(f"{label} is missing {missing}:\n{output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--compiler",
        default=ROOT / ("cnext.exe" if os.name == "nt" else "cnext"),
        type=Path,
    )
    args = parser.parse_args()
    compiler = args.compiler
    if not compiler.is_absolute():
        compiler = ROOT / compiler

    (ROOT / "build").mkdir(exist_ok=True)

    return_code, output = compile_source(
        compiler,
        'func ready(): bool { return "yes" }\nmain { printin(ready()) }\n',
    )
    if return_code == 0:
        raise RuntimeError("Return type mismatch unexpectedly compiled")
    require(
        "return mismatch diagnostic",
        output,
        [
            "error [E119] [1:22]",
            "| func ready(): bool { return \"yes\" }",
            "^",
            "= expected: bool",
            "= found: str",
            "= help: Return an expression of the declared type",
            "= rule: CN-E119",
        ],
    )
    print("PASS return mismatch diagnostics")

    return_code, output = compile_source(compiler, "main {\n    var x = 1\n")
    if return_code == 0:
        raise RuntimeError("Missing brace unexpectedly compiled")
    require(
        "missing brace diagnostic",
        output,
        [
            "error [E002] [3:1]",
            "= expected: Expect '}' after block.",
            "= found: end of file",
            "= rule: CN-E002",
        ],
    )
    print("PASS parser diagnostics")

    return_code, output = compile_source(
        compiler,
        'main {\n    int x = 1\n    x = "hello"\n}\n',
    )
    if return_code == 0:
        raise RuntimeError("Assignment type mismatch unexpectedly compiled")
    require(
        "assignment mismatch diagnostic",
        output,
        [
            "error [E101]",
            "Type mismatch in assignment",
            "= rule: CN-E101",
        ],
    )
    print("PASS assignment diagnostics")

    long_line = "main {\n    int x = 1\n    x = " + (" " * 2000) + '"hello"\n}\n'
    return_code, output = compile_source(compiler, long_line)
    if return_code == 0:
        raise RuntimeError("Long-line assignment mismatch unexpectedly compiled")
    require(
        "long-line diagnostic",
        output,
        ["error [E101]", "Type mismatch in assignment", "= rule: CN-E101"],
    )
    max_line = max((len(line) for line in output.splitlines()), default=0)
    if max_line > 400:
        raise RuntimeError(f"Diagnostic output is unbounded: longest line {max_line}")
    print("PASS bounded diagnostic output")

    return_code, output = compile_source(
        compiler,
        "main {\n    int x = when {\n        1 =>\n    }\n}\n",
    )
    if return_code == 0:
        raise RuntimeError("Malformed when arm unexpectedly compiled")
    require(
        "malformed when diagnostic",
        output,
        ["error [E001]", "Expect expression.", "= rule: CN-E001"],
    )
    print("PASS malformed when arm terminates")

    for label, source, needle in (
        (
            "switch",
            'main {\n    int x = 1\n    switch x {\n        case 1\n            printin("one")\n    }\n}\n',
            "Expect ':' after case expression.",
        ),
        (
            "match",
            'main {\n    int x = 1\n    match x {\n        1\n            printin("one")\n    }\n}\n',
            "Expect '=>' after match pattern.",
        ),
    ):
        return_code, output = compile_source(compiler, source)
        if return_code == 0:
            raise RuntimeError(f"Malformed {label} unexpectedly compiled")
        require(
            f"malformed {label} diagnostic",
            output,
            ["error [E002]", needle, "= rule: CN-E002"],
        )
        print(f"PASS malformed {label} arm terminates")

    return_code, output = compile_source(
        compiler,
        "main {\n    long wide = 1\n    int narrow = wide\n}\n",
    )
    if return_code == 0:
        raise RuntimeError("Numeric narrowing unexpectedly compiled")
    require(
        "numeric narrowing diagnostic",
        output,
        [
            "error [E101]",
            "Initializer type does not match variable type",
            "= expected: int",
            "= found: long",
            "= rule: CN-E101",
        ],
    )
    print("PASS numeric narrowing diagnostics")

    return_code, output = compile_source(
        compiler,
        "main {\n    long wide = 1\n    int narrow = wide as int\n    printin(narrow)\n}\n",
    )
    if return_code != 0:
        raise RuntimeError(f"Explicit narrowing cast unexpectedly failed:\n{output}")
    print("PASS explicit narrowing cast")

    return_code, output = compile_source(
        compiler,
        "func f(): int {\n    long wide = 1\n    return wide\n}\nmain { printin(f()) }\n",
    )
    if return_code == 0:
        raise RuntimeError("Numeric return narrowing unexpectedly compiled")
    require(
        "numeric return narrowing diagnostic",
        output,
        [
            "error [E119]",
            "Return type mismatch",
            "= expected: int",
            "= found: long",
            "= rule: CN-E119",
        ],
    )
    print("PASS numeric return narrowing diagnostics")

    return_code, output = compile_source(
        compiler,
        'func maybe(): int { return 1 }\nfunc f(): int {\n    maybe()?;\n    return 1\n}\nmain { printin(f()) }\n',
    )
    if return_code == 0:
        raise RuntimeError("Try propagation in a typed function unexpectedly compiled")
    require(
        "try propagation diagnostic",
        output,
        [
            "error [E101]",
            "Try expression requires a void function",
            "= rule: CN-E101",
        ],
    )
    print("PASS try propagation diagnostics")

    guard_files = [
        ROOT / "include" / "ast.h",
        ROOT / "tools" / "astdump.c",
        ROOT / "src2" / "parser_types.cn",
        ROOT / "src2" / "parser_bundle.cn",
    ]
    for path in guard_files:
        if "AST_GUARD" in path.read_text(encoding="utf-8"):
            raise RuntimeError(f"{path.relative_to(ROOT)} still contains AST_GUARD")
    print("PASS AST_GUARD removal")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
