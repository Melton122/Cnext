import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CNEXT = ROOT / ("cnext.exe" if sys.platform == "win32" else "cnext")
FIXTURES = sorted((ROOT / "tests" / "optimizer").glob("*.cn"))


def run(path, *args):
    result = subprocess.run(
        [str(CNEXT), "run", str(path), *args],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return result.returncode, result.stdout.replace("\r\n", "\n")


def main():
    if not CNEXT.exists():
        print(f"FAIL: compiler not found at {CNEXT}")
        return 1
    if not FIXTURES:
        print("FAIL: no optimizer fixtures found")
        return 1

    failed = 0
    for fixture in FIXTURES:
        plain_code, plain_out = run(fixture, "--no-optimize")
        opt_code, opt_out = run(fixture, "--optimize")
        if plain_code != 0 or opt_code != 0 or plain_out != opt_out:
            failed += 1
            print(f"FAIL {fixture.name}: plain={plain_code} optimized={opt_code}")
            print(f"  plain output: {plain_out!r}")
            print(f"  optimized output: {opt_out!r}")
        else:
            print(f"PASS {fixture.name}: {plain_out.strip()!r}")

    if failed:
        print(f"Optimizer semantics: {len(FIXTURES) - failed} passed, {failed} failed")
        return 1
    print(f"Optimizer semantics: {len(FIXTURES)} passed, 0 failed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
