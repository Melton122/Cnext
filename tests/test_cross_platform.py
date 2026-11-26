import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CNEXT = Path(os.environ.get("CNEXT_EXE", ROOT / ("cnext.exe" if os.name == "nt" else "cnext"))).resolve()
EXE_SUFFIX = ".exe" if os.name == "nt" else ""


class CrossPlatformTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "build").mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="cross-platform-", dir=ROOT / "build")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.output = self.work / ("program" + EXE_SUFFIX)

    def write(self, name, text):
        path = self.work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("w", encoding="utf-8", newline="\n") as output:
            output.write(text)
        return path

    def command(self, *args):
        return subprocess.run([str(arg) for arg in args], cwd=self.work,
                              capture_output=True, text=True, timeout=60)

    def assert_ok(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_download_tool_output_options_precede_terminator(self):
        source = (ROOT / "src" / "main_packages.c").read_text(encoding="utf-8")
        self.assertIn('{"curl", "-fsSL", "-o", tmp_path, "--", url, NULL}', source)
        self.assertIn('{"wget", "-q", "-O", tmp_path, "--", url, NULL}', source)

    def test_process_spawn_is_the_only_spawnvp_caller(self):
        callers = []
        for path in (ROOT / "src").glob("*.c"):
            if "_spawnvp" in path.read_text(encoding="utf-8", errors="ignore"):
                callers.append(path.name)
        self.assertEqual(callers, ["main_utils.c"])

    def test_generated_c_uses_lf_line_endings(self):
        source = self.write("main.cn", "main { printin(42) }\n")
        generated = self.work / "generated.c"
        self.assert_ok(self.command(CNEXT, "build", source, "--emit-c", generated))
        data = generated.read_bytes()
        self.assertNotIn(b"\r\n", data)
        self.assert_ok(self.command("gcc", "-std=gnu11", "-iquote", ROOT / "include",
                                    generated, "-o", self.output))
        result = self.command(self.output)
        self.assertEqual(result.stdout.strip(), "42")

    def test_io_writes_keep_exact_newlines(self):
        source = self.write("io.cn", '''import io
main {
    io.write_file("bytes.txt", "a\\nb")
    io.append_file("bytes.txt", "c\\nd")
}
''')
        self.assert_ok(self.command(CNEXT, "run", source, "-o", self.output))
        self.assertEqual((self.work / "bytes.txt").read_bytes(), b"a\nbc\nd")

    def test_emit_c_failure_removes_temp_source(self):
        source = self.write("main.cn", "main { printin(1) }\n")
        result = self.command(CNEXT, "build", source, "--emit-c", self.work / "missing" / "generated.c")
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.work / "temp_out.c").exists())

    def test_cli_does_not_build_shell_strings(self):
        source = (ROOT / "src" / "main.c").read_text(encoding="utf-8")
        self.assertNotIn('system("', source)
        self.assertIn("run_process_quiet", source)

    def test_cache_clear_removes_directory_without_shell(self):
        cache = self.work / ".cnext" / "cache"
        nested = cache / "nested"
        nested.mkdir(parents=True)
        (nested / "value.txt").write_text("value", encoding="utf-8")
        status = self.command(CNEXT, "cache", "status")
        self.assert_ok(status)
        self.assertIn("Entries: 1", status.stdout)
        clear = self.command(CNEXT, "cache", "clear")
        self.assert_ok(clear)
        self.assertFalse(cache.exists())

    def test_captured_process_drains_output_to_completion(self):
        source = (ROOT / "src" / "main_utils.c").read_text(encoding="utf-8")
        self.assertIn("append_captured_output", source)
        self.assertNotIn("while (total < output_buf_size - 1", source)
        self.assertIn("Warning: captured output truncated.", source)

    def test_windows_header_workloads_are_lean(self):
        main_source = (ROOT / "src" / "main.c").read_text(encoding="utf-8")
        registry_source = (ROOT / "src" / "registry.c").read_text(encoding="utf-8")
        utils_source = (ROOT / "src" / "main_utils.c").read_text(encoding="utf-8")
        makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
        self.assertNotIn("#include <windows.h>", main_source)
        self.assertNotIn("#include <windows.h>", registry_source)
        self.assertIn("bool remove_tree", utils_source)
        self.assertIn("int count_tree_entries", utils_source)
        self.assertIn("-DWIN32_LEAN_AND_MEAN", makefile)
        self.assertIn("-DNOMINMAX", makefile)

    def test_benchmark_harness_is_reproducible(self):
        makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
        bench = (ROOT / "tools" / "bench.py").read_text(encoding="utf-8")
        self.assertIn("tools/bench.py", makefile)
        self.assertNotIn("@time $(MAKE)", makefile)
        self.assertNotIn("@touch src/main.c", makefile)
        self.assertIn("--runs", bench)
        self.assertIn("median_ms", bench)
        self.assertIn("results.json", bench)
        self.assertIn("baseline.json", bench)

    def test_vscode_extension_maps_cn_files_to_cnext_icon(self):
        package = json.loads((ROOT / "vscode-extension" / "package.json").read_text(encoding="utf-8"))
        contributes = package["contributes"]
        self.assertEqual(contributes["configurationDefaults"]["workbench.iconTheme"], "cnext")
        self.assertTrue(any(language.get("id") == "cnext" and ".cn" in language.get("extensions", [])
                            for language in contributes["languages"]))
        theme = json.loads((ROOT / "vscode-extension" / "icon-themes" / "cnext-icon-theme.json").read_text(encoding="utf-8"))
        self.assertEqual(theme["fileExtensions"]["cn"], "_cnext_brand")
        self.assertEqual(theme["languageIds"]["cnext"], "_cnext_brand")

    def test_release_archive_includes_readme_logo(self):
        makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
        self.assertIn("@cp README.md dist/", makefile)
        self.assertIn("@cp logo.png dist/", makefile)
        self.assertIn("cnext-windows-x64-v$(VERSION).zip", makefile)
        self.assertIn("-v$(VERSION).tar.gz", makefile)

    def test_install_scripts_quote_temp_paths_and_cleanup(self):
        shell = (ROOT / "install.sh").read_text(encoding="utf-8")
        self.assertIn(r'trap "rm -rf -- \"$tmpdir\"" EXIT', shell)
        self.assertIn('curl -fsSL -o "$dest" -- "$url"', shell)

        powershell_script = (ROOT / "install.ps1").read_text(encoding="utf-8")
        self.assertIn("finally {", powershell_script)
        self.assertIn("Remove-Item -LiteralPath $TMPDIR", powershell_script)

        batch = (ROOT / "install.bat").read_text(encoding="utf-8")
        self.assertGreaterEqual(batch.count('rmdir /S /Q "%TMPDIR%"'), 4)
        self.assertIn('curl -fsSL -o "%TMPDIR%\\%FILENAME%" -- "%URL%"', batch)


if __name__ == "__main__":
    unittest.main()
