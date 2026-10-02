import sys
from reference_lunu import control, fnv64

state = 0; depth = 0; data = bytearray(32)
for line in sys.stdin:
    i, got_depth, got, got_hash, got_input_hash = (int(x) for x in line.strip().split(','))
    data[i // 8] = (i * 37 + 11) & 255
    bit = (i * 13 & 1) != 0
    expected = control(state, int(bit), data, 256, i, 3)
    assert got_hash == fnv64(data), (i, got_hash, fnv64(data))
    raw = state.to_bytes(8, 'little') + int(bit).to_bytes(8, 'little') + (fnv64(data) ^ 256).to_bytes(8, 'little') + i.to_bytes(8, 'little')
    assert got_input_hash == fnv64(raw), (i, got_input_hash, fnv64(raw))
    assert got_depth == i + 1 and got == expected, (i, got_depth, got, expected)
    state = expected; depth += 1
print("PASS: 256 C control vectors verified by independent Python oracle")
