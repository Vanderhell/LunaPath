#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "lunu_embedded.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static volatile uint32_t sink;
static uint32_t guard_value(const lunu_embed_state *s) {
#if LUNU_EMBED_GUARD == LUNU_EMBED_GUARD_NONE
    (void)s; return 0u;
#else
    return (uint32_t)s->guard;
#endif
}
static void report(const char *name, uint64_t ticks, unsigned iterations, LARGE_INTEGER freq) {
    printf("%-18s %.2f ns/op\n", name, (double)ticks * 1.0e9 / ((double)freq.QuadPart * iterations));
}

int main(void) {
    LARGE_INTEGER freq, start, end; QueryPerformanceFrequency(&freq);
    printf("profile path=%u guard=%u sizeof_path=%zu sizeof_guard=%zu sizeof_state=%zu guard_logical_bits=%u\n",
           (unsigned)LUNU_EMBED_PATH_BITS, (unsigned)LUNU_EMBED_GUARD, sizeof(lunu_embed_path),
#if LUNU_EMBED_GUARD == LUNU_EMBED_GUARD_NONE
           (size_t)0u,
#else
           sizeof(lunu_embed_guard),
#endif
           sizeof(lunu_embed_state),
#if LUNU_EMBED_GUARD == 0
           0u
#elif LUNU_EMBED_GUARD == 1
           1u
#elif LUNU_EMBED_GUARD == 2
           16u
#elif LUNU_EMBED_GUARD == 3
           32u
#else
           64u
#endif
    );
    enum { N = 1000000 };
    lunu_embed_path p, q; lunu_embed_path_zero(&p);
    for (unsigned i = 0; i < 32u; ++i) { lunu_embed_path tmp; (void)lunu_embed_child(&p, (i & 1u) != 0u, &tmp); p = tmp; }
    QueryPerformanceCounter(&start);
    for (unsigned i = 0; i < N; ++i) { (void)lunu_embed_child(&p, (i & 1u) != 0u, &q); sink += q.depth; }
    QueryPerformanceCounter(&end); report("child", (uint64_t)(end.QuadPart - start.QuadPart), N, freq);
    QueryPerformanceCounter(&start);
    for (unsigned i = 0; i < N; ++i) { (void)lunu_embed_parent(&p, &q); sink += q.depth; }
    QueryPerformanceCounter(&end); report("parent", (uint64_t)(end.QuadPart - start.QuadPart), N, freq);
    QueryPerformanceCounter(&start);
    for (unsigned i = 0; i < N; ++i) { (void)lunu_embed_neighbor(&p, i & 31u, &q); sink += q.word[0]; }
    QueryPerformanceCounter(&end); report("neighbor", (uint64_t)(end.QuadPart - start.QuadPart), N, freq);
    QueryPerformanceCounter(&start);
    for (unsigned i = 0; i < N; ++i) { (void)lunu_embed_prefix(&p, 16u, &q); sink += q.depth; }
    QueryPerformanceCounter(&end); report("prefix", (uint64_t)(end.QuadPart - start.QuadPart), N, freq);
    unsigned distance;
    QueryPerformanceCounter(&start);
    for (unsigned i = 0; i < N; ++i) { (void)lunu_embed_distance(&p, &p, &distance); sink += distance; }
    QueryPerformanceCounter(&end); report("distance", (uint64_t)(end.QuadPart - start.QuadPart), N, freq);
    uint8_t wire[34]; size_t written;
    QueryPerformanceCounter(&start);
    for (unsigned i = 0; i < N; ++i) { (void)lunu_embed_serialize(&p, wire, sizeof wire, &written); sink += (uint32_t)written; }
    QueryPerformanceCounter(&end); report("serialize", (uint64_t)(end.QuadPart - start.QuadPart), N, freq);
    (void)lunu_embed_serialize(&p, wire, sizeof wire, &written);
    QueryPerformanceCounter(&start);
    for (unsigned i = 0; i < N; ++i) { (void)lunu_embed_deserialize(wire, written, &q); sink += q.depth; }
    QueryPerformanceCounter(&end); report("deserialize", (uint64_t)(end.QuadPart - start.QuadPart), N, freq);
    static const unsigned sizes[] = { 0, 1, 4, 8, 16, 32, 64, 256 };
    uint8_t payload[256]; memset(payload, 0xa5, sizeof payload);
    for (unsigned s = 0; s < sizeof sizes / sizeof sizes[0]; ++s) {
        unsigned bytes = sizes[s]; unsigned iterations = bytes > 32u ? 100000u : N;
        QueryPerformanceCounter(&start);
        for (unsigned i = 0; i < iterations; ++i) { lunu_embed_state st; lunu_embed_state_zero(&st); (void)lunu_embed_step(&st, (i & 1u) != 0u, bytes ? payload : NULL, (uint16_t)(bytes * 8u)); sink += guard_value(&st); }
        QueryPerformanceCounter(&end); char label[32]; (void)snprintf(label, sizeof label, "step %u B", bytes);
        report(label, (uint64_t)(end.QuadPart - start.QuadPart), iterations, freq);
    }
    QueryPerformanceCounter(&start);
    for (unsigned i = 0; i < N; ++i) { lunu_embed_state st; lunu_embed_state_zero(&st); (void)lunu_embed_step_tag32(&st, (i & 1u) != 0u, i, 65535u); sink += guard_value(&st); }
    QueryPerformanceCounter(&end); report("step pre-tag", (uint64_t)(end.QuadPart - start.QuadPart), N, freq);
    return (int)(sink == UINT32_MAX);
}
