# Threefry addressing, key derivation and collision audit

Implementation: `src/torus/threefry_rng.cpp`, `tape.cpp`, `target_law.cpp`,
`collision.cpp`. The only Threefry implementation is the pinned public
Random123 header `third_party/random123/include/Random123/threefry.h`
(commit `9545ff6413f258be2f04c1d319d99aaef7521150`, SHA-256
`4c210b32…2a72`), called as `r123::Threefry4x64_R<20>`.

## Keys

For namespace `NS` and purpose `P`:
`key = SHA-256("PHASE2-TORUS-MEMORY-003|NS|P")`, key word `j` = little-endian
u64 of digest bytes `8j..8j+7`. Namespaces: `production-r1` (production and
production regeneration only), `fixture-r1` (fixtures, invariance checks),
`timing-r1` (timing only). All 33 keys must be pairwise distinct; this is
checked at every start and in the collision audit.

## Counters and extraction

Every call uses counter `(block, update, entity, subindex)`. A 32-coordinate
vector uses two calls (q = 0, 1); coordinate `16q + 4*word + lane` receives
`(output_word >> 16*lane) & 0xffff`.

| Purpose | Counter | Use |
|---|---|---|
| INITIAL_VECTOR | (block, 0, parent 0–31, q) | initial phenotype |
| TARGET_INNOVATION_VECTOR | (block, t 1–256, 0, q) | I_t |
| TARGET_COPY | (block, t 1–256, 0, 0) | R_t = word0 & 1 (also drawn at t = 1, 2) |
| FRESH_VECTOR | (block, t, parent, q) | fresh(t,i) |
| SCOUT_VECTOR | (block, t, parent, q) | scout(t,i) |
| LOCAL_REPLACE_FLAG | (block, t, parent, q) | replace iff lane & 31 == 0 |
| LOCAL_REPLACE_VALUE | (block, t, parent, q) | replacement value |
| DONOR_KEY | (block, t, 3·parent + family, 0), family 0–2 | word0 |
| TOURNAMENT_ENTRY | (block, t, survivor slot, entry 0–3) | word0 & 127 |
| CANDIDATE_TIE_KEY | (block, t, candidate 0–127, 0) | word0 |
| POLICY_MUTATION | (block, t, survivor slot, 0) | flip iff word0 & 31 == 0 |

Every draw passes `check_address_domain`, which aborts on any value outside
the declared domain. Draws are generated once per (block, update) into an
`UpdateTape`, unconditionally and independently of arm, law, start, label,
cache or branch, and the same tape is applied to all eight cells.

## Collision audit (runs before any production path)

1. The declared schema is enumerated directly from the table above
   (independently of the generator code): 164,672 addresses per block
   (64 + 256 × 643), 6,850,355,200 in total.
2. Every declared address lies in its domain; no address repeats within a
   block (sorted uniqueness).
3. For every block 0..41,599 the enumeration has counter word 0 = block and
   is otherwise identical to block 0's, so no address repeats across blocks.
4. For blocks {0, 1, 63, 64, 20800, 41599} the generator's actual calls are
   recorded and must equal the declared set exactly, with no repeated call.
5. The vector extraction is a bijection of 32 coordinates onto (q, word, lane).
6. All 33 purpose keys are pairwise distinct (purpose separation).

The invariance check (`run_invariance_check`, fixture keys only) compares
byte-identical outputs under canonical, reversed and permuted cell evaluation
order (the last with an independently regenerated tape per cell), and under
1 vs. N worker threads.
