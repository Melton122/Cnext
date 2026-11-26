"""Verify real self-hosted AST rewrites as well as behavioral preservation."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "build" / "selfhost" / ("stage2_exe.exe" if os.name == "nt" else "stage2_exe")
COMPILER = Path(os.environ.get("CNEXT_EXE", DEFAULT)).resolve()


class SelfHostedOptimizerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="selfhost-opt-", dir=ROOT / "build")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.source = self.work / "source.cn"

    def invoke(self, source, *options, emit=False):
        self.source.write_text(source, encoding="utf-8")
        output = self.work / ("generated.c" if emit else "program.exe")
        if emit:
            output.unlink(missing_ok=True)
        command = [str(COMPILER), "build" if emit else "run", str(self.source)]
        command += ["--emit-c" if emit else "-o", str(output), *options]
        result = subprocess.run(command, cwd=self.work, capture_output=True, text=True, timeout=90)
        if emit and result.returncode == 0:
            return result, output.read_text(encoding="utf-8")
        return result, result.stdout

    def emitted(self, source, *options):
        result, emitted = self.invoke(source, *options, emit=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return emitted

    def assert_output(self, source, expected):
        for options in (("--no-optimize",), ("--optimize",)):
            with self.subTest(options=options):
                result, output = self.invoke(source, *options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(output, expected)

    def test_literal_arithmetic_is_actually_folded(self):
        source = 'main { int number = (2 + 3) * 4 printin(number) }'
        plain = self.emitted(source, "--no-optimize")
        optimized = self.emitted(source, "--optimize")
        self.assertRegex(plain, r"\b2\s*\+\s*3\b")
        self.assertNotRegex(optimized, r"\b2\s*\+\s*3\b")
        self.assertNotEqual(plain, optimized)
        self.assert_output(source, "20\n")

    def test_dead_branches_and_never_entered_loops_are_removed(self):
        source = '''main {
            if true { printin("selected") } else { printin("discarded_branch_marker") }
            while false { printin("discarded_loop_marker") }
        }'''
        plain = self.emitted(source, "--no-optimize")
        optimized = self.emitted(source, "--optimize")
        for marker in ("discarded_branch_marker", "discarded_loop_marker"):
            self.assertIn(marker, plain)
            self.assertNotIn(marker, optimized)
        self.assert_output(source, "selected\n")

    def test_optimizer_is_opt_in_and_disable_flag_wins(self):
        source = 'main { printin(2 + 3) }'
        plain = self.emitted(source)
        self.assertEqual(self.emitted(source, "--no-optimize"), plain)
        self.assertEqual(self.emitted(source, "--optimize", "--no-optimize"), plain)
        self.assertEqual(self.emitted(source, "--no-optimize", "--optimize"), plain)
        self.assertNotEqual(self.emitted(source, "--optimize"), plain)

    def test_side_effects_and_short_circuit_evaluation_are_preserved(self):
        self.assert_output('''
int calls = 0
func bump(): int { calls = calls + 1 return 3 }
func flag(): bool { calls = calls + 1 return true }
main {
    printin(0 * bump())
    bool first = false && flag()
    bool second = true || flag()
    bool third = flag() && false
    printin(first)
    printin(second)
    printin(third)
    printin(calls)
}
''', "0\nfalse\ntrue\nfalse\n2\n")

    def test_typed_arrays_records_and_numeric_widening_survive(self):
        self.assert_output('''
class Box { int value }
main {
    int[] first = {3}
    int[] second = {8}
    var selected = true ? first : second
    var a = new Box()
    a.value = 42
    var b = false ? new Box() : a
    printin(selected[0])
    printin(b.value)
    int exact = 16777217
    float part = 0.5
    var joined = true ? exact : part
    bool precise = joined > 16777216.0
    printin(precise)
}
''', "3\n42\ntrue\n")

    def test_long_unsigned_float_and_nonfinite_sensitive_operations_stay_correct(self):
        self.assert_output('''main {
            long largest = 9223372036854775807
            printin(largest - 1)
            uint value = 4000000000
            printin(value / 2)
            printin(1.0 / 2.0)
            printin((20 - 3) % 5)
        }''', "9223372036854775806\n2000000000\n0.500000\n2\n")

    def test_leading_zero_spelling_is_not_reinterpreted_by_optimizer(self):
        source = 'main { printin(010 + 1) }'
        self.assert_output(source, "9\n")
        self.assertRegex(self.emitted(source, "--optimize"), r"\b010\s*\+\s*1\b")

    def test_optimizer_does_not_erase_semantic_errors(self):
        for source, message in (
            ('main { if false { int n = "bad" } }', "Type mismatch"),
            ('main { var text = str_upper("x") if false { var moved = own text } printin(text) }', "Use of moved binding"),
        ):
            with self.subTest(source=source):
                result, _ = self.invoke(source, "--optimize", emit=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(message, result.stdout + result.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path)
    options, remaining = parser.parse_known_args()
    if options.compiler:
        COMPILER = options.compiler.resolve()
    unittest.main(argv=[__file__, *remaining])
