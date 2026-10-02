# SPDX-License-Identifier: Apache-2.0
"""Run the independent Python oracle, event vectors and deterministic fault campaign."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PYTHON = os.environ.get("PYTHON", os.sys.executable)

def run(args, cwd=ROOT):
    subprocess.run(args, cwd=cwd, check=True)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--skip-faults", action="store_true", help="Skip the 100k/profile mutation campaign")
    args = parser.parse_args()
    run([PYTHON, "tools/verify_vectors.py", "properties", "1000000"])
    run([PYTHON, "legacy/v1/reference_lunu.py"])
    if not args.skip_faults:
        run([PYTHON, "tools/fault_audit.py"])
    with tempfile.TemporaryDirectory(prefix="lunapath-vectors-") as temp:
        build = Path(temp)
        for guard in range(5):
            exe, vector_file = build / f"vectors-{guard}.exe", build / f"vectors-{guard}.csv"
            run([os.environ.get("CC", "gcc"), "-std=c17", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                 "-DLUNAPATH_PATH_BITS=256", f"-DLUNAPATH_GUARD={guard}", "src/lunapath.c",
                 "tests/vector_lunapath.c", "-Iinclude", "-o", str(exe)])
            with vector_file.open("wb") as output:
                subprocess.run([str(exe)], cwd=ROOT, stdout=output, check=True)
            run([PYTHON, "tools/verify_vectors.py", "vectors", str(vector_file), str(guard)])
    print("PASS: independent oracle, V1 oracle, five 256-event vectors and selected fault campaigns")

if __name__ == "__main__":
    main()
