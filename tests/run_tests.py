"""
Extended test runner for Cnext compiler.

This runner provides additional tests beyond the built-in `cnext test` command:
- Negative tests (expect compilation failure with specific error)
- Runtime tests (expect runtime failure with specific error)
- Example compilation tests

Primary test runner: `make test` (uses built-in cnext test)
This runner: `python tests/run_tests.py`
"""
import subprocess
import os
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_EXE = "cnext.exe" if os.name == "nt" else "cnext"
DEFAULT_OUT = "out.exe" if os.name == "nt" else "out"
CNEXT_EXE = Path(os.environ.get("CNEXT_EXE", ROOT / DEFAULT_EXE))
OUT_EXE = ROOT / DEFAULT_OUT

def run_test(test_file, is_example=False):
    """Build a .cn file, run it, and check for errors."""
    if is_example:
        test_path = ROOT / "examples" / test_file
    else:
        test_path = ROOT / "tests" / test_file
    
    try:
        result = subprocess.run(
            [str(CNEXT_EXE), "build", str(test_path)],
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=10
        )
    except Exception as e:
        return False, str(e), ""
    
    c_file = ROOT / "temp_out.c"
    c_code = ""
    if c_file.exists():
        with c_file.open("r", encoding="utf-8") as f:
            c_code = f.read()
    
    stderr = result.stderr
    # Filter out gcc not found errors and warnings
    stderr_lines = [l for l in stderr.splitlines() if "gcc" not in l.lower() and "warning" not in l.lower()]
    has_error = any("error" in l.lower() for l in stderr_lines)
    
    if has_error or result.returncode != 0:
        return False, "\n".join(stderr_lines), c_code
    
    # Try to run the executable
    run_output = ""
    if OUT_EXE.exists():
        try:
            run_result = subprocess.run([str(OUT_EXE)], capture_output=True, text=True, timeout=5)
            run_output = run_result.stdout.strip()
        except Exception as e:
            run_output = f"Run failed: {e}"
    
    return True, f"OK (output: {run_output[:200]})" if run_output else "OK", c_code

def run_negative_test(name, source, expected_message):
    """Compile invalid source and verify a useful compiler error is produced."""
    with tempfile.NamedTemporaryFile("w", suffix=".cn", dir=ROOT / "tests", delete=False, encoding="utf-8") as temp:
        temp.write(source)
        temp_path = Path(temp.name)

    try:
        result = subprocess.run(
            [str(CNEXT_EXE), "build", str(temp_path)],
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=10
        )
        output = f"{result.stdout}\n{result.stderr}"
        success = result.returncode != 0 and expected_message in output
        message = "OK" if success else output.strip()
        return success, message
    finally:
        temp_path.unlink(missing_ok=True)

def run_runtime_negative_test(test_file, expected_message):
    test_path = ROOT / "tests" / test_file
    result = subprocess.run(
        [str(CNEXT_EXE), "build", str(test_path)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=10
    )
    if result.returncode != 0:
        return False, result.stderr.strip()

    run_result = subprocess.run(
        [str(OUT_EXE)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=5
    )
    output = f"{run_result.stdout}\n{run_result.stderr}"
    success = run_result.returncode != 0 and expected_message in output
    return success, "OK" if success else output.strip()

def main():
    scratch_file = ROOT / "test_output.txt"
    scratch_file.unlink(missing_ok=True)

    tests = [
        "test_methods.cn",
        "test_traits.cn",
        "test_random.cn",
        "test_testing.cn",
        "test_break_continue.cn",
        "test_array_index.cn",
        "test_increment.cn",
        "test_var_inference.cn",
        "test_functions.cn",
        "test_full.cn",
        "test_io.cn",
        "test_file.cn",
        "test_net.cn",
        "test_json.cn",
        "test_math.cn",
        "test_packages.cn",
        "test_try_catch.cn",
        "test_inheritance.cn",
        "test_interfaces.cn",
        "test_v3.cn",
        "test_os.cn",
        "test_string_utils.cn",
        "test_time.cn",
        "test_regex.cn",
        "test_collections.cn",
        "test_crypto.cn",
        "test_path.cn",
        "test_encoding.cn",
        "test_process.cn",
        "test_slices.cn",
        "test_casting.cn",
        "test_lambdas.cn",
        "test_named_args.cn",
        "test_null.cn",
        "test_null_safety_ops.cn",
        "test_operator_overloading.cn",
        "test_optimizer.cn",
        "test_tuples.cn",
        "test_typeof.cn",
        "test_variadic.cn",
        "test_default_args.cn",
        "test_constexpr.cn",
        "test_v4_features.cn",
        "test_v9.cn",
    ]
    
    all_passed = True
    for test in tests:
        success, msg, c_code = run_test(test)
        status = "PASS" if success else "FAIL"
        if not success:
            all_passed = False
        print(f"[{status}] {test}: {msg}")
        if not success and c_code:
            print(f"  Generated C code preview:\n{c_code[:500]}")
    
    # Also run existing examples
    examples = [
        "hello.cn",
        "loops.cn",
        "classes.cn",
        "v1_1_features.cn",
        "v2_features.cn",
        "v3_features.cn",
        "var.cn",
    ]
    for example in examples:
        success, msg, c_code = run_test(example, is_example=True)
        status = "PASS" if success else "FAIL"
        if not success:
            all_passed = False
        print(f"[{status}] examples/{example}: {msg}")

    negative_tests = [
        ("missing_main", "func helper() {\n}\n", "main"),
        ("undeclared_identifier", "main {\n    printin(missing)\n}\n", "Undeclared identifier"),
        ("const_postfix", "main {\n    const int x = 1\n    x++\n}\n", "Cannot mutate const variable"),
        ("unterminated_comment", "main {\n    /* nope\n}\n", "Unterminated block comment"),
    ]

    for name, source, expected in negative_tests:
        success, msg = run_negative_test(name, source, expected)
        status = "PASS" if success else "FAIL"
        if not success:
            all_passed = False
        print(f"[{status}] negative/{name}: {msg}")

    runtime_negative_tests = [
        ("test_array_bounds.cn", "Array bounds out of range"),
        ("test_testing_fail.cn", "FAIL"),
    ]

    for test, expected in runtime_negative_tests:
        success, msg = run_runtime_negative_test(test, expected)
        status = "PASS" if success else "FAIL"
        if not success:
            all_passed = False
        print(f"[{status}] negative/{test}: {msg}")
    
    if all_passed:
        print("\nAll tests passed!")
    else:
        print("\nSome tests failed.")

    scratch_file.unlink(missing_ok=True)
    
    return 0 if all_passed else 1

if __name__ == "__main__":
    sys.exit(main())
