#!/usr/bin/env python3
"""Build compiler bundles for self-hosting."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC2 = ROOT / "src2"


BUNDLES = {
    "parser": [
        "lexer.cn",
        "parser_types.cn",
        "parser_core.cn",
        "parser_type.cn",
        "parser_macro.cn",
        "parser_expr.cn",
        "parser_stmt.cn",
        "parser_decl.cn",
        "parser.cn",
    ],
    "semantics": [
        "lexer.cn",
        "parser_types.cn",
        "parser_core.cn",
        "parser_type.cn",
        "parser_macro.cn",
        "parser_expr.cn",
        "parser_stmt.cn",
        "parser_decl.cn",
        "parser.cn",
        "semantics_scope.cn",
        "semantics_types.cn",
        "semantics_predeclare.cn",
        "semantics_expr.cn",
        "semantics_analyze.cn",
        "semantics.cn",
    ],
    "full": [
        "lexer.cn",
        "parser_types.cn",
        "parser_core.cn",
        "parser_type.cn",
        "parser_macro.cn",
        "parser_expr.cn",
        "parser_stmt.cn",
        "parser_decl.cn",
        "parser.cn",
        "semantics_scope.cn",
        "semantics_types.cn",
        "semantics_predeclare.cn",
        "semantics_expr.cn",
        "semantics_analyze.cn",
        "semantics.cn",
        # codegen and main will be added later
    ],
}


def strip_main(body: str) -> str:
    lines = body.split("\n")
    for index, line in enumerate(lines):
        if line.startswith("main {"):
            return "\n".join(lines[:index]).rstrip("\n")
    return body.rstrip("\n")


def extract_main(body: str) -> str:
    lines = body.split("\n")
    for index, line in enumerate(lines):
        if line.startswith("main {"):
            return "\n".join(lines[index:]).rstrip("\n")
    return ""


def build_bundle(name: str) -> str:
    modules = BUNDLES[name]
    parts: list[str] = []
    for module in modules:
        path = SRC2 / module
        if not path.exists():
            raise FileNotFoundError(f"Missing module: {path}")
        body = path.read_text(encoding="utf-8").replace("\r\n", "\n").rstrip("\n")
        if module == "lexer.cn":
            body = strip_main(body)
        # Don't strip main from parser.cn for semantics bundle - we might want both?
        # But keep parser as-is, semantics has its own main? No, semantics.cn has no main
        parts.append(f"// --- src2/{module} ---\n{body}\n\n\n")
    return "".join(parts)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", choices=["parser", "semantics", "full"])
    parser.add_argument("-o", "--output", type=str)
    args = parser.parse_args()

    generated = build_bundle(args.bundle)
    if args.output:
        out = Path(args.output)
        out.write_text(generated, encoding="utf-8", newline="\n")
        print(f"Wrote {out} ({len(generated.splitlines())} lines)")
    else:
        print(generated)
    return 0


if __name__ == "__main__":
    sys.exit(main())
