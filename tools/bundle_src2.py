#!/usr/bin/env python3
"""Split and regenerate src2/parser_bundle.cn from the modular self-hosting sources.

The self-hosted compiler port lives in several focused modules under src2/.
The compiler only accepts a single translation unit, so the modules are
concatenated into src2/parser_bundle.cn, which is what gets built and what
the AST parity gate exercises.

Keeping the bundle *generated* avoids the two-sources-of-truth trap: the
modular files are edited, the bundle is regenerated, and the parity gate
always measures the current state of the modules. `--split` goes the other
way, extracting the modules back out of a bundle that was edited directly.

Layout of the bundle, per module:

    // --- src2/<module> ---
    <module body>

with two blank lines between modules. The lexer's standalone `main` block is
elided from the bundle (the bundle's `main` is the parser dumper), so the
bundler strips a trailing top-level `main {` block from the lexer module.

Usage:
    python tools/bundle_src2.py            # regenerate the bundle in place
    python tools/bundle_src2.py --check    # fail if the bundle is stale
    python tools/bundle_src2.py --split    # extract modules from the bundle
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

# Order matters: types first (constants and data structures), then the
# parser modules, and finally the top-level driver with its `main` block.
MODULES = [
    "lexer.cn",
    "parser_types.cn",
    "parser_core.cn",
    "parser_type.cn",
    "parser_macro.cn",
    "parser_expr.cn",
    "parser_stmt.cn",
    "parser_decl.cn",
    "parser.cn",
]

BUNDLE = "parser_bundle.cn"
MARKER = "// --- src2/{module} ---"


def strip_main(body: str) -> str:
    """Drop a trailing top-level `main { ... }` block from a module body."""
    lines = body.split("\n")
    for index, line in enumerate(lines):
        if line.startswith("main {"):
            return "\n".join(lines[:index]).rstrip("\n")
    return body.rstrip("\n")


def build(src2: Path) -> str:
    parts: list[str] = []
    for module in MODULES:
        path = src2 / module
        if not path.exists():
            raise FileNotFoundError(f"Missing module: {path}")
        body = path.read_text(encoding="utf-8").replace("\r\n", "\n").rstrip("\n")
        if module == "lexer.cn":
            body = strip_main(body)
        parts.append(f"{MARKER.format(module=module)}\n{body}\n\n\n")
    return "".join(parts)


def split(src2: Path) -> list[str]:
    """Extract each module from the bundle back into src2/<module>."""
    bundle_path = src2 / BUNDLE
    text = bundle_path.read_text(encoding="utf-8").replace("\r\n", "\n")
    lines = text.split("\n")

    starts: list[tuple[str, int]] = []
    for index, line in enumerate(lines):
        if line.startswith("// --- src2/") and line.endswith(" ---"):
            starts.append((line[len("// --- src2/"):-len(" ---")], index))
    if not starts:
        raise ValueError(f"No module markers found in {bundle_path}")

    known = {name for name, _ in starts}
    missing = [module for module in MODULES if module not in known]
    if missing:
        raise ValueError(f"Bundle is missing modules: {', '.join(missing)}")

    written: list[str] = []
    for position, (module, start) in enumerate(starts):
        end = starts[position + 1][1] if position + 1 < len(starts) else len(lines)
        body_lines = lines[start + 1:end]
        while body_lines and body_lines[-1] == "":
            body_lines.pop()
        body = "\n".join(body_lines)

        path = src2 / module
        if module == "lexer.cn":
            # The bundle has no `main`; keep the standalone lexdump driver
            # that the lexer parity gate builds.
            main_block = extract_main(path.read_text(encoding="utf-8").replace("\r\n", "\n"))
            body = f"{body}\n\n{main_block}" if main_block else body
        path.write_text(body + "\n", encoding="utf-8", newline="\n")
        written.append(module)
    return written


def extract_main(body: str) -> str:
    """Return a module's trailing top-level `main { ... }` block, if any."""
    lines = body.split("\n")
    for index, line in enumerate(lines):
        if line.startswith("main {"):
            return "\n".join(lines[index:]).rstrip("\n")
    return ""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--src2", type=Path, default=Path(__file__).resolve().parent.parent / "src2")
    parser.add_argument("--check", action="store_true",
                        help="Exit non-zero if the bundle is out of date instead of writing it.")
    parser.add_argument("--split", action="store_true",
                        help="Extract the modules back out of the bundle.")
    args = parser.parse_args()

    src2 = args.src2.resolve()
    bundle_path = src2 / BUNDLE

    if args.split:
        written = split(src2)
        print(f"Split {bundle_path} into: {', '.join(written)}")
        return 0

    generated = build(src2)

    if args.check:
        current = bundle_path.read_text(encoding="utf-8").replace("\r\n", "\n") if bundle_path.exists() else ""
        if current != generated:
            print(f"{bundle_path} is out of date. Run: python tools/bundle_src2.py")
            return 1
        print(f"{bundle_path} is up to date.")
        return 0

    bundle_path.write_text(generated, encoding="utf-8", newline="\n")
    print(f"Wrote {bundle_path} ({len(generated.splitlines())} lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
