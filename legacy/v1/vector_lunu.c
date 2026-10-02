#include "lunu_primitives.h"
#include <stdio.h>
int main(void) {
    lunu_state s = {0}; unsigned char data[32] = {0};
    for (unsigned i = 0; i < 256; ++i) {
        data[i / 8u] = (unsigned char)(i * 37u + 11u);
        uint64_t input[4] = {s.control, (uint64_t)((i * 13u & 1u) != 0), lunu_hash64(data, sizeof data) ^ 256u, s.steps};
        if (!lunu_step_payload(&s, (i * 13u & 1u) != 0, data, 256, LUNU_CONTROL_ROLLING_HASH)) return 1;
        printf("%u,%u,%llu,%llu,%llu\n", i, (unsigned)(s.path.depth), (unsigned long long)s.control,
               (unsigned long long)lunu_hash64(data, sizeof data), (unsigned long long)lunu_hash64(input, sizeof input));
    }
    return 0;
}
