# LunaPath v0.1 wire format

This document describes the existing, stable v0.1 encodings. The path format and the event byte stream are separate formats.

**Raw `lunapath_path` and `lunapath_state` C struct layouts are not wire formats.** Do not persist or transmit their native memory representation.

## Path serialization

A serialized path is:

```text
depth:uint16 big-endian | ceil(depth / 8) path bytes
```

Depth counts decisions from the root. The root path has depth zero and serializes as `00 00` with no path bytes. For a build with `LUNAPATH_PATH_BITS=256`, the maximum depth is 256 and its depth field is `01 00`; other builds accept depths only up to their configured capacity.

Decision zero is the most-significant bit of the first path byte. In general, decision `i` is stored in bit `7 - (i % 8)` of byte `2 + floor(i / 8)`. Any unused low bits in the final path byte must be zero.

Deserialization rejects input when the depth exceeds the configured capacity, the byte count is not exactly `2 + ceil(depth / 8)`, or unused final-byte bits are nonzero. On failure the destination path is unchanged. The tested root encoding is `00 00`; a tested depth-nine path with decisions `100000001` encodes as `00 09 80 80`.

## Guarded event encoding

Every guarded step folds this byte sequence into the selected guard:

```text
domain:uint8 | format:uint8 | depth_before:uint16 big-endian |
decision:uint8 | payload_bits:uint16 big-endian | payload bytes
```

The domain byte is `0x4C` (ASCII `L`) and the format/version byte is `0x02`. `depth_before` is the path depth before the step appends its decision. The decision byte is exactly `0x00` or `0x01`. The payload length is measured in bits; the payload byte count is `ceil(payload_bits / 8)`.

The logical payload bit `i` is bit `i % 8` of byte `floor(i / 8)`, so payload bits are consumed least-significant bit first within each byte. If the payload length is not byte-aligned, unused high bits in the last payload byte are ignored (equivalent to masking them to zero). A zero-bit payload contributes no payload bytes and accepts `NULL`. A nonzero payload length requires a non-NULL pointer.

`LUNAPATH_GUARD_NONE` does not maintain a guard. Other guard modes consume the event encoding above. `lunapath_state_zero()` initializes CRC16 to `0xFFFF` and CRC32 to `0xFFFFFFFF`; later events continue from the current guard state.

## CRC definitions

CRC16 is CRC-16/CCITT-FALSE:

| Parameter | Value |
|---|---:|
| Polynomial | `0x1021` |
| Init | `0xFFFF` |
| RefIn | false |
| RefOut | false |
| XorOut | `0x0000` |

Known-answer test: ASCII `123456789` produces `0x29B1`.

CRC32 is CRC-32/ISO-HDLC:

| Parameter | Value |
|---|---:|
| Polynomial | `0x04C11DB7` |
| Reflected implementation polynomial | `0xEDB88320` |
| Init | `0xFFFFFFFF` |
| RefIn | true |
| RefOut | true |
| XorOut | `0xFFFFFFFF` |

Known-answer test: ASCII `123456789` produces `0xCBF43926`.

## `lunapath_step_tag32`

The tagged step supplies a fixed 48-bit effective payload to the event encoding:

```text
tag:uint32 little-endian | original_payload_bits:uint16 little-endian
```

Thus the event header records `payload_bits=48`, followed by the six bytes above. For example, the tag value `0x12345678` is encoded `78 56 34 12`; the original payload bit count follows as two little-endian bytes. The original payload itself is not read by this API. The guard is only as informative as the caller-supplied tag.
