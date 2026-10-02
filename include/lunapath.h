#ifndef LUNAPATH_H
#define LUNAPATH_H

/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright 2026 Vanderhell */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LUNAPATH_VERSION_MAJOR 0
#define LUNAPATH_VERSION_MINOR 1
#define LUNAPATH_VERSION_PATCH 0
#define LUNAPATH_VERSION_STRING "0.1.0"

#ifndef LUNAPATH_PATH_BITS
#define LUNAPATH_PATH_BITS 64
#endif
#ifndef LUNAPATH_GUARD
#define LUNAPATH_GUARD 2
#endif

#if LUNAPATH_PATH_BITS != 32 && LUNAPATH_PATH_BITS != 64 && LUNAPATH_PATH_BITS != 128 && LUNAPATH_PATH_BITS != 256
#error "LUNAPATH_PATH_BITS must be 32, 64, 128, or 256"
#endif
#if LUNAPATH_GUARD < 0 || LUNAPATH_GUARD > 4
#error "LUNAPATH_GUARD must be NONE(0), PARITY(1), CRC16(2), CRC32(3), or ROLL64(4)"
#endif

#define LUNAPATH_GUARD_NONE 0
#define LUNAPATH_GUARD_PARITY 1
#define LUNAPATH_GUARD_CRC16 2
#define LUNAPATH_GUARD_CRC32 3
#define LUNAPATH_GUARD_ROLL64 4
#define LUNAPATH_WORDS (LUNAPATH_PATH_BITS / 32)

typedef struct {
    /* Capacity is LUNAPATH_PATH_BITS; depth counts valid decision bits. */
    uint32_t word[LUNAPATH_WORDS]; /* bit 0 is the first/root bit */
    uint16_t depth;
} lunapath_path;

#if LUNAPATH_GUARD == LUNAPATH_GUARD_NONE || LUNAPATH_GUARD == LUNAPATH_GUARD_PARITY
typedef uint8_t lunapath_guard;
#elif LUNAPATH_GUARD == LUNAPATH_GUARD_CRC16
typedef uint16_t lunapath_guard;
#elif LUNAPATH_GUARD == LUNAPATH_GUARD_CRC32
typedef uint32_t lunapath_guard;
#else
typedef uint64_t lunapath_guard;
#endif

typedef struct {
    lunapath_path path;
#if LUNAPATH_GUARD != LUNAPATH_GUARD_NONE
    lunapath_guard guard;
#endif
} lunapath_state;

/* Configuration is compile-time: LUNAPATH_PATH_BITS is 32/64/128/256;
 * LUNAPATH_GUARD is NONE/PARITY/CRC16/CRC32/ROLL64 (0 through 4). */
void lunapath_path_zero(lunapath_path *p);
bool lunapath_path_get(const lunapath_path *p, unsigned index, bool *bit);
bool lunapath_path_set(lunapath_path *p, unsigned index, bool bit);
bool lunapath_child(const lunapath_path *p, bool bit, lunapath_path *out);
bool lunapath_parent(const lunapath_path *p, lunapath_path *out);
bool lunapath_prefix(const lunapath_path *p, unsigned depth, lunapath_path *out);
bool lunapath_common_prefix(const lunapath_path *a, const lunapath_path *b, lunapath_path *out);
bool lunapath_neighbor(const lunapath_path *p, unsigned dimension, lunapath_path *out);
bool lunapath_distance(const lunapath_path *a, const lunapath_path *b, unsigned *distance);
bool lunapath_equal(const lunapath_path *a, const lunapath_path *b);
/* Canonical V1-compatible wire: depth as uint16 big endian, then root-first
 * path bits in network order (MSB first), with zero padding in the last byte. */
size_t lunapath_serialized_size(const lunapath_path *p);
bool lunapath_serialize(const lunapath_path *p, uint8_t *out, size_t capacity, size_t *written);
bool lunapath_deserialize(const uint8_t *in, size_t size, lunapath_path *out);

/* Every bool-returning mutation commits only on success. A step appends one
 * bit; its event position is the path depth before append. NULL payload is
 * valid exactly when payload_bits is zero. */
/* Guard choice is compile-time. CRC16 is CCITT-FALSE (poly 0x1021, init
 * 0xffff, no reflection, xorout 0); CRC32 is ISO-HDLC (reflected poly
 * 0xedb88320, init/xorout 0xffffffff). The event stream is bytes:
 * 4c 02, depth_before_be16, bit(00/01), payload_bits_be16, then ceil(bits/8)
 * payload bytes. Payload logical bit i is byte[i/8] bit (i%8), and unused
 * high bits of the last byte are ignored. No native struct/padding is hashed. */
void lunapath_state_zero(lunapath_state *s);
bool lunapath_step(lunapath_state *s, bool bit, const void *payload, uint16_t payload_bits);
/* O(1) with respect to the original payload: guard covers tag LE32 followed
 * by original_payload_bits LE16 as a fixed 48-bit tagged payload. Its integrity
 * is only as strong as the caller-supplied tag. */
bool lunapath_step_tag32(lunapath_state *s, bool bit, uint32_t tag, uint16_t original_payload_bits);
bool lunapath_verify_step(const lunapath_state *before, const lunapath_state *after,
                            bool bit, const void *payload, uint16_t payload_bits);
uint16_t lunapath_crc16(const void *data, size_t size);
uint32_t lunapath_crc32(const void *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif
