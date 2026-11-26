"""Check language contracts against either native or self-hosted compilers."""
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CNEXT = Path(os.environ.get("CNEXT_EXE", ROOT / ("cnext.exe" if os.name == "nt" else "cnext"))).resolve()


class LanguageContractTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="language-contract-", dir=ROOT / "build")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)

    def compile(self, source, run=False):
        path = self.work / "main.cn"
        path.write_text(source, encoding="utf-8")
        return subprocess.run([str(CNEXT), "run" if run else "build", str(path),
                               "-o", str(self.work / ("program.exe" if os.name == "nt" else "program"))],
                              cwd=self.work, capture_output=True, text=True, timeout=60)

    def test_unrelated_records_are_rejected_at_each_boundary(self):
        prefix = "class A { int x }\nclass B { int x }\n"
        cases = {
            "initializer": "main { A a = new B() }",
            "assignment": "main { A a = new A() a = new B() }",
            "argument": "func take(A a) {} main { take(new B()) }",
            "named argument": "func take(A a) {} main { take(a = new B()) }",
            "return": "func make(): A { return new B() } main {}",
            "field": "class Holder { A value } main { var h = new Holder() h.value = new B() }",
            "method": "class Holder { func take(A a) {} } main { var h = new Holder() h.take(new B()) }",
            "function result": "func make(): B { return new B() } main { A a = make() }",
            "inferred variable": "main { var b = new B() A a = b }",
            "scalar argument": "func take(A a) {} main { take(42) }",
            "constructor": "class Holder { func new(A a) {} } main { var h = new Holder(new B()) }",
            "alias": "type Alias = A main { Alias a = new B() }",
        }
        for name, body in cases.items():
            with self.subTest(name=name):
                result = self.compile(prefix + body)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("Type mismatch", result.stdout + result.stderr)

    def test_same_type_inheritance_and_null_references_remain_valid(self):
        result = self.compile('''
class A { int x }
class B extends A { }
func take(A a): int { return a.x }
func make(): A { return new B() }
main {
    A a = new B()
    a = new A()
    A? empty = null
    printin(take(a))
    printin(take(make()))
    str? text = null
    printin(text ?? "fallback")
}
''', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "0\n0\nfallback\n")

    def test_unsupported_nullable_scalars_are_explicit_errors(self):
        for declaration in ("int? n = null", "int? n = 42", "bool? n = false", "int[]? n = {1}"):
            with self.subTest(declaration=declaration):
                result = self.compile("main { " + declaration + " }")
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("Nullable scalar and array types are not supported", result.stdout + result.stderr)

    def test_nullable_aliases_and_value_structs_are_rejected(self):
        for source in (
            "type Number = int main { Number? n = 42 }",
            "type Number = int func take(Number? n) {} main {}",
            "type Number = int func make(): Number? { return 42 } main {}",
            "struct Value { int n } main { Value? v }",
            "type Texts = str[] main { Texts? names }",
        ):
            with self.subTest(source=source):
                result = self.compile(source)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("Nullable type requires", result.stdout + result.stderr)

    def test_null_is_rejected_for_scalar_and_struct_destinations(self):
        for source in (
            'main { int n = null }',
            'main { int n = 0 n = null }',
            'func take(int n) {} main { take(null) }',
            'func take(int n) {} main { take(n = null) }',
            'func make(): int { return null } main {}',
            'class Box { int n } main { var box = new Box() box.n = null }',
            'struct Point { int x } main { Point p = null }',
            'type Number = int main { Number n = null }',
            'type Numbers = int[] main { Numbers n = null }',
        ):
            with self.subTest(source=source):
                result = self.compile(source)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("null requires a reference type", result.stdout + result.stderr)
                self.assertNotIn("[E202]", result.stdout + result.stderr)

    def test_global_digests_match_known_vectors_and_module(self):
        # Exercise both padding branches and multiple complete message blocks.
        messages = ["", "hello"] + ["a" * n for n in (1, 55, 56, 63, 64, 65, 119, 120, 127, 128, 1024)]
        statements = []
        expected = []
        for text in messages:
            for name in ("md5", "sha1", "sha256"):
                statements.append(f'printin({name}("{text}"))')
                expected.append(getattr(hashlib, name)(text.encode()).hexdigest())
            statements.append(f'printin(sha256("{text}") == crypto.sha256("{text}"))')
            expected.append("true")
        result = self.compile("import crypto\nmain {\n" + "\n".join(statements) + "\n}", run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout.splitlines(), expected)

    def test_array_shapes_and_invariant_elements_are_checked(self):
        cases = (
            'main { int n = {1} }',
            'main { int[] n = 1 }',
            'main { int[] a = {1} int n = a }',
            'main { int[] a = {1} a = 3 }',
            'main { int[] a = {1} long[] b = a }',
            'func take(int[] a) {} main { take(1) }',
            'func take(int a) {} main { int[] a = {1} take(a) }',
            'func make(): int[] { return 42 } main {}',
            'func make(): int { return {1} } main {}',
            'main { int[] a = {"bad"} }',
            'class A { int x } class B extends A {} main { B[] b = {new B()} A[] a = b }',
        )
        for source in cases:
            with self.subTest(source=source):
                result = self.compile(source)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertNotIn("[E202]", result.stdout + result.stderr)

    def test_alias_cycles_are_diagnosed_without_crashing(self):
        for declarations in (
            'type A = A',
            'type A = B type B = A',
            'type A = B type B = C type C = B',
        ):
            with self.subTest(declarations=declarations):
                result = self.compile(declarations + ' main {}')
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("Cyclic or overly deep type alias", result.stdout + result.stderr)
        result = self.compile('type Number = int type Count = Number main { Count n = 42 printin(n) }', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, '42\n')

    def test_grouped_binary_expressions_preserve_the_ast(self):
        result = self.compile('''
main {
    bool yes = true
    bool no = false
    int a = 20
    int b = 5
    int c = 2
    printin(a - (b - c))
    printin(a / (b * c))
    bool grouped = yes != (no != false)
    printin(grouped)
}
''', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "17\n2\ntrue\n")

    def test_conditional_branches_require_compatible_types(self):
        for source in (
            'main { var n = true ? 1 : "text" }',
            'main { var n = true ? false : 1 }',
            'main { int[] a = {1} var n = true ? a : 1 }',
            'main { int[] a = {1} long[] b = {2} var n = true ? a : b }',
            'class A { int x } class B { int x } main { var n = true ? new A() : new B() }',
            'main { long a = 3 double b = 2.0 var n = true ? a : b }',
            'main { var n = true ? 1 : null }',
            'main { var n = when { true => 1, else => "text" } }',
            'main { CStr p = to_cstr("text") var n = true ? p : "text" }',
        ):
            with self.subTest(source=source):
                result = self.compile(source)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("Conditional branches have incompatible types", result.stdout + result.stderr)
                self.assertNotIn("[E202]", result.stdout + result.stderr)

    def test_when_requires_one_final_else(self):
        for source, message in (
            ('main { var n = when { true => 1 } }', 'requires an else arm'),
            ('main { var n = when { else => 1, true => 2 } }', 'unique and last'),
            ('main { var n = when { else => 1, else => 2 } }', 'unique and last'),
        ):
            result = self.compile(source)
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn(message, result.stdout + result.stderr)

    def test_conditional_result_metadata_and_numeric_precision(self):
        result = self.compile('''
class A { int x }
class B extends A { }
func choose(bool flag, int[] a, int[] b): int[] { return flag ? a : b }
main {
    bool flag = false
    int[] left = {1}
    int[] right = {2}
    var selected = flag ? left : right
    printin(selected[0])
    var selected_when = when { flag => left, !flag => right, else => left }
    printin(selected_when[0])
    var picked = choose(flag, left, right)
    printin(picked[0])
    var literal = flag ? left : {4, 5}
    printin(literal[1])
    var a = new A()
    a.x = 11
    var b = new B()
    b.x = 22
    var object = flag ? a : b
    printin(object.x)
    var reversed = flag ? b : a
    printin(reversed.x)
    var object_when = when { flag => b, !flag => a, else => b }
    printin(object_when.x)
    var text = flag ? null : "ok"
    printin(text ?? "fallback")
    var absent = flag ? "ok" : null
    printin(absent ?? "fallback")
    var text_when = when { flag => null, !flag => "active", else => null }
    printin(text_when ?? "fallback")
    CStr raw = null
    var foreign = when { flag => raw, else => to_cstr("foreign") }
    printin(from_cstr(foreign))
    cstr_free(foreign)
    int exact = 16777217
    float fraction = 0.5
    var number = !flag ? exact : fraction
    bool precise = number > 16777216.0
    printin(precise)
    var other = when { !flag => exact, flag => fraction, else => 0 }
    bool when_precise = other > 16777216.0
    printin(when_precise)
    printin(10 + (flag ? 1 : 2))
}
''', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "2\n2\n2\n5\n22\n11\n11\nok\nfallback\nactive\nforeign\ntrue\ntrue\n12\n")

    def test_arrays_return_from_functions_and_pass_as_literals(self):
        result = self.compile('''
class Node { int value func new(int n) { self.value = n } }
func numbers(): int[] { return {10, 20} }
func nodes(): Node[] { return {new Node(9)} }
func total(int[] items): int { return items[0] + items[1] }
main {
    var values = numbers()
    push(values, 30)
    printin(values.length)
    printin(total(values))
    printin(total({3, 4}))
    printin(total(items = {5, 6}))
    var objects = nodes()
    printin(objects[0].value)
    printin(nodes()[0].value)
    printin(numbers()[1])
    var words = str_split("a,b", ",")
    printin(words.length)
}
''', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "3\n30\n7\n11\n9\n9\n20\n2\n")

    def test_generic_array_results_fail_explicitly_until_specialized(self):
        result = self.compile('func identity<T>(T[] items): T[] { return items } main {}')
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Generic array returns are not yet supported", result.stdout + result.stderr)

    def test_gc_hint_preserves_live_values_and_closure_environments(self):
        result = self.compile('''
class Box { str value }
main {
    var box = new Box()
    box.value = str_upper("live")
    int count = 40
    var next = () => { count = count + 1 return count }
    gc()
    printin(box.value)
    printin(next())
    gc()
    printin(next())
}
''', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "LIVE\n41\n42\n")


if __name__ == "__main__":
    unittest.main()
