"""Generate src2/scaffold.cn from the native `cnext new` output.

The native scaffolder embeds `{name}`-style interpolations in its templates.
In Cnext source strings those would parse as interpolation, so every literal
`{` is emitted as a separate LB() chunk.

Templates are emitted as arrays of chunks joined by cnext_join_strings: a
single giant concat expression makes GCC's cc1 memory spike past what the
bundle compile can afford on memory-constrained machines.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[1]
native = repo / ('cnext.exe' if os.name == 'nt' else 'cnext')
name = 'ScaffoldTemplateProject'


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
        return []
    return out


rels = ['cnext.toml', 'src/main.cn', 'src/greeting.cn', 'src/models.cn',
        '.gitignore', 'README.md', '.vscode/tasks.json']

lines = []
lines.append('// Self-hosting bootstrap: `cnext new` project scaffolding.')
lines.append('// Port of create_project() in src/main.c; file templates are byte-identical')
lines.append('// (native writes in text mode, so templates carry CRLF endings on Windows).')
lines.append('//')
lines.append('// Each template is a chunk array joined at the end: one huge concat')
lines.append('// expression made cc1 memory usage explode during the stage-1 build.')
lines.append('')
lines.append('import os')
lines.append('')
lines.append('func LB(): str { return "" + to_char(123) }')
lines.append('')

with tempfile.TemporaryDirectory(dir=repo / 'build') as work:
    subprocess.run([native, 'new', name], cwd=work, check=True, stdout=subprocess.DEVNULL)
    templates = {rel: (Path(work) / name / rel).read_text(encoding='utf-8') for rel in rels}

for rel, content in templates.items():
    n_occurrences = content.count(name)
    parts = content.replace(name, '\x00').split('\x00')
    fn = 'scaffold_content_' + re.sub(r'[^a-z0-9]', '_', rel.lower()).strip('_')
    lines.append('func %s(str project_name): str {' % fn)
    lines.append('    str[] chunks = {}')
    for idx, part in enumerate(parts):
        for chunk in cn_string(part):
            lines.append('    push(chunks, ' + chunk + ')')
        if idx < len(parts) - 1:
            lines.append('    push(chunks, project_name)')
    lines.append('    str content = cnext_join_strings(chunks)')
    lines.append(r'    if os_os_name() == "Windows" { return str_replace(content, "\n", "\r\n") }')
    lines.append('    return content')
    lines.append('}')
    lines.append('')
    print(rel, 'name occurrences:', n_occurrences)

path = repo / 'src2/scaffold.cn'
logic = path.read_text(encoding='utf-8').split('func create_project(', 1)[1]
path.write_text('\n'.join(lines) + '\nfunc create_project(' + logic, encoding='utf-8', newline='\n')
