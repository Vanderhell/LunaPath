# LunaPath v0.1 validation record

This report preserves measurements from the completed LunaPath v2 host and ESP32-S3 campaign. Hardware was an ESP32-S3 QFN56 revision v0.2 with 40 MHz crystal, 8 MB PSRAM, and 16 MB flash. The tested IDF was 5.5.1 with Xtensa GCC 14.2.0. No serial-port name or device MAC is recorded here.

## API and formats

The selected path capacity is compile-time: 32, 64, 128, or 256 decisions. The guard is compile-time: NONE, PARITY, CRC16, CRC32, or ROLL64 reference. State has no runtime mode or separate step counter: each successful event appends one path bit, so the prior path depth is the event position.

Guarded events are encoded as:

```text
4c 02 | depth_before:uint16 big endian | bit:00/01 |
payload_bits:uint16 big endian | ceil(payload_bits/8) payload bytes
```

Payload bit `i` is bit `i % 8` (least-significant bit first) in byte `i / 8`; unused high bits in the final byte are ignored. Zero-bit payloads accept `NULL`. Raw payload length is limited to 65,535 bits and streamed without storage. The pre-tag API guards a caller tag encoded LE32 followed by original length LE16; integrity of original data is only as strong as that tag.

CRC16 is CRC-16/CCITT-FALSE: polynomial `0x1021`, init `0xffff`, no reflection, xorout `0`; KAT `123456789 -> 0x29b1`. CRC32 is CRC-32/ISO-HDLC: polynomial `0x04c11db7`, reflected implementation polynomial `0xedb88320`, init/xorout `0xffffffff`; KAT `123456789 -> 0xcbf43926`. Empty and binary KATs also passed.

The path wire format is a 16-bit big-endian depth followed by root-first path bits, most-significant bit first in each byte, with zero tail padding. It is V1-compatible for shared capacities. Deserialization rejects paths beyond the selected capacity and leaves output unchanged on failure. Raw C structs are not serialized and their layout is not a persistence or stable ABI contract.

## Memory matrix

Host `sizeof` measurements with x64 GCC; state padding is natural alignment.

| Profile | Path | Guard bits | State | Reduction vs V1 state |
|---|---:|---:|---:|---:|
| V1 reference | 40 B | 64 control + 64 steps | 56 B | baseline |
| LunaPath32/NONE | 8 B | 0 | 8 B | 86% |
| LunaPath32/CRC16 | 8 B | 16 | 12 B | 79% |
| LunaPath64/NONE | 12 B | 0 | 12 B | 79% |
| LunaPath64/CRC16 | 12 B | 16 | 16 B | 71% |
| LunaPath64/CRC32 | 12 B | 32 | 16 B | 71% |
| LunaPath64/ROLL64 | 12 B | 64 | 24 B | 57% |
| LunaPath128/CRC32 | 20 B | 32 | 24 B | 57% |
| LunaPath256/CRC16 | 36 B | 16 | 40 B | 29% |
| LunaPath256/CRC32 | 36 B | 32 | 40 B | 29% |
| LunaPath256/ROLL64 | 36 B | 64 | 48 B | 14% |

The complete profile matrix is in [`results.csv`](results.csv).

## Host validation

- MinGW GCC 16.1.0 strict C17 Debug (`-O0 -g`) and Release (`-O2`), `-Wall -Wextra -Wpedantic -Werror`: all 20 profile/guard builds and tests passed in each mode.
- Exhaustive V1 differential covered all 2,097,151 paths through depths 0–20 at each of four capacities: 8,388,604 path cases, zero mismatches. Checks include prefixes, parents, neighbors and involution, distance, common prefix, equality, and serialization.
- Boundary/misuse coverage includes depth 256, max+1 rejection, invalid setters, NULL handling, 65,535-bit payload, zero payload, tail masking, malformed/truncated/extra serialization, guard mismatch, and transactional failures.
- Independent Python oracle: 1,000,000 deterministic cases, seed `0x4c554e5532563201`, covering path serialization, guard updates, tail masking, zero payload, and boundary lengths.
- C-to-Python vectors: 256 consecutive events for each of NONE, PARITY, CRC16, CRC32, and ROLL64; zero mismatches.
- Fault campaign: 100,000 deterministic mutations per guard profile, 12,500 in each category. CRC16, CRC32, and ROLL64 detected every case in this corpus. PARITY detected all tested individual bit changes and guard-bit changes, about half of deletions, duplicates, and length changes, and none of adjacent reorders. NONE detects no event-stream mutation through a guard. This finite corpus is not a guarantee for arbitrary multi-bit faults.
- Windows host QPC benchmarks at GCC `-O2`, LUNAPATH64/CRC16: child 9.8 ns, parent 14.2 ns, prefix 11.0 ns, neighbor 9.1 ns, distance 55.4 ns, serialize 53.6 ns, deserialize 56.4 ns. These include wrapper/output work.

| LunaPath64 guard | 0 B | 4 B | 16 B | 32 B | 64 B | 256 B | pre-tag |
|---|---:|---:|---:|---:|---:|---:|---:|
| NONE | 11.7 ns | 11.8 ns | 11.7 ns | 11.6 ns | 11.5 ns | 11.7 ns | 11.4 ns |
| PARITY | 27.4 ns | 33.7 ns | 107.4 ns | 169.3 ns | 292.6 ns | 1,080 ns | 53.6 ns |
| CRC16 | 28.7 ns | 30.4 ns | 71.8 ns | 123.4 ns | 226.1 ns | 839.1 ns | 42.4 ns |
| CRC32 | 78.7 ns | 89.3 ns | 247.5 ns | 413.4 ns | 745.5 ns | 2,723 ns | 138.1 ns |
| ROLL64 | 33.0 ns | 33.1 ns | 50.7 ns | 69.4 ns | 106.1 ns | 345.4 ns | 37.8 ns |

Clang on the Windows host was blocked during CMake's compiler link check because the installed toolchain could not locate `oldnames.lib` and `msvcrtd.lib`. This occurs before project diagnostics and is not counted as PASS. Ubuntu GCC and Clang builds are configured in CI but have not run as hosted jobs from this local checkout. ASan/UBSan were unavailable because the installed GCC distribution lacked `-lasan` and `-lubsan`. Neither is counted as PASS.

MinGW GCC `-Os` primitive object sizes, with production functions retained and `.data/.bss=0`: LunaPath64/NONE `.text=1,920 B`; LunaPath64/CRC16 and CRC32 `.text=2,176 B`; LunaPath256/CRC32 `.text=2,240 B`; LunaPath256/ROLL64 `.text=2,272 B`. These are host object figures, not target flash estimates.

## Workflow example

The tested firmware update workflow records START, HEADER_OK, ERASE_OK, WRITE, VERIFY, and COMMIT with block index and caller CRC. The valid sequence passes; deleted or duplicated events change the path, and reordering or changed payload data changes the guard. A command/event example also detects an adjacent command reorder. Guards are corruption checks, not authentication.

## ESP32-S3 validation

- IDF 5.5.1, Xtensa GCC 14.2.0; primary build used `-Os`, with an isolated `-O2` comparison.
- Application-only flashing targeted the existing factory application partition; bootloader, partition table, NVS, eFuses, and security settings were not written.
- Hardware profiles LUNAPATH64/CRC16, LUNAPATH64/CRC32, LUNAPATH256/CRC32, and LUNAPATH64/ROLL64 passed focused tests and host-generated final vectors. Each profile replayed three times with identical path, guard, checksum, and fault summary.
- LUNAPATH64/CRC32 completed 1,000,000 valid operations and 10,000 deterministic fault mutations per replay (3/3), with zero mismatches, no crash/watchdog reset, and zero free-heap delta.
- Official IDF CPU cycle counter; CPU frequency 160 MHz. Step measurements include state initialization and result accumulation.

| `-Os` profile | Child | Parent | Neighbor | Step 0 B | 4 B | 16 B | 32 B | Pre-tag |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| LUNAPATH64/CRC16 | 76 | 92 | 68 | 763 | 1,128 | 2,221 | 3,678 | 1,313 |
| LUNAPATH64/CRC32 portable | 76 | 92 | 68 | 647 | 944 | 1,833 | 3,018 | 1,095 |
| LUNAPATH256/CRC32 | 182 | 254 | 170 | 862 | 1,165 | 2,066 | 3,267 | 1,317 |
| LUNAPATH64/ROLL64 | 82 | 93 | 64 | 817 | 966 | 1,410 | 2,003 | 1,042 |

V1 rolling step measured 5,424 cycles for a 32-byte payload and used 56 B of state. Portable LUNAPATH64/CRC32 measured 3,018 cycles. In the final ROM-backed `-Os` image, LUNAPATH64/CRC32 measured 749 cycles for a 32-byte event and 534 cycles for pre-tagged input; V1 rolling measured 5,421 cycles in that same harness.

The ESP-IDF ROM CRC32 API was verified against the portable CRC-32/ISO-HDLC implementation for empty, 4, 16, 32, 64, and 256-byte inputs and the `123456789` KAT. Direct fixed-input costs (cycles/call) were:

| Bytes | Portable C | ESP ROM |
|---:|---:|---:|
| 0 | 34 | 51 |
| 4 | 246 | 92 |
| 16 | 882 | 210 |
| 32 | 1,731 | 369 |
| 64 | 3,429 | 682 |
| 256 | 13,831 | 2,578 |

The final ROM-backed test firmware image was 195,136 B (complete IDF app and harness). The isolated Xtensa LUNAPATH64/CRC32 core object measured `.text=2,019 B`, literals `144 B`, `.data/.bss=0`; its ROM adapter added `.text=27 B`, literals `4 B`, `.data/.bss=0`. Earlier complete firmware sections for the portable `-Os` test image were `.flash.text=92,164 B`, `.flash.rodata=39,824 B`, `.iram0.text=50,715 B`, `.dram0.data=10,592 B`, and `.dram0.bss=2,240 B`; these include ESP-IDF and the test harness, not just LunaPath. The isolated portable LUNAPATH64/CRC16 object measured `.text=1,951 B`, literals `140 B`, and `.data/.bss=0`.

## Recommendation

For the tested ESP32-S3, LUNAPATH64/CRC32 with the optional ROM backend had the best measured state/performance balance: 16 B state, 64-decision capacity, and 749 cycles for a 32-byte event. LUNAPATH32/CRC16 is the smallest useful portable profile at 12 B when 32 decisions suffice. CRC16 remains reasonable where portable implementation simplicity is preferred. At 64-bit path capacity CRC32 adds no state bytes over CRC16; the optional hardware backend also measured faster on this ESP32-S3. ROLL64 remains a comparison/reference mode. No CRC or rolling mode authenticates an adversary.
