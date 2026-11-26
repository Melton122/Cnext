"""Ownership and scoped allocation contracts, usable against every compiler stage."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CNEXT = Path(os.environ.get("CNEXT_EXE", ROOT / ("cnext.exe" if os.name == "nt" else "cnext"))).resolve()


class LifetimeTests(unittest.TestCase):
    def compile(self, source, run=False):
        with tempfile.TemporaryDirectory(prefix="lifetimes-", dir=ROOT / "build") as temp:
            work = Path(temp)
            path = work / "main.cn"
            path.write_text(source, encoding="utf-8")
            return subprocess.run([str(CNEXT), "run" if run else "build", str(path),
                                   "-o", str(work / "program.exe")], cwd=work,
                                  capture_output=True, text=True, timeout=60)

    def reject(self, source, message):
        result = self.compile(source)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(message, result.stdout + result.stderr)
        self.assertNotIn("[E202]", result.stdout + result.stderr)

    def test_moves_preserve_values_and_allow_unconditional_reinitialization(self):
        result = self.compile('''
class Box { int value }
func make(): Box { var x = new Box() x.value = 7 return own x }
func take(str x) { printin(x) }
main {
    var a = str_upper("hello")
    var b = own a
    printin(b)
    a = str_lower("AGAIN")
    take(own a)
    var box = own make()
    printin(box.value)
    int[] xs = {3, 4}
    int[] ys = own xs
    printin(ys[1])
}
''', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "HELLO\nagain\n7\n4\n")

    def test_use_after_move_at_each_read_boundary(self):
        for use in ('printin(a)', 'printin("{a}")', 'var c = a', 'var c = own a',
                    'if len(a) > 0 { printin("bad") }', 'assert(len(a) > 0)'):
            with self.subTest(use=use):
                self.reject('main { var a = str_upper("x") var b = own a ' + use + ' }', "Use of moved binding")

    def test_aliases_parameters_literals_and_escaped_values_cannot_move(self):
        for source, message in (
            ('main { var a = str_upper("x") var b = a var c = own a }', "aliased value"),
            ('func f(str a) { var b = own a } main {}', "local resource binding"),
            ('main { str a = "x" var b = own a }', "borrowed, captured, or escaped"),
            ('func f(str a) {} main { var a = str_upper("x") f(a) var b = own a }', "borrowed, captured, or escaped"),
            ('main { var a = str_upper("x") defer printin("{a}") var b = own a }', "borrowed, captured, or escaped"),
            ('main { int a = 1 int b = own a }', "local resource binding"),
            ('main { var a = str_upper("x") var b = own (true ? a : a) }', "local resource binding"),
            ('main { var a = str_upper("x") var b = own a[0:1] }', "local resource binding"),
        ):
            with self.subTest(source=source):
                self.reject(source, message)

    def test_unsupported_expression_and_discarded_moves_are_rejected(self):
        for use in ('own a', 'defer printin(own a)', 'var b = true ? own a : a'):
            with self.subTest(use=use):
                self.reject('main { var a = str_upper("x") ' + use + ' }',
                            "Moves in callbacks" if use.startswith("defer") else "unconditional transfer position")
        self.reject('func f(str a): bool { return true } main { var a = str_upper("x") '
                    'bool b = false && f(own a) }', "unconditional transfer position")

    def test_conditional_reinitialization_does_not_revive_moved_binding(self):
        self.reject('main { var a = str_upper("x") var b = own a if false { a = str_upper("y") } printin(a) }', "Use of moved binding")

    def test_call_cannot_borrow_and_move_same_value(self):
        self.reject('func f(str a, str b) {} main { var a = str_upper("x") f(a, own a) }', "move and borrow")

    def test_scoped_values_remain_usable_before_deferred_cleanup(self):
        result = self.compile('''
class Box { int value }
func scoped(): int {
    mem_scope_begin()
    defer mem_scope_end()
    var box = new Box()
    box.value = 9
    return box.value
}
main { printin(scoped()) printin(mem_scope_usage()) }
''', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "9\n0\n")

    def test_scope_end_invalidates_all_local_aliases_and_interpolation(self):
        for use in ('printin(a.value)', 'printin(b.value)', 'printin("{a.value}")'):
            with self.subTest(use=use):
                self.reject('class Box { int value } main { mem_scope_begin() var a = new Box() var b = a mem_scope_end() ' + use + ' }', "memory scope ended")

    def test_scoped_references_cannot_escape(self):
        prefix = 'class Box { int value } '
        for source, message in (
            ('func f(): Box { mem_scope_begin() defer mem_scope_end() var a = new Box() return a } main {}', "returned or thrown"),
            ('main { mem_scope_begin() defer mem_scope_end() var a = new Box() var b = own a }', "arena scope"),
            ('func take(Box a) {} main { mem_scope_begin() defer mem_scope_end() take(new Box()) }', "escape through calls"),
            ('main { var outer = new Box() mem_scope_begin() defer mem_scope_end() outer = new Box() }', "outer binding"),
            ('main { mem_scope_begin() defer mem_scope_end() var a = new Box() defer printin("{a.value}") }', "cannot be captured"),
        ):
            with self.subTest(source=source):
                self.reject(prefix + source, message)

    def test_scope_operations_require_balanced_cleanup(self):
        for body, message in (
            ('mem_scope_end()', "balanced"),
            ('mem_scope_begin()', "without cleanup"),
            ('mem_scope_begin() if true { return 0 } mem_scope_end()', "without cleanup"),
            ('mem_scope_begin() defer mem_scope_end() mem_scope_end()', "already scheduled"),
            ('var x = mem_scope_begin()', "standalone statement"),
            ('while true { mem_scope_begin() break mem_scope_end() }', "without cleanup"),
        ):
            with self.subTest(body=body):
                self.reject('main { ' + body + ' }', message)

    def test_branch_results_keep_shortest_scope(self):
        for choice in ('true ? a : b', 'true ? b : a',
                       'when { true => b, else => a }', 'when { true => a, else => b }'):
            with self.subTest(choice=choice):
                self.reject('class Box { int value } main { mem_scope_begin() var a = new Box() '
                            'mem_scope_begin() var b = new Box() var selected = ' + choice +
                            ' mem_scope_end() printin(selected.value) mem_scope_end() }', "memory scope ended")

    def test_scoped_receiver_cannot_escape_to_a_method(self):
        self.reject('class Box { func save() {} } main { mem_scope_begin() defer mem_scope_end() '
                    'var b = new Box() b.save() }', "escape through calls")

    def test_callbacks_cannot_capture_scoped_objects(self):
        self.reject('class Box { int value } main { mem_scope_begin() defer mem_scope_end() '
                    'var b = new Box() var f = () => b.value }', "cannot be captured")

    def test_fresh_value_can_move_after_inner_alias_expires(self):
        result = self.compile('main { var a = str_upper("ok") { var b = a printin(b) } var c = own a printin(c) }', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "OK\nOK\n")

    def test_foreign_calls_cannot_take_checked_ownership(self):
        self.reject('extern "C" { func foreign(CStr p) } main { CStr p = to_cstr("x") foreign(own p) }',
                    "Ownership transfer to foreign or unknown calls")

    def test_default_arguments_cannot_hide_transfers(self):
        self.reject('func f(str text = own str_upper("x")) {} main {}', "not supported in default arguments")

    def test_if_branches_move_independently_and_join(self):
        result = self.compile('''
func take(str text) { printin(text) }
func choose(bool flag): str {
    var text = str_upper("chosen")
    var result = str_upper("old")
    if flag { result = own text } else { result = own text }
    return own result
}
func branch(bool flag) {
    var text = str_upper("branch")
    if flag { take(own text) } else { printin(text) }
}
main { printin(choose(true)) printin(choose(false)) branch(true) branch(false) }
''', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "CHOSEN\nCHOSEN\nBRANCH\nBRANCH\n")

    def test_move_on_any_continuing_branch_invalidates_binding(self):
        for body in ('if flag { var moved = own text }',
                     'if flag {} else { var moved = own text }',
                     'if flag { var moved = own text } else { var moved = own text }',
                     'if flag { if other { var moved = own text } }',
                     'if flag { var moved = own text } else if other {}'):
            with self.subTest(body=body):
                self.reject('func f(bool flag, bool other) { var text = str_upper("x") ' + body +
                            ' printin(text) } main {}', "Use of moved binding")

    def test_returning_branches_do_not_poison_continuing_path(self):
        result = self.compile('''
func early(bool flag): str {
    var text = str_upper("early")
    if flag { return own text }
    return own text
}
func reversed(bool flag): str {
    var text = str_upper("reversed")
    if flag {} else { return own text }
    return own text
}
func nested(bool a, bool b): str {
    var text = str_upper("nested")
    if a { if b { return own text } else { return own text } }
    return own text
}
main {
    printin(early(true)) printin(early(false))
    printin(reversed(true)) printin(reversed(false))
    printin(nested(true, false)) printin(nested(false, true))
}
''', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "EARLY\nEARLY\nREVERSED\nREVERSED\nNESTED\nNESTED\n")

    def test_both_branches_can_reinitialize_moved_binding(self):
        result = self.compile('''
func f(bool flag) {
    var text = str_upper("old")
    var moved = own text
    if flag { text = str_lower("LEFT") } else { text = str_lower("RIGHT") }
    printin(text)
}
main { f(true) f(false) }
''', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "left\nright\n")

    def test_branch_aliases_still_prevent_moves(self):
        self.reject('func f(bool flag) { var text = str_upper("x") var saved = str_upper("old") '
                    'if flag { saved = text } var moved = own text } main {}', "borrowed, captured, or escaped")
        self.reject('func f(bool flag) { var text = str_upper("x") var saved = text '
                    'if flag { var moved = own text } } main {}', "aliased value")

    def test_fresh_iteration_local_values_can_move(self):
        result = self.compile('''
func take(str text) { printin(text) }
main {
    int i = 0
    while i < 3 {
        var text = str_upper("loop")
        if i == 1 { take(own text) } else { var moved = own text printin(moved) }
        i = i + 1
    }
    for int j = 0; j < 2; j = j + 1 {
        var text = str_lower("FOR")
        take(own text)
    }
}
''', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "LOOP\nLOOP\nLOOP\nfor\nfor\n")

    def test_loop_carried_values_cannot_move(self):
        for loop in ('while false { var moved = own text }',
                     'while true { var moved = own text break }',
                     'for int i = 0; i < 2; i = i + 1 { var moved = own text }',
                     'while true { if true { var moved = own text } break }'):
            with self.subTest(loop=loop):
                self.reject('main { var text = str_upper("x") ' + loop + ' }', "carried between loop iterations")
        self.reject('main { for var text = str_upper("x"); false; printin("step") '
                    '{ var moved = own text } }', "carried between loop iterations")

    def test_inner_loop_cannot_move_outer_iteration_binding(self):
        self.reject('main { while true { var text = str_upper("x") '
                    'while false { var moved = own text } break } }', "carried between loop iterations")

    def test_skipped_loop_does_not_prove_reinitialization(self):
        self.reject('main { var text = str_upper("x") var moved = own text '
                    'while false { text = str_upper("y") } printin(text) }', "Use of moved binding")

    def test_iteration_local_move_is_checked_before_continue(self):
        self.reject('main { int i = 0 while i < 2 { var text = str_upper("x") '
                    'if i == 0 { var moved = own text } printin(text) i = i + 1 } }', "Use of moved binding")

    def test_break_path_assignment_cannot_hide_escaped_values(self):
        self.reject('func retain(str text) {} func f(bool flag) { var saved = str_upper("old") '
                    'while flag { var text = str_upper("new") if flag { saved = text retain(text) break } '
                    'break } var moved = own saved } main {}', "borrowed, captured, or escaped")

    def test_continue_excludes_exited_path_from_local_join(self):
        result = self.compile('''
main {
    int i = 0
    while i < 3 {
        var text = str_upper("iteration")
        i = i + 1
        if i == 2 { var moved = own text printin(moved) continue }
        var moved = own text
        printin(moved)
    }
}
''', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "ITERATION\nITERATION\nITERATION\n")

    def test_throwing_branch_is_excluded_from_continuing_path(self):
        result = self.compile('''
func f(bool flag): str {
    var text = str_upper("value")
    if flag { throw own text }
    return own text
}
main {
    try { printin(f(true)) } catch (str error) { printin(error) }
    printin(f(false))
}
''', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "VALUE\nVALUE\n")

    def test_arrays_and_objects_keep_identity_across_branch_transfers(self):
        result = self.compile('''
class Box { int value }
func f(bool flag) {
    var box = new Box()
    box.value = 42
    var result = new Box()
    int[] values = {7, 8}
    int[] selected = {0}
    if flag { result = own box selected = own values }
    else { selected = own values result = own box }
    var final_box = own result
    int[] final_values = own selected
    printin(final_box.value)
    printin(final_values[1])
}
main { f(true) f(false) }
''', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "42\n8\n42\n8\n")

    def test_different_branch_provenance_remains_borrowed(self):
        self.reject('func f(bool flag) { var text = str_upper("old") '
                    'if flag { text = str_upper("a") } else { text = str_upper("b") } '
                    'var moved = own text } main {}', "borrowed, captured, or escaped")

    def test_switch_arms_move_independently(self):
        result = self.compile('''
func take(str text) { printin(text) }
main {
    var text = str_upper("case")
    int choice = 1
    switch choice {
        case 1: take(own text)
        default: take(own text)
    }
}
''', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "CASE\n")

    def test_switch_move_on_any_arm_blocks_later_reads(self):
        for body in ('case 1: take(own text)',
                     'case 1: take(own text) default: printin(text)',
                     'default: take(own text)'):
            with self.subTest(body=body):
                self.reject('func take(str text) { printin(text) } main { var text = str_upper("x") '
                            'int n = 1 switch n { ' + body + ' } printin(text) }', "Use of moved binding")

    def test_switch_move_with_break_is_rejected(self):
        self.reject('func take(str text) { printin(text) } main { var text = str_upper("x") '
                    'int n = 1 switch n { case 1: take(own text) break default: printin(text) } }',
                    "Moves in switch/match arms containing break or continue")

    def test_break_only_arms_keep_after_switch_reachable(self):
        self.reject('main { int n = 1 switch n { case 1: break default: break } '
                    'var text = str_upper("x") var moved = own text printin(text) }', "Use of moved binding")

    def test_returning_switch_arm_does_not_poison_continuing_path(self):
        result = self.compile('''
func pick(int choice): str {
    var text = str_upper("pick")
    switch choice {
        case 1: return own text
        default: printin(text)
    }
    return own text
}
main { printin(pick(1)) printin(pick(2)) }
''', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "PICK\nPICK\nPICK\n")

    def test_match_arms_move_independently(self):
        result = self.compile('''
enum Kind { FIRST, SECOND }
func take(str text): int { printin(text) return 0 }
main {
    var text = str_upper("arm")
    Kind kind = FIRST
    match kind {
        FIRST => take(own text)
        _ => take(own text)
    }
}
''', run=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "ARM\n")

    def test_guarded_wildcard_is_not_exhaustive(self):
        self.reject('enum Kind { FIRST, SECOND } func take(str text) { printin(text) } '
                    'main { var text = str_upper("x") Kind kind = FIRST '
                    'match kind { FIRST => take(own text) _ if true => printin(text) } printin(text) }',
                    "Use of moved binding")


if __name__ == "__main__":
    unittest.main()
