// SPDX-License-Identifier: Apache-2.0
#include "lunapath.h"

#include <stdio.h>
#include <string.h>

#define REQUIRE(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

typedef struct { bool branch; uint8_t stage; uint16_t block; uint32_t block_crc; } event;

static bool run(const event *events, unsigned count, lunapath_state *out) {
    lunapath_state_zero(out);
    for (unsigned i = 0; i < count; ++i) {
        uint8_t payload[7] = { events[i].stage, (uint8_t)(events[i].block >> 8), (uint8_t)events[i].block,
            (uint8_t)(events[i].block_crc >> 24), (uint8_t)(events[i].block_crc >> 16),
            (uint8_t)(events[i].block_crc >> 8), (uint8_t)events[i].block_crc };
        if (!lunapath_step(out, events[i].branch, payload, 56u)) return false;
    }
    return true;
}

int main(void) {
    static const event workflow[] = {
        { false, 1u, 0u, 0u }, /* START */
        { true, 2u, 0u, 0u },  /* HEADER_OK */
        { true, 3u, 0u, 0u },  /* ERASE_OK */
        { false, 4u, 7u, UINT32_C(0x8a21d4c7) }, /* WRITE block */
        { true, 5u, 7u, UINT32_C(0x8a21d4c7) }, /* VERIFY */
        { true, 6u, 7u, UINT32_C(0x8a21d4c7) }  /* COMMIT */
    };
    lunapath_state expected, actual, damaged;
    REQUIRE(run(workflow, 6u, &expected)); REQUIRE(run(workflow, 6u, &actual));
    REQUIRE(lunapath_equal(&expected.path, &actual.path) && expected.guard == actual.guard);
    REQUIRE(run(workflow, 5u, &damaged)); REQUIRE(!lunapath_equal(&expected.path, &damaged.path));
    event altered[7]; memcpy(altered, workflow, sizeof workflow); altered[3] = altered[2]; altered[2] = workflow[3];
    REQUIRE(run(altered, 6u, &damaged)); REQUIRE(expected.guard != damaged.guard);
    memcpy(altered, workflow, sizeof workflow); altered[3].block_crc ^= 1u;
    REQUIRE(run(altered, 6u, &damaged)); REQUIRE(expected.guard != damaged.guard);
    memcpy(altered, workflow, sizeof workflow);
    altered[6] = workflow[5]; altered[5] = workflow[4]; altered[4] = workflow[3];
    altered[3] = workflow[2]; altered[2] = workflow[1];
    REQUIRE(run(altered, 7u, &damaged)); REQUIRE(!lunapath_equal(&expected.path, &damaged.path));
    puts("WORKFLOW PASS: valid, delete, duplicate, reorder, and payload corruption cases");
    static const event commands[] = {
        { true, 0x10u, 0u, 0u }, { false, 0x22u, 1u, 0x41u },
        { true, 0x31u, 1u, 0x42u }, { true, 0x40u, 1u, 0u }
    };
    REQUIRE(run(commands, 4u, &expected)); REQUIRE(run(commands, 4u, &actual));
    REQUIRE(lunapath_equal(&expected.path, &actual.path) && expected.guard == actual.guard);
    event reordered[4]; memcpy(reordered, commands, sizeof commands);
    event swap = reordered[1]; reordered[1] = reordered[2]; reordered[2] = swap;
    REQUIRE(run(reordered, 4u, &damaged)); REQUIRE(expected.guard != damaged.guard);
    puts("COMMAND PASS: deterministic command/event order and reordered command detection");
    return 0;
}
