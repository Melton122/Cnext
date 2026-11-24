#!/usr/bin/env python3

import argparse
import os
import random
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CNEXT = Path(os.environ.get("CNEXT_EXE", ROOT / ("cnext.exe" if os.name == "nt" else "cnext"))).resolve()
EXE_SUFFIX = ".exe" if os.name == "nt" else ""
CRASH_CODES = {-1073741819, 3221225477, -11, 139}
MAX_OUTPUT = 20000
TOKENS = [
    "main", "func", "class", "var", "int", "float", "str", "bool", "if", "else",
    "for", "while", "return", "throw", "try", "catch", "defer", "when", "match",
    "import", "own", "as", "in", "true", "false", "null", "printin", "input",
    "x", "y", "f", "C", "0", "1", "42", "3.14", "0x", "1e", "'", '"', "\\",
    "{", "}", "(", ")", "[", "]", ",", ";", ":", ".", "=", "+", "-", "*", "/",
    "<", ">", "!", "?", "|", "&", "/*", "*/", "//", "->", "=>", "@", "#",
]
PREFIXES = ["", "main {", "func f() {", "class C {", "func f(int x): int {"]
SUFFIXES = ["", "}", "}}", "])", ");"]

SEED_CASES = [
    "",
    "main {",
    "}",
    '"unterminated',
    "main { var x = 0x }\n",
    "main { /* unterminated\n",
    "main { func f( }\n",
    "main { x = (((((((((1 }\n",
    "main { return 1 }\n",
    "class { func }\n",
    'main { var x = "\\q" }\n',
    "main { if ( true { }\n",
    "main { for ( var i = 0; i < 10; i++ }\n",
    "main { var x = [ }\n",
    "main { when { 1 => }\n",
    "func f(,): int { return 1 }\nmain { printin(f()) }\n",
    "main { var x: } \n",
    "main { catch { } }\n",
    "main { defer return }\n",
    "main { var 1 = 2 }\n",
]


def generated_case(rng):
    kind = rng.randrange(6)
    if kind == 0:
        return " ".join(rng.choice(TOKENS) for _ in range(rng.randrange(1, 40)))
    if kind == 1:
        prefix = rng.choice(PREFIXES)
        suffix = rng.choice(SUFFIXES)
        body = " ".join(rng.choice(TOKENS) for _ in range(rng.randrange(1, 30)))
        return f"{prefix} {body} {suffix}"
    if kind == 2:
        opens = rng.randrange(1, 12)
        closes = rng.randrange(0, 12)
        return "main { " + "{" * opens + "1" + "}" * closes + " }"
    if kind == 3:
        depth = rng.randrange(1, 40)
        return "main { var x = " + "(" * depth + "1" + ")" * depth + " }"
    if kind == 4:
        length = rng.randrange(1, 80)
        junk = "".join(chr(rng.randrange(1, 128)) for _ in range(length))
        return "main { var x = " + junk + " }"
    return "".join(rng.choice(TOKENS) for _ in range(rng.randrange(1, 60)))


class InvalidInputFuzzTests(unittest.TestCase):
    seed = 20261124
    cases = 100

    def setUp(self):
        (ROOT / "build").mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="fuzz-invalid-", dir=ROOT / "build")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.output = self.work / ("program" + EXE_SUFFIX)

    def compile_case(self, index, source):
        source_path = self.work / f"case_{index}.cn"
        source_path.write_text(source, encoding="utf-8")
        try:
            return subprocess.run(
                [str(CNEXT), "build", str(source_path), "-o", str(self.output)],
                cwd=ROOT,
                capture_output=True,
                text=True,
                errors="replace",
                timeout=10,
            )
        finally:
            source_path.unlink(missing_ok=True)

    def check_case(self, index, source):
        try:
            result = self.compile_case(index, source)
        except subprocess.TimeoutExpired:
            self.fail(f"case {index} timed out:\n{source!r}")
        output = result.stdout + result.stderr
        if result.returncode < 0 or result.returncode in CRASH_CODES:
            self.fail(f"case {index} crashed with {result.returncode}:\n{source!r}\n{output[:2000]}")
        if len(output) > MAX_OUTPUT:
            self.fail(f"case {index} emitted {len(output)} bytes:\n{source!r}")
        if "internal error" in output.lower():
            self.fail(f"case {index} reported an internal error:\n{source!r}\n{output[:2000]}")

    def test_compile_does_not_execute_source(self):
        source_path = self.work / "side_effect.cn"
        source = 'import io\nmain {\n    io.write_file("side_effect.txt", "written")\n}\n'
        source_path.write_text(source, encoding="utf-8")
        result = subprocess.run(
            [str(CNEXT), "build", str(source_path), "-o", str(self.output)],
            cwd=self.work,
            capture_output=True,
            text=True,
            errors="replace",
            timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse((self.work / "side_effect.txt").exists())

    def test_seed_cases(self):
        for index, source in enumerate(SEED_CASES):
            self.check_case(index, source)

    def test_generated_cases(self):
        rng = random.Random(self.seed)
        for index, source in enumerate(generated_case(rng) for _ in range(self.cases)):
            self.check_case(len(SEED_CASES) + index, source)


def main():
    parser = argparse.ArgumentParser(description="Fuzz invalid Cnext sources for crashes")
    parser.add_argument("--cases", type=int, default=100)
    parser.add_argument("--seed", type=int, default=20261124)
    args = parser.parse_args()
    InvalidInputFuzzTests.seed = args.seed
    InvalidInputFuzzTests.cases = args.cases
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(InvalidInputFuzzTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
