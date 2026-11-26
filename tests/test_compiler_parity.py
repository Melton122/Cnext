"""Compare native and self-hosted behavior against explicit language expectations.

Run after building both compilers, for example:
  CNEXT_SELFHOST_EXE=build/stabilization/stage2.exe python tests/test_compiler_parity.py

This checks representative contracts, not full feature parity or C-source identity.
Every compiler invocation gets a separate directory so generated intermediates
cannot interfere with another test or compiler stage.
"""
import argparse
from dataclasses import dataclass
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SUFFIX = ".exe" if os.name == "nt" else ""
NATIVE = Path(os.environ.get("CNEXT_NATIVE_EXE", ROOT / ("cnext" + SUFFIX))).resolve()
SELFHOST = Path(os.environ.get(
    "CNEXT_SELFHOST_EXE", ROOT / "build" / "selfhost" / ("stage2_exe" + SUFFIX)
)).resolve()


@dataclass
class Outcome:
    build: subprocess.CompletedProcess
    execution: subprocess.CompletedProcess | None = None


class CompilerParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for label, compiler in (("native", NATIVE), ("self-hosted", SELFHOST)):
            if not compiler.is_file():
                raise RuntimeError(f"Missing {label} compiler: {compiler}")

    def compare(self, source, *, output="", exit_code=0, args=(), stdin="", c_source=None):
        outcomes = self.outcomes(source, args=args, stdin=stdin, c_source=c_source)
        for label, outcome in outcomes.items():
            with self.subTest(compiler=label):
                self.assertEqual(outcome.build.returncode, 0, self.describe(outcome.build))
                self.assertIsNotNone(outcome.execution)
                self.assertEqual(outcome.execution.returncode, exit_code, self.describe(outcome.execution))
                self.assertEqual(outcome.execution.stdout, output, self.describe(outcome.execution))
                self.assertEqual(outcome.execution.stderr, "", self.describe(outcome.execution))
        native, selfhost = outcomes.values()
        if native.execution is not None and selfhost.execution is not None:
            self.assertEqual(native.execution.returncode, selfhost.execution.returncode)
            self.assertEqual(native.execution.stdout, selfhost.execution.stdout)

    def reject(self, source, message):
        outcomes = self.outcomes(source)
        for label, outcome in outcomes.items():
            with self.subTest(compiler=label):
                diagnostic = outcome.build.stdout + outcome.build.stderr
                self.assertNotEqual(outcome.build.returncode, 0, diagnostic)
                self.assertIn(message, diagnostic)
                # A C backend failure is not proof that semantic rejection worked.
                self.assertNotIn("[E202]", diagnostic)
                self.assertNotIn("C compilation failed", diagnostic)
                self.assertIsNone(outcome.execution)
        self.assertEqual(outcomes["native"].build.returncode, outcomes["self-hosted"].build.returncode)

    @staticmethod
    def describe(result):
        return f"exit={result.returncode}\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}"

    def outcomes(self, source, *, args=(), stdin="", c_source=None):
        results = {}
        with tempfile.TemporaryDirectory(prefix="compiler-parity-", dir=ROOT / "build") as temp:
            for label, compiler in (("native", NATIVE), ("self-hosted", SELFHOST)):
                work = Path(temp) / label
                work.mkdir()
                path = work / "source with spaces.cn"
                path.write_text(source, encoding="utf-8")
                binary = work / ("program" + SUFFIX)
                command = [str(compiler), "build", str(path), "-o", str(binary), "--no-native-config"]
                if c_source is not None:
                    native = work / "native bridge.c"
                    native.write_text(c_source, encoding="utf-8")
                    command.extend(("--c-source", str(native)))
                env = dict(os.environ, CNEXT_INCLUDE=str(ROOT / "include"),
                           TMPDIR=str(work), TMP=str(work), TEMP=str(work))
                built = subprocess.run(command, cwd=work, env=env, capture_output=True,
                                       text=True, timeout=90)
                executed = None
                if built.returncode == 0:
                    self.assertTrue(binary.is_file(), self.describe(built))
                    executed = subprocess.run([str(binary), *args], cwd=work, env=env,
                                              input=stdin, capture_output=True, text=True, timeout=30)
                results[label] = Outcome(built, executed)
        return results

    def test_integer_widths_and_explicit_casts(self):
        self.compare('''
main {
    long top = 9223372036854775807
    printin(top - 1)
    int value = 42
    byte small = value as byte
    printin(small as int)
    printin(40 + 2)
}
''', output="9223372036854775806\n42\n42\n")

    def test_numeric_conditional_precision_and_grouping(self):
        self.compare('''
func f(bool flag): bool {
    int exact = 16777217
    float fraction = 0.5
    var number = flag ? exact : fraction
    return number > 16777216.0
}
main { printin(f(true)) printin(f(false)) printin(10 - (3 - 1)) }
''', output="true\nfalse\n8\n")

    def test_typed_array_results_and_string_arrays(self):
        self.compare('''
func make(): int[] { return {3, 4, 5} }
func first(int[] values): int { return values[0] }
main {
    int[] values = make()
    printin(first(values))
    printin(values[2])
    str[] words = str_split("one,two", ",")
    printin(words[1])
}
''', output="3\n5\ntwo\n")

    def test_string_split_growth_and_process_cleanup(self):
        statements, expected = [], []
        for count in (0, 8, 9, 16, 17, 65):
            text = ",".join(f"item{i}" for i in range(count))
            name = f"parts{count}"
            statements.append(f'str[] {name} = str_split("{text}", ",") printin(len({name}))')
            expected.append(str(count))
            if count:
                statements.append(f"printin({name}[{count - 1}])")
                expected.append(f"item{count - 1}")
        self.compare("main { " + "\n".join(statements) + " }", output="\n".join(expected) + "\n")

    def test_record_inheritance_and_field_initialization(self):
        self.compare('''
class Base { int value }
class Child extends Base { str name }
func value_of(Base object): int { return object.value }
main {
    var object = new Child()
    printin(value_of(object))
    object.value = 12
    object.name = "child"
    Base base = object
    printin(value_of(base))
    printin(object.name)
}
''', output="0\n12\nchild\n")

    def test_nullable_references_and_conditional_join(self):
        self.compare('''
class Box { int value }
func select(bool flag): str? { return flag ? null : "present" }
main {
    Box? absent = null
    bool empty = absent == null
    printin(empty)
    printin(select(true) ?? "fallback")
    printin(select(false) ?? "fallback")
}
''', output="true\nfallback\npresent\n")

    def test_c_strings_copy_and_release(self):
        self.compare('''
extern "C" func strcmp(CStr a, CStr b): int
main {
    CStr a = to_cstr("hello")
    CStr b = to_cstr("hello")
    printin(strcmp(a, b))
    var copied = from_cstr(a)
    cstr_free(a)
    cstr_free(b)
    printin(copied)
}
''', output="0\nhello\n")

    def test_linked_c_component(self):
        self.compare('''
extern "C" func component_add(int a, int b): int
main { printin(component_add(19, 23)) }
''', c_source="int component_add(int a, int b) { return a + b; }\n", output="42\n")

    def test_closure_copies_share_mutable_capture(self):
        self.compare('''
main {
    int value = 0
    var add = (int amount) => { value = value + amount return value }
    var copy = add
    printin(add(5))
    printin(copy(3))
    printin(value)
}
''', output="5\n8\n8\n")

    def test_deferred_cleanup_on_return_and_throw(self):
        self.compare('''
func f(bool fail): int {
    defer printin("cleanup")
    if fail { throw "error" }
    return 7
}
main {
    try { printin(f(true)) } catch (str error) { printin(error) }
    printin(f(false))
}
''', output="cleanup\nerror\ncleanup\n7\n")

    def test_branch_ownership_and_fresh_loop_values(self):
        self.compare('''
func choose(bool flag): str {
    var text = str_upper("value")
    if flag { return own text }
    return own text
}
main {
    printin(choose(true))
    printin(choose(false))
    int i = 0
    while i < 2 {
        var text = str_lower("LOOP")
        var moved = own text
        printin(moved)
        i = i + 1
    }
}
''', output="VALUE\nVALUE\nloop\nloop\n")

    def test_lexical_arena_cleanup(self):
        self.compare('''
class Box { int value }
func scoped(): int {
    mem_scope_begin()
    defer mem_scope_end()
    var box = new Box()
    box.value = 9
    return box.value
}
main { printin(scoped()) printin(mem_scope_usage()) }
''', output="9\n0\n")

    def test_program_arguments_input_and_nonzero_exit(self):
        argument = 'spaces & | ; "quotes"'
        self.compare('main { printin(sys_arg_at(1)) printin(input()) return 7 }',
                     output=argument + "\ninput text\n", exit_code=7, args=(argument,), stdin="input text\n")

    def test_concrete_type_and_array_shape_rejections(self):
        for source, message in (
            ('class A {} class B {} main { A value = new B() }', "Type mismatch"),
            ('func take(int[] values) {} main { take(42) }', "Array/scalar shape mismatch"),
            ('main { int[] values = {1} str[] words = values }', "Array element types must match exactly"),
        ):
            with self.subTest(source=source):
                self.reject(source, message)

    def test_narrowing_and_null_rejections(self):
        for source, message in (
            ('main { long big = 2147483648 int small = big }', "Type mismatch"),
            ('main { int value = null }', "null requires a reference type"),
            ('main { int? value = null }', "Nullable scalar and array types are not supported"),
        ):
            with self.subTest(source=source):
                self.reject(source, message)

    def test_ownership_rejections(self):
        for source, message in (
            ('main { var a = str_upper("x") var b = own a printin(a) }', "Use of moved binding"),
            ('main { var a = str_upper("x") var alias = a var b = own a }', "aliased value"),
            ('func f(bool flag) { var a = str_upper("x") if flag { var b = own a } printin(a) } main {}', "Use of moved binding"),
            ('main { var a = str_upper("x") while false { var b = own a } }', "carried between loop iterations"),
        ):
            with self.subTest(source=source):
                self.reject(source, message)

    def test_switch_and_match_ownership_joins(self):
        self.compare('''
func take(str text): int { printin(text) return 0 }
enum Kind { FIRST, SECOND }
func pick(int choice): str {
    var text = str_upper("pick")
    switch choice {
        case 1: return own text
        default: printin(text)
    }
    return own text
}
main {
    printin(pick(1))
    printin(pick(2))
    var text = str_upper("arm")
    Kind kind = FIRST
    match kind {
        FIRST => take(own text)
        _ => take(own text)
    }
}
''', output="PICK\nPICK\nPICK\nARM\n")

    def test_switch_move_on_any_arm_blocks_later_reads(self):
        self.reject('func take(str text) { printin(text) } main { var text = str_upper("x") '
                    'int n = 1 switch n { case 1: take(own text) default: printin(text) } printin(text) }',
                    "Use of moved binding")

    def test_match_void_arm_expressions_are_statements(self):
        self.compare('''
enum Kind { FIRST, SECOND }
func show(str text) { printin(text) }
func describe(int value): str {
    match value {
        0 => "zero"
        _ => "many"
    }
}
func score(): int {
    Kind kind = FIRST
    match kind {
        FIRST => show("arm")
        _ => show("other")
    }
    return 5
}
main {
    Kind kind = SECOND
    match kind {
        FIRST => show("main-first")
        _ => show("main-other")
    }
    printin(score())
    printin(describe(0))
}
''', output="main-other\narm\n5\nzero\n")

    def test_arena_escape_and_expired_alias_rejections(self):
        for source, message in (
            ('class Box {} func f(): Box { mem_scope_begin() defer mem_scope_end() return new Box() } main {}', "returned or thrown"),
            ('class Box { int value } main { mem_scope_begin() var a = new Box() var alias = a mem_scope_end() printin(alias.value) }', "memory scope ended"),
        ):
            with self.subTest(source=source):
                self.reject(source, message)

    def test_invalid_expression_is_rejected_before_codegen(self):
        self.reject('main { var value = }', "[E001]")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path)
    parser.add_argument("--selfhost", type=Path)
    options, remaining = parser.parse_known_args()
    if options.native:
        NATIVE = options.native.resolve()
    if options.selfhost:
        SELFHOST = options.selfhost.resolve()
    unittest.main(argv=[__file__, *remaining])
