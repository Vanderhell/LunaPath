#include "lunu_primitives.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_path(const lunu_path *p, unsigned target) {
    assert(p->depth == target);
    for (unsigned i = 0; i < p->depth; ++i) assert(lunu_path_get(p, i) == 0 || lunu_path_get(p, i) == 1);
    for (unsigned k = 0; k <= p->depth; ++k) {
        lunu_path q, r;
        assert(lunu_prefix(p, k, &q));
        assert(q.depth == k);
        assert(lunu_common_prefix(p, &q, &r) == k && lunu_equal(&q, &r));
    }
    if (p->depth) {
        lunu_path parent, child, neighbor;
        assert(lunu_parent(p, &parent));
        assert(lunu_child(&parent, lunu_path_get(p, p->depth - 1u), &child));
        assert(lunu_equal(p, &child));
        for (unsigned k = 0; k < p->depth; ++k) {
            bool valid = false;
            assert(lunu_neighbor(p, k, &neighbor));
            assert(lunu_distance(p, &neighbor, &valid) == 1 && valid);
            assert(lunu_neighbor(&neighbor, k, &child) && lunu_equal(p, &child));
        }
    }
}

static void exhaustive(unsigned target, const lunu_path *p) {
    if (p->depth == target) { test_path(p, target); return; }
    lunu_path q;
    assert(lunu_child(p, false, &q)); exhaustive(target, &q);
    assert(lunu_child(p, true, &q)); exhaustive(target, &q);
}

int main(void) {
    lunu_path root, p, q, r;
    lunu_path_zero(&root); assert(root.depth == 0);
    assert(!lunu_parent(&root, &q));
    assert(!lunu_path_get(&root, 0));
    assert(!lunu_path_set(&root, 0, true));
    assert(!lunu_prefix(&root, 1, &q));
    assert(lunu_child(&root, false, &p)); assert(lunu_path_get(&p, 0) == false);
    assert(lunu_path_set(&p, 0, true)); assert(lunu_path_get(&p, 0));
    assert(!lunu_path_set(&p, 1, true));
    assert(!lunu_neighbor(&p, 1, &q));
    assert(!lunu_prefix(&p, 257, &q));
    bool valid = true; assert(lunu_distance(&root, &p, &valid) == 0 && !valid);
    assert(lunu_common_prefix(&p, &p, &q) == p.depth && lunu_equal(&p, &q));
    assert(lunu_hash64("abc", 3) == lunu_hash64("abc", 3));

    for (unsigned n = 0; n <= 20; ++n) { lunu_path_zero(&p); exhaustive(n, &p); }
    lunu_path_zero(&p);
    for (unsigned n = 0; n < 256; ++n) assert(lunu_child(&p, (n & 1u) != 0, &p));
    assert(p.depth == 256); assert(!lunu_child(&p, false, &q));
    assert(lunu_parent(&p, &q)); assert(q.depth == 255); assert(lunu_child(&q, true, &r)); assert(r.depth == 256);

    for (unsigned n = 0; n <= 256; ++n) {
        lunu_path_zero(&p); for (unsigned i = 0; i < n; ++i) assert(lunu_child(&p, (i & 1u) != 0, &p));
        unsigned char enc[34], enc2[34]; size_t used = 0;
        assert(lunu_serialized_size(&p) == 2u + (n + 7u) / 8u);
        assert(lunu_serialize(&p, enc, sizeof enc, &used)); assert(lunu_deserialize(enc, used, &q));
        assert(lunu_equal(&p, &q)); assert(lunu_serialize(&q, enc2, sizeof enc2, &used)); assert(memcmp(enc, enc2, used) == 0);
        assert(!lunu_serialize(&p, enc, used - 1u, NULL));
    }
    assert(!lunu_deserialize(NULL, 0, &p)); assert(!lunu_deserialize((const unsigned char *)"\0", 1, &p));
    { unsigned char bad[34] = {1, 1}; assert(!lunu_deserialize(bad, sizeof bad, &p)); }
    { unsigned char bad[3] = {0, 1, 1}; assert(!lunu_deserialize(bad, sizeof bad, &p)); }
    { unsigned char bad[3] = {0, 1, 0x01}; assert(!lunu_deserialize(bad, sizeof bad, &p)); }

    { unsigned char data[32] = {0}; lunu_control_mode mode;
      for (mode = LUNU_CONTROL_NONE; mode <= LUNU_CONTROL_ROLLING_HASH; ++mode) {
          lunu_state s = {0}, before = {0};
          for (unsigned i = 0; i < 256; ++i) { before = s; data[i / 8u] = (unsigned char)i;
              assert(lunu_step_payload(&s, (i & 1u) != 0, data, 256, mode));
              assert(lunu_verify_step_payload(&before, &s, (i & 1u) != 0, data, 256, mode)); }
      }
    }
    puts("PASS: exhaustive 0..20, boundary 0..256, all public APIs and malformed inputs");
    return 0;
}
