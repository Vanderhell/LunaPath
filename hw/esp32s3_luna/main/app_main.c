#include "lunu_embedded.h"
#include "lunu_primitives.h"

#include "esp_cpu.h"
#include "esp_heap_caps.h"
#include "esp_rom_crc.h"
#include "esp_system.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define FAIL(stage) do { printf("LUNA_EMBEDDED_HW_FAIL %s\n", stage); return; } while (0)

typedef struct { bool bit; uint16_t bits; uint8_t payload[8]; } hw_event;
static volatile uint32_t hw_sink;

static bool same_guard_path(const lunu_embed_state *a, const lunu_embed_state *b) {
    if (!lunu_embed_equal(&a->path, &b->path)) return false;
#if LUNU_EMBED_GUARD != LUNU_EMBED_GUARD_NONE
    return a->guard == b->guard;
#else
    return true;
#endif
}

static bool focused_tests(void) {
    lunu_embed_path root, child, parent, prefix, neighbor, roundtrip;
    lunu_embed_path_zero(&root);
    if (root.depth != 0u || !lunu_embed_child(&root, true, &child) || child.depth != 1u) return false;
    if (!lunu_embed_parent(&child, &parent) || !lunu_embed_equal(&root, &parent)) return false;
    for (unsigned i = 0; i < LUNU_EMBED_PATH_BITS; ++i) {
        lunu_embed_path next;
        if (!lunu_embed_child(&root, (i & 1u) != 0u, &next)) return false;
        root = next;
    }
    lunu_embed_path unchanged = root;
    if (lunu_embed_child(&root, false, &child) || memcmp(&root, &unchanged, sizeof root) != 0) return false;
    if (!lunu_embed_parent(&root, &parent) || parent.depth != LUNU_EMBED_PATH_BITS - 1u) return false;
    if (!lunu_embed_prefix(&root, 7u, &prefix) || prefix.depth != 7u) return false;
    if (!lunu_embed_neighbor(&root, 0u, &neighbor)) return false;
    unsigned distance;
    if (!lunu_embed_distance(&root, &neighbor, &distance) || distance != 1u) return false;
    uint8_t wire[34]; size_t written;
    if (!lunu_embed_serialize(&root, wire, sizeof wire, &written) ||
        !lunu_embed_deserialize(wire, written, &roundtrip) || !lunu_embed_equal(&root, &roundtrip)) return false;
    uint8_t malformed[] = { 0u, 1u, 1u }; roundtrip = root;
    if (lunu_embed_deserialize(malformed, sizeof malformed, &roundtrip) || !lunu_embed_equal(&root, &roundtrip)) return false;

    static const uint8_t text[] = "123456789";
    if (lunu_embed_crc16(NULL, 0u) != 0xffffu || lunu_embed_crc16(text, 9u) != 0x29b1u) return false;
    if (lunu_embed_crc32(NULL, 0u) != 0u || lunu_embed_crc32(text, 9u) != UINT32_C(0xcbf43926)) return false;
    if (esp_rom_crc32_le(0u, text, 9u) != UINT32_C(0xcbf43926)) return false;
    uint8_t ta = 0x05u, tb = 0xfdu;
    lunu_embed_state a, b, after; lunu_embed_state_zero(&a); lunu_embed_state_zero(&b);
    if (!lunu_embed_step(&a, true, NULL, 0u) || !lunu_embed_step(&b, true, &ta, 0u) || !same_guard_path(&a, &b)) return false;
    lunu_embed_state before = a;
    if (!lunu_embed_step(&a, false, &ta, 3u) || !lunu_embed_step(&b, false, &tb, 3u) || !same_guard_path(&a, &b)) return false;
    after = before;
    if (!lunu_embed_step(&after, true, NULL, 0u)) return false;
    if (!lunu_embed_verify_step(&before, &after, true, NULL, 0u)) return false;
    lunu_embed_state unchanged_state = after;
    if (lunu_embed_step(&after, true, NULL, 1u) || memcmp(&after, &unchanged_state, sizeof after) != 0) return false;
    return true;
}

static uint64_t run_host_vector(void) {
    lunu_embed_state state; lunu_embed_state_zero(&state);
    for (unsigned n = 0; n < LUNU_EMBED_PATH_BITS; ++n) {
        uint8_t payload[5];
        for (unsigned i = 0; i < sizeof payload; ++i) payload[i] = (uint8_t)(n * 37u + i * 61u);
        uint16_t bits = (uint16_t)((n * 13u) % 41u);
        if (!lunu_embed_step(&state, (n & 1u) != 0u, bits == 0u ? NULL : payload, bits)) return UINT64_MAX;
    }
    for (unsigned n = 0; n < LUNU_EMBED_PATH_BITS; ++n) {
        bool bit;
        if (!lunu_embed_path_get(&state.path, n, &bit) || bit != ((n & 1u) != 0u)) return UINT64_MAX;
    }
#if LUNU_EMBED_GUARD == 0
    return 0u;
#else
    return (uint64_t)state.guard;
#endif
}

static bool vectors_match_host(void) {
    uint64_t final = run_host_vector();
#if LUNU_EMBED_GUARD == 2 && LUNU_EMBED_PATH_BITS == 64
    return final == UINT64_C(0xabf2);
#elif LUNU_EMBED_GUARD == 3 && LUNU_EMBED_PATH_BITS == 64
    return final == UINT64_C(0xf1f7f447);
#elif LUNU_EMBED_GUARD == 3 && LUNU_EMBED_PATH_BITS == 256
    return final == UINT64_C(0xa9d87867);
#elif LUNU_EMBED_GUARD == 4 && LUNU_EMBED_PATH_BITS == 64
    return final == UINT64_C(0xd334e8b4ffb6f37a);
#else
    return final != UINT64_MAX;
#endif
}

static uint32_t checksum_state(const lunu_embed_state *state) {
    uint8_t wire[34]; size_t size;
    if (!lunu_embed_serialize(&state->path, wire, sizeof wire, &size)) return 0u;
    uint32_t h = UINT32_C(2166136261);
    for (size_t i = 0; i < size; ++i) h = (h ^ wire[i]) * UINT32_C(16777619);
#if LUNU_EMBED_GUARD != LUNU_EMBED_GUARD_NONE
    for (unsigned i = 0; i < sizeof state->guard; ++i) h = (h ^ (uint8_t)(state->guard >> (8u * i))) * UINT32_C(16777619);
#endif
    return h;
}

static bool million_ops(uint32_t *heap_delta) {
    size_t before = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    lunu_embed_state state; lunu_embed_state_zero(&state);
    const uint8_t payload[4] = { 0x13u, 0x57u, 0x9bu, 0xdfu };
    uint32_t checksum = 0u;
    for (uint32_t i = 0; i < 1000000u; ++i) {
        if (state.path.depth == LUNU_EMBED_PATH_BITS) lunu_embed_state_zero(&state);
        if (!lunu_embed_step(&state, (i & 1u) != 0u, payload, 32u)) return false;
        checksum ^= (uint32_t)state.path.depth;
    }
    hw_sink ^= checksum;
    size_t after = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    *heap_delta = before >= after ? (uint32_t)(before - after) : (uint32_t)(after - before);
    return *heap_delta == 0u;
}

static void cycle_benchmarks(void) {
    static const unsigned sizes[] = { 0u, 4u, 16u, 32u };
    uint8_t payload[32]; memset(payload, 0x6du, sizeof payload);
    lunu_embed_path root, one, deep, out; lunu_embed_path_zero(&root);
    (void)lunu_embed_child(&root, true, &one); deep = root;
    for (unsigned i = 0; i < 32u; ++i) { (void)lunu_embed_child(&deep, (i & 1u) != 0u, &out); deep = out; }
    uint32_t start = esp_cpu_get_cycle_count(), checksum = 0u;
    for (unsigned i = 0; i < 20000u; ++i) { (void)lunu_embed_child(&root, (i & 1u) != 0u, &out); checksum ^= out.depth; }
    hw_sink ^= checksum;
    printf("HW_CYCLES child=%" PRIu32 "\n", (esp_cpu_get_cycle_count() - start) / 20000u);
    start = esp_cpu_get_cycle_count();
    checksum = 0u;
    for (unsigned i = 0; i < 20000u; ++i) { (void)lunu_embed_parent(&one, &out); checksum ^= out.depth; }
    hw_sink ^= checksum;
    printf("HW_CYCLES parent=%" PRIu32 "\n", (esp_cpu_get_cycle_count() - start) / 20000u);
    start = esp_cpu_get_cycle_count();
    checksum = 0u;
    for (unsigned i = 0; i < 20000u; ++i) { (void)lunu_embed_neighbor(&deep, i & 31u, &out); checksum ^= out.word[0]; }
    hw_sink ^= checksum;
    printf("HW_CYCLES neighbor=%" PRIu32 "\n", (esp_cpu_get_cycle_count() - start) / 20000u);
    for (unsigned k = 0; k < sizeof sizes / sizeof sizes[0]; ++k) {
        const unsigned iterations = 20000u;
        checksum = 0u;
        start = esp_cpu_get_cycle_count();
        for (unsigned i = 0; i < iterations; ++i) {
            lunu_embed_state state; lunu_embed_state_zero(&state);
            (void)lunu_embed_step(&state, (i & 1u) != 0u, sizes[k] ? payload : NULL, (uint16_t)(sizes[k] * 8u));
            checksum ^= (uint32_t)state.path.depth;
        }
        hw_sink ^= checksum;
        uint32_t cycles = esp_cpu_get_cycle_count() - start;
        printf("HW_CYCLES step_%uB=%" PRIu32 "\n", sizes[k], cycles / iterations);
    }
    start = esp_cpu_get_cycle_count();
    checksum = 0u;
    for (unsigned i = 0; i < 20000u; ++i) {
        lunu_embed_state state; lunu_embed_state_zero(&state);
        (void)lunu_embed_step_tag32(&state, (i & 1u) != 0u, i, 65535u);
        checksum ^= (uint32_t)state.path.depth;
    }
    hw_sink ^= checksum;
    uint32_t tag_cycles = (esp_cpu_get_cycle_count() - start) / 20000u;
    printf("HW_CYCLES step_pre_tag=%" PRIu32 "\n", tag_cycles);
#if LUNU_EMBED_GUARD == LUNU_EMBED_GUARD_CRC32
    static const unsigned crc_sizes[] = { 0u, 4u, 16u, 32u, 64u, 256u };
    uint8_t crc_data[256];
    for (unsigned i = 0; i < sizeof crc_data; ++i) crc_data[i] = (uint8_t)(i * 29u + 7u);
    for (unsigned k = 0; k < sizeof crc_sizes / sizeof crc_sizes[0]; ++k) {
        uint32_t size = crc_sizes[k];
        if (lunu_embed_crc32(crc_data, size) != esp_rom_crc32_le(0u, crc_data, size)) {
            printf("HW_CRC_BACKEND_MISMATCH size=%" PRIu32 "\n", size);
            continue;
        }
        uint32_t portable_start = esp_cpu_get_cycle_count();
        for (unsigned i = 0; i < 20000u; ++i) hw_sink ^= lunu_embed_crc32(crc_data, size);
        uint32_t portable_cycles = (esp_cpu_get_cycle_count() - portable_start) / 20000u;
        uint32_t rom_start = esp_cpu_get_cycle_count();
        for (unsigned i = 0; i < 20000u; ++i) hw_sink ^= esp_rom_crc32_le(0u, crc_data, size);
        uint32_t rom_cycles = (esp_cpu_get_cycle_count() - rom_start) / 20000u;
        printf("HW_CRC_BACKEND size=%" PRIu32 " portable=%" PRIu32 " rom=%" PRIu32 "\n",
               size, portable_cycles, rom_cycles);
    }
#endif
    lunu_state v1; uint8_t v1_payload[32]; memset(v1_payload, 0x6du, sizeof v1_payload);
    checksum = 0u; start = esp_cpu_get_cycle_count();
    for (unsigned i = 0; i < 20000u; ++i) {
        memset(&v1, 0, sizeof v1);
        (void)lunu_step_payload(&v1, (i & 1u) != 0u, v1_payload, 256u, LUNU_CONTROL_ROLLING_HASH);
        checksum ^= (uint32_t)v1.control ^ v1.path.depth;
    }
    hw_sink ^= checksum;
    printf("HW_CYCLES V1_roll_step_32B=%" PRIu32 " v1_state_bytes=%zu\n",
           (esp_cpu_get_cycle_count() - start) / 20000u, sizeof v1);
}

static bool replay(const hw_event *events, unsigned count, lunu_embed_state *out) {
    lunu_embed_state_zero(out);
    for (unsigned i = 0; i < count; ++i)
        if (!lunu_embed_step(out, events[i].bit, events[i].bits ? events[i].payload : NULL, events[i].bits)) return false;
    return true;
}

static bool fault_subset(uint32_t *detected_out) {
    static const hw_event original[4] = {
        { true, 32u, { 1u, 3u, 5u, 7u } }, { false, 32u, { 2u, 4u, 6u, 8u } },
        { true, 32u, { 9u, 11u, 13u, 15u } }, { false, 32u, { 10u, 12u, 14u, 16u } }
    };
    lunu_embed_state expected;
    if (!replay(original, 4u, &expected)) return false;
    uint32_t detected = 0u;
    for (uint32_t i = 0; i < 10000u; ++i) {
        hw_event events[5]; memcpy(events, original, sizeof original);
        unsigned category = i % 8u, at = (i * 13u + 3u) % 4u, count = 4u;
        if (category == 0u) events[at].payload[(i >> 3) & 3u] ^= (uint8_t)(1u << (i & 7u));
        else if (category == 1u || category == 2u) events[at].bit = !events[at].bit;
        else if (category == 3u) { for (unsigned j = at; j + 1u < count; ++j) events[j] = events[j + 1u]; --count; }
        else if (category == 4u) { for (unsigned j = count; j > at; --j) events[j] = events[j - 1u]; ++count; }
        else if (category == 5u) { unsigned j = at == 3u ? 2u : at; hw_event t = events[j]; events[j] = events[j + 1u]; events[j + 1u] = t; }
        else if (category == 7u) events[at].bits = 31u;
        lunu_embed_state changed;
        if (category == 6u) { changed = expected;
#if LUNU_EMBED_GUARD == LUNU_EMBED_GUARD_CRC16 || LUNU_EMBED_GUARD == LUNU_EMBED_GUARD_CRC32
            changed.guard ^= 1u;
#endif
        } else if (!replay(events, count, &changed)) return false;
        if (!same_guard_path(&expected, &changed)) ++detected;
    }
    *detected_out = detected;
    return detected == 10000u;
}

void app_main(void) {
    printf("LUNA_EMBEDDED_HW_START path=%u guard=%u idf=%s\n", (unsigned)LUNU_EMBED_PATH_BITS,
           (unsigned)LUNU_EMBED_GUARD, esp_get_idf_version());
    if (!focused_tests()) FAIL("selftest");
    if (!vectors_match_host()) FAIL("host_vectors");
    uint64_t final_guard = run_host_vector(); uint32_t checksum = 0u; uint32_t heap_delta = 0u; uint32_t faults = 0u;
    lunu_embed_state final_state; lunu_embed_state_zero(&final_state);
    for (unsigned i = 0; i < LUNU_EMBED_PATH_BITS; ++i) {
        uint8_t payload[5]; for (unsigned j = 0; j < sizeof payload; ++j) payload[j] = (uint8_t)(i * 37u + j * 61u);
        uint16_t bits = (uint16_t)((i * 13u) % 41u);
        if (!lunu_embed_step(&final_state, (i & 1u) != 0u, bits ? payload : NULL, bits)) FAIL("vector_step");
    }
    checksum = checksum_state(&final_state);
    if (LUNU_EMBED_GUARD == LUNU_EMBED_GUARD_CRC16 || LUNU_EMBED_GUARD == LUNU_EMBED_GUARD_CRC32) {
        if (!million_ops(&heap_delta)) FAIL("million_ops_or_heap");
        if (!fault_subset(&faults)) FAIL("fault_subset");
    }
    cycle_benchmarks();
    uint8_t final_wire[34]; size_t final_size;
    if (!lunu_embed_serialize(&final_state.path, final_wire, sizeof final_wire, &final_size)) FAIL("final_serialization");
    printf("HW_FINAL_PATH=");
    for (size_t i = 0; i < final_size; ++i) printf("%02x", final_wire[i]);
    printf("\n");
    printf("LUNA_EMBEDDED_HW_PASS path=%u guard=%u depth=%u final_guard=0x%" PRIx64
           " checksum=0x%08" PRIx32 " faults=%" PRIu32 " heap_delta=%" PRIu32 "\n",
           (unsigned)LUNU_EMBED_PATH_BITS, (unsigned)LUNU_EMBED_GUARD, (unsigned)final_state.path.depth,
           final_guard, checksum, faults, heap_delta);
}
