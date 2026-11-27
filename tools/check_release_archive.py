"""Smoke-test the actual release archive outside the source checkout."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    archive = Path(sys.argv[1]).resolve()
    suffix = ".exe" if os.name == "nt" else ""
    with tempfile.TemporaryDirectory(prefix="cnext release ") as directory:
        work = Path(directory)
        install = work / "install"
        shutil.unpack_archive(archive, install)
        native = install / ("cnext" + suffix)
        compiler = install / ("cnext-selfhost" + suffix)
        companion = install / ("cnext-tools" + suffix)
        for path in (native, compiler, companion, install / "include/runtime.h", install / "registry.txt"):
            if not path.is_file():
                raise FileNotFoundError(path)
        env = os.environ.copy()
        env.pop("CNEXT_INCLUDE", None)
        subprocess.run([native, "version"], cwd=work, env=env, check=True)
        for args in (["version"], ["doctor"], ["new", "Demo"]):
            subprocess.run([compiler, *args], cwd=work, env=env, check=True)
        app = work / "Demo"
        result = subprocess.run([compiler, "run", "src/main.cn"], cwd=app, env=env,
                                check=True, text=True, capture_output=True)
        if "Welcome to Demo" not in result.stdout:
            raise RuntimeError(result.stdout)


if __name__ == "__main__":
    main()
