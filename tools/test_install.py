# SPDX-License-Identifier: Apache-2.0
"""Install LunaPath to a temporary prefix and build/run an external consumer."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
def run(args):
    subprocess.run(args, check=True)

with tempfile.TemporaryDirectory(prefix="lunapath-install-") as temp:
    root = Path(temp)
    build = root / "library-build"
    prefix = root / "prefix"
    consumer = root / "consumer-build"
    run(["cmake", "-S", str(ROOT), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release",
         "-DLUNAPATH_BUILD_TESTS=OFF", "-DLUNAPATH_BUILD_EXAMPLES=OFF",
         f"-DCMAKE_INSTALL_PREFIX={prefix}"])
    run(["cmake", "--build", str(build), "--config", "Release", "--parallel"])
    run(["cmake", "--install", str(build), "--config", "Release"])
    run(["cmake", "-S", str(ROOT / "tests/install_consumer"), "-B", str(consumer),
         "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_PREFIX_PATH={prefix}"])
    run(["cmake", "--build", str(consumer), "--config", "Release", "--parallel"])
    suffix = ".exe" if os.name == "nt" else ""
    executable_dir = consumer / "Release" if os.name == "nt" else consumer
    run([str(executable_dir / f"lunapath_consumer_c{suffix}")])
    run([str(executable_dir / f"lunapath_consumer_cpp{suffix}")])
print("PASS: installed LunaPath::lunapath C and C++ consumer builds and runs")
