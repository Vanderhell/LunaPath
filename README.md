# LunaPath

[![CI](https://github.com/Vanderhell/LunaPath/actions/workflows/ci.yml/badge.svg)](https://github.com/Vanderhell/LunaPath/actions/workflows/ci.yml)

> Bounded path and ordered-event integrity primitives for embedded systems.

LunaPath is a bounded, allocation-free C library for representing hierarchical binary paths and validating ordered event progression with optional lightweight integrity guards. It targets microcontrollers, deterministic firmware, and embedded state machines. CRC and rolling guards detect accidental corruption; they are not cryptographic authentication.

## What it provides

- Compile-time path capacities of 32, 64, 128, or 256 decisions.
- Path operations for child, parent, prefix, common prefix, neighbor, distance, equality, and canonical serialization.
- Compile-time guards: NONE, PARITY, CRC-16/CCITT-FALSE, CRC-32/ISO-HDLC, and a V1-style rolling 64-bit reference.
- Caller-owned bounded state, zero-length events, optional payload tags, and transactional failure behavior.
- C17 implementation with no heap allocation, recursion, mutable globals, or platform dependencies in the portable core.

The serialized path format is stable for v0.1: a 16-bit big-endian depth followed by root-first path bits, most-significant bit first in each byte. Unused tail bits must be zero. In-memory C struct layout is not a persistence format and is not promised as a stable ABI.

## Example

The checked firmware workflow in [`examples/firmware_workflow.c`](examples/firmware_workflow.c) records branch decisions and block metadata, then detects deleted, reordered, duplicated, and payload-changed events. A small usage pattern is:

```c
#include "lunapath.h"

int record_header_result(lunapath_state *state, bool accepted,
                         const uint8_t *header, uint16_t header_bits) {
    return lunapath_step(state, accepted, header, header_bits);
}

int save_path(const lunapath_state *state, uint8_t *wire, size_t capacity,
              size_t *written) {
    return lunapath_serialize(&state->path, wire, capacity, written);
}
```

Initialize state with `lunapath_state_zero()` before recording events. Configure a build with `LUNAPATH_PATH_BITS` and `LUNAPATH_GUARD`; the portable default is LunaPath64/CRC16. `lunapath_step()` appends one path decision and folds the event's payload into the configured guard. `NULL` is valid when `payload_bits` is zero.

## Footprint and measured result

| Configuration | Path | State |
|---|---:|---:|
| V1 reference | 40 B | 56 B |
| LunaPath32/NONE | 8 B | 8 B |
| LunaPath32/CRC16 | 8 B | 12 B |
| LunaPath64/CRC16 | 12 B | 16 B |
| LunaPath64/CRC32 | 12 B | 16 B |
| LunaPath128/CRC32 | 20 B | 24 B |
| LunaPath256/CRC32 | 36 B | 40 B |

On the tested ESP32-S3 at 160 MHz with `-Os`, a 32-byte LunaPath64/CRC32 event step measured 749 cycles using the optional ESP-IDF ROM CRC32 backend; the V1 rolling step measured 5,421 cycles in the same harness. This is a result for that tested configuration, not a cross-platform performance guarantee. The ESP32-S3 campaign passed a 1,000,000-operation run. Host validation covered 8,388,604 differential paths with zero mismatches and 1,000,000 independent oracle cases.

CRC and rolling guards detect accidental corruption. **LunaPath is not a cryptographic authentication mechanism** and does not authenticate events against a malicious attacker.

## Build, install, and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix /path/to/prefix
```

A CMake consumer can find and link the installed library:

```cmake
find_package(LunaPath CONFIG REQUIRED)
target_link_libraries(app PRIVATE LunaPath::lunapath)
```

Set `CMAKE_PREFIX_PATH` to the installation prefix when it is not in a standard package location.

Select a profile when configuring, for example:

```sh
cmake -S . -B build-64-crc32 -DLUNAPATH_PATH_BITS=64 -DLUNAPATH_GUARD=3
```

The full Python oracle, vector, and fault campaigns are available with `python tools/validate.py`. Python is not needed to consume or build the library.

## Tested compatibility

Hosted CI builds and tests with GCC on Ubuntu, Clang on Ubuntu, and MSVC on Windows. Hardware validation covers the ESP32-S3 with ESP-IDF 5.5.1. The measured ESP32-S3 CRC32 result used LunaPath64/CRC32 with the optional ESP-IDF ROM CRC32 backend; this target-specific optimization does not change the portable LunaPath64/CRC16 default.

ESP32-S3 application sources and generic port instructions are in [`hardware/esp32s3/README.md`](hardware/esp32s3/README.md). Detailed host and hardware evidence is in [`validation/RESULTS.md`](validation/RESULTS.md), with the memory matrix in [`validation/results.csv`](validation/results.csv). The V1 implementation remains in [`legacy/v1/`](legacy/v1/) as a historical differential oracle; new projects should use the `lunapath_*` API.

## License

Apache-2.0. See [`LICENSE`](LICENSE) and [`NOTICE`](NOTICE).
