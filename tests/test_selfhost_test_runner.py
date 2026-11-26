"""Exercise the self-hosted test command without relying on the C compiler driver."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SUFFIX = ".exe" if os.name == "nt" else ""
COMPILER = Path(os.environ.get("CNEXT_EXE", ROOT / "build" / "selfhost" / ("stage2_exe" + SUFFIX))).resolve()


class SelfHostedTestRunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="selfhost-tests-", dir=ROOT / "build")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.env = dict(os.environ, CNEXT_INCLUDE=str(ROOT / "include"))

    def write(self, name, source):
        path = self.work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(source, encoding="utf-8")
        return path

    def cli(self, *args, **kwargs):
        return subprocess.run([str(COMPILER), *map(str, args)], cwd=self.work,
                              env=self.env, capture_output=True, text=True, timeout=120, **kwargs)

    def assert_ok(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def assert_clean(self):
        self.assertEqual(list(self.work.glob(".cnext-*")), [])

    def test_recursive_sorted_discovery_and_exclusions(self):
        self.write("suite/z.cn", "main {}")
        self.write("suite/a/nested.cn", "main {}")
        for directory in (".hidden", "bootstrap", "optimizer"):
            self.write(f"suite/{directory}/ignored.cn", "invalid source")
        result = self.cli("test", "suite", "--list")
        self.assert_ok(result)
        self.assertEqual(result.stdout, "a/nested.cn\nz.cn\n")
        self.assert_clean()

    def test_default_directory_and_positional_filter(self):
        self.write("tests/a/selected.cn", "main {}")
        self.write("tests/other.cn", "main {}")
        result = self.cli("test", "selected", "--list")
        self.assert_ok(result)
        self.assertEqual(result.stdout, "a/selected.cn\n")
        self.assertIn("cnext test", self.cli("--help").stdout)

    def test_optimizer_options_are_forwarded_to_builds(self):
        path = self.write("arithmetic.cn", '// EXPECT: 5\nmain { printin(2 + 3) }')
        for options in (("--optimize",), ("--optimize", "--no-optimize"),
                        ("--no-optimize", "--optimize")):
            with self.subTest(options=options):
                self.assert_ok(self.cli("test", path, *options))
                self.assert_clean()

    def test_single_file_with_spaces_and_shell_metacharacters(self):
        path = self.write("suite & symbols/test ; literal.cn", '// EXPECT: literal\nmain { printin("literal") }')
        result = self.cli("test", path)
        self.assert_ok(result)
        self.assertIn("Results: 1 passed, 0 failed", result.stdout)
        self.assert_clean()

    def test_stdin_lines_and_ordered_expected_substrings(self):
        path = self.write("stdin.cn", '// STDIN: first\n// STDIN:\n// EXPECT: first\n// EXPECT: 0\n'
                          'main { printin(input()) printin(len(input())) }')
        self.assert_ok(self.cli("test", path))
        self.assert_clean()

    def test_no_stdin_marker_uses_eof_instead_of_inherited_input(self):
        path = self.write("eof.cn", '// EXPECT: 0\nmain { printin(len(input())) }')
        self.assert_ok(self.cli("test", path, input="must not reach the test\n"))
        self.assert_clean()

    def test_successful_program_without_output_or_expectations(self):
        path = self.write("quiet.cn", "main {}")
        self.assert_ok(self.cli("test", path))
        self.assert_clean()

    def test_expectation_failure_does_not_stop_later_tests(self):
        self.write("suite/a.cn", '// EXPECT: missing\nmain { printin("other") }')
        self.write("suite/z.cn", '// EXPECT: later\nmain { printin("later") }')
        result = self.cli("test", "suite")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("FAIL: a.cn", result.stdout)
        self.assertIn("PASS: z.cn", result.stdout)
        self.assertIn("Results: 1 passed, 1 failed", result.stdout)
        self.assert_clean()

    def test_compile_error_fails_and_cleans_up(self):
        path = self.write("broken.cn", "main { unknown_name() }")
        result = self.cli("test", path)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("compile error", result.stdout)
        self.assert_clean()

    def test_nonzero_exit_fails_even_when_expectations_match(self):
        path = self.write("nonzero.cn", '// EXPECT: matched\nmain { printin("matched") return 7 }')
        result = self.cli("test", path)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("status 7; expected 0", result.stdout)
        self.assert_clean()

    def test_explicit_exit_status_and_combined_stderr(self):
        path = self.write("expected failure.cn", '// EXIT: 1\n// EXPECT: FAIL\ntest "failure" { assert false }')
        self.assert_ok(self.cli("test", path))
        path.write_text('// EXIT: 7\n// EXPECT: seven\nmain { printin("seven") return 7 }')
        self.assert_ok(self.cli("test", path))
        self.assert_clean()

    def test_invalid_exit_metadata_fails_without_running_program(self):
        for metadata in ("// EXIT: -1", "// EXIT: 256", "// EXIT: no", "// EXIT:", "// EXIT: 1\n// EXIT: 1"):
            with self.subTest(metadata=metadata):
                path = self.write("invalid.cn", metadata + '\nmain { write_file("ran.txt", "bad") }')
                result = self.cli("test", path)
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn("invalid EXPECT/STDIN/EXIT metadata", result.stdout)
                self.assertFalse((self.work / "ran.txt").exists())
                self.assert_clean()

    def test_empty_missing_and_invalid_cli_are_nonzero(self):
        (self.work / "empty").mkdir()
        result = self.cli("test", "empty")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("No test files", result.stdout)
        self.assertEqual(self.cli("test", "./missing.cn").returncode, 74)
        self.assertEqual(self.cli("test", "--unknown").returncode, 64)
        self.assertEqual(self.cli("test", "--cc").returncode, 64)
        self.assertEqual(self.cli("test", "one", "two").returncode, 64)
        self.assert_clean()

    def test_capture_bridge_refuses_existing_output_and_missing_input(self):
        self.write("preserved.log", "keep me")
        path = self.write("bridge.cn", '''
extern "C" func cnext_test_execute(str[] args, str input_path, str output_path): int
main {
    str[] args = {"not-a-real-executable"}
    printin(cnext_test_execute(args, "", "preserved.log"))
    printin(cnext_test_execute(args, "missing-input.txt", "must-not-create.log"))
}
''')
        result = self.cli("run", path)
        self.assert_ok(result)
        self.assertEqual(result.stdout, "-1\n-1\n")
        self.assertEqual((self.work / "preserved.log").read_text(), "keep me")
        self.assertFalse((self.work / "must-not-create.log").exists())
        self.assert_clean()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path)
    args, remaining = parser.parse_known_args()
    if args.compiler:
        COMPILER = args.compiler.resolve()
    unittest.main(argv=[__file__, *remaining])
