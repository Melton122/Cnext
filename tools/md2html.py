import argparse
import html
import re
import textwrap
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SRC = ROOT / "docs" / "cnext-beginner-book.md"
DEFAULT_OUT = ROOT / "Cnext_The_Complete_Guide.html"
PLACEHOLDER = "\u0000code%d\u0000"

BLOCK = re.compile(r"^(#{1,6}\s|```|>|\||\s*[-*+]\s|\s*\d+[.)]\s|-{3,}|<details)")
UL = re.compile(r"^\s*[-*+]\s+(.+)$")
OL = re.compile(r"^\s*\d+[.)]\s+(.+)$")
DETAILS = re.compile(r"^<details(?:\s[^>]*)?>\s*<summary>(.*?)</summary>\s*$")
HEADING = re.compile(r"^(#{1,6})\s+(.*)$")


def esc(text):
    return html.escape(text, quote=False)


def slug(text):
    plain = re.sub(r"[*`_]", "", text).strip().lower()
    part = re.match(r"^part\s+([ivxlcdm]+)\b", plain)
    if part:
        return "part-" + part.group(1)
    appendix = re.match(r"^appendix\s+([a-z])\b", plain)
    if appendix:
        return "app" + appendix.group(1)
    section = re.match(r"^(\d+)\.(\d+)\b", plain)
    if section:
        return "sec-%s-%s" % (section.group(1), section.group(2))
    chapter = re.match(r"^(\d+)\.", plain)
    if chapter:
        return "ch" + chapter.group(1)
    plain = re.sub(r"[^a-z0-9]+", "-", plain).strip("-")
    return plain or "section"


def inline(text):
    codes = []

    def keep_code(match):
        codes.append("<code>" + esc(match.group(1)) + "</code>")
        return PLACEHOLDER % (len(codes) - 1)

    text = re.sub(r"`([^`]+)`", keep_code, text)
    text = esc(text)

    def link(match):
        label, url = match.group(1), match.group(2).strip()
        if not re.match(r"^(https?://|mailto:|#|[\w./-]+(?:[?#][^\s]*)?)$", url):
            url = "#"
        return '<a href="%s">%s</a>' % (html.escape(url, quote=True), label)

    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", link, text)
    text = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", text)
    text = re.sub(r"(?<!\*)\*(?!\*)([^*]+)\*(?!\*)", r"<em>\1</em>", text)
    for index, rendered in enumerate(codes):
        text = text.replace(PLACEHOLDER % index, rendered)
    return text


def callout_kind(label):
    text = label.lower()
    if "exercise" in text:
        return "exercise"
    if "theory" in text:
        return "theory"
    if "real world" in text:
        return "realworld"
    if "tip" in text:
        return "tip"
    if "warning" in text:
        return "warning"
    if "key point" in text:
        return "key"
    if "difficulty" in text:
        return "difficulty"
    return "note"


def collect_list(lines, start, marker):
    items = []
    index = start
    while index < len(lines):
        line = lines[index].strip()
        if not line:
            break
        match = marker.match(line)
        if match:
            items.append([match.group(1)])
            index += 1
            continue
        if line.startswith("</details>"):
            break
        if line.startswith("```") and items:
            fence = [line]
            index += 1
            while index < len(lines):
                code = lines[index].strip()
                fence.append(code)
                index += 1
                if code.startswith("```"):
                    break
            items[-1].append(("code", fence[0][3:].strip(), fence[1:-1]))
            continue
        if BLOCK.match(line):
            break
        if items:
            items[-1].append(line)
        index += 1
    return items, index


def code_block(language, code):
    language = re.sub(r"[^a-zA-Z0-9_-]", "", language.split()[0]) if language.strip() else ""
    code_class = ' class="language-%s"' % language if language else ""
    body = textwrap.dedent("\n".join(code))
    return "<pre><code%s>%s</code></pre>" % (code_class, esc(body))


CSS = """@page{size:A4;margin:1.8cm}
:root{--brand:#2c3e50;--brand2:#3498db;--accent:#27ae60;--text:#1f2d3d;--muted:#5c6b7a;--line:#dfe5eb;--code:#f5f7fa;--tip:#eaf6ff;--warn:#fff6e0;--note:#f0f4f8;--ex:#eafaf0;--theory:#f4f0fa;--real:#fff6fb}
*{box-sizing:border-box}
html{scroll-behavior:smooth}
body{font-family:-apple-system,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;line-height:1.7;color:var(--text);max-width:920px;margin:0 auto;padding:40px 28px 100px;background:#fff}
a{color:var(--brand2);text-decoration:none}
a:hover{text-decoration:underline}
h1,h2,h3,h4{color:var(--brand);font-weight:650;scroll-margin-top:18px}
h1{font-size:2.1em;margin:1.2em 0 .4em}
h2{font-size:1.55em;margin:1.6em 0 .4em;padding-bottom:6px;border-bottom:2px solid var(--line)}
h3{font-size:1.22em;margin:1.3em 0 .3em}
h4{font-size:1.05em;margin:1em 0 .2em;color:var(--muted)}
p{margin:.65em 0}
strong{color:#0f1c2b}
ul,ol{margin:.5em 0 .6em 1.7em}
li{margin:.32em 0}
li>p:first-child{margin-top:0}
li>p:last-child{margin-bottom:0}
code{font-family:Consolas,"SFMono-Regular",Menlo,monospace;background:#eef1f6;padding:2px 6px;border-radius:4px;font-size:.9em}
pre{position:relative;background:var(--code);border:1px solid var(--line);border-left:4px solid var(--brand2);padding:16px 18px;margin:1em 0;overflow-x:auto;border-radius:0 6px 6px 0;line-height:1.55}
pre code{background:none;padding:0;font-size:.86em;white-space:pre}
.copy{position:absolute;top:8px;right:8px;border:1px solid var(--line);background:#fff;color:var(--muted);border-radius:4px;padding:2px 9px;font-size:.72em;cursor:pointer;opacity:.65}
.copy:hover{opacity:1;color:var(--brand)}
.table-wrap{overflow-x:auto;margin:1em 0}
table{width:100%;border-collapse:collapse;font-size:.95em}
th,td{border:1px solid var(--line);padding:8px 12px;text-align:left;vertical-align:top}
th{background:var(--brand);color:#fff;font-weight:600}
tr:nth-child(even){background:#f7f9fb}
hr{border:none;border-top:1.5px solid var(--line);margin:2em 0}
blockquote{border-left:4px solid var(--brand2);padding:10px 18px;margin:1em 0;background:var(--note);color:var(--text)}
.callout{padding:13px 18px;margin:1.1em 0;border-radius:6px;border-left:4px solid var(--brand2);background:var(--note)}
.callout .cl{display:block;font-weight:700;text-transform:uppercase;letter-spacing:.5px;font-size:.78em;margin-bottom:5px}
.callout p:first-of-type{margin-top:0}
.callout p:last-child{margin-bottom:0}
.callout .callout{margin:.8em 0 0}
.key{background:var(--note);border-left-color:var(--brand)}
.key .cl{color:var(--brand)}
.theory{background:var(--theory);border-left-color:#8e44ad}
.theory .cl{color:#8e44ad}
.realworld{background:var(--real);border-left-color:#e84393}
.realworld .cl{color:#d81b60}
.tip{background:var(--tip);border-left-color:var(--brand2)}
.tip .cl{color:#2980b9}
.warning{background:var(--warn);border-left-color:#f39c12}
.warning .cl{color:#e67e22}
.exercise{background:var(--ex);border-left-color:var(--accent)}
.exercise .cl{color:var(--accent)}
.note .cl{color:var(--muted)}
.difficulty{margin:.3em 0 1em}
.tag{display:inline-block;font-size:.72em;font-weight:700;letter-spacing:.5px;padding:3px 10px;border-radius:12px;vertical-align:middle}
.tag.beg{background:#e8f5e9;color:#2e7d32}
.tag.int{background:#e3f2fd;color:#1565c0}
.tag.adv{background:#fce4ec;color:#c2185b}
details.solution{margin:.6em 0 1.2em;border:1px dashed var(--brand2);border-radius:6px;padding:8px 14px;background:#fcfdff}
details.solution summary{cursor:pointer;color:var(--brand2);font-weight:600}
details.solution[open] summary{margin-bottom:.4em}
.cover{text-align:center;padding:100px 20px 60px;border-bottom:1px solid var(--line)}
.cover .logo{font-size:4.5em;line-height:1;color:var(--brand2)}
.cover .kicker{letter-spacing:4px;text-transform:uppercase;color:var(--brand2);font-size:.85em;font-weight:700;margin-top:10px}
.cover h1{font-size:3.4em;margin:6px 0;border:none}
.cover .subtitle{font-size:1.4em;color:var(--muted);margin:8px 0 26px}
.cover .tagline{max-width:600px;margin:0 auto;color:var(--text)}
.cover .meta{font-size:.9em;color:var(--muted);margin-top:32px}
.toc{background:var(--note);border:1px solid var(--line);border-radius:8px;padding:20px 26px;margin:26px 0}
.toc h2{border:none;margin:0 0 10px}
.toc-list{list-style:none;margin:0;padding:0;columns:2;column-gap:32px}
.toc-list li{padding:3px 0;break-inside:avoid}
.toc-list .toc-part{font-weight:700;color:var(--brand);margin-top:12px}
h1.part{text-align:center;padding:56px 20px;margin:2em 0;background:linear-gradient(135deg,#2c3e50,#34495e);color:#fff;border-radius:10px;border:none}
main{padding-top:10px}
footer{margin-top:60px;padding-top:18px;border-top:1px solid var(--line);color:var(--muted);font-size:.88em;display:flex;justify-content:space-between;gap:16px}
@media(max-width:640px){body{padding:20px 16px 80px}.cover h1{font-size:2.6em}.toc-list{columns:1}footer{flex-direction:column}}
@media print{body{font-size:10.5pt;padding:0}.copy,footer a{display:none}.callout,pre,.table-wrap,details.solution,tr{page-break-inside:avoid}h1.part{page-break-before:always;background:#2c3e50;color:#fff}.toc{page-break-after:always}}
"""

SCRIPT = """document.querySelectorAll('pre').forEach(function (block) {
  var button = document.createElement('button');
  button.type = 'button';
  button.className = 'copy';
  button.textContent = 'Copy';
  button.addEventListener('click', function () {
    var source = block.querySelector('code') || block;
    if (navigator.clipboard) {
      navigator.clipboard.writeText(source.textContent).then(function () {
        button.textContent = 'Copied';
        setTimeout(function () { button.textContent = 'Copy'; }, 1200);
      });
    }
  });
  block.appendChild(button);
});
"""

HEAD = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="description" content="Cnext: The Complete Beginner's Guide — a practical introduction to a modern language that compiles to C.">
<title>Cnext: The Complete Beginner's Guide</title>
<style>
""" + CSS + """</style>
</head>
<body>
"""

TAIL = """<footer>
<span>Cnext: The Complete Beginner's Guide</span>
<a href="#top">Back to top &uarr;</a>
</footer>
<script>
""" + SCRIPT + """</script>
</body>
</html>
"""


class Book:
    def __init__(self, lines):
        self.lines = lines
        self.title = "Cnext: The Complete Beginner's Guide"
        self.subtitle = ""
        self.skip_text = set()
        self.headings = []
        self.id_map = {}
        self.prepare()

    def prepare(self):
        title_index = None
        for index, line in enumerate(self.lines):
            match = HEADING.match(line.strip())
            if match and len(match.group(1)) == 1:
                self.title = match.group(2).strip()
                title_index = index
                break
        if title_index is None:
            return
        self.skip_text.add(self.lines[title_index].strip())

        cursor = title_index + 1
        while cursor < len(self.lines) and not self.lines[cursor].strip():
            cursor += 1
        if cursor < len(self.lines):
            candidate = self.lines[cursor].strip()
            if candidate.startswith("*") and not candidate.startswith("**"):
                while cursor < len(self.lines) and self.lines[cursor].strip():
                    text = self.lines[cursor].strip()
                    self.subtitle += text.strip("*").strip() + " "
                    self.skip_text.add(text)
                    cursor += 1
                self.subtitle = self.subtitle.strip()

        counts = {}
        in_code = False
        for line in self.lines:
            stripped = line.strip()
            if stripped.startswith("```"):
                in_code = not in_code
                continue
            if in_code:
                continue
            match = HEADING.match(stripped)
            if not match:
                continue
            level, text = len(match.group(1)), match.group(2).strip()
            base = slug(text)
            number = counts.get(base, 0)
            counts[base] = number + 1
            hid = base if number == 0 else "%s-%d" % (base, number + 1)
            self.headings.append((level, text, hid))
            self.id_map.setdefault((level, text), []).append(hid)

    def heading_id(self, level, text):
        choices = self.id_map.get((level, text), [])
        return choices.pop(0) if choices else slug(text)

    def toc_html(self):
        items = []
        for level, text, hid in self.headings:
            if level == 1 and re.match(r"^PART\b", text, re.I):
                items.append('<li class="toc-part">%s</li>' % inline(text))
            elif level == 2 and not text.lower().startswith("table of contents"):
                items.append('<li><a href="#%s">%s</a></li>' % (hid, inline(text)))
        return '<nav class="toc" id="toc"><h2>Table of Contents</h2><ol class="toc-list">\n%s\n</ol></nav>' % "\n".join(items)

    def render_item(self, chunks):
        parts = []
        paragraph = []
        for chunk in chunks:
            if isinstance(chunk, tuple):
                if paragraph:
                    parts.append("<p>%s</p>" % inline(" ".join(paragraph)))
                    paragraph = []
                parts.append(code_block(chunk[1], chunk[2]))
            else:
                paragraph.append(chunk)
        if paragraph:
            parts.append("<p>%s</p>" % inline(" ".join(paragraph)))
        return "\n".join(parts)

    def render(self, lines):
        out = []
        index = 0
        total = len(lines)
        while index < total:
            line = lines[index].strip()
            if not line:
                index += 1
                continue
            if line in self.skip_text:
                index += 1
                continue
            if line.startswith("<!--"):
                while index < total and "-->" not in lines[index]:
                    index += 1
                index += 1
                continue
            if line.startswith("</details>"):
                index += 1
                continue
            if re.fullmatch(r"-{3,}", line):
                out.append("<hr/>")
                index += 1
                continue
            if line.startswith("## Table of Contents"):
                out.append(self.toc_html())
                index += 1
                while index < total:
                    following = lines[index].strip()
                    if following.startswith("#") or re.fullmatch(r"-{3,}", following):
                        break
                    index += 1
                if index < total and re.fullmatch(r"-{3,}", lines[index].strip()):
                    index += 1
                continue

            details = DETAILS.match(line)
            if details:
                cursor = index + 1
                inner = []
                while cursor < total and "</details>" not in lines[cursor]:
                    inner.append(lines[cursor])
                    cursor += 1
                out.append('<details class="solution"><summary>%s</summary>\n%s\n</details>' % (inline(details.group(1)), self.render(inner)))
                index = cursor + 1
                continue

            heading = HEADING.match(line)
            if heading:
                level = len(heading.group(1))
                text = heading.group(2).strip()
                hid = self.heading_id(level, text)
                css_class = ""
                if level == 1 and re.match(r"^PART\b", text, re.I):
                    css_class = ' class="part"'
                elif level == 2:
                    css_class = ' class="chapter"'
                out.append('<h%d id="%s"%s>%s</h%d>' % (level, hid, css_class, inline(text), level))
                index += 1
                continue

            if line.startswith("```"):
                language = line[3:].strip()
                cursor = index + 1
                code = []
                while cursor < total and not lines[cursor].strip().startswith("```"):
                    code.append(lines[cursor])
                    cursor += 1
                out.append(code_block(language, code))
                index = cursor + 1
                continue

            if line.startswith(">"):
                quoted = []
                while index < total and lines[index].strip().startswith(">"):
                    quoted.append(lines[index].strip()[1:].lstrip())
                    index += 1
                label = None
                if quoted:
                    first = re.match(r"^\*\*(.+?)\*\*\s*(.*)$", quoted[0])
                    if first:
                        label = first.group(1)
                        kind = callout_kind(label)
                        remainder = first.group(2).strip()
                        if remainder:
                            quoted[0] = remainder
                        else:
                            quoted.pop(0)
                        if kind == "difficulty":
                            level_name = label.lower()
                            badge = "beg" if "beginner" in level_name else "int" if "intermediate" in level_name else "adv"
                            display = label.split(":", 1)[1].strip() if ":" in label else label
                            out.append('<p class="difficulty"><span class="tag %s">%s</span></p>' % (badge, inline(display)))
                            continue
                        out.append('<aside class="callout %s"><span class="cl">%s</span>\n%s\n</aside>' % (kind, inline(label), self.render(quoted)))
                        continue
                out.append("<blockquote>\n%s\n</blockquote>" % self.render(quoted))
                continue

            if line.startswith("|"):
                rows = []
                while index < total and lines[index].strip().startswith("|"):
                    raw = lines[index].strip().strip("|")
                    rows.append([cell.strip() for cell in raw.split("|")])
                    index += 1
                body = []
                header = True
                for row in rows:
                    if row and all(re.fullmatch(r":?-{2,}:?", cell) for cell in row):
                        continue
                    tag = "th" if header else "td"
                    header = False
                    body.append("<tr>%s</tr>" % "".join("<%s>%s</%s>" % (tag, inline(cell), tag) for cell in row))
                out.append('<div class="table-wrap"><table>\n%s\n</table></div>' % "\n".join(body))
                continue

            if UL.match(line):
                items, index = collect_list(lines, index, UL)
                out.append("<ul>\n%s\n</ul>" % "\n".join("<li>%s</li>" % self.render_item(item) for item in items))
                continue

            if OL.match(line):
                items, index = collect_list(lines, index, OL)
                out.append("<ol>\n%s\n</ol>" % "\n".join("<li>%s</li>" % self.render_item(item) for item in items))
                continue

            paragraph = [line]
            index += 1
            while index < total:
                following = lines[index].strip()
                if not following or BLOCK.match(following) or following.startswith("</details>"):
                    break
                paragraph.append(following)
                index += 1
            out.append("<p>%s</p>" % inline(" ".join(paragraph)))
        return "\n".join(out)

    def build(self):
        title = inline(self.title)
        subtitle = inline(self.subtitle)
        cover = """<header class="cover" id="top">
<div class="logo" aria-hidden="true">&#9670;</div>
<div class="kicker">The Complete Guide</div>
<h1>Cnext</h1>
<p class="subtitle">A Modern Language That Compiles to C</p>
<p class="tagline">%s</p>
<div class="meta">Edition for Cnext v10 &middot; 2026 &middot; %s</div>
</header>
""" % (subtitle, title)
        return HEAD + cover + "<main>\n" + self.render(self.lines) + "\n</main>\n" + TAIL


def main():
    parser = argparse.ArgumentParser(description="Build the Cnext HTML book from its Markdown source.")
    parser.add_argument("--src", type=Path, default=DEFAULT_SRC, help="Markdown source file")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="HTML output file")
    args = parser.parse_args()
    lines = args.src.read_text(encoding="utf-8").splitlines()
    document = Book(lines).build()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(document, encoding="utf-8")
    print("WROTE %s (%d bytes)" % (args.out, len(document.encode("utf-8"))))


if __name__ == "__main__":
    main()
