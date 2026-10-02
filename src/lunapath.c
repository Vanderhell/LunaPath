// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vanderhell
#include "lunapath.h"

#include <string.h>

#define PATH_MAX ((unsigned)LUNAPATH_PATH_BITS)

static bool valid(const lunapath_path *p) { return p != NULL && p->depth <= PATH_MAX; }
static uint32_t bitmask(unsigned i) { return UINT32_C(1) << (i & 31u); }
static bool get_unchecked(const lunapath_path *p, unsigned i) {
    return (p->word[i / 32u] & bitmask(i)) != 0u;
}

void lunapath_path_zero(lunapath_path *p) { if (p != NULL) { memset(p, 0, sizeof *p); } }
bool lunapath_path_get(const lunapath_path *p, unsigned i, bool *bit) {
    if (!valid(p) || bit == NULL || i >= p->depth) return false;
    *bit = get_unchecked(p, i); return true;
}
bool lunapath_path_set(lunapath_path *p, unsigned i, bool bit) {
    if (!valid(p) || i >= p->depth) return false;
    if (bit) p->word[i / 32u] |= bitmask(i); else p->word[i / 32u] &= ~bitmask(i);
    return true;
}
bool lunapath_child(const lunapath_path *p, bool bit, lunapath_path *out) {
    if (!valid(p) || out == NULL || p->depth >= PATH_MAX) return false;
    lunapath_path next = *p;
    if (bit) next.word[next.depth / 32u] |= bitmask(next.depth);
    else next.word[next.depth / 32u] &= ~bitmask(next.depth);
    ++next.depth; *out = next; return true;
}
bool lunapath_parent(const lunapath_path *p, lunapath_path *out) {
    if (!valid(p) || out == NULL || p->depth == 0u) return false;
    lunapath_path next = *p; --next.depth;
    for (unsigned i = (next.depth + 31u) / 32u; i < LUNAPATH_WORDS; ++i) next.word[i] = 0u;
    if ((next.depth & 31u) != 0u) next.word[next.depth / 32u] &= bitmask(next.depth) - 1u;
    *out = next; return true;
}
bool lunapath_prefix(const lunapath_path *p, unsigned depth, lunapath_path *out) {
    if (!valid(p) || out == NULL || depth > p->depth) return false;
    lunapath_path next = *p; next.depth = (uint16_t)depth;
    for (unsigned i = (depth + 31u) / 32u; i < LUNAPATH_WORDS; ++i) next.word[i] = 0u;
    if ((depth & 31u) != 0u) next.word[depth / 32u] &= bitmask(depth) - 1u;
    *out = next; return true;
}
bool lunapath_common_prefix(const lunapath_path *a, const lunapath_path *b, lunapath_path *out) {
    if (!valid(a) || !valid(b) || out == NULL) return false;
    unsigned n = a->depth < b->depth ? a->depth : b->depth, k = 0u;
    while (k < n && get_unchecked(a, k) == get_unchecked(b, k)) ++k;
    return lunapath_prefix(a, k, out);
}
bool lunapath_neighbor(const lunapath_path *p, unsigned d, lunapath_path *out) {
    if (!valid(p) || out == NULL || d >= p->depth) return false;
    lunapath_path next = *p; next.word[d / 32u] ^= bitmask(d); *out = next; return true;
}
bool lunapath_distance(const lunapath_path *a, const lunapath_path *b, unsigned *distance) {
    if (!valid(a) || !valid(b) || distance == NULL || a->depth != b->depth) return false;
    unsigned count = 0u;
    for (unsigned i = 0; i < a->depth; ++i) count += (unsigned)(get_unchecked(a, i) != get_unchecked(b, i));
    *distance = count; return true;
}
bool lunapath_equal(const lunapath_path *a, const lunapath_path *b) {
    if (!valid(a) || !valid(b) || a->depth != b->depth) return false;
    for (unsigned i = 0; i < a->depth; ++i) if (get_unchecked(a, i) != get_unchecked(b, i)) return false;
    return true;
}
size_t lunapath_serialized_size(const lunapath_path *p) {
    return valid(p) ? 2u + (p->depth + 7u) / 8u : 0u;
}
bool lunapath_serialize(const lunapath_path *p, uint8_t *out, size_t cap, size_t *written) {
    size_t need = lunapath_serialized_size(p);
    if (need == 0u || out == NULL || cap < need) return false;
    uint8_t bytes[2u + LUNAPATH_PATH_BITS / 8u];
    bytes[0] = (uint8_t)(p->depth >> 8); bytes[1] = (uint8_t)p->depth;
    memset(bytes + 2, 0, need - 2u);
    for (unsigned i = 0; i < p->depth; ++i) if (get_unchecked(p, i)) bytes[2u + i / 8u] |= (uint8_t)(1u << (7u - i % 8u));
    memcpy(out, bytes, need); if (written != NULL) *written = need; return true;
}
bool lunapath_deserialize(const uint8_t *in, size_t size, lunapath_path *out) {
    if (in == NULL || out == NULL || size < 2u) return false;
    unsigned depth = ((unsigned)in[0] << 8) | in[1];
    if (depth > PATH_MAX || size != 2u + (depth + 7u) / 8u) return false;
    if ((depth & 7u) != 0u && (in[size - 1u] & ((1u << (8u - (depth & 7u))) - 1u)) != 0u) return false;
    lunapath_path next; lunapath_path_zero(&next); next.depth = (uint16_t)depth;
    for (unsigned i = 0; i < depth; ++i) if ((in[2u + i / 8u] & (uint8_t)(1u << (7u - i % 8u))) != 0u) next.word[i / 32u] |= bitmask(i);
    *out = next; return true;
}

static uint16_t crc16_byte(uint16_t crc, uint8_t byte) {
    crc ^= (uint16_t)((uint16_t)byte << 8);
    for (unsigned i = 0; i < 8u; ++i) crc = (uint16_t)((crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1));
    return crc;
}
static uint32_t crc32_byte(uint32_t crc, uint8_t byte) {
    crc ^= byte;
    for (unsigned i = 0; i < 8u; ++i) crc = (crc & 1u) ? (crc >> 1) ^ UINT32_C(0xedb88320) : crc >> 1;
    return crc;
}
uint16_t lunapath_crc16(const void *data, size_t size) {
    const uint8_t *p = (const uint8_t *)data; uint16_t crc = 0xffffu;
    if (data == NULL && size != 0u) return 0u;
    for (size_t i = 0; i < size; ++i) crc = crc16_byte(crc, p[i]);
    return crc;
}
uint32_t lunapath_crc32(const void *data, size_t size) {
    const uint8_t *p = (const uint8_t *)data; uint32_t crc = UINT32_MAX;
    if (data == NULL && size != 0u) return 0u;
    for (size_t i = 0; i < size; ++i) crc = crc32_byte(crc, p[i]);
    return crc ^ UINT32_MAX;
}

#if LUNAPATH_GUARD != LUNAPATH_GUARD_NONE
static uint8_t event_byte(const lunapath_path *path, bool bit, uint16_t payload_bits,
                          const uint8_t *payload, unsigned index) {
    if (index == 0u) return 0x4cu; /* ASCII L, event format version 2 */
    if (index == 1u) return 2u;
    if (index == 2u) return (uint8_t)(path->depth >> 8);
    if (index == 3u) return (uint8_t)path->depth;
    if (index == 4u) return bit ? 1u : 0u;
    if (index == 5u) return (uint8_t)(payload_bits >> 8);
    if (index == 6u) return (uint8_t)payload_bits;
    index -= 7u;
    if (index >= (payload_bits + 7u) / 8u) return 0u;
    uint8_t value = payload[index];
    if (index + 1u == (payload_bits + 7u) / 8u && (payload_bits & 7u) != 0u)
        value &= (uint8_t)((1u << (payload_bits & 7u)) - 1u);
    return value;
}
#endif
#if LUNAPATH_GUARD == LUNAPATH_GUARD_CRC32
static uint32_t crc32_update_block(uint32_t crc, const uint8_t *data, size_t size) {
#if defined(LUNAPATH_USE_ESP_CRC32) && LUNAPATH_USE_ESP_CRC32
    extern uint32_t lunapath_esp_crc32_update(uint32_t raw_crc, const uint8_t *bytes, size_t length);
    return lunapath_esp_crc32_update(crc, data, size);
#else
    for (size_t i = 0; i < size; ++i) crc = crc32_byte(crc, data[i]);
    return crc;
#endif
}
#endif
#if LUNAPATH_GUARD != LUNAPATH_GUARD_NONE
static lunapath_guard advance_guard(lunapath_guard old, const lunapath_path *path, bool bit,
                                     uint16_t payload_bits, const uint8_t *payload) {
    unsigned count = 7u + (payload_bits + 7u) / 8u;
#if LUNAPATH_GUARD == LUNAPATH_GUARD_PARITY
    uint8_t parity = (uint8_t)old;
    for (unsigned i = 0; i < count; ++i) { uint8_t b = event_byte(path, bit, payload_bits, payload, i); while (b != 0u) { parity ^= (uint8_t)(b & 1u); b >>= 1; } }
    return parity;
#elif LUNAPATH_GUARD == LUNAPATH_GUARD_CRC16
    uint16_t crc = (uint16_t)old;
    for (unsigned i = 0; i < count; ++i) crc = crc16_byte(crc, event_byte(path, bit, payload_bits, payload, i));
    return crc;
#elif LUNAPATH_GUARD == LUNAPATH_GUARD_CRC32
    (void)count;
    uint32_t crc = (uint32_t)old ^ UINT32_MAX;
    uint8_t header[7];
    for (unsigned i = 0; i < sizeof header; ++i) header[i] = event_byte(path, bit, payload_bits, payload, i);
    crc = crc32_update_block(crc, header, sizeof header);
    size_t payload_bytes = (payload_bits + 7u) / 8u;
    if (payload_bytes != 0u) {
        if ((payload_bits & 7u) == 0u) {
            crc = crc32_update_block(crc, payload, payload_bytes);
        } else {
            if (payload_bytes > 1u) crc = crc32_update_block(crc, payload, payload_bytes - 1u);
            uint8_t tail = payload[payload_bytes - 1u] & (uint8_t)((1u << (payload_bits & 7u)) - 1u);
            crc = crc32_update_block(crc, &tail, 1u);
        }
    }
    return crc ^ UINT32_MAX;
#else
    uint64_t h = UINT64_C(1469598103934665603); /* FNV-1a over old state LE plus canonical event */
    for (unsigned i = 0; i < 8u; ++i) { h ^= (uint8_t)(old >> (8u * i)); h *= UINT64_C(1099511628211); }
    for (unsigned i = 0; i < count; ++i) { h ^= event_byte(path, bit, payload_bits, payload, i); h *= UINT64_C(1099511628211); }
    return (h << 1) | (h >> 63);
#endif
}
#endif
void lunapath_state_zero(lunapath_state *s) {
    if (s != NULL) { memset(s, 0, sizeof *s);
#if LUNAPATH_GUARD == LUNAPATH_GUARD_CRC16
        s->guard = UINT16_C(0xffff);
#elif LUNAPATH_GUARD == LUNAPATH_GUARD_CRC32
        s->guard = UINT32_MAX;
#endif
    }
}
static bool step_inner(lunapath_state *s, bool bit, const void *payload, uint16_t payload_bits, bool tagged, uint32_t tag) {
    if (s == NULL || !valid(&s->path) || (!tagged && payload == NULL && payload_bits != 0u)) return false;
    if (s->path.depth >= PATH_MAX) return false;
    uint8_t tag_bytes[6]; const uint8_t *bytes = (const uint8_t *)payload;
    if (tagged) {
        tag_bytes[0] = (uint8_t)tag; tag_bytes[1] = (uint8_t)(tag >> 8);
        tag_bytes[2] = (uint8_t)(tag >> 16); tag_bytes[3] = (uint8_t)(tag >> 24);
        tag_bytes[4] = (uint8_t)payload_bits; tag_bytes[5] = (uint8_t)(payload_bits >> 8);
        bytes = tag_bytes; payload_bits = 48u;
    }
    lunapath_state next = *s;
#if LUNAPATH_GUARD != LUNAPATH_GUARD_NONE
    next.guard = advance_guard(s->guard, &s->path, bit, payload_bits, bytes);
#else
    (void)bytes;
#endif
    if (!lunapath_child(&s->path, bit, &next.path)) return false;
    *s = next; return true;
}
bool lunapath_step(lunapath_state *s, bool bit, const void *payload, uint16_t payload_bits) {
    return step_inner(s, bit, payload, payload_bits, false, 0u);
}
bool lunapath_step_tag32(lunapath_state *s, bool bit, uint32_t tag, uint16_t original_payload_bits) {
    return step_inner(s, bit, NULL, original_payload_bits, true, tag);
}
bool lunapath_verify_step(const lunapath_state *before, const lunapath_state *after,
                            bool bit, const void *payload, uint16_t payload_bits) {
    if (before == NULL || after == NULL) return false;
    lunapath_state expected = *before;
    if (!lunapath_step(&expected, bit, payload, payload_bits) ||
        !lunapath_equal(&expected.path, &after->path)) return false;
#if LUNAPATH_GUARD != LUNAPATH_GUARD_NONE
    return expected.guard == after->guard;
#else
    return true;
#endif
}
