// SPDX-License-Identifier: Apache-2.0
#include "lunapath.h"

#include <stdio.h>

int main(void) {
#if LUNAPATH_GUARD == 0
    const size_t guard_bytes = 0u;
#else
    const size_t guard_bytes = sizeof(lunapath_guard);
#endif
#if LUNAPATH_GUARD == 0
    const unsigned guard_bits = 0u;
#elif LUNAPATH_GUARD == 1
    const unsigned guard_bits = 1u;
#elif LUNAPATH_GUARD == 2
    const unsigned guard_bits = 16u;
#elif LUNAPATH_GUARD == 3
    const unsigned guard_bits = 32u;
#else
    const unsigned guard_bits = 64u;
#endif
    printf("%u,%u,%zu,%zu,%zu,%u,%u,%zu\n", (unsigned)LUNAPATH_PATH_BITS,
           (unsigned)LUNAPATH_GUARD, sizeof(lunapath_path), guard_bytes,
           sizeof(lunapath_state), (unsigned)LUNAPATH_PATH_BITS, guard_bits,
           sizeof(lunapath_state) - sizeof(lunapath_path) - guard_bytes);
    return 0;
}
