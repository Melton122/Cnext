#!/usr/bin/env python3
"""Require bootstrap C-source parity and a stage-2 to stage-3 fixed point."""
import argparse
import difflib
import hashlib
import os
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]


def normalized(path):
    # The bootstrap driver writes CRLF; compare source independent of host EOLs.
    return path.read_bytes().replace(b"\r\n", b"\n")


def require_equal(left, right, diff_path):
    before, after = normalized(left), normalized(right)
    if before != after:
        diff = difflib.unified_diff(
            before.decode("utf-8").splitlines(keepends=True),
            after.decode("utf-8").splitlines(keepends=True),
            fromfile=str(left), tofile=str(right),
        )
        diff_path.write_text("".join(diff), encoding="utf-8")
        raise RuntimeError(f"Bootstrap sources differ: {left.name} / {right.name}. See {diff_path}")
    if diff_path.exists():
        diff_path.unlink()
    return hashlib.sha256(before).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, default=ROOT / "build" / "selfhost")
    args = parser.parse_args()
    directory = args.directory.resolve()
    stage1_source = directory / "stage1_src.c"
    stage2_source = directory / "stage2_src.c"
    stage3_source = directory / "stage3_src.c"
    compiler = directory / ("stage2_exe.exe" if os.name == "nt" else "stage2_exe")
    bundle = directory / "stage1_bundle.cn"

    require_equal(stage1_source, stage2_source, directory / "stage0-stage1.diff")
    print("PASS: stage 0 and stage 1 emit identical C", flush=True)

    # Removing an old result ensures a failed compiler cannot reuse stale output.
    if stage3_source.exists():
        stage3_source.unlink()
    generated = subprocess.run(
        [str(compiler), str(bundle), "-o", str(stage3_source)],
        cwd=ROOT, capture_output=True, text=True, timeout=120,
    )
    if generated.returncode:
        raise RuntimeError(f"Stage 2 failed to compile itself:\n{generated.stdout}{generated.stderr}")
    if generated.stdout:
        raise RuntimeError(f"Stage 2 wrote unexpected stdout:\n{generated.stdout}")
    digest = require_equal(stage2_source, stage3_source, directory / "stage1-stage2.diff")
    print("PASS: stage 1 and stage 2 emit identical C (bootstrap fixed point)")
    print(f"Generated C SHA-256: {digest}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
