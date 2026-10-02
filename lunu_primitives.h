#ifndef LUNU_PRIMITIVES_H
#define LUNU_PRIMITIVES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LUNU_MAX_BITS 256u
#define LUNU_WORDS 4u

typedef struct {
    uint64_t word[LUNU_WORDS]; /* bit 0 is the first/root bit */
    uint16_t depth;
} lunu_path;

typedef struct {
    lunu_path path;
    uint64_t control;
    uint64_t steps;
} lunu_state;

typedef enum {
    LUNU_CONTROL_NONE = 0,
    LUNU_CONTROL_PARITY = 1,
    LUNU_CONTROL_PARITY_SEQUENCE = 2,
    LUNU_CONTROL_ROLLING_HASH = 3
} lunu_control_mode;

void lunu_path_zero(lunu_path *p);
bool lunu_path_get(const lunu_path *p, unsigned index);
bool lunu_path_set(lunu_path *p, unsigned index, bool bit);
bool lunu_child(const lunu_path *p, bool bit, lunu_path *out);
bool lunu_parent(const lunu_path *p, lunu_path *out);
bool lunu_prefix(const lunu_path *p, unsigned depth, lunu_path *out);
unsigned lunu_common_prefix(const lunu_path *a, const lunu_path *b,
                            lunu_path *out);
bool lunu_neighbor(const lunu_path *p, unsigned dimension, lunu_path *out);
unsigned lunu_distance(const lunu_path *a, const lunu_path *b, bool *valid);
bool lunu_equal(const lunu_path *a, const lunu_path *b);
uint64_t lunu_hash64(const void *data, size_t size);

/* O(1) STEP. data_digest is a caller-owned digest of the payload. */
bool lunu_step(lunu_state *s, bool bit, uint64_t data_digest);
bool lunu_verify_step(const lunu_state *before, const lunu_state *after,
                      bool bit, uint64_t data_digest);
bool lunu_step_payload(lunu_state *s, bool bit, const void *payload,
                       size_t payload_bits, lunu_control_mode mode);
bool lunu_verify_step_payload(const lunu_state *before, const lunu_state *after,
                              bool bit, const void *payload, size_t payload_bits,
                              lunu_control_mode mode);

/* Canonical big-endian serialization: depth byte count, then ceil(depth/8) bits. */
size_t lunu_serialized_size(const lunu_path *p);
bool lunu_serialize(const lunu_path *p, uint8_t *out, size_t capacity,
                    size_t *written);
bool lunu_deserialize(const uint8_t *in, size_t size, lunu_path *out);

#endif
