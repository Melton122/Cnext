"""CNext single-module registry. Public WSGI API and local maintainer commands."""
import argparse
from contextlib import closing
import gzip
import hashlib
import io
import json
from pathlib import Path
import re
import secrets
import sqlite3
import tarfile
import tomllib
from urllib.parse import parse_qs, urlsplit
from wsgiref.simple_server import make_server

MAX_UPLOAD = 2 * 1024 * 1024
MAX_EXPANDED = 8 * 1024 * 1024
SCHEMA = """
CREATE TABLE IF NOT EXISTS publishers (id TEXT PRIMARY KEY);
CREATE TABLE IF NOT EXISTS releases (
    id INTEGER PRIMARY KEY, publisher TEXT NOT NULL, name TEXT NOT NULL, version TEXT NOT NULL,
    description TEXT NOT NULL, manifest BLOB NOT NULL, source BLOB NOT NULL,
    inspected INTEGER NOT NULL DEFAULT 0, status TEXT NOT NULL DEFAULT 'pending'
);
CREATE UNIQUE INDEX IF NOT EXISTS published_version ON releases(name,version) WHERE status='approved';
"""


def read_package(payload):
    with gzip.GzipFile(fileobj=io.BytesIO(payload)) as compressed:
        raw = compressed.read(MAX_EXPANDED + 1)
    if len(raw) > MAX_EXPANDED:
        raise ValueError("Expanded package too large")
    files = {}
    with tarfile.open(fileobj=io.BytesIO(raw), mode="r:") as archive:
        for member in archive:
            name = member.name.removeprefix("./")
            if name not in ("cnext.toml", "src/lib.cn") or not member.isfile() or name in files:
                raise ValueError("Expected only cnext.toml and src/lib.cn")
            files[name] = archive.extractfile(member).read()
    meta = tomllib.loads(files["cnext.toml"].decode())["package"]
    name, version = meta["name"], meta["version"]
    if not re.fullmatch(r"[a-zA-Z][a-zA-Z0-9_-]{0,95}", name):
        raise ValueError("Invalid package name")
    if not re.fullmatch(r"(?:0|[1-9][0-9]{0,8})(?:\.(?:0|[1-9][0-9]{0,8})){2}", version):
        raise ValueError("Expected a major.minor.patch release version")
    description = meta.get("description", "")
    if not isinstance(description, str) or len(description) > 256 or any(c in description for c in "|\r\n\0"):
        raise ValueError("Invalid description")
    files["src/lib.cn"].decode()
    return name, version, description, files["cnext.toml"], files["src/lib.cn"]


class Registry:
    def __init__(self, database, public_url):
        self.database = database
        self.base = public_url.rstrip("/")
        url = urlsplit(self.base)
        if url.scheme != "https" or not url.hostname or url.query or url.fragment or url.username:
            raise ValueError("Configure an HTTPS public URL")
        with closing(sqlite3.connect(database)) as db:
            db.executescript(SCHEMA)

    def __call__(self, env, respond):
        with closing(sqlite3.connect(self.database)) as db, db:
            try:
                status, body = self.request(db, env)
            except (ValueError, KeyError, TypeError, OSError, EOFError, tarfile.TarError):
                status, body = "400 Bad Request", b"Invalid submission\n"
        respond(status, [("Content-Type", "text/plain; charset=utf-8"), ("Cache-Control", "no-store"),
                         ("Content-Length", str(len(body)))])
        return [body]

    def request(self, db, env):
        path, method = env["PATH_INFO"], env["REQUEST_METHOD"]
        if method == "POST" and path == "/api/register":
            token = secrets.token_urlsafe(32)
            db.execute("INSERT INTO publishers VALUES (?)", (hashlib.sha256(token.encode()).hexdigest(),))
            return "201 Created", (token + "\n").encode()
        if (method, path) in (("GET", "/api/me"), ("POST", "/api/publish")):
            header = env.get("HTTP_AUTHORIZATION", "")
            publisher = hashlib.sha256(header.removeprefix("Bearer ").encode()).hexdigest()
            if not header.startswith("Bearer ") or not db.execute("SELECT 1 FROM publishers WHERE id=?", (publisher,)).fetchone():
                return "401 Unauthorized", b"Invalid token\n"
            if path == "/api/me":
                return "200 OK", b"Authenticated\n"
            size = int(env.get("CONTENT_LENGTH") or 0)
            if not 0 < size <= MAX_UPLOAD:
                return "413 Payload Too Large", b"Invalid upload length\n"
            fields = read_package(env["wsgi.input"].read(size))
            db.execute("INSERT INTO releases(publisher,name,version,description,manifest,source) VALUES(?,?,?,?,?,?)",
                       (publisher, *fields))
            return "202 Accepted", b"Pending maintainer review\n"
        if method == "GET" and path == "/api/search":
            q = parse_qs(env.get("QUERY_STRING", "")).get("q", [""])[0]
            rows = db.execute("SELECT name,version,description FROM releases WHERE status='approved' AND instr(name,?) ORDER BY name,id LIMIT 100", (q,))
            return "200 OK", "".join("|".join(row) + "\n" for row in rows).encode()
        if method == "GET" and path.startswith("/api/packages/"):
            rows = db.execute("SELECT version,description,id FROM releases WHERE status='approved' AND name=? ORDER BY id",
                              (path.removeprefix("/api/packages/"),))
            return "200 OK", "".join(f"{v}|{d}|{self.base}/source/{i}.cn\n" for v,d,i in rows).encode()
        match = re.fullmatch(r"/source/([0-9]{1,18})\.cn", path)
        if method == "GET" and match:
            row = db.execute("SELECT source FROM releases WHERE status='approved' AND id=?", (match[1],)).fetchone()
            if row:
                return "200 OK", row[0]
        return "404 Not Found", b"Not found\n"


def review(db, action, submission, directory=None):
    if action == "pending":
        for row in db.execute("SELECT id,name,version,publisher FROM releases WHERE status='pending' ORDER BY id"):
            print(*row)
        return
    db.execute("BEGIN IMMEDIATE")
    row = db.execute("SELECT name,version,publisher,manifest,source FROM releases WHERE id=? AND status='pending'",
                     (submission,)).fetchone()
    if not row:
        raise ValueError("Submission is absent or already decided")
    name, version, publisher, manifest, source = row
    if action == "inspect":
        directory.mkdir()
        (directory / "src").mkdir()
        (directory / "cnext.toml").write_bytes(manifest)
        (directory / "src/lib.cn").write_bytes(source)
        db.execute("UPDATE releases SET inspected=1 WHERE id=?", (submission,))
        print(json.dumps(dict(id=submission, name=name, version=version, publisher=publisher,
                              source_sha256=hashlib.sha256(source).hexdigest())))
        return
    if action == "approve":
        if not db.execute("SELECT inspected FROM releases WHERE id=?", (submission,)).fetchone()[0]:
            raise ValueError("Inspect this submission before approving it")
        owner = db.execute("SELECT publisher FROM releases WHERE name=? AND status='approved' LIMIT 1", (name,)).fetchone()
        if owner and owner[0] != publisher:
            raise ValueError("Package belongs to another publisher")
    status = "approved" if action == "approve" else "rejected"
    db.execute("UPDATE releases SET status=? WHERE id=?", (status, submission))


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--db", type=Path, required=True)
    actions = p.add_subparsers(dest="action", required=True)
    server = actions.add_parser("serve")
    server.add_argument("--public-url", required=True)
    server.add_argument("--port", type=int, default=8080)
    actions.add_parser("pending")
    for action in ("inspect", "approve", "reject"):
        cmd = actions.add_parser(action)
        cmd.add_argument("id", type=int)
        if action == "inspect":
            cmd.add_argument("directory", type=Path)
    args = p.parse_args()
    if args.action == "serve":
        app = Registry(args.db, args.public_url)
        with make_server("127.0.0.1", args.port, app) as http:
            http.serve_forever()
    else:
        with closing(sqlite3.connect(args.db)) as db, db:
            try:
                review(db, args.action, getattr(args, "id", None), getattr(args, "directory", None))
            except (ValueError, sqlite3.IntegrityError) as err:
                p.exit(1, f"{err}\n")


if __name__ == "__main__":
    main()
