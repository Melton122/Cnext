#!/usr/bin/env python3
"""Codegen parity sweep: stage0 (C compiler) vs stage1 (self-hosted).

Compiles every tests/*.cn file with both stages, normalizes line endings,
and reports which files produce identical generated C.
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def run(command, **kwargs):
    try:
        return subprocess.run(
            command,
            cwd=ROOT,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=120,
            **kwargs,
        )
    except subprocess.TimeoutExpired:
        return subprocess.CompletedProcess(command, 124)


def normalize(path):
    return path.read_bytes().replace(b"\r\n", b"\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage0", type=Path, default=Path(os.environ.get("CNEXT_EXE", "cnext.exe" if os.name == "nt" else "cnext")))
    parser.add_argument(
        "--stage1",
        type=Path,
        default=Path("build/selfhost") / ("stage2_exe.exe" if os.name == "nt" else "stage2_exe"),
    )
    parser.add_argument("--out", type=Path, default=Path("out_parity"))
    parser.add_argument("--paths", nargs="*", default=["tests/*.cn"])
    parser.add_argument("--check", action="store_true", help="Fail if any output differs or compilation fails")
    args = parser.parse_args()
    if not args.stage0.is_absolute():
        args.stage0 = ROOT / args.stage0
    if not args.stage1.is_absolute():
        args.stage1 = ROOT / args.stage1

    files = []
    for pattern in args.paths:
        files.extend(sorted(ROOT.glob(pattern)))
    if not files:
        print("No input files matched", file=sys.stderr)
        return 1

    args.out.mkdir(parents=True, exist_ok=True)
    stage0 = str(args.stage0)
    stage1 = str(args.stage1)
    results = []
    for source in files:
        print(f"checking {source}", flush=True)
        stem = source.stem
        out_s0 = args.out / f"{stem}_s0.c"
        out_s1 = args.out / f"{stem}_s1.c"
        out_s0.unlink(missing_ok=True)
        out_s1.unlink(missing_ok=True)
        r0 = run([stage0, "build", str(source), "--emit-c", str(out_s0)])
        r1 = run([stage1, str(source), "-o", str(out_s1)])
        status = "ok"
        if r0.returncode or not out_s0.exists():
            status = f"stage0-failed({r0.returncode})"
        elif r1.returncode or not out_s1.exists():
            status = f"stage1-failed({r1.returncode})"
        else:
            c0 = normalize(out_s0)
            c1 = normalize(out_s1)
            if c0 == c1:
                status = "identical"
            else:
                status = "diff"
        results.append((source.name, out_s0, out_s1, status))

    identical = 0
    diffs = []
    print(f"{'file':40} {'s0':>7} {'s1':>7}  status")
    for name, out_s0, out_s1, status in results:
        size0 = out_s0.stat().st_size if out_s0.exists() else -1
        size1 = out_s1.stat().st_size if out_s1.exists() else -1
        print(f"{name:40} {size0:7} {size1:7}  {status}")
        if status == "identical":
            identical += 1
        elif status == "diff":
            diffs.append(name)
    total = len(results)
    print(f"\n{identical}/{total} identical")
    if diffs:
        print("Differing files:")
        for name in diffs:
            print(f"  {name}")
    return 1 if args.check and identical != total else 0


if __name__ == "__main__":
    raise SystemExit(main())
