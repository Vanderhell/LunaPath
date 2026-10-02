#include "lunu_primitives.h"
#include <string.h>

static uint64_t mask_for(unsigned n) { return UINT64_C(1) << (n & 63u); }
static unsigned pop64(uint64_t x) {
    unsigned n = 0; while (x) { x &= x - 1u; ++n; } return n;
}
static void trim(lunu_path *p) {
    unsigned w = (p->depth + 63u) / 64u;
    unsigned rem = p->depth & 63u;
    if (w == 0) { memset(p->word, 0, sizeof p->word); return; }
    if (rem) p->word[w - 1u] &= (mask_for(rem) - 1u);
    for (unsigned i = w; i < LUNU_WORDS; ++i) p->word[i] = 0;
}
void lunu_path_zero(lunu_path *p) { memset(p, 0, sizeof *p); }
bool lunu_path_get(const lunu_path *p, unsigned i) {
    return i < p->depth && (p->word[i / 64u] & mask_for(i)) != 0;
}
bool lunu_path_set(lunu_path *p, unsigned i, bool bit) {
    /* Public setter is intentionally not a builder: only existing bits may change. */
    if (i >= p->depth || i >= LUNU_MAX_BITS) return false;
    if (bit) p->word[i / 64u] |= mask_for(i); else p->word[i / 64u] &= ~mask_for(i);
    return true;
}
bool lunu_child(const lunu_path *p, bool bit, lunu_path *out) {
    if (p->depth >= LUNU_MAX_BITS) return false;
    *out = *p; if (bit) out->word[out->depth / 64u] |= mask_for(out->depth);
    else out->word[out->depth / 64u] &= ~mask_for(out->depth);
    ++out->depth; return true;
}
bool lunu_parent(const lunu_path *p, lunu_path *out) {
    if (p->depth == 0) return false;
    *out = *p; --out->depth; trim(out); return true;
}
bool lunu_prefix(const lunu_path *p, unsigned depth, lunu_path *out) {
    if (depth > p->depth || depth > LUNU_MAX_BITS) return false;
    *out = *p; out->depth = (uint16_t)depth; trim(out); return true;
}
unsigned lunu_common_prefix(const lunu_path *a, const lunu_path *b, lunu_path *out) {
    unsigned n = a->depth < b->depth ? a->depth : b->depth, k = 0;
    while (k < n && lunu_path_get(a, k) == lunu_path_get(b, k)) ++k;
    if (out) (void)lunu_prefix(a, k, out);
    return k;
}
bool lunu_neighbor(const lunu_path *p, unsigned d, lunu_path *out) {
    if (d >= p->depth) return false;
    *out = *p; out->word[d / 64u] ^= mask_for(d); return true;
}
unsigned lunu_distance(const lunu_path *a, const lunu_path *b, bool *valid) {
    if (valid) *valid = a->depth == b->depth;
    if (a->depth != b->depth) return 0;
    unsigned total = 0, words = (a->depth + 63u) / 64u;
    for (unsigned i = 0; i < words; ++i) total += pop64(a->word[i] ^ b->word[i]);
    if (a->depth & 63u) total -= pop64((a->word[words - 1u] ^ b->word[words - 1u]) & ~((mask_for(a->depth & 63u)) - 1u));
    return total;
}
bool lunu_equal(const lunu_path *a, const lunu_path *b) {
    if (a->depth != b->depth) return false;
    for (unsigned i = 0; i < a->depth; ++i) if (lunu_path_get(a, i) != lunu_path_get(b, i)) return false;
    return true;
}
uint64_t lunu_hash64(const void *data, size_t size) {
    const uint8_t *p = (const uint8_t *)data; uint64_t h = UINT64_C(1469598103934665603);
    while (size--) { h ^= *p++; h *= UINT64_C(1099511628211); } return h;
}
static uint64_t rol1(uint64_t x) { return (x << 1) | (x >> 63); }
bool lunu_step(lunu_state *s, bool bit, uint64_t data_digest) {
    return lunu_step_payload(s, bit, &data_digest, sizeof data_digest * 8u,
                             LUNU_CONTROL_ROLLING_HASH);
}
bool lunu_verify_step(const lunu_state *before, const lunu_state *after, bool bit, uint64_t data_digest) {
    return lunu_verify_step_payload(before, after, bit, &data_digest,
                                    sizeof data_digest * 8u,
                                    LUNU_CONTROL_ROLLING_HASH);
}
static unsigned payload_parity(const uint8_t *p, size_t bits) {
    unsigned parity = 0; for (size_t i = 0; i < bits; ++i) parity ^= (p[i / 8u] >> (i % 8u)) & 1u; return parity;
}
static uint64_t payload_digest(const uint8_t *p, size_t bits) {
    return lunu_hash64(p, (bits + 7u) / 8u) ^ (uint64_t)bits;
}
static uint64_t control_next(uint64_t old, bool bit, const uint8_t *p, size_t bits,
                             uint64_t step, lunu_control_mode mode) {
    unsigned parity = payload_parity(p, bits);
    if (mode == LUNU_CONTROL_NONE) return 0;
    if (mode == LUNU_CONTROL_PARITY) return old ^ (uint64_t)bit ^ parity;
    if (mode == LUNU_CONTROL_PARITY_SEQUENCE) return old ^ (uint64_t)bit ^ parity ^ (step * UINT64_C(0x9e3779b97f4a7c15));
    uint64_t input[4] = {old, (uint64_t)bit, payload_digest(p, bits), step};
    return rol1(lunu_hash64(input, sizeof input));
}
bool lunu_step_payload(lunu_state *s, bool bit, const void *payload, size_t payload_bits,
                       lunu_control_mode mode) {
    if (!payload || payload_bits == 0 || payload_bits > 256 || (payload_bits + 7u) / 8u > 32u) return false;
    lunu_path next; if (!lunu_child(&s->path, bit, &next)) return false;
    s->path = next; s->control = control_next(s->control, bit, (const uint8_t *)payload,
                                               payload_bits, s->steps, mode); ++s->steps; return true;
}
bool lunu_verify_step_payload(const lunu_state *before, const lunu_state *after, bool bit,
                              const void *payload, size_t payload_bits, lunu_control_mode mode) {
    if (!before || !after || !payload || payload_bits == 0 || payload_bits > 256) return false;
    if (after->path.depth != before->path.depth + 1u || after->steps != before->steps + 1u) return false;
    if (after->path.depth > LUNU_MAX_BITS) return false;
    for (unsigned i = 0; i < before->path.depth; ++i) if (lunu_path_get(&before->path, i) != lunu_path_get(&after->path, i)) return false;
    if (lunu_path_get(&after->path, before->path.depth) != bit) return false;
    uint64_t expected = control_next(before->control, bit, (const uint8_t *)payload,
                                     payload_bits, before->steps, mode);
    return expected == after->control;
}
size_t lunu_serialized_size(const lunu_path *p) { return 2u + (p->depth + 7u) / 8u; }
bool lunu_serialize(const lunu_path *p, uint8_t *out, size_t cap, size_t *written) {
    size_t need = lunu_serialized_size(p); if (cap < need || p->depth > LUNU_MAX_BITS) return false;
    out[0] = (uint8_t)(p->depth >> 8); out[1] = (uint8_t)p->depth;
    memset(out + 2, 0, need - 2); for (unsigned i = 0; i < p->depth; ++i)
        if (lunu_path_get(p, i)) out[2 + i / 8u] |= (uint8_t)(1u << (7u - i % 8u));
    if (written) *written = need;
    return true;
}
bool lunu_deserialize(const uint8_t *in, size_t size, lunu_path *out) {
    if (size < 2) return false;
    unsigned d = ((unsigned)in[0] << 8) | in[1];
    if (d > LUNU_MAX_BITS || size != 2u + (d + 7u) / 8u) return false;
    lunu_path_zero(out); out->depth = (uint16_t)d;
    for (unsigned i = 0; i < d; ++i) if (in[2 + i / 8u] & (1u << (7u - i % 8u))) (void)lunu_path_set(out, i, true);
    if (d % 8u && (in[size - 1] & ((1u << (8u - d % 8u)) - 1u))) return false;
    return true;
}
