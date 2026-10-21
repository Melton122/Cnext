#!/usr/bin/env python3
"""Token parity checker for the self-hosting lexer bootstrap.

This script compares the reference lexer dump generated from src/lexer.c against
an external candidate lexer, typically the future Cnext port in src2/lexer.cn.
It is intentionally generic so the same harness can be reused once a working
candidate compiler is available.
"""

from __future__ import annotations

import argparse
import difflib
import glob
import os
import shlex
import subprocess
import sys
from pathlib import Path


def build_reference(binary: Path, repo_root: Path) -> None:
    if binary.exists():
        return

    src = repo_root / "src" / "lexer.c"
    lexdump = repo_root / "tools" / "lexdump.c"
    if not src.exists() or not lexdump.exists():
        raise FileNotFoundError(f"Missing lexer sources: {src} / {lexdump}")

    cmd = [
        "gcc",
        "-std=gnu11",
        "-Wall",
        "-Wextra",
        "-I",
        str(repo_root / "include"),
        str(lexdump),
        str(src),
        "-o",
        str(binary),
    ]
    subprocess.run(cmd, cwd=str(repo_root), check=True)


def list_files(patterns: list[str], repo_root: Path) -> list[Path]:
    files: list[Path] = []
    seen: set[Path] = set()
    for pattern in patterns:
        for match in glob.glob(str(repo_root / pattern), recursive=True):
            path = Path(match)
            if path.is_file() and path.suffix in {".cn", ".c", ".h"}:
                resolved = path.resolve()
                if resolved not in seen:
                    seen.add(resolved)
                    files.append(resolved)
    return sorted(files)


def dump_reference(binary: Path, file_path: Path) -> str:
    result = subprocess.run([str(binary), str(file_path)], capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise RuntimeError(f"Reference lexer failed on {file_path}:\n{result.stderr}")
    return result.stdout.replace("\r\n", "\n")


def dump_candidate(template: str, file_path: Path) -> str:
    command = template.strip()
    if not command:
        raise ValueError("Candidate command is empty.")

    file_token = str(file_path)
    if os.name == "nt":
        # Use the Windows command parser semantics so .\ and quoted paths work.
        file_token = file_token.replace("/", "\\")
        command = command.replace("{file}", f'"{file_token}"')
        # shlex keeps the surrounding quotes in the token; subprocess then
        # re-escapes them and the child receives a literal-quoted argument
        # (e.g. '"C:\path\file.cn"'), which no file API can open. Strip them
        # so the path arrives exactly as written, matching the reference call.
        args = [
            arg[1:-1] if len(arg) >= 2 and arg.startswith('"') and arg.endswith('"') else arg
            for arg in shlex.split(command, posix=False)
        ]
    else:
        command = command.replace("{file}", shlex.quote(file_token))
        args = shlex.split(command)

    result = subprocess.run(args, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise RuntimeError(f"Candidate lexer failed on {file_path}:\n{result.stderr}")
    return result.stdout.replace("\r\n", "\n")


def compare_pages(ref_text: str, cand_text: str, file_path: Path) -> bool:
    if ref_text == cand_text:
        return True

    ref_lines = ref_text.splitlines()
    cand_lines = cand_text.splitlines()
    diff = list(difflib.unified_diff(ref_lines, cand_lines, fromfile="reference", tofile="candidate", lineterm=""))
    print(f"\nMismatch in {file_path}")
    for line in diff[:80]:
        print(line)
    return False


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compare a reference lexer dump to a candidate lexer dump.")
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--ref-binary", type=Path, default=None)
    parser.add_argument("--cand-cmd", type=str, default=None, help="Shell command template used to produce candidate tokens; use {file} as placeholder.")
    parser.add_argument("--paths", nargs="*", default=["tests/*.cn", "examples/*.cn"], help="Glob patterns relative to repo root")
    parser.add_argument("--fail-fast", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()

    ref_binary = args.ref_binary.resolve() if args.ref_binary else (repo_root / "tools" / "lexdump")
    if args.ref_binary is None:
        build_reference(ref_binary, repo_root)

    if args.cand_cmd is None:
        print("No candidate command was provided.")
        print("Example:")
        if os.name == "nt":
            print(f"  python {Path(__file__).name} --cand-cmd \".\\tools\\lexdump.exe {{file}}\"")
        else:
            print(f"  python {Path(__file__).name} --cand-cmd \"./tools/lexdump {{file}}\"")
        print("Or provide a custom lexer harness that prints the same format as tools/lexdump.c.")
        return 0

    files = list_files(args.paths, repo_root)
    if not files:
        print("No input files matched the supplied patterns.")
        return 2

    ok = True
    for file_path in files:
        try:
            ref_text = dump_reference(ref_binary, file_path)
            cand_text = dump_candidate(args.cand_cmd, file_path)
        except Exception as exc:
            print(f"ERROR: {file_path}: {exc}")
            ok = False
            if args.fail_fast:
                return 1
            continue

        if not compare_pages(ref_text, cand_text, file_path):
            ok = False
            if args.fail_fast:
                return 1

    if ok:
        print(f"Parity check passed for {len(files)} files.")
        return 0

    print(f"Parity check failed for {len(files)} files.")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
