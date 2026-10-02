# SPDX-License-Identifier: Apache-2.0
"""Source-level check for the production core's allocation/state constraints."""
from pathlib import Path
import re

source = Path(__file__).resolve().parents[1] / "src/lunapath.c"
text = source.read_text(encoding="utf-8")
code = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
if re.search(r"\b(?:malloc|calloc|realloc|free)\s*\(", code):
    raise SystemExit("FAIL: heap allocation call found in production core")
if any(token in code for token in ("esp_idf", "freertos", "windows.h", "pthread", "fork(")):
    raise SystemExit("FAIL: platform dependency found in production core")
if re.search(r"(?m)^\s*static\s+[^\n(]+;\s*$", code):
    raise SystemExit("FAIL: file-scope mutable/static object found in production core")
definitions = re.finditer(
    r"(?m)^(?:static\s+)?(?:bool|void|size_t|uint(?:8|16|32|64)_t|lunapath_guard)\s+(\w+)\s*\([^;]*?\)\s*\{",
    code,
)
for match in definitions:
    depth, end = 1, match.end()
    while depth and end < len(code):
        depth += (code[end] == "{") - (code[end] == "}")
        end += 1
    if re.search(rf"\b{re.escape(match.group(1))}\s*\(", code[match.end():end - 1]):
        raise SystemExit(f"FAIL: recursive production function {match.group(1)}")
print("PASS: no heap calls, recursion, mutable file-scope state, or platform dependency")
