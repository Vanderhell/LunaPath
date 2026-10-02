"""Independent Python oracle for the public Lunu contract (Python 3)."""
from dataclasses import dataclass
import random

MASK = (1 << 64) - 1
def fnv64(data):
    h = 1469598103934665603
    for byte in data:
        h = ((h ^ byte) * 1099511628211) & MASK
    return h

@dataclass(frozen=True)
class Path:
    bits: tuple
    @property
    def depth(self): return len(self.bits)
    def child(self, bit): return Path(self.bits + (int(bool(bit)),))
    def parent(self):
        if not self.bits: raise ValueError("root")
        return Path(self.bits[:-1])
    def prefix(self, n):
        if not 0 <= n <= self.depth: raise ValueError("depth")
        return Path(self.bits[:n])
    def neighbor(self, n):
        if not 0 <= n < self.depth: raise ValueError("dimension")
        b = list(self.bits); b[n] ^= 1; return Path(tuple(b))
    def distance(self, other):
        if self.depth != other.depth: raise ValueError("depth")
        return sum(x != y for x, y in zip(self.bits, other.bits))
    def serialize(self):
        out = bytearray((self.depth >> 8, self.depth & 255))
        for start in range(0, self.depth, 8):
            v = 0; chunk = self.bits[start:start + 8]
            for bit in chunk: v = (v << 1) | bit
            out.append(v << (8 - len(chunk)))
        return bytes(out)
    @staticmethod
    def deserialize(raw):
        if len(raw) < 2: raise ValueError("short")
        depth = raw[0] << 8 | raw[1]
        if depth > 256 or len(raw) != 2 + (depth + 7) // 8: raise ValueError("size")
        if depth % 8 and raw[-1] & ((1 << (8 - depth % 8)) - 1): raise ValueError("padding")
        return Path(tuple((raw[2 + i // 8] >> (7 - i % 8)) & 1 for i in range(depth)))

def parity(data, bits):
    return sum((data[i // 8] >> (i % 8)) & 1 for i in range(bits)) & 1
def digest(data, bits): return fnv64(data[:(bits + 7) // 8]) ^ bits
def control(old, bit, data, bits, step, mode):
    if mode == 0: return 0
    p = parity(data, bits)
    if mode == 1: return old ^ bit ^ p
    if mode == 2: return old ^ bit ^ p ^ ((step * 0x9e3779b97f4a7c15) & MASK)
    raw = old.to_bytes(8, "little") + int(bit).to_bytes(8, "little") + digest(data, bits).to_bytes(8, "little") + step.to_bytes(8, "little")
    return ((fnv64(raw) << 1) | (fnv64(raw) >> 63)) & MASK

def step(path, state, bit, data, bits, mode):
    return path.child(bit), control(state, bit, data, bits, path.depth, mode)

def run():
    random.seed(0x4C554E55)
    for depth in range(21):
        p = Path(tuple((depth + i) & 1 for i in range(depth)))
        assert Path.deserialize(p.serialize()) == p
        for k in range(depth + 1): assert p.prefix(k).bits == p.bits[:k]
        for k in range(depth): assert p.distance(p.neighbor(k)) == 1
    for _ in range(1_000_000):
        depth = random.randrange(257)
        p = Path(()) if depth == 0 else Path(tuple(int(ch) for ch in f'{random.getrandbits(depth):0{depth}b}'))
        assert Path.deserialize(p.serialize()) == p
        data = random.randbytes(32); bit = random.randrange(2); mode = random.randrange(4)
        q, c = step(p, 123, bit, data, 256, mode)
        assert q == p.child(bit) and c == control(123, bit, data, 256, depth, mode)
    print("PASS: independent oracle boundary sweep and 1,000,000 properties; seed=0x4C554E55")

if __name__ == "__main__": run()
