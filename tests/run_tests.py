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
        "test_array_builtins_contract.cn",
        "test_increment.cn",
        "test_var_inference.cn",
        "test_functions.cn",
        "test_full.cn",
        "test_io.cn",
        "test_file.cn",
        "test_net.cn",
        "test_json.cn",
        "test_math.cn",
        # test_packages.cn removed — requires external package registry (test_pkg doesn't exist)
        "test_ternary.cn",
        "test_try_catch.cn",
        "test_inheritance.cn",
        "test_interfaces.cn",
        "test_v3.cn",
        "test_v3_new.cn",
        "test_os.cn",
        "test_string_utils.cn",
        "test_string_utils_v8.cn",
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
        "test_optimizer_v8.cn",
        "test_tuples.cn",
        "test_typeof.cn",
        "test_variadic.cn",
        "test_default_args.cn",
        "test_constexpr.cn",
        "test_v4_features.cn",
        "test_v8.cn",
        "test_v8_comprehensive.cn",
        "test_v9.cn",
        "test_v9_features.cn",
        "test_async.cn",
        "test_coroutines.cn",
        "test_generators_full.cn",
        "test_generics_full.cn",
        "test_closures_full.cn",
        "test_new_types.cn",
        "test_threading.cn",
        "test_int.cn",
        "test_int_only.cn",
        "test_uint.cn",
        "test_ubyte.cn",
        "test_simple.cn",
        "test_simple_uint.cn",
        "test_cli_features.cn",
        "test_math_builtins.cn",
        "test_string_builtins.cn",
        "test_core_builtins.cn",
        "test_encoding_builtins.cn",
        "test_system_builtins.cn",
        "test_typeconv_builtins.cn",
        "test_file_builtins.cn",
        "test_comprehensive_builtins.cn",
        "test_json_arrays.cn",
        "test_encoding_edges.cn",
        "test_array_params.cn",
        "test_bit_extract.cn",
        "test_bit_match.cn",
        "test_c_interop.cn",
        "test_c_strings.cn",
        "test_class_ctor_params.cn",
        "test_class_field_arrays.cn",
        "test_class_return.cn",
        "test_class_selfref.cn",
        "test_class_zero_init.cn",
        "test_defer.cn",
        "test_defer_captures.cn",
        "test_defer_matrix.cn",
        "test_for_in_element_types.cn",
        "test_forward_calls.cn",
        "test_generic_class_field_array.cn",
        "test_interp_blank_placeholder.cn",
        "test_mem_scope.cn",
        "test_memory.cn",
        "test_obj_array_element_var.cn",
        "test_parser_shape.cn",
        "test_refcount.cn",
        "test_sensor_record.cn",
        "test_str_field_compare.cn",
        "test_str_split_join.cn",
        "test_strict_numeric_types.cn",
        "test_try_scopes.cn",
        "test_when_try_parity.cn",
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
        "comprehensive_demo.cn",
    ]
    for example in examples:
        success, msg, c_code = run_test(example, is_example=True)
        status = "PASS" if success else "FAIL"
        if not success:
            all_passed = False
        print(f"[{status}] examples/{example}: {msg}")

    negative_tests = [
        ("return_type", 'func ready(): bool { return "yes" }\nmain { printin(ready()) }', "Return type mismatch"),
        ("return_integer_as_text", 'func name(): str { return 123 }\nmain {}', "Return type mismatch"),
        ("return_null_as_int", 'func n(): int { return null }\nmain {}', "Return type mismatch"),
        ("field_layout_variable", 'main { str p = "#"\nprintin(1 ~> p) }', "quoted field layout"),
        ("field_layout_empty", 'main { printin(1 ~> "") }', "1 to 64 bits"),
        ("field_layout_wide", 'main { printin(1 ~> "' + "#" * 65 + '") }', "1 to 64 bits"),
        ("field_layout_unselected", 'main { printin(1 ~> "?_?") }', "select at least one bit"),
        ("field_layout_split", 'main { printin(1 ~> "#_?_#") }', "one contiguous field"),
        ("field_layout_digit", 'main { printin(1 ~> "#1") }', "only #, ? and _"),
        ("field_layout_float", 'main { printin(0.5 ~> "#") }', "integer operand"),
        ("field_layout_array", 'main { int[] a = {1}\nprintin(a ~> "#") }', "integer operand"),
        ("bit_pattern_digit", 'main { printin(1 ~= "12") }', "only 0, 1, ? and _"),
        ("bit_pattern_empty", 'main { printin(1 ~= "_") }', "1 to 64 bits"),
        ("bit_pattern_width", 'main { printin(1 ~= "' + "?" * 65 + '") }', "1 to 64 bits"),
        ("bit_pattern_variable", 'main { str p = "1"\nprintin(1 ~= p) }', "quoted bit pattern"),
        ("bit_pattern_float", 'main { printin(1.5 ~= "?") }', "integer operand"),
        ("bit_pattern_array", 'main { int[] a = {1}\nprintin(a ~= "?") }', "integer operand"),
        ("bit_pattern_text", 'main { str[] a = {"1"}\nprintin(a[0] ~= "?") }', "integer operand"),
        ("missing_main", "func helper() {\n}\n", "main"),
        ("undeclared_identifier", "main {\n    printin(missing)\n}\n", "Undeclared identifier"),
        ("const_postfix", "main {\n    const int x = 1\n    x++\n}\n", "Cannot mutate const variable"),
        ("unterminated_comment", "main {\n    /* nope\n}\n", "Unterminated block comment"),
        ("duplicate_decl", "main {\n    var x = 1\n    var x = 2\n}\n", "already declared"),
        ("break_outside_loop", "main {\n    break\n}\n", "break"),
        ("continue_outside_loop", "main {\n    continue\n}\n", "continue"),
        ("top_level_defer", "defer printin(\"no\")\nmain {}\n", "Statements cannot appear at the top level"),
        ("top_level_when", 'when { true => printin("no") }\nmain {}\n', "Statements cannot appear at the top level"),
        ("top_level_yield", "yield 1\nmain {}\n", "Statements cannot appear at the top level"),
        ("defer_return", "func f() { defer { return } }\nmain { f() }\n", "Cannot use 'return' inside a deferred body"),
        ("defer_break", "func f() { while true { defer { break } } }\nmain { f() }\n", "Cannot use 'break' inside a deferred body"),
        ("defer_continue", "func f() { while true { defer { continue } } }\nmain { f() }\n", "Cannot use 'continue' inside a deferred body"),
        ("defer_throw", "func f() { defer { throw \"no\" } }\nmain { f() }\n", "Cannot use 'throw' inside a deferred body"),
        ("assign_to_literal", "main {\n    42 = 5\n}\n", "Invalid assignment"),
        ("assign_type_mismatch", "main {\n    int x = 1\n    x = \"hello\"\n}\n", "Type mismatch in assignment"),
        ("try_propagation_non_void", "func maybe(): int { return 1 }\nfunc f(): int {\n    maybe()?;\n    return 1\n}\nmain { printin(f()) }\n", "Try expression requires a void function"),
        ("option_operand_undeclared", "main {\n    var opt = option(missing)\n}\n", "Undeclared identifier"),
        ("missing_brace", "main {\n    var x = 1\n", "}"),
        ("type_mismatch_init", "main {\n    int x = \"hello\"\n}\n", "type"),
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

    # --- Formatter tests ---
    print("\n--- Formatter Tests ---")
    formatter_tests = [
        ("format_basic", "main {\nvar x = 1\nprintin(x)\n}\n"),
        ("format_functions", "func add(int a, int b): int {\nreturn a + b\n}\nmain {\nvar result = add(1, 2)\nprintin(result)\n}\n"),
        ("format_classes", "class Point {\nvar x: int\nvar y: int\nfunc distance(): float {\nreturn sqrt(x * x + y * y)\n}\n}\n"),
        ("format_nested", "main {\nfor var i = 0; i < 10; i++ {\nif i > 5 {\nprintin(i)\n}\n}\n}\n"),
    ]

    for name, source in formatter_tests:
        with tempfile.NamedTemporaryFile("w", suffix=".cn", dir=ROOT / "tests", delete=False, encoding="utf-8") as temp:
            temp.write(source)
            temp_path = Path(temp.name)
        try:
            result = subprocess.run(
                [str(CNEXT_EXE), "fmt", str(temp_path)],
                cwd=ROOT, capture_output=True, text=True, timeout=10
            )
            # Read the formatted output
            with open(temp_path, "r", encoding="utf-8") as f:
                formatted = f.read()
            success = result.returncode == 0 and len(formatted) > 0
            status = "PASS" if success else "FAIL"
            if not success:
                all_passed = False
            print(f"[{status}] formatter/{name}: {'OK' if success else result.stderr.strip()[:100]}")
        finally:
            temp_path.unlink(missing_ok=True)

    # --- Linter tests ---
    print("\n--- Linter Tests ---")
    linter_tests = [
        ("lint_clean", "main {\nvar x = 1\nprintin(x)\n}\n", True),
        ("lint_unused_var", "main {\nvar x = 1\nprintin(1)\n}\n", True),  # should complete without crash
        ("lint_empty", "", True),
    ]

    for name, source, should_succeed in linter_tests:
        with tempfile.NamedTemporaryFile("w", suffix=".cn", dir=ROOT / "tests", delete=False, encoding="utf-8") as temp:
            temp.write(source)
            temp_path = Path(temp.name)
        try:
            result = subprocess.run(
                [str(CNEXT_EXE), "lint", str(temp_path)],
                cwd=ROOT, capture_output=True, text=True, timeout=10
            )
            success = (result.returncode == 0) == should_succeed
            status = "PASS" if success else "FAIL"
            if not success:
                all_passed = False
            print(f"[{status}] linter/{name}: {'OK' if success else result.stderr.strip()[:100]}")
        finally:
            temp_path.unlink(missing_ok=True)

    # --- Memory safety / stress tests ---
    print("\n--- Memory Safety Tests ---")
    stress_source = "main {\n    var big = \"\"\n    for var i = 0; i < 100; i++ {\n        big = big + \"x\"\n    }\n    printin(len(big))\n    printin(\"stress OK\")\n}\n"
    stress_path = ROOT / "tests" / "_stress_test.cn"
    with open(stress_path, "w", encoding="utf-8") as f:
        f.write(stress_source)
    try:
        result = subprocess.run(
            [str(CNEXT_EXE), "build", str(stress_path)],
            cwd=ROOT, capture_output=True, text=True, timeout=10
        )
        if result.returncode == 0 and OUT_EXE.exists():
            run_result = subprocess.run([str(OUT_EXE)], capture_output=True, text=True, timeout=10)
            success = run_result.returncode == 0 and "stress OK" in run_result.stdout
        else:
            success = False
        status = "PASS" if success else "FAIL"
        if not success:
            all_passed = False
        print(f"[{status}] memory/stress_basic: {'OK' if success else 'FAIL'}")
    finally:
        stress_path.unlink(missing_ok=True)

    # Deep recursion test
    recursion_source = """func fib(int n): int {
    if n <= 1 { return n }
    return fib(n - 1) + fib(n - 2)
}
main {
    printin(fib(20))
    printin("recursion OK")
}
"""
    with tempfile.NamedTemporaryFile("w", suffix=".cn", dir=ROOT / "tests", delete=False, encoding="utf-8") as temp:
        temp.write(recursion_source)
        temp_path = Path(temp.name)
    try:
        result = subprocess.run(
            [str(CNEXT_EXE), "build", str(temp_path)],
            cwd=ROOT, capture_output=True, text=True, timeout=10
        )
        if result.returncode == 0 and OUT_EXE.exists():
            run_result = subprocess.run([str(OUT_EXE)], capture_output=True, text=True, timeout=10)
            success = run_result.returncode == 0 and "recursion OK" in run_result.stdout
        else:
            success = False
        status = "PASS" if success else "FAIL"
        if not success:
            all_passed = False
        print(f"[{status}] memory/recursion: {'OK' if success else 'FAIL'}")
    finally:
        temp_path.unlink(missing_ok=True)

    # Concurrent allocator stress: 8 threads hammer track/untrack + arena + pool
    print("\n--- Thread-Safe Memory Stress ---")
    thread_stress_source = r"""
#include "runtime.h"
#ifdef _WIN32
#include <windows.h>
static HANDLE threads[8];
static DWORD WINAPI worker(LPVOID arg) {
#else
#include <pthread.h>
static pthread_t threads[8];
static void* worker(void* arg) {
#endif
    long id = (long)(intptr_t)arg;
    if (_cnext_try_depth || _cnext_defer_count || _cnext_error_message.data) exit(4);
    _cnext_push_try();
    if (setjmp(_cnext_try_stack[0].buf) == 0) cnext_throw_cstr("worker");
    if (_cnext_try_depth || _cnext_error_message.length != 6) exit(4);
    for (int i = 0; i < 20000; i++) {
        char* buf = (char*)malloc(64 + (i % 256));
        _cnext_track(buf);
        CnextString s = cnext_to_string_int((int)(id * 1000000 + i));
        cnext_free(s);
        CnextArena* a = cnext_mem_arena_create();
        cnext_mem_arena_alloc(a, (size_t)(i % 512) + 8);
        cnext_mem_arena_free(a);
        if (cnext_mem_arena_usage(a) != 0) exit(3);
        cnext_mem_arena_destroy(a);
        void* p = ARENA_ALLOC(((size_t)i % 64) + 4);
        if (p == NULL) exit(3);
        void* q = POOL_ALLOC(((size_t)i % 32) + 4);
        if (q == NULL) exit(3);
        _cnext_untrack(buf);
        free(buf);
    }
    return 0;
}
int main(void) {
    _cnext_push_try();
    _cnext_error_message = (CnextString){"main", 4};
    for (long i = 0; i < 8; i++) {
#ifdef _WIN32
        threads[i] = CreateThread(NULL, 0, worker, (LPVOID)i, 0, NULL);
#else
        pthread_create(&threads[i], NULL, worker, (void*)(intptr_t)i);
#endif
    }
    for (int i = 0; i < 8; i++) {
#ifdef _WIN32
        WaitForSingleObject(threads[i], INFINITE);
#else
        pthread_join(threads[i], NULL);
#endif
    }
    if (_cnext_try_depth != 1 || _cnext_error_message.length != 4) return 4;
    _cnext_pop_try();
    printf("THREAD STRESS OK\n");
    return 0;
}
"""
    c_path = ROOT / "tests" / "_thread_stress.c"
    out_path = ROOT / "tests" / ("_thread_stress.exe" if os.name == "nt" else "_thread_stress")
    c_path.write_text(thread_stress_source, encoding="utf-8")
    try:
        compile_cmd = ["gcc", "-std=gnu11", "-O2", "-w",
                       "-iquote", str(ROOT / "include"),
                       str(c_path), "-o", str(out_path)]
        if os.name == "nt":
            compile_cmd += ["-lwinhttp", "-lws2_32"]
        else:
            compile_cmd += ["-lpthread"]
        compile_result = subprocess.run(compile_cmd, capture_output=True, text=True, timeout=60,
                                        cwd=ROOT)
        if compile_result.returncode != 0 or not out_path.exists():
            success = False
            detail = compile_result.stderr.strip()[:100] or "compile failed"
        else:
            run_result = subprocess.run([str(out_path)], capture_output=True, text=True, timeout=60)
            success = run_result.returncode == 0 and "THREAD STRESS OK" in run_result.stdout
            detail = "OK" if success else run_result.stdout.strip()[:100]
        status = "PASS" if success else "FAIL"
        if not success:
            all_passed = False
        print(f"[{status}] memory/thread_stress: {detail}")
    except (FileNotFoundError, OSError):
        print("[SKIP] memory/thread_stress: gcc not available")
    finally:
        c_path.unlink(missing_ok=True)
        out_path.unlink(missing_ok=True)

    scratch_file.unlink(missing_ok=True)

    return 0 if all_passed else 1

if __name__ == "__main__":
    sys.exit(main())
