"""Verify allocator reclamation using observed malloc/free calls, not counters alone."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AllocatorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="allocator-contract-", dir=ROOT / "build")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.executable = Path(cls.temp.name) / ("allocator.exe" if os.name == "nt" else "allocator")
        result = subprocess.run(shlex.split(os.environ.get("CC", "gcc")) + [
            "-std=gnu11", "-O2", "-iquote", str(ROOT / "include"),
            str(ROOT / "tests" / "runtime" / "allocator_contract.c"), "-o", str(cls.executable),
        ], capture_output=True, text=True, timeout=60)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)

    def run_mode(self, mode):
        return subprocess.run([str(self.executable), mode], capture_output=True, text=True, timeout=15)

    def test_alignment_reuse_and_observed_bulk_reclamation(self):
        result = self.run_mode("normal")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("allocator contracts passed", result.stdout)

    def test_split_growth_is_tracked_and_freed_exactly_once(self):
        result = self.run_mode("split-growth")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("split growth contracts passed", result.stdout)

    def test_invalid_sizes_fail_without_wraparound(self):
        for mode in ("arena-overflow", "pool-overflow", "arena-accounting", "pool-accounting", "null-arena", "array-overflow"):
            with self.subTest(mode=mode):
                result = self.run_mode(mode)
                self.assertEqual(result.returncode, 70, result.stdout + result.stderr)
                self.assertIn("overflow" if "null" not in mode else "null arena", result.stderr)

    def test_allocation_failures_release_locks_and_partial_allocations(self):
        for mode in ("create-oom", "block-oom", "large-oom", "ledger-oom", "pool-block-oom", "pool-ledger-oom", "array-oom"):
            with self.subTest(mode=mode):
                result = self.run_mode(mode)
                self.assertEqual(result.returncode, 70, result.stdout + result.stderr)
                self.assertIn("out of memory", result.stderr)


if __name__ == "__main__":
    unittest.main()
