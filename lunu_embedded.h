#ifndef LUNU_EMBEDDED_H
#define LUNU_EMBEDDED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef LUNU_EMBED_PATH_BITS
#define LUNU_EMBED_PATH_BITS 64
#endif
#ifndef LUNU_EMBED_GUARD
#define LUNU_EMBED_GUARD 2
#endif

#if LUNU_EMBED_PATH_BITS != 32 && LUNU_EMBED_PATH_BITS != 64 && LUNU_EMBED_PATH_BITS != 128 && LUNU_EMBED_PATH_BITS != 256
#error "LUNU_EMBED_PATH_BITS must be 32, 64, 128, or 256"
#endif
#if LUNU_EMBED_GUARD < 0 || LUNU_EMBED_GUARD > 4
#error "LUNU_EMBED_GUARD must be NONE(0), PARITY(1), CRC16(2), CRC32(3), or ROLL64(4)"
#endif

#define LUNU_EMBED_GUARD_NONE 0
#define LUNU_EMBED_GUARD_PARITY 1
#define LUNU_EMBED_GUARD_CRC16 2
#define LUNU_EMBED_GUARD_CRC32 3
#define LUNU_EMBED_GUARD_ROLL64 4
#define LUNU_EMBED_WORDS (LUNU_EMBED_PATH_BITS / 32)

typedef struct {
    uint32_t word[LUNU_EMBED_WORDS]; /* bit 0 is the first/root bit */
    uint16_t depth;
} lunu_embed_path;

#if LUNU_EMBED_GUARD == LUNU_EMBED_GUARD_NONE || LUNU_EMBED_GUARD == LUNU_EMBED_GUARD_PARITY
typedef uint8_t lunu_embed_guard;
#elif LUNU_EMBED_GUARD == LUNU_EMBED_GUARD_CRC16
typedef uint16_t lunu_embed_guard;
#elif LUNU_EMBED_GUARD == LUNU_EMBED_GUARD_CRC32
typedef uint32_t lunu_embed_guard;
#else
typedef uint64_t lunu_embed_guard;
#endif

typedef struct {
    lunu_embed_path path;
#if LUNU_EMBED_GUARD != LUNU_EMBED_GUARD_NONE
    lunu_embed_guard guard;
#endif
} lunu_embed_state;

void lunu_embed_path_zero(lunu_embed_path *p);
bool lunu_embed_path_get(const lunu_embed_path *p, unsigned index, bool *bit);
bool lunu_embed_path_set(lunu_embed_path *p, unsigned index, bool bit);
bool lunu_embed_child(const lunu_embed_path *p, bool bit, lunu_embed_path *out);
bool lunu_embed_parent(const lunu_embed_path *p, lunu_embed_path *out);
bool lunu_embed_prefix(const lunu_embed_path *p, unsigned depth, lunu_embed_path *out);
bool lunu_embed_common_prefix(const lunu_embed_path *a, const lunu_embed_path *b, lunu_embed_path *out);
bool lunu_embed_neighbor(const lunu_embed_path *p, unsigned dimension, lunu_embed_path *out);
bool lunu_embed_distance(const lunu_embed_path *a, const lunu_embed_path *b, unsigned *distance);
bool lunu_embed_equal(const lunu_embed_path *a, const lunu_embed_path *b);
/* Canonical V1-compatible wire: depth as uint16 big endian, then root-first
 * path bits in network order (MSB first), with zero padding in the last byte. */
size_t lunu_embed_serialized_size(const lunu_embed_path *p);
bool lunu_embed_serialize(const lunu_embed_path *p, uint8_t *out, size_t capacity, size_t *written);
bool lunu_embed_deserialize(const uint8_t *in, size_t size, lunu_embed_path *out);

/* Guard choice is compile-time. CRC16 is CCITT-FALSE (poly 0x1021, init
 * 0xffff, no reflection, xorout 0); CRC32 is ISO-HDLC (reflected poly
 * 0xedb88320, init/xorout 0xffffffff). The event stream is bytes:
 * 4c 02, depth_before_be16, bit(00/01), payload_bits_be16, then ceil(bits/8)
 * payload bytes. Payload logical bit i is byte[i/8] bit (i%8), and unused
 * high bits of the last byte are ignored. No native struct/padding is hashed. */
void lunu_embed_state_zero(lunu_embed_state *s);
bool lunu_embed_step(lunu_embed_state *s, bool bit, const void *payload, uint16_t payload_bits);
/* O(1) with respect to the original payload: guard covers tag LE32 followed
 * by original_payload_bits LE16 as a fixed 48-bit tagged payload. Its integrity
 * is only as strong as the caller-supplied tag. */
bool lunu_embed_step_tag32(lunu_embed_state *s, bool bit, uint32_t tag, uint16_t original_payload_bits);
bool lunu_embed_verify_step(const lunu_embed_state *before, const lunu_embed_state *after,
                            bool bit, const void *payload, uint16_t payload_bits);
uint16_t lunu_embed_crc16(const void *data, size_t size);
uint32_t lunu_embed_crc32(const void *data, size_t size);

#endif
