#!/usr/bin/env python3
"""Rebuild the self-hosted parser and report AST parity against the C parser.

Thin wrapper around the build + `tests/parity_parser.py` steps so the
Milestone B loop is one command. Prints the mismatching file names and the
first differing hunk per file, then the pass/fail tally.

Usage:
    python tools/parser_parity.py                 # tests/*.cn
    python tools/parser_parity.py --paths "examples/*.cn"
    python tools/parser_parity.py --full          # show every diff hunk
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def run(cmd: list[str], **kwargs) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, cwd=str(ROOT), **kwargs)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--paths", nargs="*", default=["tests/*.cn"])
    parser.add_argument("--full", action="store_true", help="Print the complete diff output.")
    parser.add_argument("--no-build", action="store_true", help="Skip the rebuild step.")
    args = parser.parse_args()

    if not args.no_build:
        run([sys.executable, str(ROOT / "tools" / "bundle_src2.py")], check=True)
        build = run([str(ROOT / "cnext.exe"), "build", "src2/parser_bundle.cn",
                     "-o", "build/selfhost/parser_selfhost.exe"], capture_output=True, text=True)
        if build.returncode != 0:
            print(build.stdout)
            print(build.stderr, file=sys.stderr)
            return 2

    cmd = [sys.executable, "-u", str(ROOT / "tests" / "parity_parser.py"),
           "--ref-binary", "build/selfhost/astdump.exe",
           "--cand-cmd", "build/selfhost/parser_selfhost.exe {file}",
           "--paths", *args.paths]
    result = run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")

    if args.full:
        print(result.stdout)
    else:
        for line in result.stdout.splitlines():
            if line.startswith("Mismatch") or line.startswith("ERROR") or "parity" in line:
                print(line)
    if result.stderr.strip():
        print(result.stderr, file=sys.stderr)
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
