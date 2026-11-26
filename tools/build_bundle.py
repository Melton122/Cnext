#!/usr/bin/env python3
from __future__ import annotations
import argparse
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
SRC2 = ROOT / 'src2'
BUNDLES = {
    'parser': ['lexer.cn','parser_types.cn','originmap.cn','privatenames.cn','parser_core.cn','parser_type.cn','parser_macro.cn','parser_expr.cn','parser_stmt.cn','parser_decl.cn','parser.cn'],
    'semantics': ['lexer.cn','parser_types.cn','originmap.cn','privatenames.cn','parser_core.cn','parser_type.cn','parser_macro.cn','parser_expr.cn','parser_stmt.cn','parser_decl.cn','parser.cn','semantics_scope.cn','semantics_types.cn','semantics_predeclare.cn','semantics_expr.cn','semantics_analyze.cn','lifetimes.cn','semantics.cn'],
    'full': ['lexer.cn','parser_types.cn','originmap.cn','privatenames.cn','parser_core.cn','parser_type.cn','parser_macro.cn','parser_expr.cn','parser_stmt.cn','parser_decl.cn','parser.cn','semantics_scope.cn','semantics_types.cn','semantics_predeclare.cn','semantics_expr.cn','semantics_analyze.cn','lifetimes.cn','semantics.cn'],
    'codegen': ['lexer.cn','parser_types.cn','originmap.cn','privatenames.cn','parser_core.cn','parser_type.cn','parser_macro.cn','parser_expr.cn','parser_stmt.cn','parser_decl.cn','parser.cn','semantics_scope.cn','semantics_types.cn','semantics_predeclare.cn','semantics_expr.cn','semantics_analyze.cn','lifetimes.cn','semantics.cn','optimizer.cn','codegen_types.cn','codegen_emit_utils.cn','codegen_node.cn','codegen_expr.cn','codegen_generic.cn','codegen_classspec.cn','codegen_closure.cn','codegen_gen.cn','codegen.cn','main.cn'],
}
def strip_main(body):
    lines = body.split('\n')
    for index, line in enumerate(lines):
        if line.startswith('main {'):
            return '\n'.join(lines[:index]).rstrip('\n')
    return body.rstrip('\n')
def build_bundle(name):
    modules = BUNDLES[name]
    parts = []
    for module in modules:
        path = SRC2 / module
        if not path.exists():
            raise FileNotFoundError('Missing module: %s' % path)
        body = path.read_text(encoding='utf-8').replace('\r\n','\n').rstrip('\n')
        if module == 'lexer.cn' or module == 'parser.cn':
            body = strip_main(body)
        parts.append('// --- src2/%s ---\n%s\n\n\n' % (module, body))
    return ''.join(parts)
def main():
    parser = argparse.ArgumentParser(description='Build compiler bundles for self-hosting.')
    parser.add_argument('bundle', choices=['parser','semantics','full','codegen'])
    parser.add_argument('-o','--output', type=str)
    args = parser.parse_args()
    generated = build_bundle(args.bundle)
    if args.output:
        out = Path(args.output)
        out.write_text(generated, encoding='utf-8', newline='\n')
        print('Wrote %s (%d lines)' % (out, len(generated.splitlines())), file=sys.stderr)
    else:
        sys.stdout.write(generated)
    return 0
if __name__ == '__main__':
    raise SystemExit(main())
