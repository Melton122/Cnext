import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CNEXT = Path(os.environ.get("CNEXT_EXE", ROOT / ("cnext.exe" if os.name == "nt" else "cnext"))).resolve()


class PackageSecurityTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "build").mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="package-security-", dir=ROOT / "build")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        suffix = ".exe" if os.name == "nt" else ""
        self.cnext = self.work / ("cnext" + suffix)
        shutil.copy2(CNEXT, self.cnext)

    def write(self, name, text):
        path = self.work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("w", encoding="utf-8", newline="\n") as output:
            output.write(text)
        return path

    def command(self, *args):
        return subprocess.run([str(self.cnext if arg == CNEXT else arg) for arg in args], cwd=self.work,
                              capture_output=True, text=True, errors="replace", timeout=30)

    def test_download_validates_url_before_file_branch(self):
        source = (ROOT / "src" / "main_packages.c").read_text(encoding="utf-8")
        guard = source.index("if (!is_safe_url(url))")
        file_branch = source.index('if (strncmp(url, "file://", 7) == 0)')
        self.assertLess(guard, file_branch)
        self.assertIn('{"curl", "-fsSL", "-o", tmp_path, "--", url, NULL}', source)
        self.assertIn('{"wget", "-q", "-O", tmp_path, "--", url, NULL}', source)
        self.assertIn("finalize_package_file(tmp_path, out_path)", source)
        self.assertIn("is_allowed_dependency_url(value)", source)

    def test_registry_requires_https_and_unique_temp_paths(self):
        source = (ROOT / "src" / "registry.c").read_text(encoding="utf-8")
        self.assertNotIn('strncmp(url, "http://", 7)', source)
        self.assertIn("if (!is_https_url(url))", source)
        self.assertNotIn("http_response.tmp", source)
        self.assertIn("make_cache_temp_path", source)
        self.assertIn("is_safe_registry_name(package_name)", source)
        self.assertIn('strpbrk(token, "\\r\\n")', source)

    def test_registry_login_verifies_token_with_auth_header(self):
        source = (ROOT / "src" / "registry.c").read_text(encoding="utf-8")
        self.assertIn("static bool http_get_ex(const char* url, const char* token,", source)
        self.assertIn("http_get_ex(url, token, &body, &status, NULL)", source)
        self.assertIn('Authorization: Bearer %s', source)
        self.assertIn("Warning: Refusing non-HTTPS request", source)

    def test_file_package_install_uses_validated_temp_copy(self):
        self.write("pkg.cn", "func helper(): int { return 42 }\n")
        self.write("registry.txt", "safe = file://pkg.cn\nbad = file://../outside.cn\n")
        result = self.command(CNEXT, "install", "safe")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.work / "packages" / "safe.cn").read_text(encoding="utf-8"),
                         "func helper(): int { return 42 }\n")
        self.assertFalse((self.work / "packages" / "safe.cn.0.tmp").exists())
        bad = self.command(CNEXT, "install", "bad")
        self.assertNotEqual(bad.returncode, 0)
        self.assertIn("Unsafe local package path", bad.stderr)
        self.assertFalse((self.work / "packages" / "bad.cn").exists())

    def test_remote_registry_rejects_insecure_urls(self):
        result = self.command(CNEXT, "login", "http://example.invalid")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("must use HTTPS", result.stderr)
        info = self.command(CNEXT, "info", "../escape")
        self.assertEqual(info.returncode, 0)
        self.assertIn("Invalid package name", info.stderr)

    def test_manifest_install_and_update_report_failures(self):
        for command in ("install", "update"):
            with self.subTest(command=command):
                result = self.command(CNEXT, command)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.write("cnext.toml", '[dependencies]\nmissing = "file://does-not-exist.cn"\n')
        for command in ("install", "update"):
            with self.subTest(command=command):
                result = self.command(CNEXT, command)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("1 failed", result.stdout + result.stderr)

    def test_update_replaces_package_content(self):
        package = self.write("pkg.cn", "func version(): int { return 1 }\n")
        self.write("cnext.toml", '[dependencies]\nsafe = "file://pkg.cn"\n')
        result = self.command(CNEXT, "install")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        package.write_text("func version(): int { return 2 }\n", encoding="utf-8")
        result = self.command(CNEXT, "update")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.work / "packages" / "safe.cn").read_text(), package.read_text())

    def test_malformed_dependencies_do_not_succeed_silently(self):
        for entry in ('missing = 123', 'missing = "file://pkg.cn', 'not a dependency'):
            self.write("cnext.toml", '[dependencies]\n' + entry + '\n')
            with self.subTest(entry=entry):
                result = self.command(CNEXT, "install")
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("Invalid dependency declaration", result.stderr)

    def test_scoped_package_install_in_fresh_project(self):
        self.write("pkg.cn", "func helper(): int { return 42 }\n")
        self.write("cnext.toml", '  [dependencies]\n  vendor/lib = "file://pkg.cn" # local fixture\n')
        result = self.command(CNEXT, "install")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.work / "packages" / "vendor" / "lib.cn").read_text(),
                         (self.work / "pkg.cn").read_text())


if __name__ == "__main__":
    unittest.main()
