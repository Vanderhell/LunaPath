"""Rebuild and run the strict GCC LUNA Embedded host matrix in a temp dir."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent
CC = os.environ.get("CC", "gcc")
PROFILES = (32, 64, 128, 256)
GUARDS = (0, 1, 2, 3, 4)

def run(command):
    subprocess.run(command, cwd=ROOT, check=True)

def main():
    with tempfile.TemporaryDirectory(prefix="lunu-v2-", dir=ROOT) as temp:
        build = Path(temp)
        outputs = {}
        for optimization, flags in (("debug", ("-O0", "-g")), ("release", ("-O2",))):
            for path_bits in PROFILES:
                for guard in GUARDS:
                    name = f"{optimization}-{path_bits}-{guard}.exe"
                    executable = build / name
                    command = [CC, "-std=c17", *flags, "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                               f"-DLUNU_EMBED_PATH_BITS={path_bits}", f"-DLUNU_EMBED_GUARD={guard}",
                               "lunu_embedded.c", "lunu_primitives.c", "test_lunu_embedded.c", "-o", str(executable)]
                    run(command)
                    run([str(executable)])
                    outputs[(optimization, path_bits, guard)] = executable
        for path_bits in PROFILES:
            run([str(outputs[("release", path_bits, 2)]), "exhaustive"])
    print("PASS: strict GCC Debug/Release, 20 profiles each; exhaustive paths through depth 20 x 4 capacities")

if __name__ == "__main__":
    main()
