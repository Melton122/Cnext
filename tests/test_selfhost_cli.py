"""Exercise build/run and C linking through an existing self-hosted compiler."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SUFFIX = ".exe" if os.name == "nt" else ""
COMPILER = ROOT / "build" / "selfhost" / ("stage2_exe" + SUFFIX)


class SelfHostedCliTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="selfhost-cli-", dir=ROOT / "build")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.source = self.write("source with spaces & symbols.cn", 'main { printin("hello") }\n')
        self.output = self.work / ("program with spaces & symbols" + SUFFIX)
        self.env = dict(os.environ, TMPDIR=str(self.work), TMP=str(self.work), TEMP=str(self.work))
        # Test automatic runtime discovery, independent of the caller's setup.
        self.env.pop("CNEXT_INCLUDE", None)

    def write(self, name, content):
        path = self.work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        return path

    def command(self, *args, **kwargs):
        return subprocess.run(list(map(str, args)), cwd=self.work, env=self.env,
                              capture_output=True, text=True, timeout=90, **kwargs)

    def cli(self, *args, **kwargs):
        return self.command(COMPILER, *args, **kwargs)

    def assert_ok(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def assert_clean(self):
        self.assertEqual(list(self.work.glob(".cnext-*")), [])

    def test_companion_commands(self):
        doctor = self.cli("doctor")
        self.assert_ok(doctor)
        self.assertIn("Cnext Doctor", doctor.stdout)
        repl = self.cli("repl", input="6 * 7\n.exit\n")
        self.assert_ok(repl)
        self.assertIn("42", repl.stdout)
        for command in ("search", "info", "remove"):
            with self.subTest(command=command):
                result = self.cli(command)
                self.assertEqual(result.returncode, 64)
                self.assertIn("Usage: cnext " + command, result.stderr)

    def test_init_manifest(self):
        manifest = self.work / "cnext.toml"
        for args, name in [((), "my-project"), (("Sample Project",), "Sample Project")]:
            result = self.cli("init", *args)
            self.assert_ok(result)
            self.assertEqual(result.stdout, f"Created cnext.toml for project '{name}'\n")
            text = (f'[package]\nname = "{name}"\nversion = "0.1.0"\n'
                    'description = "A Cnext project"\n\n[dependencies]\n')
            self.assertEqual(manifest.read_bytes(), text.replace("\n", os.linesep).encode())

    def test_new_matches_native_project(self):
        name = "Sample Project"
        native_dir = self.work / "native"
        native_dir.mkdir()
        native = subprocess.run([ROOT / ("cnext" + SUFFIX), "new", name], cwd=native_dir,
                                capture_output=True, text=True, timeout=90)
        self.assert_ok(native)
        result = self.cli("new", name)
        self.assert_ok(result)
        self.assertEqual(result.stdout, native.stdout)

        def snapshot(directory):
            return {p.relative_to(directory): p.read_bytes() if p.is_file() else None
                    for p in directory.rglob("*")}

        self.assertEqual(snapshot(self.work / name), snapshot(native_dir / name))

    def test_help_and_legacy_emission(self):
        self.assertIn("Usage:", self.cli("--help").stdout)
        legacy = self.cli(self.source)
        self.assert_ok(legacy)
        generated = self.work / "generated C.c"
        for args in ((self.source, "-o", generated),
                     ("build", self.source, "--emit-c", generated)):
            result = self.cli(*args)
            self.assert_ok(result)
            self.assertEqual(result.stdout, "")
            self.assertEqual(generated.read_text(), legacy.stdout)

    def test_build_and_default_output(self):
        self.assert_ok(self.cli("build", self.source, "-o", self.output))
        self.assertEqual(self.command(self.output).stdout, "hello\n")
        self.assert_ok(self.cli("build", self.source))
        default = self.work / ("a.exe" if os.name == "nt" else "a.out")
        self.assertEqual(self.command(default).stdout, "hello\n")
        self.assert_clean()

    def test_run_arguments_stdin_and_exit_status(self):
        self.source.write_text('main { printin(sys_arg_at(1)); printin(input()); return 7 }\n')
        argument = 'spaces & | ; $() "quotes" \\ trailing\\'
        result = self.cli("run", self.source, "--", argument, input="from stdin\n")
        self.assertEqual(result.returncode, 7, result.stdout + result.stderr)
        self.assertEqual(result.stdout, argument + "\nfrom stdin\n")
        self.assert_clean()

    def test_run_retains_requested_binary(self):
        self.assert_ok(self.cli("run", self.source, "-o", self.output))
        self.assertTrue(self.output.is_file())
        self.assertEqual(self.command(self.output).stdout, "hello\n")
        self.assert_clean()

    def test_unwritable_system_temp_and_environment_restoration(self):
        missing_temp = str(self.work / "missing temporary directory")
        self.env.update(TMPDIR=missing_temp, TMP=missing_temp, TEMP=missing_temp)
        self.source.write_text('main { printin(getenv("TEMP")) }\n')
        result = self.cli("run", self.source)
        self.assert_ok(result)
        self.assertEqual(result.stdout, missing_temp + "\n")
        self.assert_clean()

    def test_c_sources_and_headers(self):
        self.source.write_text('extern "C" func native_add(int a, int b): int\nmain { printin(native_add(19, 22)) }\n')
        header = self.write("headers & spaces/offset.h", "#define OFFSET 1\n")
        native = self.write("sources & spaces/add.c", '#include <offset.h>\nint native_add(int a, int b) { return a+b+OFFSET; }\n')
        result = self.cli("run", self.source, "--c-source", native,
                          "--include-dir", str(header.parent) + os.sep)
        self.assert_ok(result)
        self.assertEqual(result.stdout, "42\n")
        self.assert_clean()

    def test_objects_and_library_order(self):
        if not shutil.which("ar"):
            self.skipTest("ar is required")
        self.source.write_text('extern "C" func native_add(int a, int b): int\nmain { printin(native_add(19, 23)) }\n')
        helper = self.write("helper.c", "int native_adjust(int x) { return x+1; }\n")
        bridge = self.write("bridge.c", "int native_adjust(int);\nint native_add(int a,int b) { return native_adjust(a+b)-1; }\n")
        helper_object, bridge_object = self.work / "helper.o", self.work / "bridge.o"
        self.assert_ok(self.command("gcc", "-c", helper, "-o", helper_object))
        self.assert_ok(self.command("gcc", "-c", bridge, "-o", bridge_object))
        archive = self.work / "libfixture.a"
        self.assert_ok(self.command("ar", "rcs", archive, helper_object))
        result = self.cli("run", self.source, "--link-lib", "fixture",
                          "--library-dir", self.work, "--object", bridge_object)
        self.assert_ok(result)
        self.assertEqual(result.stdout, "42\n")
        self.assert_clean()

    def test_invalid_options(self):
        self.assertEqual(self.cli().returncode, 2)
        for args in (("build",), ("build", self.source, "-o"),
                     ("build", self.source, "--unknown"),
                     ("build", self.source, "-o", self.output, "-o", self.output),
                     ("run", self.source, "--emit-c", self.output),
                     ("build", self.source, "--emit-c", self.output, "--object", "missing.o")):
            with self.subTest(args=args):
                self.assertEqual(self.cli(*args).returncode, 64)
        self.assertFalse(self.output.exists())

    def test_invalid_source_and_output(self):
        bad = self.write("bad.cn", "not valid Cnext !!!\n")
        self.assertNotEqual(self.cli("build", bad, "-o", self.output).returncode, 0)
        missing_output = self.work / "missing" / "out.c"
        self.assertEqual(self.cli("build", self.source, "--emit-c", missing_output).returncode, 74)
        self.assertFalse(self.output.exists())
        self.assert_clean()

    def test_missing_compiler_and_runtime(self):
        result = self.cli("run", self.source, "--cc", "cnext_missing_c_compiler")
        self.assertEqual(result.returncode, 127, result.stdout + result.stderr)
        self.assertEqual(self.cli("build", self.source, "--runtime-dir", "missing").returncode, 74)
        self.assert_clean()

    def test_compile_failure_does_not_run_old_binary(self):
        self.source.write_text('main { printin("old executable") }\n')
        self.assert_ok(self.cli("build", self.source, "-o", self.output))
        result = self.cli("run", self.source, "-o", self.output, "--c-source", "missing.c")
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("old executable", result.stdout)
        self.assert_clean()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, default=COMPILER)
    args, remaining = parser.parse_known_args()
    COMPILER = args.compiler.resolve()
    unittest.main(argv=[__file__, *remaining])
