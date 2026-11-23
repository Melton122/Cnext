#!/usr/bin/env python3
"""Compile and run the bootstrap corpus through the self-hosted compiler."""

import argparse
import os
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {
    "test_defer_captures.cn": "captured\n42\n42\nouter-deferred\n6\n20\n10\n7\n7\n9\ncaught\n0\n1\n",
    "test_push_strings.cn": "hello\nworld\n2\n",
    "test_nested_closure.cn": "16\n20\n20\n101\n103\n6\n8\n41\n42\n13\n1\n",
    "test_capture_cells.cn": "11\n13\n16\n1\n2\n2\n5\n5\n0\n100\n5\n1\n1\n3\ncell unwind\n1\n",
    "test_closure_temporaries.cn": "5150\n1\n9\n10\ncaught temporary\n1\n",
    "test_closure_exits.cn": "12\n11\n1\n1\n5\ncaught\n1\n",
    "test_closure_escape.cn": "2\n42\n2\n42\n2\nindirect reference escape rejected\nreference replacement rejected\n42\n1\n",
    "test_closure_cleanup.cn": "5050\n1\nbefore deferred call\n12\n1\n",
    "test_closure_alias.cn": "11\n2\n2\n33\n11\nhello!\n",
    "test_nested_generics.cn": "42\nselfhost\n",
    "test_generator_for.cn": "0\n10\n1\n11\n2\n12\n1\n2\n",
    "test_when_try_parity.cn": "7\nselected\n",
    "test_for_in_element_types.cn": "2\n3\n4\na!\nb!\n1\n2\na\nb\n",
    "test_local_object.cn": "Tests: 1 passed, 0 failed\n",
    "test_defer_order.cn": "body\n2\n7\nthrowing\nlast registered\n4\ncaught\ninside scope\nscope cleanup\nafter scope\n",
    "test_defer_matrix.cn": "main-body\nouter-body\ninner-body\ninner-cleanup\nafter-inner\nouter-cleanup\n0\nloop-cleanup\n1\nloop-cleanup\nafter-loop\nthrow-body\nthrow-inner\nthrow-outer\ncaught\nsecond\nfirst\n",
    "test_extern.cn": "42\n0\nselfhost\n",
    "test_c_strings.cn": "hello\n0\n",
    "test_hello.cn": "hello\n",
    "test_add.cn": "5\n",
    "test_vars.cn": "30\ntest\n",
    "test_if.cn": "yes\n",
    "test_while.cn": "0\n1\n2\n",
    "test_recursive.cn": "8\n",
    "test_array.cn": "1\n2\n3\n",
    "test_array_builtins_contract.cn": "5\n3\n9\n1\n1\ntrue\n2\n1\n2\n3\n2\n3\n3\n2\n1\n3\n20\n10\n2\n4\n6\n1\n4\n6\n4\n5\n0\n0\n",
    "test_string_ops.cn": "hi!\n3\n105\nn=42\nlength ok\nequal\n",
    "test_class_codegen.cn": "7\n7\n",
}


def run(command, **kwargs):
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, **kwargs)
    if result.returncode:
        raise RuntimeError(
            f"Command failed ({result.returncode}): {shlex.join(map(str, command))}\n"
            f"{result.stdout[-4000:]}{result.stderr[-4000:]}"
        )
    return result


def main():
    default_stage0 = ROOT / ("cnext.exe" if os.name == "nt" else "cnext")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage0", type=Path, default=Path(os.environ.get("CNEXT_EXE", default_stage0)))
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"))
    parser.add_argument("--stage1", type=Path, help="Test an existing bootstrap binary without rebuilding it")
    parser.add_argument("--strict", action="store_true", help="Run strict C validation on generated code")
    args = parser.parse_args()
    if not args.stage0.is_absolute():
        args.stage0 = ROOT / args.stage0
    cc = shlex.split(args.cc)
    strict_cc = cc + ["-Wall", "-Wextra", "-Wpedantic"]

    build_dir = ROOT / "build"
    build_dir.mkdir(exist_ok=True)
    stage1_dir = build_dir / "bootstrap"
    stage1_dir.mkdir(exist_ok=True)
    stage1 = args.stage1 or stage1_dir / ("cnext1_bootstrap_test.exe" if os.name == "nt" else "cnext1_bootstrap_test")
    if not stage1.is_absolute():
        stage1 = ROOT / stage1
    with tempfile.TemporaryDirectory(prefix="cnext-bootstrap-", dir=build_dir) as temp_dir:
        temp = Path(temp_dir)
        bundle = temp / "codegen.cn"
        if args.stage1 is None:
            run([sys.executable, ROOT / "tools" / "build_bundle.py", "codegen", "-o", bundle])
            run([args.stage0, "build", bundle, "-o", stage1])

        for filename, expected in EXPECTED.items():
            c_source = temp / f"{Path(filename).stem}.c"
            generated = run(
                [stage1, ROOT / "tests" / "bootstrap" / filename, "-o", c_source.relative_to(ROOT)]
            )
            if generated.stdout:
                raise RuntimeError("Stage 1 unexpectedly wrote generated C to stdout")
            if filename in {"test_nested_closure.cn", "test_capture_cells.cn", "test_closure_cleanup.cn", "test_closure_exits.cn", "test_closure_escape.cn", "test_closure_temporaries.cn"}:
                # Probe the runtime's tracking list in the generated translation
                # unit: each loop iteration must release its capture environment.
                with c_source.open("a", encoding="utf-8") as probe:
                    probe.write("\nint bootstrap_tracked_allocations(void) {\n"
                                "    int count = 0;\n"
                                "    for (_AllocNode* node = _cnext_allocs; node; node = node->next) count++;\n"
                                "    return count;\n}\n")
            if filename == "test_extern.cn":
                emitted = c_source.read_text(encoding="utf-8")
                for prototype in ("int abs(int value);", "int strcmp(CStr left, CStr right);",
                                  "CnextString cnext_join_strings(CnextArrayString parts);"):
                    if prototype not in emitted:
                        raise RuntimeError(f"Missing extern declaration: {prototype}")
            if filename == "test_hello.cn":
                stdout_c = run([stage1, ROOT / "tests" / "bootstrap" / filename]).stdout
                if stdout_c != c_source.read_text(encoding="utf-8"):
                    raise RuntimeError("Stage 1 stdout output differs from its -o output")
            executable = temp / (
                f"{Path(filename).stem}.exe" if os.name == "nt" else Path(filename).stem
            )
            run(
                [
                    *cc,
                    "-std=gnu11",
                    "-iquote",
                    ROOT / "include",
                    c_source,
                    "-o",
                    executable.relative_to(ROOT),
                ]
            )

            if args.strict:
                # Strict validation: Generated C must produce zero warnings with -Wpedantic
                val_result = subprocess.run(
                    [
                        *strict_cc,
                        "-std=gnu11",
                        "-iquote",
                        str(ROOT / "include"),
                        str(c_source),
                        "-c",
                    ],
                    cwd=ROOT,
                    text=True,
                    capture_output=True,
                )
                if val_result.stderr:
                    raise RuntimeError(
                        f"Strict C validation failed for {filename}:\n{val_result.stderr}"
                    )

            actual = run([executable.relative_to(ROOT)]).stdout
            if actual != expected:
                raise RuntimeError(f"{filename}: expected {expected!r}, got {actual!r}")
            print(f"PASS {filename}")

        invalid_escape = subprocess.run(
            [str(stage1), str(ROOT / "tests" / "bootstrap" / "invalid_closure_escape.cn"),
             "-o", str(temp / "invalid_escape.c")],
            cwd=ROOT, text=True, capture_output=True, timeout=30,
        )
        if invalid_escape.returncode == 0 or "reference captures cannot escape" not in invalid_escape.stdout + invalid_escape.stderr:
            raise RuntimeError("Stage rejected-reference-capture check failed:\n" + invalid_escape.stdout + invalid_escape.stderr)
        print("PASS rejects escaping local reference captures")

        invalid_defer_source = temp / "invalid_defer.cn"
        for keyword, source in {
            "return": "func f() { defer { return } }\nmain { f() }\n",
            "break": "func f() { while true { defer { break } } }\nmain {}\n",
            "continue": "func f() { while true { defer { continue } } }\nmain {}\n",
            "throw": 'func f() { defer { throw "no" } }\nmain {}\n',
        }.items():
            invalid_defer_source.write_text(source, encoding="utf-8")
            invalid_defer = subprocess.run(
                [str(stage1), str(invalid_defer_source),
                 "-o", str((temp / "invalid_defer.c").relative_to(ROOT))],
                cwd=ROOT, text=True, capture_output=True, timeout=30,
            )
            defer_output = invalid_defer.stdout + invalid_defer.stderr
            if invalid_defer.returncode == 0 or f"Cannot use '{keyword}' inside a deferred body" not in defer_output:
                raise RuntimeError("Stage rejected-defer-control check failed:\n" + defer_output)
            print(f"PASS rejects {keyword} from deferred body")

        for statement in ('defer printin("no")', 'when { true => printin("no") }', "yield 1"):
            invalid_defer_source.write_text(statement + "\nmain {}\n", encoding="utf-8")
            rejected = subprocess.run(
                [str(stage1), str(invalid_defer_source), "-o", str(temp / "invalid_top_level.c")],
                cwd=ROOT, text=True, capture_output=True, timeout=30,
            )
            if rejected.returncode == 0 or "Statements cannot appear at the top level" not in rejected.stdout + rejected.stderr:
                raise RuntimeError("Stage accepted a top-level statement:\n" + rejected.stdout + rejected.stderr)
        print("PASS rejects top-level defer, when, and yield")

        failed = subprocess.run(
            [str(stage1)],
            cwd=ROOT,
            text=True,
            capture_output=True,
        )
        if failed.returncode != 2 or "Usage:" not in failed.stdout:
            raise RuntimeError("Stage 1 did not return the expected usage error status")

        bad_source = temp / "bad.cn"
        bad_source.write_text("not valid Cnext !!!\n", encoding="utf-8")
        failed = subprocess.run(
            [
                str(stage1),
                str(bad_source.relative_to(ROOT)),
                "-o",
                str((temp / "bad.c").relative_to(ROOT)),
            ],
            cwd=ROOT,
            text=True,
            capture_output=True,
        )
        if failed.returncode == 0:
            raise RuntimeError("Stage 1 accepted invalid source with a success exit status")

        bad_output = temp / "missing-dir" / "out.c"
        failed = subprocess.run(
            [
                str(stage1),
                str(Path("tests") / "bootstrap" / "test_hello.cn"),
                "-o",
                str(bad_output.relative_to(ROOT)),
            ],
            cwd=ROOT,
            text=True,
            capture_output=True,
        )
        if failed.returncode == 0:
            raise RuntimeError("Stage 1 accepted an unwritable output path with a success exit status")

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
