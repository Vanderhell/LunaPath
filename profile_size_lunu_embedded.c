#include "lunu_embedded.h"

#include <stdio.h>

int main(void) {
#if LUNU_EMBED_GUARD == 0
    const size_t guard_bytes = 0u;
#else
    const size_t guard_bytes = sizeof(lunu_embed_guard);
#endif
#if LUNU_EMBED_GUARD == 0
    const unsigned guard_bits = 0u;
#elif LUNU_EMBED_GUARD == 1
    const unsigned guard_bits = 1u;
#elif LUNU_EMBED_GUARD == 2
    const unsigned guard_bits = 16u;
#elif LUNU_EMBED_GUARD == 3
    const unsigned guard_bits = 32u;
#else
    const unsigned guard_bits = 64u;
#endif
    printf("%u,%u,%zu,%zu,%zu,%u,%u,%zu\n", (unsigned)LUNU_EMBED_PATH_BITS,
           (unsigned)LUNU_EMBED_GUARD, sizeof(lunu_embed_path), guard_bytes,
           sizeof(lunu_embed_state), (unsigned)LUNU_EMBED_PATH_BITS, guard_bits,
           sizeof(lunu_embed_state) - sizeof(lunu_embed_path) - guard_bytes);
    return 0;
}
