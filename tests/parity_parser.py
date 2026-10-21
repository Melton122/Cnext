#!/usr/bin/env python3
"""AST parity checker for the self-hosting parser bootstrap (Milestone B).

Compares the reference AST dump generated from the hand-written C parser
(tools/astdump.c) against a candidate dumper, typically the Cnext port in
src2/parser.cn. The dump format is documented in tools/astdump.c.

Usage (Windows):
  python tests/parity_parser.py --cand-cmd ".\\parser_selfhost.exe {file}"

Usage (Linux/macOS):
  python tests/parity_parser.py --cand-cmd "./parser_selfhost {file}"
"""

from __future__ import annotations

import argparse
import difflib
import glob
import os
import shlex
import subprocess
from pathlib import Path


def build_reference(binary: Path, repo_root: Path) -> None:
    if binary.exists():
        return

    sources = [
        "src/parser.c", "src/parser_core.c", "src/parser_decl.c", "src/parser_expr.c",
        "src/parser_macro.c", "src/parser_stmt.c", "src/parser_type.c",
        "src/ast.c", "src/lexer.c", "src/diagnostics.c",
    ]
    for source in sources:
        if not (repo_root / source).exists():
            raise FileNotFoundError(f"Missing parser source: {source}")

    cmd = [
        "gcc", "-std=gnu11", "-w", "-I", str(repo_root / "include"),
        str(repo_root / "tools" / "astdump.c"),
        *[str(repo_root / source) for source in sources],
        "-o", str(binary),
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
    result = subprocess.run([str(binary), str(file_path)], capture_output=True, text=True,
                            encoding="utf-8", errors="replace", check=False)
    if result.returncode != 0:
        raise RuntimeError(f"Reference parser failed on {file_path}:\n{result.stderr}")
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

    result = subprocess.run(args, capture_output=True, text=True, encoding="utf-8",
                            errors="replace", check=False)
    if result.returncode != 0:
        raise RuntimeError(f"Candidate parser failed on {file_path}:\n{result.stderr}")
    return result.stdout.replace("\r\n", "\n")


def compare_pages(ref_text: str, cand_text: str, file_path: Path) -> bool:
    if ref_text == cand_text:
        return True

    diff = list(difflib.unified_diff(ref_text.splitlines(), cand_text.splitlines(),
                                     fromfile="reference", tofile="candidate", lineterm=""))
    print(f"\nMismatch in {file_path}")
    for line in diff[:60]:
        print(line)
    return False


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compare a reference AST dump to a candidate AST dump.")
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--ref-binary", type=Path, default=None)
    parser.add_argument("--cand-cmd", type=str, default=None,
                        help="Command template used to produce candidate dumps; use {file} as placeholder.")
    parser.add_argument("--paths", nargs="*", default=["tests/*.cn", "examples/*.cn"],
                        help="Glob patterns relative to repo root")
    parser.add_argument("--fail-fast", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()

    default_ref = "astdump.exe" if os.name == "nt" else "astdump"
    ref_binary = args.ref_binary.resolve() if args.ref_binary else (repo_root / "tools" / default_ref)
    if args.ref_binary is None:
        build_reference(ref_binary, repo_root)

    if args.cand_cmd is None:
        print("No candidate command was provided.")
        print("Example:")
        print(f"  python {Path(__file__).name} --cand-cmd \".\\parser_selfhost.exe {{file}}\"")
        return 0

    files = list_files(args.paths, repo_root)
    if not files:
        print("No input files matched the supplied patterns.")
        return 2

    ok = True
    passed = 0
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

        if compare_pages(ref_text, cand_text, file_path):
            passed += 1
        else:
            ok = False
            if args.fail_fast:
                return 1

    if ok:
        print(f"AST parity check passed for {len(files)} files.")
        return 0

    print(f"AST parity check failed: {passed}/{len(files)} files match.")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
