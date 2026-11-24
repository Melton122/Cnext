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
PART_HEADING = re.compile(r"^PART\s+([IVXLCDM]+)\s*(?::|[—–-])\s*(.*)$", re.I)
CHAPTER_HEADING = re.compile(r"^(\d+)\.\s+(.*)$")
APPENDIX_HEADING = re.compile(r"^Appendix\s+([A-Z])\s*(?::|[—–-])\s*(.*)$", re.I)
CALLOUT_LABEL = re.compile(
    r"^(Key point|Theory|Real world|Tip|Warning|Exercise|Difficulty)\b\s*(?::|[—–-]\s*|\s+)?(.*)$",
    re.I,
)

LANG_LABELS = {
    "cnext": "Cnext",
    "bash": "Shell",
    "sh": "Shell",
    "shell": "Shell",
    "python": "Python",
    "json": "JSON",
    "toml": "TOML",
    "c": "C",
}


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


def callout_label(label):
    """Split a callout label into its kind and an optional detail."""
    match = CALLOUT_LABEL.match(label.strip())
    if not match:
        return "", label.strip()
    kind, detail = match.group(1), match.group(2).strip()
    return kind, detail


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
    token = re.sub(r"[^a-zA-Z0-9_-]", "", language.split()[0]) if language.strip() else ""
    label = LANG_LABELS.get(token.lower()) or (token.upper() if token == "c" else (token.capitalize() if token else "Plain text"))
    code_class = ' class="language-%s"' % token if token else ""
    body = textwrap.dedent("\n".join(code))
    return (
        '<div class="codeblock">\n'
        '<div class="codebar"><span class="lang">%s</span>'
        '<button type="button" class="copy">Copy</button></div>\n'
        "<pre><code%s>%s</code></pre>\n"
        "</div>"
    ) % (esc(label), code_class, esc(body))


CSS = """@page{size:A4;margin:18mm 16mm 20mm}
:root{
  --ink:#1a2634;--ink-strong:#0c1723;--muted:#5d6d7e;--muted-soft:#8494a5;
  --line:#e3e9f0;--line-strong:#d2dce7;--paper:#fff;--canvas:#f6f8fb;
  --brand:#123a5c;--brand-soft:#eaf1f8;--accent:#2563eb;--accent-ink:#1d4ed8;
  --accent-soft:#eef4ff;--violet:#6d28d9;--rose:#be185d;--amber:#b45309;
  --green:#15803d;--sky:#0369a1;
  --code-bg:#101c28;--code-head:#0a141e;--code-ink:#e3ebf3;--code-muted:#8ea4b8;
  --shadow:0 1px 2px rgba(12,23,35,.05),0 8px 24px -18px rgba(12,23,35,.25);
  --font-sans:"Segoe UI Variable Text","Segoe UI",-apple-system,BlinkMacSystemFont,Roboto,Helvetica,Arial,sans-serif;
  --font-serif:"Iowan Old Style","Palatino Linotype",Palatino,Georgia,"Times New Roman",serif;
  --font-mono:"Cascadia Code",Consolas,"SFMono-Regular",Menlo,monospace;
}
*{box-sizing:border-box}
html{scroll-behavior:smooth;-webkit-text-size-adjust:100%}
body{margin:0;background:var(--paper);color:var(--ink);font-family:var(--font-sans);font-size:16.5px;line-height:1.75;-webkit-font-smoothing:antialiased}
::selection{background:#dbeafe;color:var(--ink-strong)}
a{color:var(--accent-ink);text-decoration:none}
a:hover{text-decoration:underline;text-underline-offset:2px}
.skip{position:absolute;left:-9999px;top:0;background:var(--brand);color:#fff;padding:10px 16px;z-index:200;border-radius:0 0 8px 0}
.skip:focus{left:0}
.progress{position:fixed;inset:0 0 auto;height:3px;z-index:120;pointer-events:none}
.progress span{display:block;height:100%;width:0;background:linear-gradient(90deg,var(--accent),#0ea5e9);transition:width .1s linear}
.topbar{position:sticky;top:0;z-index:100;background:rgba(255,255,255,.88);backdrop-filter:saturate(1.6) blur(12px);border-bottom:1px solid var(--line)}
.topbar-inner{max-width:1180px;margin:0 auto;padding:0 32px;height:60px;display:flex;align-items:center;gap:20px}
.brand{display:flex;align-items:center;gap:11px;color:var(--ink-strong);text-decoration:none;min-width:0}
.brand:hover{text-decoration:none}
.brand-mark{width:32px;height:32px;border-radius:9px;background:linear-gradient(145deg,#123a5c,#1f5f8f);color:#fff;display:grid;place-items:center;font-family:var(--font-serif);font-size:19px;font-weight:700;letter-spacing:-.02em;box-shadow:inset 0 1px 0 rgba(255,255,255,.25)}
.brand-text{display:flex;flex-direction:column;line-height:1.12}
.brand-text strong{font-size:15px;letter-spacing:.01em}
.brand-text small{font-size:11px;color:var(--muted);letter-spacing:.02em}
.topbar-actions{margin-left:auto;display:flex;align-items:center;gap:18px}
.edition{font-size:.72rem;font-weight:650;letter-spacing:.09em;text-transform:uppercase;color:var(--muted);border:1px solid var(--line);background:#fbfcfe;border-radius:999px;padding:4px 10px}
.toplink{font-size:.85rem;color:var(--muted);font-weight:550}
.toplink:hover{color:var(--accent-ink);text-decoration:none}
.nav-toggle{display:none;align-items:center;gap:7px;border:1px solid var(--line-strong);background:#fff;color:var(--ink);border-radius:8px;padding:6px 11px;font:inherit;font-size:.83rem;font-weight:600;cursor:pointer}
.nav-toggle:hover{border-color:var(--muted-soft)}
.shell{max-width:1180px;margin:0 auto;padding:0 32px;display:grid;grid-template-columns:258px minmax(0,1fr);align-items:start}
.sidebar{position:sticky;top:60px;max-height:calc(100vh - 60px);overflow-y:auto;padding:32px 22px 48px 0;border-right:1px solid var(--line);scrollbar-width:thin;scrollbar-color:#cbd5e1 transparent}
.sidebar::-webkit-scrollbar{width:8px}
.sidebar::-webkit-scrollbar-thumb{background:#d5dee8;border-radius:8px}
.sidebar-close{display:none}
.side-nav{font-size:.84rem}
.nav-group + .nav-group{margin-top:20px;padding-top:16px;border-top:1px solid #eff3f8}
.nav-label{margin:0 0 3px;color:var(--accent);font-size:.67rem;font-weight:800;letter-spacing:.13em;text-transform:uppercase}
.nav-title{display:block;color:var(--brand);font-size:.92rem;font-weight:700;line-height:1.35;margin:0 0 7px;text-decoration:none}
.nav-title:hover{color:var(--accent-ink);text-decoration:none}
.nav-group ul{list-style:none;margin:0;padding:0}
.nav-group li{margin:0}
.side-nav a[href^="#"]:not(.nav-title){display:block;color:#46586b;padding:4px 9px;margin:1px -9px;border-radius:6px;line-height:1.42;text-decoration:none;transition:background .12s ease,color .12s ease}
.side-nav a[href^="#"]:not(.nav-title):hover{background:#f1f5fa;color:var(--ink-strong)}
.side-nav a.is-active:not(.nav-title){background:var(--accent-soft);color:var(--accent-ink);font-weight:650}
.side-nav code,.toc code{background:none;border:0;padding:0;font-size:.93em;color:inherit}
.content{min-width:0;padding:0 0 96px 52px}
.cover{position:relative;padding:76px 0 56px;border-bottom:1px solid var(--line);overflow:hidden}
.cover::before{content:"";position:absolute;inset:-40px -20% auto;height:320px;background:radial-gradient(520px 200px at 22% 10%,rgba(37,99,235,.10),transparent 72%),radial-gradient(380px 180px at 78% 0%,rgba(14,165,233,.08),transparent 70%);pointer-events:none}
.cover-inner{position:relative;max-width:720px}
.kicker{margin:0 0 10px;color:var(--accent-ink);font-size:.76rem;font-weight:800;letter-spacing:.19em;text-transform:uppercase}
.cover-title{margin:0;font-family:var(--font-serif);font-size:clamp(3.1rem,6vw,4.4rem);line-height:1.02;letter-spacing:-.025em;color:var(--brand);font-weight:700}
.subtitle{margin:14px 0 0;font-size:clamp(1.15rem,2.2vw,1.42rem);line-height:1.35;color:#2c4157;font-weight:500;letter-spacing:-.01em}
.tagline{max-width:57ch;margin:20px 0 0;color:var(--muted);font-size:1.02rem;line-height:1.7}
.cover-meta{display:flex;flex-wrap:wrap;gap:8px;margin-top:26px}
.cover-meta span{font-size:.75rem;font-weight:600;color:#4a5c6e;background:#f7f9fc;border:1px solid var(--line);border-radius:999px;padding:5px 11px}
.cover-actions{display:flex;flex-wrap:wrap;gap:10px;margin:28px 0 0}
.btn{display:inline-flex;align-items:center;justify-content:center;gap:8px;background:var(--brand);color:#fff;border:1px solid var(--brand);border-radius:9px;padding:9px 17px;font-size:.9rem;font-weight:650;text-decoration:none;box-shadow:0 1px 1px rgba(12,23,35,.12)}
.btn:hover{background:#0e2f4b;text-decoration:none;color:#fff}
.btn.ghost{background:#fff;color:var(--brand);border-color:var(--line-strong);box-shadow:none}
.btn.ghost:hover{background:#f8fafc;border-color:#b9c7d6}
main{padding-top:6px}
h1,h2,h3,h4{color:var(--brand);scroll-margin-top:82px}
h1.part{display:flex;flex-direction:column;gap:5px;margin:64px 0 0;padding:38px 38px;border-radius:18px;background:radial-gradient(560px 220px at 80% -20%,rgba(56,189,248,.18),transparent 70%),linear-gradient(140deg,#123a5c 0%,#173f63 55%,#1f5f8f 100%);color:#fff;box-shadow:var(--shadow);border:none;font-weight:700;letter-spacing:-.01em}
.part-label{font-size:.72rem;font-weight:800;letter-spacing:.2em;text-transform:uppercase;color:#9ccaf5}
.part-title{font-family:var(--font-serif);font-size:clamp(1.7rem,3.4vw,2.25rem);font-weight:700;line-height:1.15;letter-spacing:-.015em}
h1.part + blockquote{margin:18px 0 0;padding:0 4px;border:0;background:none;color:var(--muted);font-size:1.03rem;max-width:65ch}
h2.chapter{display:flex;flex-direction:column;gap:3px;margin:66px 0 20px;padding-bottom:14px;border-bottom:1px solid var(--line-strong);font-size:1.55rem}
h2.chapter.plain{margin-top:42px}
.chapter-no{font-size:.7rem;font-weight:800;letter-spacing:.15em;text-transform:uppercase;color:var(--accent-ink)}
.chapter-title{display:block;color:var(--brand);font-size:clamp(1.5rem,2.7vw,1.9rem);font-weight:700;line-height:1.22;letter-spacing:-.018em}
.chapter-title code{font-size:.88em;background:#eef2f7;border-color:#e1e7ef}
h3{font-size:1.15rem;font-weight:650;line-height:1.4;margin:2.15em 0 .45em;color:var(--ink-strong)}
h4{font-size:.98rem;font-weight:700;margin:1.8em 0 .35em;color:var(--muted)}
.anchor{color:var(--muted-soft);font-weight:400;font-size:.82em;margin-left:8px;opacity:0;text-decoration:none;transition:opacity .12s ease}
h3:hover .anchor,h4:hover .anchor,.anchor:focus{opacity:1}
p{margin:.85em 0;color:var(--ink)}
strong{color:var(--ink-strong);font-weight:650}
em{color:inherit}
ul,ol{margin:.75em 0 .95em 1.4em;padding:0}
li{margin:.38em 0;padding-left:.15em}
li::marker{color:var(--muted-soft)}
li>p:first-child{margin-top:0}
li>p:last-child{margin-bottom:0}
blockquote{margin:1.5em 0;padding:16px 22px;background:#f7f9fc;border-left:3px solid var(--accent);border-radius:0 10px 10px 0;color:#33465a}
blockquote p{margin:.35em 0}
hr{display:none}
code{font-family:var(--font-mono);font-size:.855em;background:#f1f5f9;color:#10344f;border:1px solid #e3eaf2;border-radius:5px;padding:.08em .38em;overflow-wrap:break-word}
.codeblock{margin:1.35em 0;border-radius:12px;overflow:hidden;background:var(--code-bg);border:1px solid #0a141e;box-shadow:0 1px 2px rgba(12,23,35,.08),0 12px 28px -22px rgba(12,23,35,.45)}
.codebar{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:7px 10px 7px 16px;background:var(--code-head);border-bottom:1px solid rgba(255,255,255,.06)}
.lang{font-size:.68rem;font-weight:700;letter-spacing:.12em;text-transform:uppercase;color:var(--code-muted)}
.copy{border:1px solid rgba(255,255,255,.18);background:rgba(255,255,255,.04);color:#c9d7e4;border-radius:6px;padding:3px 10px;font:inherit;font-size:.72rem;font-weight:600;cursor:pointer;transition:background .12s ease,color .12s ease,border-color .12s ease}
.copy:hover{background:rgba(255,255,255,.1);color:#fff;border-color:rgba(255,255,255,.32)}
.copy.is-copied{color:#7ee2a8;border-color:rgba(126,226,168,.45)}
.codeblock pre{margin:0;padding:18px 20px;overflow-x:auto;line-height:1.62;tab-size:4}
.codeblock code{display:block;background:none;border:0;border-radius:0;padding:0;color:var(--code-ink);font-size:.835rem;white-space:pre;overflow-wrap:normal}
.table-wrap{margin:1.35em 0;border:1px solid var(--line-strong);border-radius:11px;overflow:hidden}
table{width:100%;border-collapse:collapse;font-size:.92rem;line-height:1.55}
th,td{padding:10px 14px;text-align:left;vertical-align:top;border-bottom:1px solid var(--line)}
th{background:#f3f6fa;color:#33465a;font-weight:700;font-size:.82rem;letter-spacing:.02em}
tbody tr:last-child td{border-bottom:0}
tr:nth-child(even) td{background:#fbfcfe}
td code{white-space:normal}
.callout{position:relative;margin:1.6em 0;padding:15px 20px 15px 21px;border:1px solid transparent;border-left:3px solid var(--sky);border-radius:0 12px 12px 0;background:#f5f8fc;color:var(--ink)}
.callout .cl{display:flex;align-items:baseline;flex-wrap:wrap;gap:4px 8px;margin-bottom:6px;font-size:.7rem;font-weight:800;letter-spacing:.11em;text-transform:uppercase;color:var(--sky);line-height:1.4}
.callout .cl-kind::before{content:"";display:inline-block;width:7px;height:7px;border-radius:50%;background:currentColor;margin-right:7px;vertical-align:middle;position:relative;top:-1px}
.callout .cl-detail{font-size:.82rem;font-weight:600;letter-spacing:.01em;text-transform:none;color:var(--muted)}
.callout p{margin:.4em 0}
.callout p:last-child{margin-bottom:0}
.callout ul,.callout ol{margin:.5em 0 .2em 1.3em}
.callout .callout{margin:.9em 0 0;background:#fff}
.key{background:#f3f7fb;border-left-color:var(--brand)}
.key .cl{color:var(--brand)}
.theory{background:#faf8ff;border-left-color:var(--violet)}
.theory .cl{color:var(--violet)}
.realworld{background:#fff7fa;border-left-color:var(--rose)}
.realworld .cl{color:var(--rose)}
.tip{background:#f2f9fd;border-left-color:var(--sky)}
.tip .cl{color:var(--sky)}
.warning{background:#fffaeb;border-left-color:#d97706}
.warning .cl{color:var(--amber)}
.exercise{background:#f2fbf5;border-left-color:#16a34a}
.exercise .cl{color:var(--green)}
.note{background:#f6f8fa;border-left-color:#64748b}
.note .cl{color:#475569}
.difficulty{margin:-6px 0 16px}
.tag{display:inline-flex;align-items:center;gap:7px;background:var(--accent-soft);border:1px solid #dbe7ff;color:var(--accent-ink);border-radius:999px;padding:3px 10px;font-size:.67rem;font-weight:800;letter-spacing:.11em}
.tag::before{content:"";width:6px;height:6px;border-radius:50%;background:currentColor}
.tag.beg{background:#effaf3;border-color:#d3f2de;color:#166534}
.tag.int{background:#eef4ff;border-color:#dbe7ff;color:#1d4ed8}
.tag.adv{background:#fdf2f8;border-color:#fbd5e8;color:#9d174d}
details.solution{margin:1.3em 0 1.9em;border:1px solid var(--line-strong);border-left:3px solid var(--accent);border-radius:10px;background:#fbfdff;overflow:hidden}
details.solution summary{display:flex;align-items:center;gap:9px;padding:11px 16px;cursor:pointer;color:var(--accent-ink);font-weight:650;font-size:.93rem;list-style:none}
details.solution summary::-webkit-details-marker{display:none}
details.solution summary::before{content:"▸";color:var(--muted-soft);font-size:.9em;transition:transform .15s ease}
details.solution[open] summary::before{transform:rotate(90deg)}
details.solution[open] summary{border-bottom:1px solid var(--line);background:#f7faff}
details.solution>pre,details.solution>.codeblock,details.solution>p,details.solution>ul,details.solution>ol{margin-left:16px;margin-right:16px}
details.solution>.codeblock:last-child{margin-bottom:16px}
.toc{margin:46px 0 10px;padding:28px;border:1px solid var(--line-strong);border-radius:16px;background:linear-gradient(180deg,#fbfcfe,#f5f8fc);box-shadow:var(--shadow)}
.toc-head{display:flex;align-items:baseline;justify-content:space-between;gap:16px;margin-bottom:20px;padding-bottom:14px;border-bottom:1px solid var(--line)}
.toc-head h2{margin:0;padding:0;border:0;font-size:1.3rem;letter-spacing:-.01em}
.toc-head p{margin:0;color:var(--muted);font-size:.8rem}
.toc-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:22px 34px}
.toc-group h3{margin:0 0 7px;padding:0 0 7px;border-bottom:1px solid var(--line);font-size:.7rem;font-weight:800;letter-spacing:.12em;text-transform:uppercase;color:var(--muted)}
.toc-label{display:block;color:var(--accent-ink);font-size:.66rem;letter-spacing:.14em;margin-bottom:2px}
.toc-group ul{list-style:none;margin:0;padding:0}
.toc-group li{margin:0;padding:0}
.toc-group a{display:block;padding:2.5px 0;color:#31465b;font-size:.88rem;line-height:1.45}
.toc-group a:hover{color:var(--accent-ink);text-decoration:none}
footer{margin-top:64px;padding-top:20px;border-top:1px solid var(--line);color:var(--muted);font-size:.85rem;display:flex;justify-content:space-between;align-items:center;gap:16px;flex-wrap:wrap}
footer a{color:var(--muted)}
footer a:hover{color:var(--accent-ink)}
.nav-backdrop{display:none}
@media (max-width:1099px){
  .shell{display:block;padding:0 24px}
  .content{padding:0;max-width:820px;margin:0 auto}
  .sidebar{position:fixed;top:0;bottom:0;left:0;width:min(330px,88vw);max-height:none;padding:22px 20px 48px;background:#fff;border-right:1px solid var(--line-strong);box-shadow:0 20px 60px rgba(12,23,35,.22);transform:translateX(-102%);transition:transform .22s ease;z-index:130}
  body.nav-open .sidebar{transform:none}
  .sidebar-close{display:grid;place-items:center;position:absolute;top:12px;right:12px;width:32px;height:32px;border:1px solid var(--line);background:#fff;border-radius:8px;color:var(--muted);font:inherit;font-size:1.15rem;line-height:1;cursor:pointer}
  .nav-toggle{display:inline-flex}
  .nav-backdrop{position:fixed;inset:0;background:rgba(9,20,32,.45);z-index:125;backdrop-filter:blur(1px)}
  body.nav-open .nav-backdrop{display:block}
  body.nav-open{overflow:hidden}
}
@media (max-width:720px){
  body{font-size:16px}
  .topbar-inner{height:56px;padding:0 18px;gap:12px}
  .brand-text small{display:none}
  .edition,.toplink.repo{display:none}
  .topbar-actions{gap:12px}
  .shell{padding:0 18px}
  .cover{padding:52px 0 42px}
  .cover-meta{gap:6px}
  .cover-meta span{font-size:.7rem;padding:4px 9px}
  h1.part{margin-top:48px;padding:28px 24px;border-radius:14px}
  h2.chapter{margin-top:50px}
  .toc{padding:20px;border-radius:13px}
  .toc-head{display:block}
  .toc-head p{margin-top:4px}
  .toc-grid{grid-template-columns:1fr;gap:18px}
  .callout{padding:13px 16px;border-radius:0 10px 10px 0}
  .codeblock pre{padding:15px 16px}
  .codeblock code{font-size:.78rem}
  footer{flex-direction:column;align-items:flex-start}
}
@media (prefers-reduced-motion:reduce){
  html{scroll-behavior:auto}
  *,*::before,*::after{transition:none!important;animation:none!important}
}
@media print{
  @page{size:A4;margin:18mm 16mm 20mm}
  body{background:#fff;font-size:10.5pt;line-height:1.6}
  .progress,.topbar,.sidebar,.nav-backdrop,.skip,.copy,.cover-actions,.nav-toggle,.sidebar-close{display:none!important}
  .shell{display:block;max-width:none;padding:0}
  .content{max-width:none;padding:0}
  .cover{padding:40px 0;border:0;text-align:center;page-break-after:always}
  .cover::before{display:none}
  .cover-inner{max-width:none;margin:0 auto}
  .cover-title{font-size:58pt}
  .subtitle{font-size:16pt}
  .tagline{margin-left:auto;margin-right:auto;font-size:11pt}
  .cover-meta{justify-content:center}
  .cover-meta span{border-color:#ccc;background:#fff}
  h1.part{page-break-before:always;margin:0 0 18px;padding:26px 28px;border-radius:8px;background:#fff!important;color:#123a5c!important;border:1.5px solid #123a5c}
  .part-label{color:#2563eb!important}
  .part-title{color:#123a5c!important}
  h1.part + blockquote{margin-top:12px}
  h2.chapter{page-break-after:avoid;margin-top:28px}
  h3,h4{page-break-after:avoid}
  .callout,.codeblock,.table-wrap,details.solution,tr,li{page-break-inside:avoid}
  .callout{print-color-adjust:exact;-webkit-print-color-adjust:exact}
  .codeblock{background:#f5f7fa;border:1px solid #d7dee7}
  .codebar{background:#eef2f7;border-bottom:1px solid #d7dee7}
  .lang{color:#5d6d7e}
  .codeblock code{color:#17222f}
  .toc{page-break-after:always;box-shadow:none}
  .toc-grid{columns:2;column-gap:26px;display:block}
  .toc-group{break-inside:avoid;margin-bottom:14px}
  a{color:inherit;text-decoration:none}
}
"""

SCRIPT = """(function () {
  var body = document.body;
  var toggle = document.querySelector('.nav-toggle');
  var closeBtn = document.querySelector('.sidebar-close');
  var backdrop = document.querySelector('.nav-backdrop');

  function setNav(open) {
    body.classList.toggle('nav-open', open);
    if (toggle) toggle.setAttribute('aria-expanded', open ? 'true' : 'false');
  }
  if (toggle) toggle.addEventListener('click', function () { setNav(!body.classList.contains('nav-open')); });
  if (closeBtn) closeBtn.addEventListener('click', function () { setNav(false); });
  if (backdrop) backdrop.addEventListener('click', function () { setNav(false); });
  document.addEventListener('keydown', function (event) { if (event.key === 'Escape') setNav(false); });
  document.querySelectorAll('.side-nav a').forEach(function (link) {
    link.addEventListener('click', function () { setNav(false); });
  });

  document.querySelectorAll('.codeblock').forEach(function (block) {
    var button = block.querySelector('.copy');
    var code = block.querySelector('code');
    if (!button || !code) return;
    button.addEventListener('click', function () {
      var text = code.textContent;
      var done = function () {
        button.textContent = 'Copied';
        button.classList.add('is-copied');
        window.setTimeout(function () { button.textContent = 'Copy'; button.classList.remove('is-copied'); }, 1400);
      };
      var fallback = function () {
        var area = document.createElement('textarea');
        area.value = text;
        area.setAttribute('readonly', '');
        area.style.position = 'fixed';
        area.style.opacity = '0';
        document.body.appendChild(area);
        area.select();
        try { document.execCommand('copy'); done(); } catch (error) { button.textContent = 'Press Ctrl+C'; }
        document.body.removeChild(area);
      };
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(done, fallback);
      } else {
        fallback();
      }
    });
  });

  var progress = document.getElementById('progress-bar');
  var chapterTargets = Array.prototype.slice.call(document.querySelectorAll('h2.chapter[id]'));
  var navLinks = Array.prototype.slice.call(document.querySelectorAll('.side-nav a[href^="#"]'));

  function renderPosition() {
    if (progress) {
      var doc = document.documentElement;
      var max = doc.scrollHeight - doc.clientHeight;
      var ratio = max > 0 ? window.pageYOffset / max : 0;
      progress.style.width = Math.min(100, Math.max(0, ratio * 100)) + '%';
    }
    if (chapterTargets.length && navLinks.length) {
      var marker = window.pageYOffset + 130;
      var activeId = chapterTargets[0].id;
      chapterTargets.forEach(function (heading) {
        if (heading.getBoundingClientRect().top + window.pageYOffset <= marker) activeId = heading.id;
      });
      navLinks.forEach(function (link) {
        link.classList.toggle('is-active', link.getAttribute('href') === '#' + activeId);
      });
    }
  }

  var queued = false;
  function onScroll() {
    if (queued) return;
    queued = true;
    window.requestAnimationFrame(function () { renderPosition(); queued = false; });
  }
  window.addEventListener('scroll', onScroll, { passive: true });
  window.addEventListener('resize', function () {
    if (window.innerWidth > 1099) setNav(false);
    onScroll();
  });
  renderPosition();

  var solutions = Array.prototype.slice.call(document.querySelectorAll('details.solution'));
  window.addEventListener('beforeprint', function () {
    solutions.forEach(function (item) { item.dataset.wasOpen = item.open ? '1' : '0'; item.open = true; });
  });
  window.addEventListener('afterprint', function () {
    solutions.forEach(function (item) { if (item.dataset.wasOpen === '0') item.open = false; });
  });
})();
"""

HEAD = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="description" content="Cnext: The Complete Guide. A practical, structured introduction to a modern language that compiles to C.">
<meta name="color-scheme" content="light">
<title>Cnext: The Complete Guide</title>
<style>
""" + CSS + """</style>
</head>
<body>
"""

TOPBAR = """<header class="topbar">
<div class="topbar-inner">
<a class="brand" href="#top" aria-label="Cnext, The Complete Guide">
<span class="brand-mark" aria-hidden="true">C</span>
<span class="brand-text"><strong>Cnext</strong><small>The Complete Guide</small></span>
</a>
<div class="topbar-actions">
<span class="edition">v10 &middot; 2026</span>
<a class="toplink" href="#toc">Contents</a>
<a class="toplink repo" href="https://github.com/Melton122/cnext" rel="noopener">GitHub</a>
<button type="button" class="nav-toggle" aria-expanded="false" aria-controls="sidebar">Menu</button>
</div>
</div>
</header>
"""

FOOTER = """<footer>
<span>Cnext: The Complete Guide &middot; Edition v10 &middot; 2026</span>
<a href="#top">Back to top &uarr;</a>
</footer>"""

TAIL = """<script>
""" + SCRIPT + """</script>
</body>
</html>
"""


class Book:
    def __init__(self, lines):
        self.lines = lines
        self.title = "Cnext: The Complete Guide"
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
        for index, line in enumerate(self.lines):
            stripped = line.strip()
            if stripped.startswith("```"):
                in_code = not in_code
                continue
            if in_code:
                continue
            match = HEADING.match(stripped)
            if not match:
                continue
            if index == title_index:
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

    def counts(self):
        chapters = sum(1 for level, text, _ in self.headings if level == 2 and CHAPTER_HEADING.match(text))
        parts = sum(1 for level, text, _ in self.headings if level == 1 and PART_HEADING.match(text))
        appendices = sum(
            1 for level, text, _ in self.headings if level == 2 and APPENDIX_HEADING.match(text)
        )
        return chapters, parts, appendices

    def groups(self):
        groups = []
        current = None
        for level, text, hid in self.headings:
            if level == 1:
                part = PART_HEADING.match(text)
                if part:
                    label, title = "Part %s" % part.group(1).upper(), part.group(2).strip()
                elif text.lower().startswith("reference"):
                    label, title = "Reference", "Appendices"
                else:
                    label, title = "", text
                current = {"label": label, "title": title, "anchor": hid, "items": []}
                groups.append(current)
            elif level == 2 and not text.lower().startswith("table of contents"):
                if current is None:
                    current = {"label": "", "title": "Start Here", "anchor": "", "items": []}
                    groups.append(current)
                current["items"].append((text, hid))
        return groups

    def toc_html(self):
        blocks = []
        for group in self.groups():
            heading = inline(group["title"])
            if group["label"]:
                heading = '<span class="toc-label">%s</span>%s' % (inline(group["label"]), heading)
            items = "\n".join(
                '<li><a href="#%s">%s</a></li>' % (hid, inline(text)) for text, hid in group["items"]
            )
            blocks.append(
                '<section class="toc-group"><h3>%s</h3><ul>\n%s\n</ul></section>' % (heading, items)
            )
        chapters, parts, appendices = self.counts()
        summary = "%d parts &middot; %d chapters &middot; %d appendices" % (parts, chapters, appendices)
        return (
            '<nav class="toc" id="toc" aria-label="Table of contents">\n'
            '<div class="toc-head"><h2>Contents</h2><p>%s</p></div>\n'
            '<div class="toc-grid">\n%s\n</div>\n</nav>'
        ) % (summary, "\n".join(blocks))

    def sidebar_html(self):
        blocks = []
        for group in self.groups():
            if group["anchor"]:
                heading = '<a class="nav-title" href="#%s">%s</a>' % (
                    group["anchor"],
                    inline(group["title"]),
                )
            else:
                heading = '<span class="nav-title">%s</span>' % inline(group["title"])
            if group["label"]:
                heading = '<p class="nav-label">%s</p>%s' % (inline(group["label"]), heading)
            items = "\n".join(
                '<li><a href="#%s">%s</a></li>' % (hid, inline(text)) for text, hid in group["items"]
            )
            blocks.append('<section class="nav-group">%s<ul>\n%s\n</ul></section>' % (heading, items))
        return (
            '<aside class="sidebar" id="sidebar" aria-label="Book navigation">\n'
            '<button type="button" class="sidebar-close" aria-label="Close navigation">&times;</button>\n'
            '<nav class="side-nav">\n%s\n</nav>\n</aside>'
        ) % "\n".join(blocks)

    def heading_html(self, level, text, hid):
        if level == 2:
            chapter = CHAPTER_HEADING.match(text)
            appendix = APPENDIX_HEADING.match(text)
            if chapter:
                return (
                    '<h2 id="%s" class="chapter"><span class="chapter-no">Chapter %s</span>'
                    '<span class="chapter-title">%s</span></h2>'
                ) % (hid, chapter.group(1), inline(chapter.group(2)))
            if appendix:
                return (
                    '<h2 id="%s" class="chapter"><span class="chapter-no">Appendix %s</span>'
                    '<span class="chapter-title">%s</span></h2>'
                ) % (hid, appendix.group(1).upper(), inline(appendix.group(2)))
            return '<h2 id="%s" class="chapter plain"><span class="chapter-title">%s</span></h2>' % (
                hid,
                inline(text),
            )
        anchor = '<a class="anchor" href="#%s" aria-label="Link to this section">#</a>' % hid
        return "<h%d id=\"%s\">%s%s</h%d>" % (level, hid, inline(text), anchor, level)

    def part_html(self, text, hid):
        part = PART_HEADING.match(text)
        if part:
            label = "Part %s" % part.group(1).upper()
            title = part.group(2).strip()
        elif text.lower().startswith("reference"):
            label, title = "Reference", "Appendices"
        else:
            label, title = "Section", text
        return (
            '<h1 id="%s" class="part"><span class="part-label">%s</span>'
            '<span class="part-title">%s</span></h1>'
        ) % (hid, inline(label), inline(title))

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
                out.append(
                    '<details class="solution"><summary>%s</summary>\n%s\n</details>'
                    % (inline(details.group(1)), self.render(inner))
                )
                index = cursor + 1
                continue

            heading = HEADING.match(line)
            if heading:
                level = len(heading.group(1))
                text = heading.group(2).strip()
                hid = self.heading_id(level, text)
                if level == 1:
                    out.append(self.part_html(text, hid))
                else:
                    out.append(self.heading_html(level, text, hid))
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
                            badge = (
                                "beg"
                                if "beginner" in level_name and "intermediate" not in level_name
                                else "int"
                                if "intermediate" in level_name and "advanced" not in level_name
                                else "adv"
                            )
                            display = label.split(":", 1)[1].strip() if ":" in label else label
                            out.append(
                                '<p class="difficulty"><span class="tag %s">%s</span></p>'
                                % (badge, inline(display))
                            )
                            continue
                        kind_word, detail = callout_label(label)
                        label_html = ""
                        if kind_word:
                            label_html = '<span class="cl-kind">%s</span>' % inline(kind_word)
                            if detail:
                                label_html += '<span class="cl-detail">%s</span>' % inline(detail)
                        else:
                            label_html = '<span class="cl-kind">%s</span>' % inline(detail)
                        out.append(
                            '<aside class="callout %s"><span class="cl">%s</span>\n%s\n</aside>'
                            % (kind, label_html, self.render(quoted))
                        )
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
                    body.append(
                        "<tr>%s</tr>"
                        % "".join("<%s>%s</%s>" % (tag, inline(cell), tag) for cell in row)
                    )
                out.append('<div class="table-wrap"><table>\n%s\n</table></div>' % "\n".join(body))
                continue

            if UL.match(line):
                items, index = collect_list(lines, index, UL)
                out.append(
                    "<ul>\n%s\n</ul>"
                    % "\n".join("<li>%s</li>" % self.render_item(item) for item in items)
                )
                continue

            if OL.match(line):
                items, index = collect_list(lines, index, OL)
                out.append(
                    "<ol>\n%s\n</ol>"
                    % "\n".join("<li>%s</li>" % self.render_item(item) for item in items)
                )
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
        chapters, parts, appendices = self.counts()
        cover = """<header class="cover" id="top">
<div class="cover-inner">
<p class="kicker">The Complete Guide</p>
<h1 class="cover-title">Cnext</h1>
<p class="subtitle">A Modern Language That Compiles to C</p>
<p class="tagline">%s</p>
<div class="cover-meta"><span>%d chapters</span><span>%d parts</span><span>%d appendices</span><span>Edition v10</span></div>
<p class="cover-actions"><a class="btn" href="#ch1">Start reading</a><a class="btn ghost" href="#toc">Browse contents</a></p>
</div>
</header>
""" % (inline(self.subtitle), chapters, parts, appendices)
        return (
            HEAD
            + '<a class="skip" href="#main">Skip to content</a>\n'
            + '<div class="progress" aria-hidden="true"><span id="progress-bar"></span></div>\n'
            + TOPBAR
            + '<div class="shell">\n'
            + self.sidebar_html()
            + '\n<div class="content">\n'
            + cover
            + '<main id="main">\n'
            + self.render(self.lines)
            + "\n</main>\n"
            + FOOTER
            + "\n</div>\n</div>\n"
            + '<div class="nav-backdrop" aria-hidden="true"></div>\n'
            + TAIL
        )


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
