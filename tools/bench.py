#!/usr/bin/env python3
import argparse
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import statistics
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def run_command(command, cwd=ROOT):
    result = subprocess.run([str(part) for part in command], cwd=cwd, capture_output=True,
                            text=True, errors="replace", timeout=900)
    if result.returncode != 0:
        raise RuntimeError(f"Command failed: {' '.join(str(part) for part in command)}\n"
                           f"{result.stdout}{result.stderr}")
    return result


def first_line(command):
    executable = shutil.which(str(command[0]))
    if not executable:
        return "unavailable"
    try:
        result = subprocess.run([executable, *command[1:]], cwd=ROOT, capture_output=True,
                                text=True, errors="replace", timeout=30)
        if result.returncode != 0:
            return "unavailable"
        return result.stdout.splitlines()[0].strip() if result.stdout else "unavailable"
    except (OSError, subprocess.TimeoutExpired):
        return "unavailable"


def timed(command, cwd=ROOT):
    start = time.perf_counter()
    run_command(command, cwd)
    return (time.perf_counter() - start) * 1000.0


def summarize(samples):
    return {
        "samples_ms": [round(sample, 2) for sample in samples],
        "median_ms": round(statistics.median(samples), 2),
        "min_ms": round(min(samples), 2),
        "max_ms": round(max(samples), 2),
    }


def metadata(args, make_command):
    commit = first_line(["git", "rev-parse", "--short", "HEAD"])
    if commit == "unavailable":
        commit = "unknown"
    return {
        "platform": platform.platform(),
        "python": sys.version.split()[0],
        "processor": platform.processor() or "unknown",
        "cpu_count": os.cpu_count() or 1,
        "jobs": args.jobs,
        "runs": args.runs,
        "commit": commit,
        "make": first_line([*make_command, "--version"]),
        "gcc": first_line(["gcc", "--version"]),
        "compiler": first_line([args.compiler, "version"]),
    }


def measure_cases(args, make_command):
    build_command = [*make_command, "-j", str(args.jobs)]
    generated = args.directory / "parser_bundle.c"
    emit_command = [args.compiler, "build", args.workload, "--emit-c", generated]
    source = ROOT / "src" / "main.c"
    cases = {"clean-build": [], "incremental-build": [], "cnext-emit-c": []}

    for _ in range(args.runs):
        run_command([*make_command, "clean"])
        cases["clean-build"].append(timed(build_command))
        source.touch()
        cases["incremental-build"].append(timed(build_command))
        cases["cnext-emit-c"].append(timed(emit_command))

    return {name: summarize(samples) for name, samples in cases.items()}


def compare(results, baseline):
    report = []
    baseline_cases = baseline.get("cases", {}) if baseline else {}
    for name, current in results["cases"].items():
        previous = baseline_cases.get(name)
        if not previous:
            report.append((name, current["median_ms"], None, None))
            continue
        before = previous["median_ms"]
        report.append((name, current["median_ms"], before, current["median_ms"] / before if before else None))
    return report


def print_report(results, comparison):
    print(f"Platform: {results['metadata']['platform']}")
    print(f"Commit: {results['metadata']['commit']}  Jobs: {results['metadata']['jobs']}  Runs: {results['metadata']['runs']}")
    print(f"{'case':20s} {'median':>12s} {'baseline':>12s} {'ratio':>8s}")
    for name, median, before, ratio in comparison:
        baseline_text = f"{before:.2f} ms" if before is not None else "-"
        ratio_text = f"{ratio:.2f}x" if ratio is not None else "-"
        print(f"{name:20s} {median:9.2f} ms {baseline_text:>12s} {ratio_text:>8s}")


def main():
    parser = argparse.ArgumentParser(description="Measure reproducible Cnext build and compiler workloads")
    parser.add_argument("--make", default=os.environ.get("MAKE", "make"))
    parser.add_argument("--compiler", type=Path, default=ROOT / ("cnext.exe" if os.name == "nt" else "cnext"))
    parser.add_argument("--workload", type=Path, default=ROOT / "src2" / "parser_bundle.cn")
    parser.add_argument("--directory", type=Path, default=ROOT / "build" / "bench")
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--jobs", type=int, default=min(4, os.cpu_count() or 1))
    parser.add_argument("--save-baseline", action="store_true")
    parser.add_argument("--check-regression", type=float)
    args = parser.parse_args()

    if args.runs < 1 or args.jobs < 1:
        raise RuntimeError("--runs and --jobs must be positive")
    args.compiler = args.compiler.resolve()
    args.workload = args.workload.resolve()
    if not args.compiler.exists():
        raise RuntimeError(f"Compiler not found: {args.compiler}")
    if not args.workload.exists():
        raise RuntimeError(f"Workload not found: {args.workload}")

    args.directory.mkdir(parents=True, exist_ok=True)
    make_command = shlex.split(args.make)
    results_path = args.directory / "results.json"
    baseline_path = args.directory / "baseline.json"
    baseline = json.loads(baseline_path.read_text(encoding="utf-8")) if baseline_path.exists() else None

    print(f"Benchmarking with {' '.join(str(part) for part in make_command)} -j{args.jobs} "
          f"for {args.runs} run(s)...", flush=True)
    report = {
        "metadata": metadata(args, make_command),
        "cases": measure_cases(args, make_command),
    }
    results_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    comparison = compare(report, baseline)
    print_report(report, comparison)
    print(f"Results: {results_path}")

    if args.save_baseline:
        baseline_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(f"Baseline: {baseline_path}")

    if args.check_regression is not None:
        regressions = [name for name, _, before, ratio in comparison
                       if before is not None and ratio is not None and ratio > args.check_regression]
        if regressions:
            raise RuntimeError(f"Benchmark regression above {args.check_regression:.2f}x: {', '.join(regressions)}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.TimeoutExpired, ValueError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
