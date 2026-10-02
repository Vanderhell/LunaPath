"""Independent byte-oriented oracle for LUNA Embedded events and profiles."""
import random
import sys

MASK64 = (1 << 64) - 1

def crc16(data):
    c = 0xffff
    for b in data:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ (0x1021 if c & 0x8000 else 0)) & 0xffff
    return c

def crc32(data):
    c = 0xffffffff
    for b in data:
        c ^= b
        for _ in range(8):
            c = (c >> 1) ^ (0xedb88320 if c & 1 else 0)
    return c ^ 0xffffffff

def event(depth, bit, bits, payload):
    # Payload bits are numbered LSB first within each byte; unused high tail bits are zeroed.
    raw = bytearray((0x4c, 2, depth >> 8, depth & 255, int(bit), bits >> 8, bits & 255))
    n = (bits + 7) // 8
    raw.extend(payload[:n])
    if n and bits & 7:
        raw[-1] &= (1 << (bits & 7)) - 1
    return bytes(raw)

def advance(old, profile, raw):
    if profile == 0:
        return 0
    if profile == 1:
        return old ^ (sum(b.bit_count() for b in raw) & 1)
    if profile == 2:
        return crc16_from(old, raw)
    if profile == 3:
        return crc32_from(old, raw)
    h = 1469598103934665603
    for b in old.to_bytes(8, "little") + raw:
        h = ((h ^ b) * 1099511628211) & MASK64
    return ((h << 1) | (h >> 63)) & MASK64

def crc16_from(old, raw):
    c = old
    for b in raw:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ (0x1021 if c & 0x8000 else 0)) & 0xffff
    return c

def crc32_from(old, raw):
    c = old ^ 0xffffffff
    for b in raw:
        c ^= b
        for _ in range(8):
            c = (c >> 1) ^ (0xedb88320 if c & 1 else 0)
    return c ^ 0xffffffff

def vectors(path, profile):
    old = 0xffff if profile == 2 else (0xffffffff if profile == 3 else 0)
    rows = open(path, encoding="ascii").read().splitlines()
    assert len(rows) == 256, len(rows)
    for depth, line in enumerate(rows):
        i, bit, bits, hx, before, after = line.split(",")
        assert int(i) == depth
        payload = bytes.fromhex(hx)
        assert int(before, 16) == old
        new = advance(old, profile, event(depth, int(bit), int(bits), payload))
        assert int(after, 16) == new, (depth, after, hex(new))
        old = new

def expected_vector(profile, count):
    old = 0xffff if profile == 2 else (0xffffffff if profile == 3 else 0)
    for n in range(count):
        bits = (n * 13) % 41
        payload = bytes((n * 37 + i * 61) & 255 for i in range((bits + 7) // 8))
        old = advance(old, profile, event(n, n & 1, bits, payload))
    return old

def props(count):
    rng = random.Random(0x4c554e5532563201)
    for n in range(count):
        capacity = (32, 64, 128, 256)[n & 3]
        depth = rng.randrange(capacity + 1)
        value = rng.getrandbits(depth)
        raw = bytearray((depth >> 8, depth & 255)) + bytearray((depth + 7) // 8)
        for bit_index in range(depth):
            if value & (1 << bit_index):
                raw[2 + bit_index // 8] |= 1 << (7 - bit_index % 8)
        back = 0
        for bit_index in range(depth):
            if raw[2 + bit_index // 8] & (1 << (7 - bit_index % 8)):
                back |= 1 << bit_index
        assert len(raw) == 2 + (depth + 7) // 8 and back == value
        payload_bits = (0, 1, 3, 7, 8, 9, 31, 32, 33, 255, 256, 4095, 65535)[n % 13]
        plen = (payload_bits + 7) // 8
        payload = bytearray(rng.randbytes(plen))
        changed = bytearray(payload)
        if plen and payload_bits & 7:
            changed[-1] ^= ((1 << (8 - (payload_bits & 7))) - 1) << (payload_bits & 7)
        assert event(depth, n & 1, payload_bits, payload) == event(depth, n & 1, payload_bits, changed)
        # Exercise every guard equation incrementally on bounded event payloads.
        guard_bits = (0, 1, 3, 7, 8, 9, 31, 32, 33, 255, 256)[n % 11]
        guard_data = bytearray(rng.randbytes((guard_bits + 7) // 8))
        guard_tail_variant = bytearray(guard_data)
        if guard_data and guard_bits & 7:
            guard_tail_variant[-1] |= ((1 << (8 - (guard_bits & 7))) - 1) << (guard_bits & 7)
        mode = n % 5
        initial = 0xffff if mode == 2 else (0xffffffff if mode == 3 else (rng.getrandbits(64) if mode == 4 else rng.randrange(2)))
        canonical = event(depth, n & 1, guard_bits, guard_data)
        updated = advance(initial, mode, canonical)
        assert updated == advance(initial, mode, event(depth, n & 1, guard_bits, guard_tail_variant))
        width = (0, 1, 16, 32, 64)[mode]
        assert 0 <= updated < (1 << width) if width else updated == 0
        # Fixed endian canonical encoding and boundary arithmetic are checked on every case.
        assert int.from_bytes(raw[:2], "big") == depth

if __name__ == "__main__":
    assert crc16(b"123456789") == 0x29b1
    assert crc32(b"123456789") == 0xcbf43926
    if len(sys.argv) == 4 and sys.argv[1] == "vectors":
        vectors(sys.argv[2], int(sys.argv[3])); print("PASS: 256 independent event vectors")
    elif len(sys.argv) == 3 and sys.argv[1] == "properties":
        props(int(sys.argv[2])); print(f"PASS: {sys.argv[2]} deterministic Python properties; seed=0x4c554e5532563201")
    elif len(sys.argv) == 2 and sys.argv[1] == "hwconstants":
        for profile in range(5):
            print(f"guard={profile} events64=0x{expected_vector(profile, 64):x} events256=0x{expected_vector(profile, 256):x}")
    else:
        raise SystemExit("usage: verify_embedded_vectors.py vectors FILE GUARD | properties COUNT")
