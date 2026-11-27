"""Exercise publish, version resolution and consumption through a loopback HTTPS registry."""
import sys
from urllib.request import Request, urlopen
import os
from pathlib import Path
import ssl
from contextlib import contextmanager
import subprocess
import tempfile
import threading
import unittest
from wsgiref.simple_server import make_server

ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / "build/selfhost" / ("stage2_exe.exe" if os.name == "nt" else "stage2_exe")


class PackageWorkflowTest(unittest.TestCase):
    def test_publish_then_consume(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as tmp:
            work = Path(tmp)
            cert, key = work / "cert.pem", work / "key.pem"
            subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                            "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost",
                            "-keyout", key, "-out", cert], capture_output=True, check=True)
            sys.path.insert(0, str(ROOT / "tools"))
            from package_registry import Registry
            db = work / "registry.db"
            requests = []

            @contextmanager
            def registry_server(database):
                with make_server("127.0.0.1", 0, None) as server:
                    base = f"https://localhost:{server.server_port}"
                    registry = Registry(database, base)

                    def record(env, respond):
                        requests.append((base, env["PATH_INFO"]))
                        return registry(env, respond)

                    server.set_app(record)
                    tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                    tls.load_cert_chain(cert, key)
                    server.socket = tls.wrap_socket(server.socket, server_side=True)
                    worker = threading.Thread(target=server.serve_forever)
                    worker.start()
                    try:
                        yield base
                    finally:
                        server.shutdown()
                        worker.join()

            def moderate(action, *args, expected=0):
                result = subprocess.run([sys.executable, ROOT / "tools/package_registry.py", "--db", db,
                                         action, *map(str, args)], capture_output=True, text=True)
                self.assertEqual(result.returncode, expected, result.stderr)
                return result.stdout

            with registry_server(db) as base, registry_server(work / "other.db") as other:
                trust = ssl.create_default_context(cafile=str(cert))
                with urlopen(Request(base + "/api/register", method="POST", data=b""), context=trust) as response:
                    token = response.read().decode().strip()
                profile = work / "profile"
                (profile / ".cnext").mkdir(parents=True)
                (profile / ".curlrc").write_text(f'cacert = "{cert.as_posix()}"\n')
                env = dict(os.environ, HOME=str(profile), USERPROFILE=str(profile),
                           CURL_HOME=str(profile), CURL_CA_BUNDLE=str(cert), NO_PROXY="localhost", CNEXT_REGISTRY_URL=base)

                def cli(cwd, *args, stdin=None, expected=0):
                    result = subprocess.run([STAGE, *args], cwd=cwd, env=env, capture_output=True,
                                            text=True, input=stdin, timeout=30)
                    self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
                    return result.stdout

                cli(work, "login", stdin="wrong\n", expected=1)
                cli(work, "login", stdin=token + "\n")
                cli(work, "new", "answer")
                producer = work / "answer"
                (producer / "src/lib.cn").write_text("func answer(): int { return 42 }\n")
                token_file = profile / ".cnext/token"
                scoped_token = token_file.read_bytes()
                token_file.write_text(token + "\n")
                cli(producer, "publish", expected=1)
                self.assertNotIn((base, "/api/publish"), requests)
                token_file.write_bytes(scoped_token)
                env["CNEXT_REGISTRY_URL"] = other
                cli(producer, "publish", expected=1)
                self.assertNotIn((other, "/api/publish"), requests)
                env["CNEXT_REGISTRY_URL"] = base
                cli(producer, "publish")
                self.assertNotIn("answer@0.1.0", cli(work, "search", "answer"))
                self.assertIn("answer", moderate("pending"))
                moderate("approve", 1, expected=1)
                moderate("inspect", 1, work / "review1")
                self.assertEqual((work / "review1/src/lib.cn").read_bytes(), (producer / "src/lib.cn").read_bytes())
                moderate("approve", 1)
                cli(work, "new", "consumer")
                app = work / "consumer"
                with (app / "cnext.toml").open("a") as manifest:
                    manifest.write('answer = "^0.1.0"\n')
                cli(app, "install")
                self.assertEqual((app / "packages/answer.cn").read_bytes(), (producer / "src/lib.cn").read_bytes())
                (app / "src/main.cn").write_text("import answer\nmain { printin(answer()) }\n")
                cli(app, "build", "src/main.cn", "-o", "program")
                self.assertEqual(cli(app, "run", "src/main.cn").strip(), "42")
                (app / "tests").mkdir()
                (app / "tests/test_answer.cn").write_text("import answer\nmain { printin(answer()) }\n// EXPECT: 42\n")
                self.assertIn("1 passed, 0 failed", cli(app, "test"))
                manifest = producer / "cnext.toml"
                manifest.write_text(manifest.read_text().replace('version = "0.1.0"', 'version = "0.1.1"'))
                (producer / "src/lib.cn").write_text("func answer(): int { return 43 }\n")
                cli(producer, "publish")
                self.assertNotIn("0.1.1", cli(app, "info", "answer"))
                moderate("inspect", 2, work / "review2")
                moderate("approve", 2)
                self.assertIn("answer@0.1.1", cli(app, "search", "answer"))
                self.assertIn("0.1.1", cli(app, "info", "answer"))
                cli(app, "update")
                self.assertEqual(cli(app, "run", "src/main.cn").strip(), "43")
                manifest = app / "cnext.toml"
                manifest.write_text(manifest.read_text().replace('name = "consumer"', 'name = "answer-client"'))
                before = manifest.read_bytes()
                cli(app, "remove", "answer")
                self.assertFalse((app / "packages/answer.cn").exists())
                self.assertEqual(manifest.read_bytes(), b"".join(line for line in before.splitlines(True)
                                                                if not line.startswith(b"answer =")))
                cli(producer, "publish")
                moderate("reject", 3)
                moderate("approve", 3, expected=1)
                with urlopen(Request(base + "/api/register", method="POST", data=b""), context=trust) as response:
                    other_token = response.read().decode().strip()
                cli(work, "login", stdin=other_token + "\n")
                manifest = producer / "cnext.toml"
                manifest.write_text(manifest.read_text().replace("0.1.1", "0.1.2"))
                cli(producer, "publish")
                moderate("inspect", 4, work / "review4")
                moderate("approve", 4, expected=1)
                self.assertNotIn("0.1.2", cli(app, "info", "answer"))
                cli(work, "logout")
                self.assertFalse((profile / ".cnext/token").exists())
                cli(producer, "publish", expected=1)


if __name__ == "__main__":
    unittest.main()
