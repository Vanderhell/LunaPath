# LUNA Embedded v2 — validation record

## Repository and baseline

- Initial branch: `master`; initial repository was unborn, with no HEAD SHA. The supplied V1 files were all untracked. V1 source and historical tests/results were left unchanged.
- Host toolchain: MinGW GCC 16.1.0, Clang 22.1.8, CMake 4.3.3, Python 3.11.9. The host repository had no CMake/Make build definition; V1 was built directly with GCC.
- V1 strict GCC C17 build and tests passed. `sizeof(lunu_path)=40`, `sizeof(lunu_state)=56`.
- V1 independent Python oracle: 1,000,000 properties passed; 256 vector cases matched; historical 66-mutation audit reproduced (`NONE 34/66`, `PARITY 66/66`, sequence parity 66/66, rolling64 66/66). V1 host benchmark reproduced approximately 7 ns child, 5.3 ns parent/prefix/neighbor, 309 ns rolling step over a 256-bit payload. V1 benchmark uses coarse `clock()` timing and is not directly comparable to the V2 QPC timings below.

## V2 design and event definition

V2 compiles one path capacity at a time: 32, 64, 128, or 256 bits. Path words are naturally aligned `uint32_t` words and depth is `uint16_t`, so depth 256 is representable. Guard choice is compile-time: NONE, PARITY, CRC16, CRC32, or ROLL64 reference. There is no mode field in state. NONE omits the physical guard member.

There is no stored step counter. A fresh state starts at path depth zero; each successful V2 event appends exactly one path bit, and the event position is the old `path.depth`. Therefore `step == depth` throughout the API-created V2 progression. V1 callers could seed or mutate its public `steps` field independently; that arbitrary V1 state is outside the V2 initialized append-only sequence contract. No guarded V2 transition has a separate position that can diverge from depth.

Canonical guarded event bytes are:

```text
4c 02 | depth_before:uint16 big endian | bit:00/01 |
payload_bits:uint16 big endian | ceil(payload_bits/8) payload bytes
```

Payload logical bit `i` is bit `i % 8` (LSB first) in byte `i / 8`. Unused high bits of the last byte are masked and do not affect the guard. No native structs or padding are processed. Zero-bit payload accepts `NULL`. Raw payload length is bounded to 65,535 bits and streamed without payload storage. `step_tag32` processes a fixed 48-bit surrogate payload: tag LE32 followed by original length LE16; its integrity is only as strong as the caller-supplied tag.

CRC16 is CRC-16/CCITT-FALSE (`poly=0x1021`, `init=0xffff`, `refin/refout=false`, `xorout=0`), with KAT `123456789 -> 0x29b1`. CRC32 is CRC-32/ISO-HDLC (`poly=0x04c11db7`, reflected implementation polynomial `0xedb88320`, `init/xorout=0xffffffff`, reflected input/output), with KAT `123456789 -> 0xcbf43926`. Both also pass empty and binary KATs. These checksums detect corruption; they do not authenticate against an adversary.

Path wire format remains V1-compatible: depth BE16 followed by root-first path bits MSB-first in each byte, zero padding in the final byte. Deserialization is bounded by the selected compile-time capacity and commits output only on success. A combined full-state wire format was deferred: no current use case needs to persist the guard with the path, and path interoperability already has a fixed canonical format.

Production core uses no heap, recursion, mutable globals, packed structs, or platform headers. `validate_embedded.py` builds strict GCC Debug and Release tests for all 20 profile/guard combinations. It then exhausts every path through depth 20 under each capacity.

## Memory

All figures are host `sizeof` measurements on x64 GCC. Full profile matrix is in [RESULTS_EMBEDDED_V2.csv](RESULTS_EMBEDDED_V2.csv). Padding is natural alignment, not packing.

| Profile | `sizeof(path)` | Logical guard | `sizeof(state)` | State vs V1 56 B |
|---|---:|---:|---:|---:|
| V1 | 40 B | 64 stored control + 64 steps | 56 B | baseline |
| LUNA32/NONE | 8 B | 0 bits | 8 B | −86% |
| LUNA32/CRC16 | 8 B | 16 bits | 12 B | −79% |
| LUNA64/NONE | 12 B | 0 bits | 12 B | −79% |
| LUNA64/CRC16 | 12 B | 16 bits | 16 B | −71% |
| LUNA64/CRC32 | 12 B | 32 bits | 16 B | −71% |
| LUNA64/ROLL64 | 12 B | 64 bits | 24 B | −57% |
| LUNA128/CRC32 | 20 B | 32 bits | 24 B | −57% |
| LUNA256/CRC16 | 36 B | 16 bits | 40 B | −29% |
| LUNA256/CRC32 | 36 B | 32 bits | 40 B | −29% |
| LUNA256/ROLL64 | 36 B | 64 bits | 48 B | −14% |

The smallest state for a useful checksum is LUNA32/CRC16 at 12 B when workflows fit within 32 decisions. The recommended default for the tested ESP32-S3 is LUNA64/CRC32 at 16 B: it doubles logical guard width over CRC16 without increasing this profile's physical state size, and it was faster in the on-target measurements. Use LUNA256/CRC32 at 40 B for longer paths. LUNA64/CRC16 remains a good option where portable host timing or target-specific CRC16 behavior is preferable.

## Host validation

- GCC strict C17 Debug (`-O0 -g`) and Release (`-O2`) with `-Wall -Wextra -Wpedantic -Werror`: all 20 profile/guard combinations passed in each build.
- Exhaustive V1 differential: all 2,097,151 paths through depth 0–20 for each of the four path capacities (8,388,604 path cases total), including prefixes, parents, all neighbors, involution, distance, common prefix, and canonical serialization; zero mismatches.
- Boundary and misuse tests cover depth 256, max+1 rejection, setter indices at/beyond depth, NULL pointers, raw payload length 65,535, zero payload, tail-bit masking, truncated/extra/noncanonical serialization, guard mismatch, and transactional failures.
- Independent Python oracle: 1,000,000 deterministic cases, seed `0x4c554e5532563201`; path serialization, all guard equations, zero/tail payloads, and boundary payload lengths passed.
- C-to-Python event vectors: 256 consecutive events for each of NONE, PARITY, CRC16, CRC32, and ROLL64; zero mismatches.
- Host mutation campaign: 100,000 deterministic mutations per guard profile (12,500 each across payload bit, path/event bit, deletion, duplication, adjacent reorder, guard bit, and length corruption). CRC16, CRC32, and ROLL64 detected all cases in this corpus. PARITY detected each tested single-bit mutation and guard-bit mutation, about half of delete/duplicate/length cases, and none of the adjacent reorders. NONE detected no event-stream mutation through its guard. These results do not imply detection of arbitrary multi-bit faults.
- V2 QPC benchmarks at GCC `-O2` on the Windows host. Typical LUNA64/CRC16 path operations: child 9.8 ns, parent 14.2 ns, prefix 11.0 ns, neighbor 9.1 ns, distance 55.4 ns, serialize 53.6 ns, deserialize 56.4 ns. Results are per operation and include wrapper/output work.

| LUNA64 guard | step 0 B | 4 B | 16 B | 32 B | 64 B | 256 B | pre-tag |
|---|---:|---:|---:|---:|---:|---:|---:|
| NONE | 11.7 ns | 11.8 ns | 11.7 ns | 11.6 ns | 11.5 ns | 11.7 ns | 11.4 ns |
| PARITY | 27.4 ns | 33.7 ns | 107.4 ns | 169.3 ns | 292.6 ns | 1,080 ns | 53.6 ns |
| CRC16 | 28.7 ns | 30.4 ns | 71.8 ns | 123.4 ns | 226.1 ns | 839.1 ns | 42.4 ns |
| CRC32 | 78.7 ns | 89.3 ns | 247.5 ns | 413.4 ns | 745.5 ns | 2,723 ns | 138.1 ns |
| ROLL64 | 33.0 ns | 33.1 ns | 50.7 ns | 69.4 ns | 106.1 ns | 345.4 ns | 37.8 ns |

Clang was attempted but is blocked before project diagnostics: the installed Clang 22 Windows target cannot find system `string.h`/`assert.h`. ASan/UBSan were attempted but the GCC distribution lacks link libraries `-lasan` and `-lubsan`; neither is reported as PASS.

MinGW GCC `-Os` primitive object sizes (all production functions retained, `.data/.bss=0`): LUNA64/NONE `.text=1,920 B`; LUNA64/CRC16 and CRC32 `.text=2,176 B`; LUNA256/CRC32 `.text=2,240 B`; LUNA256/ROLL64 `.text=2,272 B`. MinGW emits additional unwind/read-only sections; these are host object measurements, not ESP32 flash figures.

## Fault and workflow evidence

The firmware workflow example records START → HEADER_OK → ERASE_OK → WRITE → VERIFY → COMMIT, with block number and caller CRC in the event payload. Under CRC16, the valid workflow passes; deleting or duplicating an event changes path state; reordering changes the guard; changing the block payload/tag changes the guard. A separate command/event example rejects adjacent command reordering. No authentication claim is made.

| Host mutation class | NONE | PARITY | CRC16 | CRC32 | ROLL64 |
|---|---:|---:|---:|---:|---:|
| payload bit | 0/12,500 | 12,500/12,500 | 12,500/12,500 | 12,500/12,500 | 12,500/12,500 |
| path bit | 0/12,500 | 12,500/12,500 | 12,500/12,500 | 12,500/12,500 | 12,500/12,500 |
| event bit | 0/12,500 | 12,500/12,500 | 12,500/12,500 | 12,500/12,500 | 12,500/12,500 |
| delete | 0/12,500 | 6,251/12,500 | 12,500/12,500 | 12,500/12,500 | 12,500/12,500 |
| duplicate | 0/12,500 | 6,286/12,500 | 12,500/12,500 | 12,500/12,500 | 12,500/12,500 |
| adjacent reorder | 0/12,500 | 0/12,500 | 12,500/12,500 | 12,500/12,500 | 12,500/12,500 |
| guard bit | 12,500/12,500 | 12,500/12,500 | 12,500/12,500 | 12,500/12,500 | 12,500/12,500 |
| length | 0/12,500 | 6,220/12,500 | 12,500/12,500 | 12,500/12,500 | 12,500/12,500 |

## ESP32-S3 campaign

- <PORT> read-only identification: ESP32-S3 QFN56, revision v0.2, 40 MHz crystal, 8 MB PSRAM, 16 MB flash (manufacturer `0x20`, device `0x4018`); esptool v4.10.dev2. Secure Boot and flash encryption were disabled.
- ESP-IDF 5.5.1, Xtensa GCC 14.2.0. Primary firmware used IDF size optimization (`-Os`); an isolated IDF performance build used `-O2`.
- Read partition table at `0x8000`: NVS `0x9000/0x6000`, PHY `0xf000/0x1000`, factory app `0x10000/0x100000`. IDF `app-flash` wrote only the app at `0x10000`; no bootloader, partition table, or NVS writes. All images fit the existing 1 MiB app slot.
- Hardware profiles: LUNA64/CRC16, LUNA64/CRC32, LUNA256/CRC32, and LUNA64/ROLL64. All passed focused tests and host-generated final vector guards. Each was reset and replayed 3 times with identical final serialized path, guard, checksum, and fault counters.
- Recommended LUNA64/CRC32 completed 1,000,000 valid operations and 10,000 deterministic fault mutations on each replay (3/3), with zero mismatches, no crash/watchdog reset, and 0-byte free-heap delta. Firmware emitted `LUNA_EMBEDDED_HW_PASS` each time.
- Optional CRC32 backend: IDF 5.5.1's documented `esp_rom_crc32_le` was compatible with CRC-32/ISO-HDLC using the documented complement adaptation for incremental raw-state updates. The portable CRC remains the default and canonical implementation; the ESP-only adapter is compiled only for CRC32 when `LUNA_USE_ESP_CRC32=ON`. ROM and portable results matched for empty, 4, 16, 32, 64, and 256-byte inputs; both passed the `123456789` known-answer test. This ROM-backed image also matched the host event-stream final guard and passed 3/3 deterministic replays.
- Direct CRC benchmark in the ROM-backed `-Os` firmware (cycles/call, same fixed input):

| bytes | portable C CRC32 | ESP ROM CRC32 |
|---:|---:|---:|
| 0 | 34 | 51 |
| 4 | 246 | 92 |
| 16 | 882 | 210 |
| 32 | 1,731 | 369 |
| 64 | 3,429 | 682 |
| 256 | 13,831 | 2,578 |

- The ROM-backed LUNA64/CRC32 step measured 749 cycles at 32 payload bytes and 534 cycles through the pre-tag API in this build, versus 5,421 cycles for V1 rolling at 32 bytes. This build used `-Os`, IDF 5.5.1, and a 160 MHz CPU. The direct ROM backend is materially faster beyond zero-length CRC input; zero-length API overhead remains lower in portable C.
- The ROM-backed final test firmware image was 195,136 B (complete IDF app and harness). The isolated Xtensa `esp_crc_backend.c` object contributed 27 B of `.text`, 4 B of literal data, and 0 B `.data/.bss`; it calls the ROM routine. This is separate from the portable-core object measurement above.
- Cycle counter: official IDF `esp_cpu_get_cycle_count()`, configured CPU frequency 160 MHz. Each number is cycles/op; step benchmark includes state initialization and result accumulation.

| `-Os` profile | child | parent | neighbor | step 0 B | 4 B | 16 B | 32 B | pre-tag |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| LUNA64/CRC16 | 76 | 92 | 68 | 763 | 1,128 | 2,221 | 3,678 | 1,313 |
| LUNA64/CRC32 | 76 | 92 | 68 | 647 | 944 | 1,833 | 3,018 | 1,095 |
| LUNA256/CRC32 | 182 | 254 | 170 | 862 | 1,165 | 2,066 | 3,267 | 1,317 |
| LUNA64/ROLL64 | 82 | 93 | 64 | 817 | 966 | 1,410 | 2,003 | 1,042 |

V1 rolling step on the same ESP32-S3 harness: 5,424 cycles for 32 B and 56 B state. Under `-Os`, LUNA64/CRC32 32 B step was 3,018 cycles (44% fewer); LUNA64/ROLL64 was 2,003 cycles (63% fewer). Under `-O2`, V1 step was 3,210 cycles; LUNA64/CRC16 was 3,664 cycles (14% more) and CRC32 was 2,925 cycles (9% fewer). At 160 MHz those are approximately 20.1 μs, 22.9 μs, and 18.3 μs respectively. This gives CRC16 a faster portable-host result but CRC32 the stronger/faster measured result on this target.

The `-Os` LUNA64/CRC32 test firmware binary (includes IDF and harness) was 194,720 B; the `-O2` binary was 205,168 B. ESP ELF sections for the complete `-Os` test firmware: `.flash.text=92,164 B`, `.flash.rodata=39,824 B`, `.iram0.text=50,715 B`, `.dram0.data=10,592 B`, `.dram0.bss=2,240 B`; these totals include ESP-IDF and the test harness, not just LUNA. The isolated ESP `lunu_embedded.c` object for LUNA64/CRC16 `-Os` measured `.text=1,951 B`, literal/rodata `140 B`, `.data=0`, `.bss=0` (2,091 B allocated code+literal total). V1 object in the same comparison firmware was 1,871 B `.text`.

## Recommendation and classification

- **Recommended ESP32-S3 profile:** LUNA64/CRC32, 16 B state, 64-decision capacity. It shares the LUNA64/CRC16 physical state size, has a 32-bit guard, passed the deterministic host and on-device campaigns, and was faster than both CRC16 and V1 rolling on the measured ESP32-S3.
- **Smallest useful option:** LUNA32/CRC16, 12 B state, for workflows capped at 32 decisions.
- **CRC16 versus CRC32:** CRC16 remains a sensible portability/host-performance choice. On this S3, CRC32 gave no extra state bytes at LUNA64 and ran faster; its wider residue makes it the measured default. The finite fault corpus does not establish arbitrary error-detection rates.
- **ROLL64:** retain only as a V1-style performance/reference comparison; its 64 logical bits cost more state and do not replace a standardized CRC definition.
- **Advantage over plain `(depth, bitstring)`:** path-only behavior remains exactly that capability. The added embedded value is the compact bounded API, fixed canonical path bytes, and optional ordered, payload-sensitive guard, demonstrated by the firmware workflow and command sequence.
- **Classification: READY / USEFUL.** All host and selected ESP32-S3 gates passed. No cryptographic authentication claim is made.
