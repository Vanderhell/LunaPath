#include "lunu_embedded.h"

#include <inttypes.h>
#include <stdio.h>

static uint64_t guard_value(const lunu_embed_state *s) {
#if LUNU_EMBED_GUARD == LUNU_EMBED_GUARD_NONE
    (void)s; return 0u;
#else
    return (uint64_t)s->guard;
#endif
}

int main(void) {
    lunu_embed_state state; lunu_embed_state_zero(&state);
    for (unsigned n = 0; n < 256u; ++n) {
        uint8_t payload[5];
        for (unsigned i = 0; i < sizeof payload; ++i) payload[i] = (uint8_t)(n * 37u + i * 61u);
        uint16_t bits = (uint16_t)((n * 13u) % 41u);
        bool bit = (n & 1u) != 0u;
        uint64_t before = guard_value(&state);
        if (!lunu_embed_step(&state, bit, bits == 0u ? NULL : payload, bits)) return 2;
        uint64_t after = guard_value(&state);
        printf("%u,%u,%u,", n, bit ? 1u : 0u, (unsigned)bits);
        for (unsigned i = 0; i < (bits + 7u) / 8u; ++i) printf("%02x", payload[i]);
        printf(",%016" PRIx64 ",%016" PRIx64 "\n", before, after);
    }
    return 0;
}
