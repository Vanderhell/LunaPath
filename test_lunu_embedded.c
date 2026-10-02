#include "lunu_embedded.h"
#include "lunu_primitives.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static uint32_t rng = UINT32_C(0x91e10da5);
static uint32_t next_random(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

static int path_differential(void) {
    for (unsigned depth = 0; depth <= LUNU_EMBED_PATH_BITS; ++depth) {
        lunu_embed_path ep; lunu_path vp; lunu_embed_path_zero(&ep); lunu_path_zero(&vp);
        for (unsigned i = 0; i < depth; ++i) {
            bool b = (next_random() & 1u) != 0u; lunu_embed_path en; lunu_path vn;
            CHECK(lunu_embed_child(&ep, b, &en)); CHECK(lunu_child(&vp, b, &vn)); ep = en; vp = vn;
            CHECK(lunu_embed_equal(&ep, &en));
        }
        uint8_t ewire[34], vwire[34]; size_t ew = 0, vw = 0;
        CHECK(lunu_embed_serialize(&ep, ewire, sizeof ewire, &ew));
        CHECK(lunu_serialize(&vp, vwire, sizeof vwire, &vw)); CHECK(ew == vw && memcmp(ewire, vwire, ew) == 0);
        lunu_embed_path decoded; CHECK(lunu_embed_deserialize(vwire, vw, &decoded)); CHECK(lunu_embed_equal(&ep, &decoded));
        for (unsigned k = 0; k <= depth; ++k) {
            lunu_embed_path a; lunu_path b; CHECK(lunu_embed_prefix(&ep, k, &a)); CHECK(lunu_prefix(&vp, k, &b));
            uint8_t aw[34], bw[34]; size_t an, bn; CHECK(lunu_embed_serialize(&a, aw, sizeof aw, &an)); CHECK(lunu_serialize(&b, bw, sizeof bw, &bn));
            CHECK(an == bn && memcmp(aw, bw, an) == 0);
        }
        if (depth != 0u) {
            unsigned dim = next_random() % depth; lunu_embed_path en; lunu_path vn;
            CHECK(lunu_embed_neighbor(&ep, dim, &en)); CHECK(lunu_neighbor(&vp, dim, &vn));
            uint8_t aw[34], bw[34]; size_t an, bn; CHECK(lunu_embed_serialize(&en, aw, sizeof aw, &an)); CHECK(lunu_serialize(&vn, bw, sizeof bw, &bn)); CHECK(an == bn && memcmp(aw, bw, an) == 0);
            unsigned d; bool valid; CHECK(lunu_embed_distance(&ep, &en, &d) && d == 1u); CHECK(lunu_distance(&vp, &vn, &valid) == 1u && valid);
        }
        if (depth != 0u) {
            lunu_embed_path ea; lunu_path va; CHECK(lunu_embed_parent(&ep, &ea)); CHECK(lunu_parent(&vp, &va));
            uint8_t aw[34], bw[34]; size_t an, bn; CHECK(lunu_embed_serialize(&ea, aw, sizeof aw, &an)); CHECK(lunu_serialize(&va, bw, sizeof bw, &bn)); CHECK(an == bn && memcmp(aw, bw, an) == 0);
        }
    }
    return 0;
}

static bool same_wire(const lunu_embed_path *ep, const lunu_path *vp) {
    uint8_t a[34], b[34]; size_t an, bn;
    return lunu_embed_serialize(ep, a, sizeof a, &an) &&
           lunu_serialize(vp, b, sizeof b, &bn) && an == bn && memcmp(a, b, an) == 0;
}

static int exhaustive_small_paths(void) {
    for (unsigned depth = 0; depth <= 20u; ++depth) {
        uint32_t count = UINT32_C(1) << depth;
        for (uint32_t value = 0; value < count; ++value) {
            lunu_embed_path ep; lunu_path vp; lunu_embed_path_zero(&ep); lunu_path_zero(&vp);
            for (unsigned i = 0; i < depth; ++i) {
                bool bit = ((value >> i) & 1u) != 0u; lunu_embed_path en; lunu_path vn;
                CHECK(lunu_embed_child(&ep, bit, &en)); CHECK(lunu_child(&vp, bit, &vn)); ep = en; vp = vn;
            }
            CHECK(same_wire(&ep, &vp));
            for (unsigned k = 0; k <= depth; ++k) {
                lunu_embed_path epre; lunu_path vpre;
                CHECK(lunu_embed_prefix(&ep, k, &epre)); CHECK(lunu_prefix(&vp, k, &vpre)); CHECK(same_wire(&epre, &vpre));
            }
            if (depth != 0u) {
                lunu_embed_path eparent; lunu_path vparent;
                CHECK(lunu_embed_parent(&ep, &eparent)); CHECK(lunu_parent(&vp, &vparent)); CHECK(same_wire(&eparent, &vparent));
                for (unsigned k = 0; k < depth; ++k) {
                    lunu_embed_path en, en2; lunu_path vn, vn2; unsigned ed; bool valid;
                    CHECK(lunu_embed_neighbor(&ep, k, &en)); CHECK(lunu_neighbor(&vp, k, &vn)); CHECK(same_wire(&en, &vn));
                    CHECK(lunu_embed_neighbor(&en, k, &en2)); CHECK(lunu_neighbor(&vn, k, &vn2)); CHECK(lunu_embed_equal(&en2, &ep) && lunu_equal(&vn2, &vp));
                    CHECK(lunu_embed_distance(&ep, &en, &ed) && ed == 1u); CHECK(lunu_distance(&vp, &vn, &valid) == 1u && valid);
                    lunu_embed_path ec; lunu_path vc; CHECK(lunu_embed_common_prefix(&ep, &en, &ec));
                    CHECK(lunu_common_prefix(&vp, &vn, &vc) == k && same_wire(&ec, &vc));
                }
            }
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    (void)argv;
    static const uint8_t kat[] = "123456789";
    static const uint8_t binary_kat[] = { 0x00u, 0x01u, 0x80u, 0xffu };
    CHECK(lunu_embed_crc16(NULL, 0u) == 0xffffu); CHECK(lunu_embed_crc16(kat, 9u) == 0x29b1u);
    CHECK(lunu_embed_crc16(binary_kat, sizeof binary_kat) == 0xb698u);
    CHECK(lunu_embed_crc32(NULL, 0u) == 0u); CHECK(lunu_embed_crc32(kat, 9u) == UINT32_C(0xcbf43926));
    CHECK(lunu_embed_crc32(binary_kat, sizeof binary_kat) == UINT32_C(0x3607c2ed));
    CHECK(path_differential() == 0);
    if (argc > 1) { CHECK(exhaustive_small_paths() == 0); puts("PASS: exhaustive all paths through depth 20"); }
    lunu_embed_state a, b, c; lunu_embed_state_zero(&a); b = a; c = a;
    CHECK(lunu_embed_step(&a, true, NULL, 0u)); CHECK(lunu_embed_step(&b, true, NULL, 0u)); CHECK(memcmp(&a, &b, sizeof a) == 0);
    uint8_t tail_a = 0x05u, tail_b = 0xfdu;
    CHECK(lunu_embed_step(&a, false, &tail_a, 3u)); CHECK(lunu_embed_step(&b, false, &tail_b, 3u)); CHECK(memcmp(&a, &b, sizeof a) == 0);
    lunu_embed_state null_zero = c, nonnull_zero = c;
    CHECK(lunu_embed_step(&null_zero, true, NULL, 0u)); CHECK(lunu_embed_step(&nonnull_zero, true, &tail_a, 0u));
    CHECK(memcmp(&null_zero, &nonnull_zero, sizeof null_zero) == 0);
    uint8_t largest[8192] = { 0u }; largest[8191] = 0x7fu;
    lunu_embed_state large_a, large_b; lunu_embed_state_zero(&large_a); lunu_embed_state_zero(&large_b);
    CHECK(lunu_embed_step(&large_a, true, largest, UINT16_MAX)); largest[8191] = 0xffu;
    CHECK(lunu_embed_step(&large_b, true, largest, UINT16_MAX)); CHECK(memcmp(&large_a, &large_b, sizeof large_a) == 0);
    lunu_embed_state expected_step = c; CHECK(lunu_embed_step(&expected_step, true, NULL, 0u));
    CHECK(lunu_embed_verify_step(&c, &expected_step, true, NULL, 0u));
    CHECK(!lunu_embed_verify_step(&c, &b, true, NULL, 0u));
#if LUNU_EMBED_GUARD != LUNU_EMBED_GUARD_NONE
    lunu_embed_state bad_guard = expected_step; bad_guard.guard ^= 1u;
    CHECK(!lunu_embed_verify_step(&c, &bad_guard, true, NULL, 0u));
#endif
    CHECK(lunu_embed_step_tag32(&c, false, UINT32_C(0x12345678), 1000u));
    lunu_embed_state before = c; CHECK(!lunu_embed_step(&c, true, NULL, 1u)); CHECK(memcmp(&before, &c, sizeof c) == 0);
    lunu_embed_path p; lunu_embed_path_zero(&p); p.depth = (uint16_t)LUNU_EMBED_PATH_BITS;
    lunu_embed_path unchanged = p; CHECK(!lunu_embed_child(&p, false, &p)); CHECK(memcmp(&p, &unchanged, sizeof p) == 0);
    CHECK(!lunu_embed_path_set(&p, LUNU_EMBED_PATH_BITS, true)); CHECK(!lunu_embed_path_set(&p, LUNU_EMBED_PATH_BITS + 1u, true));
    CHECK(!lunu_embed_path_get(&p, 0u, NULL)); CHECK(!lunu_embed_child(&p, true, NULL));
    lunu_embed_state full; lunu_embed_state_zero(&full); full.path = p; lunu_embed_state full_before = full;
    CHECK(!lunu_embed_step(&full, true, NULL, 0u)); CHECK(memcmp(&full, &full_before, sizeof full) == 0);
    CHECK(!lunu_embed_step(&full, true, NULL, 1u)); CHECK(memcmp(&full, &full_before, sizeof full) == 0);
    uint8_t malformed[36] = { 0u, 1u, 1u }; lunu_embed_path out; lunu_embed_path_zero(&out); out.depth = 2u; out.word[0] = 3u;
    lunu_embed_path out_before = out; CHECK(!lunu_embed_deserialize(malformed, 3u, &out)); CHECK(memcmp(&out, &out_before, sizeof out) == 0);
    CHECK(!lunu_embed_deserialize(malformed, 2u, &out)); CHECK(memcmp(&out, &out_before, sizeof out) == 0);
    malformed[2] = 0x80u; malformed[3] = 0u;
    CHECK(!lunu_embed_deserialize(malformed, 4u, &out)); CHECK(memcmp(&out, &out_before, sizeof out) == 0);
    malformed[2] = 1u;
    malformed[0] = (uint8_t)((LUNU_EMBED_PATH_BITS + 1u) >> 8); malformed[1] = (uint8_t)(LUNU_EMBED_PATH_BITS + 1u);
    CHECK(!lunu_embed_deserialize(malformed, 2u + (LUNU_EMBED_PATH_BITS + 8u) / 8u, &out)); CHECK(memcmp(&out, &out_before, sizeof out) == 0);
    size_t untouched_written = 123u; uint8_t untouched_wire[34]; memset(untouched_wire, 0x5a, sizeof untouched_wire);
    CHECK(!lunu_embed_serialize(&p, untouched_wire, 0u, &untouched_written));
    CHECK(untouched_written == 123u && untouched_wire[0] == 0x5au);
    puts("PASS: LUNA Embedded core, V1 path differential, canonical CRC, payload and transaction checks");
    return 0;
}
