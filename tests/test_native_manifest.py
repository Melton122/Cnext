"""Build real mixed-language projects with the native and self-hosted drivers."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CNEXT = Path(os.environ.get("CNEXT_EXE", ROOT / ("cnext.exe" if os.name == "nt" else "cnext"))).resolve()
SUFFIX = ".exe" if os.name == "nt" else ""
HOST = "windows" if os.name == "nt" else "macos" if __import__("sys").platform == "darwin" else "linux"


class NativeManifestTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="native-manifest-", dir=ROOT / "build")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.source = self.write("main.cn", 'extern "C" func native_add(int a, int b): int\nmain { printin(native_add(19, 23)) }\n')
        self.output = self.work / ("program with spaces" + SUFFIX)

    def write(self, name, text):
        path = self.work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return path

    def cli(self, *args):
        return subprocess.run([str(CNEXT), *map(str, args)], cwd=self.work,
                              capture_output=True, text=True, timeout=60)

    def native(self, name="native.c"):
        return self.write(name, "int native_add(int a, int b) { return a + b; }\n")

    def assert_run(self, *options):
        result = self.cli("run", self.source, "-o", self.output, *options)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "42\n")

    def test_auto_manifest_sources_headers_and_special_paths(self):
        self.write("headers with spaces/offset.h", "#define OFFSET 2\n")
        self.write("native & sources/helper 'quoted'.c", '#include "offset.h"\nint native_add(int a, int b) { return a + b - OFFSET + 2; }\n')
        self.write("cnext.toml", '''[project]
name = "demo"
[native]
c_sources = [
    "native & sources/helper 'quoted'.c", # path is an argv value
]
include_dirs = ['headers with spaces']
''')
        self.assert_run()

    def test_explicit_manifest_paths_are_relative_to_its_directory(self):
        self.native("project with spaces/native.c")
        manifest = self.write("project with spaces/cnext.toml", '[native]\nc_sources = ["native.c"]\n')
        self.write("cnext.toml", '[native]\nc_sources = ["must-not-be-used.c"]\n')
        self.assert_run("--manifest", manifest)

    def test_base_and_host_settings_merge_and_other_targets_are_inactive(self):
        self.write("base.c", "int host_adjust(int x); int native_add(int a, int b) { return host_adjust(a + b); }\n")
        self.write("host.c", "int host_adjust(int x) { return x; }\n")
        other = "linux" if HOST != "linux" else "windows"
        self.write("cnext.toml", f'''[native.{HOST}]
c_sources = ["host.c"]
[native]
c_sources = ["base.c"]
[native.{other}]
c_sources = ["this-target-is-not-active.c"]
''')
        self.assert_run()

    def test_command_line_sources_append_to_manifest(self):
        self.write("base.c", "int adjust(int x); int native_add(int a, int b) { return adjust(a + b); }\n")
        extra = self.write("extra.c", "int adjust(int x) { return x; }\n")
        self.write("cnext.toml", '[native]\nc_sources = ["base.c"]\n')
        self.assert_run("--c-source", extra)

    def test_objects_archives_and_library_order(self):
        if not shutil.which("ar"):
            self.skipTest("ar is required for static archive fixtures")
        bridge = self.write("bridge.c", "int first(int x); int native_add(int a, int b) { return first(a + b); }\n")
        first = self.write("first.c", "int second(int x); int first(int x) { return second(x); }\n")
        second = self.write("second.c", "int second(int x) { return x; }\n")
        libraries = self.work / "libraries with spaces"
        libraries.mkdir()
        for source in (bridge, first, second):
            result = subprocess.run(["gcc", "-c", str(source), "-o", str(source.with_suffix(".o"))], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for source in (first, second):
            result = subprocess.run(["ar", "rcs", str(libraries / ("lib" + source.stem + ".a")), str(source.with_suffix(".o"))], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.write("cnext.toml", '''[native]
objects = ["bridge.o"]
library_dirs = ["libraries with spaces"]
libraries = ["first", "second"]
''')
        self.assert_run()

    def test_missing_manifest_and_missing_native_file_fail(self):
        result = self.cli("build", self.source, "--manifest", "absent.toml", "-o", self.output)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Could not read native manifest", result.stdout + result.stderr)
        self.write("cnext.toml", '[native]\nc_sources = ["absent.c"]\n')
        result = self.cli("build", self.source, "-o", self.output)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.output.exists())

    def test_disable_native_settings_and_pure_c_emission(self):
        native = self.native()
        self.write("cnext.toml", '[native]\nunknown_key = ["invalid"]\n')
        self.assert_run("--no-native-config", "--c-source", native)
        generated = self.work / "generated.c"
        result = self.cli("build", self.source, "--emit-c", generated)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("native_add", generated.read_text())
        conflict = self.cli("build", self.source, "--manifest", "cnext.toml", "--no-native-config")
        self.assertNotEqual(conflict.returncode, 0)
        self.assertIn("Cannot combine", conflict.stdout + conflict.stderr)

    def test_invalid_native_settings_fail_before_linking(self):
        inactive = "linux" if HOST != "linux" else "windows"
        cases = (
            '[native]\nc_sorces = ["native.c"]',
            '[native]\nc_sources = "native.c"',
            '[native]\nc_sources = [42]',
            '[native]\nc_sources = ["native.c"',
            '[native]\nc_sources = [""]',
            '[native]\nc_sources = ["bad\\npath.c"]',
            '[native]\nc_sources = ["a.c"]\nc_sources = ["b.c"]',
            '[native]\n[native]',
            '[native.solaris]\nc_sources = []',
            '["native"]\nc_sources = []',
            '[native]\nlibraries = ["-Wl,bad"]',
            '[native]\nc_sources = ["native.c"] junk',
            '[native]\nc_sources = [' + ','.join('"native.c"' for _ in range(129)) + ']',
            f'[native.{inactive}]\nc_sources = 42',
        )
        for content in cases:
            self.write("cnext.toml", content)
            with self.subTest(content=content[:80]):
                result = self.cli("build", self.source, "-o", self.output)
                self.assertEqual(result.returncode, 64, result.stdout + result.stderr)
                self.assertIn("cnext.toml:", result.stdout + result.stderr)
                self.assertFalse(self.output.exists())

    def test_truncated_native_arrays_fail_with_bounded_diagnostics(self):
        for suffix in ('', '[', '["', '["x', '["x\\', '["x"', '["x",', '[#comment\n', "['x", "['x',"):
            self.write("cnext.toml", '[native]\nc_sources = ' + suffix)
            with self.subTest(suffix=suffix):
                result = self.cli("build", self.source, "-o", self.output)
                self.assertEqual(result.returncode, 64, result.stdout + result.stderr)
                self.assertLess(len(result.stdout + result.stderr), 2048)

    def test_manifest_failure_preserves_existing_output(self):
        self.output.write_bytes(b"previous artifact")
        self.write("cnext.toml", '[native]\nc_sources = 42\n')
        result = self.cli("build", self.source, "-o", self.output)
        self.assertEqual(result.returncode, 64, result.stdout + result.stderr)
        self.assertEqual(self.output.read_bytes(), b"previous artifact")

    def test_response_file_prefix_is_treated_as_a_filename(self):
        self.native("@native.c")
        self.write("cnext.toml", '[native]\nc_sources = ["@native.c"]\n')
        self.assert_run()

    def test_response_file_prefix_in_manifest_directory_is_a_path(self):
        self.native("@project/native.c")
        manifest = self.write("@project/cnext.toml", '[native]\nc_sources = ["native.c"]\n')
        self.assert_run("--manifest", manifest.relative_to(self.work))

    def test_manifest_control_characters_and_zero_bytes_fail(self):
        result = self.cli("build", self.source, "--manifest", "bad\nmanifest.toml")
        self.assertEqual(result.returncode, 64, result.stdout + result.stderr)
        self.assertIn("Invalid native manifest path", result.stdout + result.stderr)
        self.write("cnext.toml", '[native]\x00\nc_sources = []')
        result = self.cli("build", self.source)
        self.assertEqual(result.returncode, 64, result.stdout + result.stderr)
        self.assertIn("contains zero bytes", result.stdout + result.stderr)

    def test_utf8_bom_and_crlf_manifest(self):
        self.native()
        self.write("cnext.toml", '\ufeff[native]\r\nc_sources = ["native.c"]\r\n')
        self.assert_run()

    def test_combined_manifest_and_cli_limit_is_enforced(self):
        self.write("cnext.toml", '[native]\nc_sources = [' + ','.join('"native.c"' for _ in range(128)) + ']\n')
        result = self.cli("build", self.source, "--c-source", "extra.c", "-o", self.output)
        self.assertEqual(result.returncode, 64, result.stdout + result.stderr)
        self.assertIn("Too many native build arguments", result.stdout + result.stderr)
        self.assertFalse(self.output.exists())

    @unittest.skipUnless(os.name == "nt", "Windows path separators")
    def test_windows_literal_and_escaped_path_strings(self):
        self.native("native/helper.c")
        for value in ("'native\\helper.c'", json.dumps("native\\helper.c")):
            self.write("cnext.toml", '[native]\nc_sources = [' + value + ']\n')
            self.assert_run()


if __name__ == "__main__":
    unittest.main()
