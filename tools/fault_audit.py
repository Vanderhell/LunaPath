# SPDX-License-Identifier: Apache-2.0
"""Fixed-seed deterministic mutation campaign for the LunaPath event equation."""
import random
from verify_vectors import advance, event

N_PER_CLASS = 12_500
CLASSES = ("payload_bit", "path_bit", "event_bit", "delete", "duplicate",
           "adjacent_reorder", "guard_bit", "length")

def audit(profile):
    rng = random.Random(0x4c554e5500000000 | profile)
    counts = {name: [0, 0] for name in CLASSES}
    for case in range(N_PER_CLASS * len(CLASSES)):
        category = CLASSES[case % len(CLASSES)]
        events = []
        for i in range(6):
            bits = 32
            data = bytearray(rng.randbytes(4))
            events.append([rng.randrange(2), bits, data])
        initial = 0xffff if profile == 2 else (0xffffffff if profile == 3 else 0)
        old = initial
        for i, (bit, bits, payload) in enumerate(events):
            old = advance(old, profile, event(i, bit, bits, payload))
        expected = old
        where = rng.randrange(len(events))
        mutated = [[b, n, bytearray(d)] for b, n, d in events]
        if category == "payload_bit":
            j = rng.randrange(32); mutated[where][2][j // 8] ^= 1 << (j & 7)
        elif category in ("path_bit", "event_bit"):
            mutated[where][0] ^= 1
        elif category == "delete":
            del mutated[where]
        elif category == "duplicate":
            mutated.insert(where, list(mutated[where]))
        elif category == "adjacent_reorder":
            where = min(where, len(mutated) - 2)
            mutated[where], mutated[where + 1] = mutated[where + 1], mutated[where]
        elif category == "length":
            mutated[where][1] -= 1
        if category == "guard_bit":
            altered = expected ^ 1
        else:
            altered = initial
            for i, (bit, bits, payload) in enumerate(mutated):
                altered = advance(altered, profile, event(i, bit, bits, payload))
        counts[category][1] += 1
        counts[category][0] += altered != expected
    return counts

if __name__ == "__main__":
    for profile, name in enumerate(("NONE", "PARITY", "CRC16", "CRC32", "ROLL64")):
        print(name)
        for category, (detected, total) in audit(profile).items():
            print(f"  {category}: {detected}/{total}")
