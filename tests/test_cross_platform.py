import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CNEXT = Path(os.environ.get("CNEXT_EXE", ROOT / ("cnext.exe" if os.name == "nt" else "cnext"))).resolve()
EXE_SUFFIX = ".exe" if os.name == "nt" else ""


class CrossPlatformTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "build").mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="cross-platform-", dir=ROOT / "build")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.output = self.work / ("program" + EXE_SUFFIX)

    def write(self, name, text):
        path = self.work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("w", encoding="utf-8", newline="\n") as output:
            output.write(text)
        return path

    def command(self, *args):
        return subprocess.run([str(arg) for arg in args], cwd=self.work,
                              capture_output=True, text=True, timeout=60)

    def assert_ok(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_download_tool_output_options_precede_terminator(self):
        source = (ROOT / "src" / "main_packages.c").read_text(encoding="utf-8")
        self.assertIn('{"curl", "-fsSL", "-o", out_path, "--", url, NULL}', source)
        self.assertIn('{"wget", "-q", "-O", out_path, "--", url, NULL}', source)

    def test_process_spawn_is_the_only_spawnvp_caller(self):
        callers = []
        for path in (ROOT / "src").glob("*.c"):
            if "_spawnvp" in path.read_text(encoding="utf-8", errors="ignore"):
                callers.append(path.name)
        self.assertEqual(callers, ["main_utils.c"])

    def test_generated_c_uses_lf_line_endings(self):
        source = self.write("main.cn", "main { printin(42) }\n")
        generated = self.work / "generated.c"
        self.assert_ok(self.command(CNEXT, "build", source, "--emit-c", generated))
        data = generated.read_bytes()
        self.assertNotIn(b"\r\n", data)
        self.assert_ok(self.command("gcc", "-std=gnu11", "-iquote", ROOT / "include",
                                    generated, "-o", self.output))
        result = self.command(self.output)
        self.assertEqual(result.stdout.strip(), "42")

    def test_io_writes_keep_exact_newlines(self):
        source = self.write("io.cn", '''import io
main {
    io.write_file("bytes.txt", "a\\nb")
    io.append_file("bytes.txt", "c\\nd")
}
''')
        self.assert_ok(self.command(CNEXT, "run", source, "-o", self.output))
        self.assertEqual((self.work / "bytes.txt").read_bytes(), b"a\nbc\nd")

    def test_emit_c_failure_removes_temp_source(self):
        source = self.write("main.cn", "main { printin(1) }\n")
        result = self.command(CNEXT, "build", source, "--emit-c", self.work / "missing" / "generated.c")
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.work / "temp_out.c").exists())


if __name__ == "__main__":
    unittest.main()
