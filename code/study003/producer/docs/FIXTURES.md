# Deterministic fixtures (design section 14) — NOT YET COMPILED OR RUN

`src/apps/torus_fixtures.cpp` implements all 16 prospective groups. It uses
hand-built tapes wherever an expected value must be derivable by hand, and
the non-production `fixture-r1` namespace wherever Threefry is needed. It
never uses production keys and never emits an estimate. It writes
`fixture_receipt.json` (status `ALL_FIXTURES_PASSED` or `FIXTURES_FAILED`)
and `fixture_key_derivation.json` to a new, explicit output directory.

| # | Group | How it is checked |
|---:|---|---|
| 1 | Official KATs | design all-zero/all-one vectors, plus every `threefry4x64 20` line of the pinned official `kat_vectors`; SHA-256 self-test |
| 2 | Address enumeration, collision audit, invariance | full `run_collision_audit` and `run_invariance_check` |
| 3 | Circular-distance wrap | (0,65535), (65535,0), equality, antipode 32,768 in several forms |
| 4 | Maximum losses | Q = 2^35, population 2^40, accuracy numerators 0 and 2^40 |
| 5 | 1- and 2-update hand trace | every family, tie by key, local replacement, label flips M→F and F→M, cache invalidation, cache of the parent's pre-update phenotype even when the cache probe won, SHAM writes but never reads caches; exact losses 3,935,000,800 / 1,536,000,000 / 1,024,000,000 |
| 6 | Target law | t = 1, 2 ignore R; direct, chained and doubly chained copies; ZERO/HALF share innovations (hand and Threefry-generated) |
| 7 | Memory better / worse | cache at wrap distance 1 (loss 32) vs. antipode (loss 2^35) against fresh 32,000,000 |
| 8 | Cache equals parent | both evaluated, identical losses, 128 queries |
| 9 | Invalid cache | ACTIVE invalid-M, ACTIVE F and SHAM all use exactly fresh(t,i) |
| 10 | Donor tie | equal losses with distinct torus phenotypes; lower key; lower family; strictly lower loss beats a larger key |
| 11 | Local child | flagged replacement equal to donor (flagged 3, changed 1); exact 1/32 and (1/32)(1−2^−16) over full domains |
| 12 | Tournament | duplicate entries, `&127` masking, cross-label equal-loss decided by key, forced key tie → index, one candidate wins 5 slots, label-complement invariance |
| 13 | Rank table | formula vs. Appendix A for all 128 ranks, Σ N_r = 2^28, exhaustive 128^4 enumeration through the production order primitive, best-candidate 0.988342165946960 / 0.366437715922037 |
| 14 | SHAM N1/N2 | full 256-update paths from ALL_F, ALL_M and a mixed assignment and its complement |
| 15 | Random-coordinate invariance | tape is a pure function of (block, update); label/arm invariance of candidates and entries; order/thread invariance |
| 16 | Regeneration | non-audit block 70: each single-cell regeneration is byte-identical to the full-block path and update rows |

Expected values in groups 3–13 were derived by hand from the frozen design
while writing the source; they have not been executed. A fixture failure is a
mechanical finding for source review; it may not change the design.
