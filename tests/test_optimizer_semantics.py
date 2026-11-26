import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CNEXT = Path(os.environ.get("CNEXT_EXE", ROOT / ("cnext.exe" if sys.platform == "win32" else "cnext"))).resolve()
FIXTURES = sorted((ROOT / "tests" / "optimizer").glob("*.cn"))


def run(path, *args):
    with tempfile.TemporaryDirectory(prefix="optimizer-", dir=ROOT / "build") as temp:
        result = subprocess.run(
            [str(CNEXT), "run", str(path), "-o", str(Path(temp) / "program.exe"), *args],
            cwd=temp, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=90,
        )
        return result.returncode, result.stdout.replace("\r\n", "\n"), result.stderr


def main():
    global CNEXT
    parser = argparse.ArgumentParser(description="Compare optimized and unoptimized program behavior.")
    parser.add_argument("--compiler", type=Path)
    options = parser.parse_args()
    if options.compiler is not None:
        CNEXT = options.compiler.resolve()
    if not CNEXT.exists():
        print(f"FAIL: compiler not found at {CNEXT}")
        return 1
    if not FIXTURES:
        print("FAIL: no optimizer fixtures found")
        return 1

    failed = 0
    for fixture in FIXTURES:
        plain_code, plain_out, plain_error = run(fixture, "--no-optimize")
        opt_code, opt_out, opt_error = run(fixture, "--optimize")
        if plain_code != 0 or opt_code != 0 or plain_out != opt_out:
            failed += 1
            print(f"FAIL {fixture.name}: plain={plain_code} optimized={opt_code}")
            print(f"  plain output: {plain_out!r}")
            print(f"  optimized output: {opt_out!r}")
            if plain_error or opt_error:
                print(f"  diagnostics: {plain_error}\n{opt_error}")
        else:
            print(f"PASS {fixture.name}: {plain_out.strip()!r}")

    if failed:
        print(f"Optimizer semantics: {len(FIXTURES) - failed} passed, {failed} failed")
        return 1
    print(f"Optimizer semantics: {len(FIXTURES)} passed, 0 failed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
