import re
SRC = r'C:\Users\melto\cnext\docs\cnext-beginner-book.md'
OUT = r'C:\Users\melto\cnext\Cnext_The_Complete_Guide.html'
_MP = {'\u2192':'->','\u2190':'<-','\u2194':'<->','\u25b6':'>','\u2500':'-','\u2501':'-','\u2502':'-','\u251c':'-','\u2514':'-','\u00b2':'^2','\u00b3':'^3','\u00b9':'^1','\u00b7':'-'}
def san(s):
    for k,v in _MP.items(): s=s.replace(k,v)
    return s
def he(s):
    return s.replace('&','&amp;').replace('<','&lt;').replace('>','&gt;')
BO=re.compile(r'\*\*(.+?)\*\*')
IT=re.compile(r'(?<!\*)\*(?!\*)([^*]+)\*(?!\*)')
CO=re.compile(r'`([^`]+)`')
def inline(s):
    s=he(san(s)); s=CO.sub(r'<code>\1</code>',s); s=BO.sub(r'<b>\1</b>',s); s=IT.sub(r'<i>\1</i>',s)
    return s
PB=re.compile(r'^(#{1,4}\s|```|>|\||[-*+]\s|\d+[.)]\s|-{3,})')
UL=re.compile(r'^[-*+]\s+(.+)$')
OL=re.compile(r'^\d+[.)]\s+(.+)$')
def collect_list(lines,i,mark):
    items=[]; j=i; n=len(lines)
    while j<n:
        st=lines[j].strip()
        if not st: break
        m=mark.match(st)
        if m: items.append(m.group(1)); j+=1; continue
        if PB.match(st): break
        if items: items[-1]+= ' '+st
        j+=1
    return items,j
def render(lines):
    out=[]; i=0; n=len(lines)
    while i<n:
        s=lines[i].strip()
        if not s: i+=1; continue
        if s.startswith('<!--') or s.startswith('<details') or s=='</details>': i+=1; continue
        if s.startswith('<summary>'): i+=1; continue
        if re.fullmatch(r'-{3,}', s): out.append('<hr/>'); i+=1; continue
        m=re.match(r'^(#{1,6})\s+(.*)$', s)
        if m:
            lvl=len(m.group(1)); out.append('<h%d>%s</h%d>'%(lvl,inline(m.group(2)),lvl)); i+=1; continue
        if s.startswith('```'):
            j=i+1; b=[]
            while j<n and not lines[j].strip().startswith('```'):
                b.append(lines[j]); j+=1
            out.append('<pre>'+he(san('\n'.join(b)))+'</pre>'); i=j+1; continue
        if s.startswith('>'):
            j=i; inner=[]
            while j<n and lines[j].strip().startswith('>'):
                inner.append(lines[j].strip()[1:].lstrip()); j+=1
            out.append('<blockquote>'+render(inner)+'</blockquote>'); i=j; continue
        if s.startswith('|'):
            rows=[]; j=i
            while j<n and lines[j].strip().startswith('|'):
                raw=lines[j].strip().strip('|')
                rows.append([c.strip() for c in raw.split('|')]); j+=1
            out.append('<table>'); header=True
            for r0 in rows:
                if r0 and re.fullmatch(r'[-:]+', r0[0].strip()): continue
                tag='th' if header else 'td'; header=False
                out.append('<tr>'+''.join('<%s>%s</%s>'%(tag,inline(c),tag) for c in r0)+'</tr>')
            out.append('</table>'); i=j; continue
        if UL.match(s):
            items,j=collect_list(lines,i,UL)
            out.append('<ul>'+''.join('<li>'+inline(x)+'</li>' for x in items)+'</ul>'); i=j; continue
        if OL.match(s):
            items,j=collect_list(lines,i,OL)
            out.append('<ol>'+''.join('<li>'+inline(x)+'</li>' for x in items)+'</ol>'); i=j; continue
        para=[lines[i]]; j=i+1
        while j<n:
            nx=lines[j].strip()
            if not nx or PB.match(nx): break
            para.append(lines[j]); j+=1
        text=' '.join(x for x in para if x.strip())
        if text.strip(): out.append('<p>'+inline(text)+'</p>')
        i=j
    return '\n'.join(out)

CSS = '''body{font-family:-apple-system,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;line-height:1.7;color:#1f2d3d;max-width:920px;margin:0 auto;padding:40px 28px 120px;background:#fff}
h1{font-size:2em;margin:1.1em 0 .4em;color:#2c3e50;border-bottom:2px solid #e4eaef;padding-bottom:6px;page-break-before:always}
h1:first-of-type{page-break-before:auto}
h2{font-size:1.55em;margin:1.5em 0 .4em;color:#2c3e50}
h3{font-size:1.22em;margin:1.2em 0 .3em;color:#24374a}
h4{font-size:1.05em;color:#5c6b7a;margin:1em 0 .2em}
p{margin:.6em 0}
strong{color:#0f1c2b}
ul,ol{margin:.5em 0 .6em 1.7em}
li{margin:.3em 0}
code{font-family:Consolas,Menlo,monospace;background:#eef1f6;padding:1px 5px;border-radius:4px;font-size:.92em}
pre{background:#f5f7fa;border:1px solid #e0e6ee;border-left:4px solid #3498db;padding:14px 18px;margin:1em 0;overflow:auto;border-radius:0 6px 6px 0;line-height:1.5}
pre code{background:none;padding:0}
table{width:100%;border-collapse:collapse;margin:1em 0;font-size:.96em}
th,td{border:1px solid #dfe5eb;padding:7px 11px;text-align:left;vertical-align:top}
th{background:#2c3e50;color:#fff}
tr:nth-child(even){background:#f7f9fb}
blockquote{border-left:4px solid #8e44ad;background:#f4f0fa;padding:10px 18px;margin:1em 0;color:#3a2f4a}
blockquote blockquote{border-left-color:#8e44ad}
hr{border:none;border-top:1.5px solid #e4eaef;margin:1.7em 0;page-break-after:avoid}
@media print{body{font-size:10.5pt;padding:0}pre,blockquote,table{page-break-inside:avoid}}
'''

body=render(open(SRC,encoding='utf-8').read().splitlines())
doc=('<!doctype html>\n<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Cnext: The Complete Beginner\'s Guide</title><style>'+CSS+'</style></head><body>')
doc+=body+('</body></html>')
open(OUT,'w',encoding='utf-8').write(doc)
print('WROTE',OUT,len(doc))
