import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / ("jsonfmt.exe" if os.name == "nt" else "jsonfmt")


class JsonfmtCliTests(unittest.TestCase):
    def setUp(self):
        build_dir = ROOT.parents[1] / "build"
        build_dir.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="jsonfmt-", dir=build_dir)
        self.addCleanup(self.temp.cleanup)
        self.dir = Path(self.temp.name)

    def run_app(self, *args):
        return subprocess.run([str(APP), *[str(arg) for arg in args]],
                              cwd=self.dir, capture_output=True, text=True, timeout=30)

    def write(self, name, text):
        path = self.dir / name
        path.write_text(text, encoding="utf-8")
        return path

    def assert_ok(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_help(self):
        result = self.run_app("--help")
        self.assert_ok(result)
        self.assertIn("Usage: jsonfmt", result.stdout)

    def test_usage_error(self):
        result = self.run_app()
        self.assertEqual(result.returncode, 64)
        self.assertIn("Usage: jsonfmt", result.stdout)

    def test_format_to_stdout(self):
        self.write("in.json", '{"a":1}')
        result = self.run_app("in.json")
        self.assert_ok(result)
        self.assertEqual(result.stdout.strip(), '{"a":1}')

    def test_format_to_file(self):
        self.write("in.json", '{"a":1}')
        result = self.run_app("in.json", "-o", "out.json")
        self.assert_ok(result)
        self.assertEqual((self.dir / "out.json").read_text(encoding="utf-8"), '{"a":1}')

    def test_check_valid(self):
        self.write("in.json", '{"a":1}')
        result = self.run_app("in.json", "--check")
        self.assert_ok(result)
        self.assertEqual(result.stdout.strip(), "OK")

    def test_check_invalid(self):
        self.write("bad.json", "not json")
        result = self.run_app("bad.json", "--check")
        self.assertEqual(result.returncode, 65)
        self.assertIn("invalid JSON", result.stdout)

    def test_missing_file(self):
        result = self.run_app("missing.json")
        self.assertEqual(result.returncode, 66)
        self.assertIn("input file not found", result.stdout)

    def test_unknown_option(self):
        self.write("in.json", '{"a":1}')
        result = self.run_app("in.json", "--bogus")
        self.assertEqual(result.returncode, 64)
        self.assertIn("unknown option", result.stdout)


if __name__ == "__main__":
    unittest.main()
