# RESULT

## Fixed defects

- `lunu_path_set()` now changes only `index < depth`; append is exclusively `child()`/`STEP`.
- `child()` no longer relies on the public setter, so append works at depth 255 → 256.
- `equal()` and `distance()` operate only on logical bits; unused backing bits cannot change identity or distance.
- `lunu_verify_step_payload()` independently checks depth, prefix, appended bit, step counter and control value; it does not call `lunu_step()`.
- Added a C17-safe payload API accepting pointer plus bit length (1..256 bits) and four explicit control modes.
- Test initializer is `lunu_state s = {0}`.

## Final API

The original digest API remains as a compatibility wrapper. New experiments use `lunu_step_payload()` with `LUNU_CONTROL_NONE`, `PARITY`, `PARITY_SEQUENCE`, or `ROLLING_HASH`. Payload hashing is deterministic FNV-1a 64-bit; this is not cryptographic authentication.

## Build results

- GCC 16.1.0, `-std=c17 -O2 -Wall -Wextra -Wpedantic -Werror`: PASS.
- Clang 22 strict build: BLOCKED by MinGW/Clang system-header conflict (`include_next` and duplicate `max_align_t`), before project diagnostics. This is not reported as PASS.
- ASan/UBSan: BLOCKED at link (`-lasan` and `-lubsan` unavailable).

## Exhaustive tests

PASS: `lunu_tests_gcc.exe` completed all paths through depth 20, all depths 0..256, explicit 255→256 success and 256→257 failure, parent/child, prefixes, common prefixes, neighbors, distances, equality, serialization round trips, and malformed serialization.

## Property tests

PASS: independent Python oracle completed a boundary sweep over depths 0..20 and 1,000,000 reproducible cases over depths 0..256, with seed `0x4C554E55` and all four controls.

## Fault injection

The C API has deterministic structural rejection for wrong depth, prefix, appended bit, step count, payload length, and malformed serialization. The executed model fault audit covered 66 mutations per mode (path bit, payload bit, deleted step, duplicated step): mode NONE detected 34/66, parity 66/66, sequence 66/66, rolling hash 66/66; false positives were 0 in all four runs. Payload errors before digesting are undetectable. Digest collisions and parity cancellations remain probabilistic/undetectable respectively; no guaranteed integrity claim is made.

## Reference cross-check

`reference_lunu.py` independently implements the same FNV-1a, payload parity, sequence term, rotate, path and serialization equations. PASS: the C vector stream and Python verifier matched 256 consecutive control vectors. The million-case Python property run also exited 0.

## Sanitizers

NOT RUN: GCC has no ASan/UBSan runtime libraries available for linking in this environment.

## Benchmarks

Measured with GCC `-O2`, one million iterations on the current host:

```text
child: 8.000 ns/op
parent+prefix+neighbor: 5.333 ns/op each
step rolling 256-bit payload: 319.000 ns/op
sizeof(lunu_path)=40
sizeof(lunu_state)=56
```

The control state costs 16 bytes over the path and rolling control is materially more expensive than topology-only operations. These are coarse `clock()` measurements, not a portable performance claim.

## Newly discovered properties

The useful property is allocation-free refinement with explicit depth and canonical wire format. It is operationally convenient, but it is not a new mathematical structure: it remains a length-tagged bitstring/implicit binary tree. No non-trivial advantage over that baseline was demonstrated.

## Weaknesses

- Parity controls have cancellations and poor detection guarantees.
- Rolling FNV is a checksum, not a MAC; adversarial integrity requires a cryptographic construction outside this primitive.
- A fixed 256-bit backing array costs 40 bytes even for short paths.
- Hamming neighbors are meaningful only when dimensions are semantically comparable.
- Clang and sanitizers are not completed in this host.

## Comparison against plain bitstring

Plain `(depth, bitstring)` already provides parent, child, prefix, common ancestor, serialization and Hamming distance. Lunu adds a small, explicit API and optional streaming control state, but no demonstrated asymptotic or semantic advantage. For a single native integer, the plain integer baseline is smaller and faster.

## Final verdict

**WEAK**

The implementation is now stricter, canonical, independently specified and substantially better tested. The primitive is useful as a disciplined embedded encoding for binary refinement, but the audit does not justify `USEFUL` as a standalone new primitive because its core is equivalent to an explicitly depth-tagged bitstring and its control layer is optional, probabilistic and costly.
