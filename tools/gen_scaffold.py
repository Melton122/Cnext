"""Generate src2/scaffold.cn from the native `cnext new` output in build/tmp/proj_n.

The native scaffolder embeds `{name}`-style interpolations in its templates.
In Cnext source strings those would parse as interpolation, so every literal
`{` is emitted as an LB() concatenation instead.
"""
import io
import os
import re

root = os.path.join('build', 'tmp', 'proj_n')
name = 'build/tmp/proj_n'


def cn_string(s):
    s = s.replace('\\', '\\\\')
    s = s.replace('\r', '\\r').replace('\n', '\\n').replace('\t', '\\t')
    out = []
    buf = []
    for ch in s:
        if ch == '{':
            if buf:
                out.append('"' + ''.join(buf) + '"')
                buf = []
            out.append('LB()')
        elif ch == '"':
            buf.append('\\"')
        else:
            buf.append(ch)
    if buf:
        out.append('"' + ''.join(buf) + '"')
    if not out:
        return '""'
    return ' + '.join(out)


rels = ['cnext.toml', 'src/main.cn', 'src/greeting.cn', 'src/models.cn',
        '.gitignore', 'README.md', '.vscode/tasks.json']

lines = []
lines.append('// Self-hosting bootstrap: `cnext new` project scaffolding.')
lines.append('// Port of create_project() in src/main.c; file templates are byte-identical.')
lines.append('')
lines.append('func LB(): str { return to_char(123) }')
lines.append('')

for rel in rels:
    with io.open(os.path.join(root, rel.replace('/', os.sep)), encoding='utf-8', newline='') as f:
        content = f.read()
    n_occurrences = content.count(name)
    parts = content.replace(name, '\x00').split('\x00')
    chunks = []
    for idx, part in enumerate(parts):
        e = cn_string(part)
        if e and e != '""':
            chunks.append(e)
        if idx < len(parts) - 1:
            chunks.append('project_name')
    expr = ' + '.join(chunks) if chunks else '""'
    prev = None
    while prev != expr:
        prev = expr
        expr = re.sub(r'\+ "" \+', '+ ', expr)
        expr = re.sub(r'^"" \+ ', '', expr)
    fn = 'scaffold_content_' + re.sub(r'[^a-z0-9]', '_', rel.lower()).strip('_')
    lines.append('func %s(str project_name): str {' % fn)
    lines.append('    return ' + expr)
    lines.append('}')
    lines.append('')
    print(rel, 'name occurrences:', n_occurrences)

with io.open('src2/scaffold_cn_gen.tmp', 'w', encoding='utf-8', newline='') as f:
    f.write('\n'.join(lines))
print('generated', len(lines), 'lines')
