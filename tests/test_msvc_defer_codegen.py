#!/usr/bin/env python3
"""Validate the MSVC defer lowering and compile/run it when MSVC is installed."""

import argparse
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def run(command, **kwargs):
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, **kwargs)
    if result.returncode:
        raise RuntimeError(
            f"Command failed ({result.returncode}): {' '.join(map(str, command))}\n"
            f"{result.stdout[-4000:]}{result.stderr[-4000:]}"
        )
    return result


def find_vcvarsall():
    if os.name != "nt":
        return None
    roots = [
        Path("C:/Program Files/Microsoft Visual Studio"),
        Path("C:/Program Files (x86)/Microsoft Visual Studio"),
    ]
    for root in roots:
        if not root.exists():
            continue
        for path in root.glob("**/VC/Auxiliary/Build/vcvarsall.bat"):
            return path
    return None


def compile_with_msvc(c_source, executable):
    vcvarsall = find_vcvarsall()
    if not vcvarsall:
        print("SKIP MSVC compile/run: vcvarsall.bat not found")
        return False
    include = ROOT / "include"
    command = (
        f'"{vcvarsall}" x86 && '
        f'cl /nologo /std:c11 /experimental:c11atomics /I"{include}" '
        f'/Fo:"{executable.with_suffix(".obj")}" '
        f'/Fe:"{executable}" "{c_source}" /link /NODEFAULTLIB:OLDNAMES.lib'
    )
    result = subprocess.run(command, shell=True, cwd=executable.parent, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(
            f"MSVC compile failed ({result.returncode})\n"
            f"{result.stdout[-4000:]}{result.stderr[-4000:]}"
        )
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--source", type=Path, default=ROOT / "tests" / "test_defer_matrix.cn")
    args = parser.parse_args()

    compiler = Path(args.compiler)
    if not compiler.is_absolute():
        compiler = ROOT / compiler

    build_dir = ROOT / "build"
    build_dir.mkdir(exist_ok=True)
    source = args.source if args.source.is_absolute() else ROOT / args.source
    with tempfile.TemporaryDirectory(prefix="cnext-msvc-defer-", dir=build_dir) as temp_dir:
        c_source = Path(temp_dir) / "test_defer_matrix.c"
        run([compiler, "build", source, "--emit-c", c_source])
        generated = c_source.read_text(encoding="utf-8")

        checks = {
            "file-scope defer helper": re.search(
                r"^static void _cnext_defer_fn_\d+\(void\* _d\) \{", generated, re.M
            ) is not None,
            "defer helper guard": re.search(r"if \(\*_f_\d+ & 1\) \{ return; \}", generated) is not None,
            "defer helper flag": re.search(r"\*_f_\d+ \|= 1;", generated) is not None,
            "defer registration": re.search(
                r"_cnext_defer_push\(_cnext_defer_fn_\d+, &_cnext_defer_flag_\d+\);", generated
            ) is not None,
            "scope mark": re.search(r"^\s+volatile int _cnext_defer_scope_mark_\d+ = _cnext_defer_count;", generated, re.M) is not None,
            "try frame": "__try {" in generated,
            "finally frame": "} __finally {" in generated,
            "finally cleanup": re.search(r"_cnext_defer_run_to\(_cnext_defer_scope_mark_\d+\);", generated) is not None,
            "no GNU defer cleanup attribute": "__attribute__((cleanup(_cnext_defer_fn_" not in generated,
            "no inline GNU helper": re.search(
                r"^\s+void _cnext_defer_fn_\d+\(void\* _d\) \{ unsigned int\*", generated, re.M
            ) is None,
        }
        failures = [name for name, ok in checks.items() if not ok]
        if failures:
            raise RuntimeError("MSVC defer lowering check failed: " + ", ".join(failures))
        print("PASS MSVC defer lowering shape")

        executable = Path(temp_dir) / "test_defer_matrix.exe"
        if not compile_with_msvc(c_source, executable):
            return 0
        actual = run([executable]).stdout
        expected = "".join(
            line[len("// EXPECT: "):] + "\n"
            for line in source.read_text(encoding="utf-8").splitlines()
            if line.startswith("// EXPECT: ")
        )
        if actual != expected:
            raise RuntimeError(f"MSVC defer output mismatch:\n{actual!r}\n!=\n{expected!r}")
        print("PASS MSVC compile/run")
    return 0


if __name__ == "__main__":
    sys.exit(main())
