"""Exercise the public Cnext CLI against real C sources, objects, and archives."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;]*m")
ROOT = Path(__file__).resolve().parents[1]
CNEXT = Path(os.environ.get("CNEXT_EXE", ROOT / ("cnext.exe" if os.name == "nt" else "cnext"))).resolve()
EXE_SUFFIX = ".exe" if os.name == "nt" else ""


class NativeLinkingTests(unittest.TestCase):
    def setUp(self):
        # A private working directory also isolates the compiler's temp_out.c.
        (ROOT / "build").mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="native-link-", dir=ROOT / "build")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.source = self.write("main.cn", '''
extern "C" func native_add(int a, int b): int
main { printin(native_add(19, 23)) }
''')
        self.output = self.work / ("program" + EXE_SUFFIX)

    def write(self, name, text):
        path = self.work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return path

    def command(self, *args):
        return subprocess.run([str(arg) for arg in args], cwd=self.work,
                              capture_output=True, text=True, timeout=60)

    def cnext(self, command, *options):
        return self.command(CNEXT, command, self.source, "-o", self.output, *options)

    def assert_ok(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_c_sources_and_include_directories_with_spaces(self):
        header = self.write("headers with spaces/offset.h", "#define OFFSET 1\n")
        first = self.write("sources with spaces & symbols/add.c", '''
#include <offset.h>
int native_adjust(int value);
int native_add(int a, int b) { return native_adjust(a + b) - OFFSET; }
''')
        second = self.write("sources with spaces & symbols/adjust.c",
                            "int native_adjust(int value) { return value + 1; }\n")
        self.output = self.work / ("output with spaces" + EXE_SUFFIX)
        result = self.cnext("run", "--c-source", first, "--c-source", second,
                            "--include-dir", str(header.parent) + os.sep)
        self.assert_ok(result)
        self.assertEqual(result.stdout.strip(), "42")

    def test_double_precision_c_boundary(self):
        self.source = self.write("precision.cn", '''
extern "C" func native_half(double value): double
main {
    double result = native_half(2.0000000002)
    if result > 1.0 && result < 1.000000001 {
        printin("precision preserved")
    } else {
        printin("precision lost")
    }
}
''')
        native = self.write("precision.c", "double native_half(double x) { return x / 2.0; }\n")
        result = self.cnext("run", "--c-source", native)
        self.assert_ok(result)
        self.assertEqual(result.stdout.strip(), "precision preserved")

    def test_object_and_static_library_order(self):
        if not shutil.which("ar"):
            self.skipTest("ar is needed to create the test's static library")
        helper = self.write("helper.c", "int native_adjust(int x) { return x + 1; }\n")
        bridge = self.write("bridge.c", '''
int native_adjust(int value);
int native_add(int a, int b) { return native_adjust(a + b) - 1; }
''')
        helper_object = self.work / "helper.o"
        bridge_object = self.work / "bridge.o"
        self.assert_ok(self.command("gcc", "-c", helper, "-o", helper_object))
        self.assert_ok(self.command("gcc", "-c", bridge, "-o", bridge_object))
        library_dir = self.work / "libraries with spaces"
        library_dir.mkdir()
        archive = library_dir / "libnative_fixture.a"
        self.assert_ok(self.command("ar", "rcs", archive, helper_object))
        # A library supplied before its consumer on the CLI must still link.
        self.assert_ok(self.cnext("build", "--link-lib", "native_fixture",
                                  "--library-dir", library_dir, "--object", bridge_object))
        run = self.command(self.output)
        self.assert_ok(run)
        self.assertEqual(run.stdout.strip(), "42")
        # Archives may also be linked directly without a library search path.
        self.assert_ok(self.cnext("build", "--object", bridge_object, "--object", archive))
        self.assertEqual(self.command(self.output).stdout.strip(), "42")

    def test_invalid_native_arguments(self):
        options = ("--c-source", "--object", "--include-dir", "--library-dir", "--link-lib")
        for option in options:
            for values in ((), ("",), ("--debug",)):
                with self.subTest(option=option, values=values):
                    result = self.cnext("build", option, *values)
                    self.assertEqual(result.returncode, 64, result.stdout + result.stderr)
                    self.assertIn("Expected a value after " + option, result.stderr)

    def test_borrowed_c_strings_are_copied_and_not_freed(self):
        self.source = self.write("strings.cn", '''
extern "C" {
    func native_text(): CStr
    func native_change()
    func native_null(): CStr
    func native_binary(): CStr
}
main {
    var borrowed = native_text()
    str saved = from_cstr(borrowed)
    native_change()
    printin(saved)
    printin(from_cstr(borrowed))
    try { cstr_free(borrowed) }
    catch (str error) { printin("borrowed release rejected") }
    try { printin(from_cstr(native_null())) }
    catch (str error) { printin("null pointer rejected") }
    printin(from_cstr(native_binary()))
}
''')
        native = self.write("strings.c", r'''
#include <stddef.h>
static char text[] = "hello";
const char* native_text(void) { return text; }
void native_change(void) { text[0] = 'H'; }
const char* native_null(void) { return NULL; }
const char* native_binary(void) { return "a\0b"; }
''')
        result = self.cnext("run", "--c-source", native)
        self.assert_ok(result)
        self.assertEqual(result.stdout.splitlines(), [
            "hello", "Hello", "borrowed release rejected", "null pointer rejected", "a"])

    def test_c_string_type_errors_are_reported_before_codegen(self):
        cases = {
            "implicit_argument": 'extern "C" func puts(CStr text): int\nmain { puts("hello") }',
            "implicit_initializer": 'main { CStr text = "hello" }',
            "implicit_assignment": 'main { CStr text = null\ntext = "hello" }',
            "implicit_return": 'func text(): CStr { return "hello" }\nmain {}',
            "implicit_str_return": 'func text(): str { return to_cstr("hello") }\nmain {}',
            "implicit_str_initializer": 'main { str text = to_cstr("hello") }',
            "implicit_cast": 'main { var text = "hello" as CStr }',
            "wrong_named_type": 'class Other {}\nmain { CStr text = new Other() }',
            "wrong_builtin_type": 'main { var text = to_cstr(42) }',
            "wrong_array_type": 'main { str[] values = {"a"}\nvar text = to_cstr(values) }',
            "wrong_from_type": 'main { var text = from_cstr("hello") }',
            "wrong_free_type": 'main { cstr_free("hello") }',
            "implicit_default": 'func text(CStr value = "hello") {}\nmain {}',
            "implicit_fallback": 'main { CStr value = null\nvar text = value ?? "hello" }',
            "implicit_ternary": 'main { var text = true ? to_cstr("a") : "b" }',
            "implicit_concat": 'main { var text = to_cstr("a") + "b" }',
            "missing_argument": 'main { var text = to_cstr() }',
            "extra_argument": 'main { var text = to_cstr("a", "b") }',
        }
        for name, source in cases.items():
            with self.subTest(name=name):
                self.source = self.write(name + ".cn", source)
                result = self.cnext("build")
                self.assertEqual(result.returncode, 65, result.stdout + result.stderr)
                self.assertIn("Semantic analysis failed", result.stderr)
                self.assertNotIn("C compilation failed", result.stderr)

    def test_escaping_reference_closure_is_rejected(self):
        self.source = ROOT / "tests" / "bootstrap" / "invalid_closure_escape.cn"
        result = self.cnext("build")
        self.assertEqual(result.returncode, 65, result.stdout + result.stderr)
        self.assertIn("reference captures cannot escape", result.stderr)
        self.assertNotIn("C compilation failed", result.stderr)

    def test_mutable_capture_cells(self):
        self.assert_closure_fixture("test_capture_cells.cn")

    def test_nested_closure_factories(self):
        self.assert_closure_fixture("test_nested_closure.cn")

    def test_push_string_literals(self):
        from test_bootstrap_codegen import EXPECTED

        self.source = ROOT / "tests" / "bootstrap" / "test_push_strings.cn"
        result = self.cnext("run")
        self.assert_ok(result)
        self.assertEqual(result.stdout, EXPECTED["test_push_strings.cn"])

    def assert_closure_fixture(self, filename):
        from test_bootstrap_codegen import EXPECTED

        source = ROOT / "tests" / "bootstrap" / filename
        generated = self.work / (source.stem + ".c")
        self.assert_ok(self.command(CNEXT, "build", source, "--emit-c", generated))
        with generated.open("a", encoding="utf-8") as output:
            output.write("\nint bootstrap_tracked_allocations(void) {\n"
                         "    int count = 0;\n"
                         "    for (_AllocNode* p = _cnext_allocs; p; p = p->next) count++;\n"
                         "    return count;\n}\n")
        for optimization in ("-O0", "-O2"):
            with self.subTest(optimization=optimization):
                self.assert_ok(self.command("gcc", "-std=gnu11", optimization,
                                            "-iquote", ROOT / "include", generated,
                                            "-o", self.output))
                result = self.command(self.output)
                self.assert_ok(result)
                self.assertEqual(result.stdout, EXPECTED[filename])

    def test_native_argument_limit(self):
        result = self.cnext("build", *(["--object", "unused.o"] * 129))
        self.assertEqual(result.returncode, 64)
        self.assertIn("Too many native build arguments", result.stderr)

    def test_emit_c_rejects_link_options(self):
        result = self.cnext("build", "--emit-c", "generated.c", "--link-lib", "unused")
        self.assertEqual(result.returncode, 64)
        self.assertIn("Cannot combine --emit-c", result.stderr)
        self.assertFalse((self.work / "generated.c").exists())

    def test_emit_c_rejects_output_option(self):
        generated = self.work / "generated.c"
        result = self.command(CNEXT, "build", self.source, "--emit-c", generated,
                              "-o", self.output)
        self.assertEqual(result.returncode, 64)
        self.assertIn("Cannot combine --emit-c with -o", result.stderr)
        self.assertFalse(generated.exists())

    def test_main_return_uses_cleanup_path(self):
        source = self.write("early_return.cn", '''
main {
    defer printin("defer ran")
    return 7
}
''')
        generated = self.work / "early_return.c"
        self.assert_ok(self.command(CNEXT, "build", source, "--emit-c", generated))
        text = generated.read_text(encoding="utf-8")
        self.assertIn("_cnext_main_result = 7; goto _cnext_main_cleanup;", text)
        self.assertLess(text.index("_cnext_main_cleanup:"),
                        text.index("return _cnext_main_result;"))
        self.assertNotIn("return 7;", text)
        self.assert_ok(self.command("gcc", "-std=gnu11", "-iquote", ROOT / "include",
                                    generated, "-o", self.output))
        result = self.command(self.output)
        self.assertEqual(result.returncode, 7)
        self.assertEqual(result.stdout, "defer ran\n")

    def test_raw_string_newlines_are_escaped(self):
        source = self.work / "raw.cn"
        with source.open("w", encoding="utf-8", newline="\n") as output:
            output.write(r'''main {
    str raw = r"line one
line two C:\temp"
    printin(len(raw))
}
''')
        generated = self.work / "raw.c"
        self.assert_ok(self.command(CNEXT, "build", source, "--emit-c", generated))
        self.assert_ok(self.command("gcc", "-std=gnu11", "-iquote", ROOT / "include",
                                    generated, "-o", self.output))
        result = self.command(self.output)
        self.assertEqual(result.stdout.strip(), "25")

    def test_print_macro_does_not_shadow_user_identifier(self):
        source = self.write("shadow.cn", '''
main {
    int _x = 42
    printin(_x)
}
''')
        generated = self.work / "shadow.c"
        self.assert_ok(self.command(CNEXT, "build", source, "--emit-c", generated))
        self.assert_ok(self.command("gcc", "-std=gnu11", "-Werror=shadow",
                                    "-iquote", ROOT / "include", generated,
                                    "-o", self.output))
        result = self.command(self.output)
        self.assertEqual(result.stdout.strip(), "42")

    def test_link_failure_is_reported(self):
        result = self.cnext("build", "--link-lib", "cnext_library_that_does_not_exist")
        self.assertNotEqual(result.returncode, 0)
        stderr = ANSI_ESCAPE.sub("", result.stderr)
        self.assertIn("error [E202]", stderr)
        self.assertIn("cannot find -lcnext_library_that_does_not_exist", stderr)
        self.assertIn("= rule: CN-E202", stderr)
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
