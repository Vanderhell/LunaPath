// SPDX-License-Identifier: Apache-2.0
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef LARGE_INTEGER benchmark_time;
static int benchmark_now(benchmark_time *time) { return QueryPerformanceCounter(time) != 0; }
static double elapsed_ns(benchmark_time start, benchmark_time end) {
    LARGE_INTEGER frequency;
    (void)QueryPerformanceFrequency(&frequency);
    return (double)(end.QuadPart - start.QuadPart) * 1.0e9 / (double)frequency.QuadPart;
}
#else
#define _POSIX_C_SOURCE 200809L
#include <time.h>
typedef struct timespec benchmark_time;
static int benchmark_now(benchmark_time *time) { return clock_gettime(CLOCK_MONOTONIC, time) == 0; }
static double elapsed_ns(benchmark_time start, benchmark_time end) {
    return (double)(end.tv_sec - start.tv_sec) * 1.0e9 + (double)(end.tv_nsec - start.tv_nsec);
}
#endif

#include "lunapath.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static volatile uint32_t sink;
static uint32_t guard_value(const lunapath_state *s) {
#if LUNAPATH_GUARD == LUNAPATH_GUARD_NONE
    (void)s; return 0u;
#else
    return (uint32_t)s->guard;
#endif
}
static void report(const char *name, benchmark_time start, benchmark_time end, unsigned iterations) {
    printf("%-18s %.2f ns/op\n", name, elapsed_ns(start, end) / iterations);
}

int main(void) {
    benchmark_time start, end;
    printf("profile path=%u guard=%u sizeof_path=%zu sizeof_guard=%zu sizeof_state=%zu guard_logical_bits=%u\n",
           (unsigned)LUNAPATH_PATH_BITS, (unsigned)LUNAPATH_GUARD, sizeof(lunapath_path),
#if LUNAPATH_GUARD == LUNAPATH_GUARD_NONE
           (size_t)0u,
#else
           sizeof(lunapath_guard),
#endif
           sizeof(lunapath_state),
#if LUNAPATH_GUARD == 0
           0u
#elif LUNAPATH_GUARD == 1
           1u
#elif LUNAPATH_GUARD == 2
           16u
#elif LUNAPATH_GUARD == 3
           32u
#else
           64u
#endif
    );
    enum { N = 1000000 };
    lunapath_path p, q; lunapath_path_zero(&p);
    for (unsigned i = 0; i < 32u; ++i) { lunapath_path tmp; (void)lunapath_child(&p, (i & 1u) != 0u, &tmp); p = tmp; }
    if (!benchmark_now(&start)) return 1;
    for (unsigned i = 0; i < N; ++i) { (void)lunapath_child(&p, (i & 1u) != 0u, &q); sink += q.depth; }
    if (!benchmark_now(&end)) return 1;
    report("child", start, end, N);
    if (!benchmark_now(&start)) return 1;
    for (unsigned i = 0; i < N; ++i) { (void)lunapath_parent(&p, &q); sink += q.depth; }
    if (!benchmark_now(&end)) return 1;
    report("parent", start, end, N);
    if (!benchmark_now(&start)) return 1;
    for (unsigned i = 0; i < N; ++i) { (void)lunapath_neighbor(&p, i & 31u, &q); sink += q.word[0]; }
    if (!benchmark_now(&end)) return 1;
    report("neighbor", start, end, N);
    if (!benchmark_now(&start)) return 1;
    for (unsigned i = 0; i < N; ++i) { (void)lunapath_prefix(&p, 16u, &q); sink += q.depth; }
    if (!benchmark_now(&end)) return 1;
    report("prefix", start, end, N);
    unsigned distance;
    if (!benchmark_now(&start)) return 1;
    for (unsigned i = 0; i < N; ++i) { (void)lunapath_distance(&p, &p, &distance); sink += distance; }
    if (!benchmark_now(&end)) return 1;
    report("distance", start, end, N);
    uint8_t wire[34]; size_t written;
    if (!benchmark_now(&start)) return 1;
    for (unsigned i = 0; i < N; ++i) { (void)lunapath_serialize(&p, wire, sizeof wire, &written); sink += (uint32_t)written; }
    if (!benchmark_now(&end)) return 1;
    report("serialize", start, end, N);
    (void)lunapath_serialize(&p, wire, sizeof wire, &written);
    if (!benchmark_now(&start)) return 1;
    for (unsigned i = 0; i < N; ++i) { (void)lunapath_deserialize(wire, written, &q); sink += q.depth; }
    if (!benchmark_now(&end)) return 1;
    report("deserialize", start, end, N);
    static const unsigned sizes[] = { 0, 1, 4, 8, 16, 32, 64, 256 };
    uint8_t payload[256]; memset(payload, 0xa5, sizeof payload);
    for (unsigned s = 0; s < sizeof sizes / sizeof sizes[0]; ++s) {
        unsigned bytes = sizes[s]; unsigned iterations = bytes > 32u ? 100000u : N;
        if (!benchmark_now(&start)) return 1;
        for (unsigned i = 0; i < iterations; ++i) { lunapath_state st; lunapath_state_zero(&st); (void)lunapath_step(&st, (i & 1u) != 0u, bytes ? payload : NULL, (uint16_t)(bytes * 8u)); sink += guard_value(&st); }
        if (!benchmark_now(&end)) return 1;
        char label[32]; (void)snprintf(label, sizeof label, "step %u B", bytes);
        report(label, start, end, iterations);
    }
    if (!benchmark_now(&start)) return 1;
    for (unsigned i = 0; i < N; ++i) { lunapath_state st; lunapath_state_zero(&st); (void)lunapath_step_tag32(&st, (i & 1u) != 0u, i, 65535u); sink += guard_value(&st); }
    if (!benchmark_now(&end)) return 1;
    report("step pre-tag", start, end, N);
    return (int)(sink == UINT32_MAX);
}
