# Output formats (PHASE2-PERFORMANCE-CONVERSION-002)

All binary records are fixed-size and little-endian. Python `struct` formats are in
`tools/mmem_common.py`; a machine-readable copy is in `schemas/binary_records.json`.
Cell index = 2*arm + start (INFO=0, NONINFO=1, SHAM=2; ALL_F=0, ALL_M=1):
0 INFO|ALL_F, 1 INFO|ALL_M, 2 NONINFO|ALL_F, 3 NONINFO|ALL_M, 4 SHAM|ALL_F, 5 SHAM|ALL_M.
Shard s holds blocks 1300*s … 1300*s+1299 in ascending order. Within a block, records are
cell-major, then update-major, then candidate-major. Probe-source codes: 0 fresh mask,
1 true cache `c`, 2 decoy `x XOR pi(c XOR x)`, 255 not a policy probe.

## Production directory layout (`--out NEW_DIR`)

```
preflight/philox_kat.json        literal Random123 vectors, actual outputs, PASS/FAIL
preflight/fixtures.json          K0, S0, KEYS, COLLISION, F1–F13 results
preflight/purpose_keys.json      ten production key texts and key words
preflight/collision_audit.json   counter collision receipt (six paired cells)
shards/shard_XX/updates.bin      per-update records, 1300*6*256 per shard
shards/shard_XX/paths.bin        per-path summaries, 1300*6 per shard
shards/shard_XX/blocks.bin       per-block records, 1300 per shard
shards/shard_00/audit_rows_blocks_0000_0063.bin           candidate rows, blocks 0–63
shards/shard_00/audit_permutations_blocks_0000_0063.bin   shared permutations, blocks 0–63
run_manifest.json                hashes (spec, review, design-GO, config), counts, timing, compiler, outputs
```

The Slurm template adds `job_environment.txt`, `job_request.txt`, `python_preflight.json`,
`production_exit_status.txt` and the job stdout/stderr in the caller's directory.

## Per-update record (48 bytes; 63,897,600 records; 3,067,084,800 bytes)

| off | type | field |
|---|---|---|
| 0 | u32 | block |
| 4 | u16 | update (1–256) |
| 6 | u8 | cell |
| 7 | u8 | M count after mutation/cache transition |
| 8 | u16 | total population mismatch of survivors vs T_t (0–1024) |
| 10 | u8 | parents labelled M with valid cache at start of update (diagnostic, any arm) |
| 11 | u8 | valid-probe use: policy probes derived from the cache (true or decoy; 0 in SHAM) |
| 12 | u8 | valid-probe survivors (true or decoy) |
| 13 | u8 | F→M count |
| 14 | u8 | M→F count |
| 15 | u8 | query count (must be 128) |
| 16 | u64 | survival retries: sum of the 32 accepted u32 retry indices (exact, < 2^37) |
| 24 | u8 | recurrence applied (t ≥ 3 and R_t = 1; identical in all six cells) |
| 25 | u8 | true-cache use (probe = c) |
| 26 | u8 | decoy use (probe = x XOR pi(c XOR x); NONINFO recurrent updates only) |
| 27 | u8 | true-cache probe survivors |
| 28 | u8 | decoy probe survivors |
| 29 | u8 | valid-M parents with displacement weight 0 (diagnostic, any arm) |
| 30 | u8 | valid-M parents with displacement weight 32 (diagnostic, any arm) |
| 31 | u8 | decoy applications at displacement weight 0 (direction retained) |
| 32 | u8 | decoy applications at displacement weight 32 (direction retained) |
| 33 | u8 | decoy applications whose probe equals the true cache (includes 31 and 32) |
| 34 | u16 | reserved (0) |
| 36 | u32 | DECOY_PERMUTATION retry total for this (block, update) — shared evidence |
| 40 | u32 | FNV-1a 32 of perm[0..31] for this (block, update) — shared evidence |
| 44 | u32 | reserved (0) |

Python struct `<IHBBHBBBBBBQBBBBBBBBBBHIII`.

## Per-path summary (176 bytes; 249,600 records; 43,929,600 bytes)

| off | type | field |
|---|---|---|
| 0 | u32 | block |
| 4 | u8×4 | cell, arm, start, reserved (0) |
| 8 | u32 | late M sum over updates 193–256 (L = sum/2048) |
| 12 | u32 | late mismatch sum over 193–256 (accuracy P = 1 − sum/65536) |
| 16 | u32 | total queries (32768) |
| 20–32 | u32×4 | F→M, M→F, valid-probe use, valid-probe survivors (path totals) |
| 36 | u64 | survival retries, path total (exact, < 2^45) |
| 44 | u16×4 | updates with M fixation, M extinction, late fixation, late extinction |
| 52 | 32 bytes | final-state SHA-256 (see below) |
| 84 | u32 | final M count |
| 88 | u32 | final total mismatch |
| 92 | u32 | reserved (0) |
| 96 | 32 bytes | paired-trajectory SHA-256 (see below) |
| 128–160 | u32×9 | true-cache use, decoy use, true-cache survivors, decoy survivors, valid-M weight-0, valid-M weight-32, decoy weight-0, decoy weight-32, decoy identical to cache (path totals of update offsets 25–33) |
| 164 | u32 | recurrent updates (sum of update offset 24) |
| 168 | u64 | DECOY_PERMUTATION retry total over the path (sum of update offset 36) |

Python struct `<IBBBBIIIIIIIQHHHH32sIII32sIIIIIIIIIIQ`.

Final-state hash: SHA-256 of ASCII `PCONV-FINAL-STATE-V1`, block u32, cell u8, completed updates
u32, then for each slot genotype u32, label u8, cache-valid u8, cache u32, then T_256 u32, T_255 u32.

Paired-trajectory hash (audit evidence only; never read by a scientific operator): SHA-256 of
ASCII `PCONV-PAIRED-TRAJECTORY-V1`, block u32, arm u8 (cell and start are excluded), then after
every completed update, all little-endian: update u16, target u32, recurrence bit R_t u8,
recurrence applied u8, total mismatch u16; the 32 survivor candidate indices as u8 in
survivor-slot order; the 32 post-update genotypes as u32 in slot order; one u32 post-update label
mask (bit i = label of slot i, 1 = M), XORed with `0xffffffff` for an ALL_M start and unchanged
for ALL_F. This is the study-001 content with the law byte removed (single law). For correct SHAM
paths, records 4/5 have identical paired hashes, which jointly binds the target, survivor-index,
genotype and slotwise label-complement trajectories (N1 and N2). `merge_analyze.py` requires the
equality in every block.

## Per-block record (96 bytes; 41,600 records; 3,993,600 bytes)

L_c and P_c are the late M and mismatch sums of cell c.

| off | type | field |
|---|---|---|
| 0 | u32 | block |
| 4 | u8 | N1 flag (1 = pass) |
| 5 | u8 | N2 flag |
| 6 | u8 | all 1536 path-updates had 128 queries |
| 7 | u8 | audit block (block < 64) |
| 8 | i32 | Delta_P numerator (/131072) = (P2+P3) − (P0+P1) |
| 12 | i32 | D_INFO numerator (/2048) = L1 − L0 |
| 16 | i32 | D_NONINFO numerator (/2048) = L3 − L2 |
| 20 | i32 | E_INFO numerator (/4096) = (L0+L1) − (L2+L3) |
| 24 | i32 | E_NONINFO numerator (/4096) = (L2+L3) − 2048 |
| 28 | i32 | B_INFO numerator (/131072) = (P4+P5) − (P0+P1) |
| 32 | i32 | B_NONINFO numerator (/131072) = (P4+P5) − (P2+P3) |
| 36 | u32×6 | late M sums by cell (L_c) |
| 60 | u32×6 | late mismatch sums by cell (P_c) |
| 84 | u8 | C1 flag: INFO/NONINFO coupling identity held for both starts |
| 85 | u8 | reserved (0) |
| 86 | u16 | first update at which a NONINFO\|ALL_F decoy probe differed from the true cache (0 = never) |
| 88 | u16 | same for ALL_M |
| 90 | u16 | recurrent updates in this block |
| 92 | u32 | DECOY_PERMUTATION retry total for the block |

Because accuracy is 1 − mismatch/65536 per cell, an accuracy difference has the opposite sign of
the corresponding mismatch difference; the numerators above already carry the accuracy sign.

## Audit candidate row (88 bytes; blocks 0–63; 12,582,912 rows; 1,107,296,256 bytes)

| off | type | field |
|---|---|---|
| 0 | u32 | block |
| 4 | u16 | update |
| 6 | u8 | cell |
| 7 | u8 | candidate index = 32*family + parent |
| 8 | u8 | family (0 parent, 1 policy probe, 2 scout, 3 local child) |
| 9 | u8 | producing parent slot |
| 10 | u8 | producing parent's pre-update (inherited) label |
| 11 | u8 | parent cache valid |
| 12 | u32 | parent true cache c (0 if invalid) |
| 16 | u32 | candidate genotype (for family 1: the actual policy probe) |
| 20 | u32 | target T_t |
| 24 | u8 | mismatch |
| 25 | u8 | recurrence bit R_t |
| 26 | u8 | recurrence applied (t ≥ 3, R_t = 1) |
| 27 | u8 | probe source (family 1: 0 fresh, 1 true cache, 2 decoy; else 255) |
| 28 | u64 | exact integer weight 2^(32−h) |
| 36 | u8 | selected rank = survivor slot (255 if not selected) |
| 37 | u8 | post-mutation label (selected only, else 255) |
| 38 | u8 | policy flip (selected only, else 255) |
| 39 | u8 | donor family (family 3 only, else 255) |
| 40 | u64 | W_d at the draw that selected this row (else 0) |
| 48 | u64 | accepted 64-bit survival x (else 0) |
| 56 | u64 | Z_d (else 0) |
| 64 | u32 | accepted survival retry index (else 0) |
| 68 | u32 | shared permutation identifier = block*256 + (update−1) (index into the permutation file) |
| 72 | u32 | parent genotype x |
| 76 | u32 | true displacement mask c XOR x (0 if cache invalid) |
| 80 | u8 | parent distance popcount(genotype XOR x) |
| 81 | u8 | true displacement weight (255 if cache invalid) |
| 82 | u16 | reserved (0) |
| 84 | u32 | applied mask: genotype = base XOR mask, base = x for families 0–2 and the donor genotype for family 3 (family 0: 0; family 1: fresh mask, c XOR x, or pi(c XOR x); family 2: scout mask; family 3: local mask) |

## Audit permutation record (416 bytes; blocks 0–63 × updates 1–256; 16,384 records; 6,815,744 bytes)

| off | type | field |
|---|---|---|
| 0 | u32 | block |
| 4 | u16 | update |
| 6 | u8 | Fisher–Yates steps whose accepted retry is nonzero |
| 7 | u8 | reserved (0) |
| 8 | u32 | sum of accepted retry indices over the 31 steps |
| 12 | u8×32 | perm[0..31]: source bit s → destination perm[s] |
| 44 | u64×31 | accepted x for steps i = 31, 30, …, 1 |
| 292 | u32×31 | accepted retry (subindex) for steps i = 31, 30, …, 1 |

Replay: start from [0..31]; for i = 31..1 with bound n = i+1, check low64(x·n) ≥ (2^64 − n) mod n,
set j = high64(x·n), swap perm[i] and perm[j]; the result must equal the stored perm. An
independent auditor with the key derivation can also regenerate each x from
`PHASE2-PERFORMANCE-CONVERSION-002|production-r1|DECOY_PERMUTATION` at counter (block, update, i, retry)
and confirm that every lower retry was rejected.

## Independent 64-block replay (what the saved rows support)

For every audit path a non-implementing reviewer can, using only these files: rebuild each
candidate genotype from the parent genotype, true cache, shared permutation and applied masks;
check the probe-source rule (INFO: true cache for valid M; NONINFO: true cache when t < 3 or
R_t = 0, decoy otherwise; SHAM: fresh), the decoy's parent distance, and that fresh, scout and local
masks are identical across the six cells; recompute every mismatch and weight; replay each Lemire
survival draw and sequential selection; follow label inheritance, policy flips and cache transitions
into the next update; recompute each per-update record, the paired-trajectory hash and the per-path
and per-block records; and reconstruct all 19 estimates from the block records.

## Analysis outputs (`merge_analyze.py --out NEW_DIR`)

* `estimates_19.json`: records 1–3 primary (Delta_P, D_INFO, D_NONINFO) with exact fraction,
  60-digit decimal estimate, half-width, bounds and family decision; records 4–5 allele family
  (E_INFO, E_NONINFO) and 6–7 performance family (B_INFO, B_NONINFO) with a classification at the
  1/32 scale that cannot alter the primary decision; records 8–13 late M frequency per cell;
  records 14–19 late accuracy per cell (descriptive, no bound).
* `decision.json`: primary decision, rule applied, the rule-5 "wholly inside (−1/32, +1/32)" flag,
  secondary classifications, the interpretive branch and the mandatory interpretation labels.
* `diagnostics_descriptive.json`: per-cell mutation counts, true-cache and decoy use and survival,
  weight-0/32 counts, permutation retries, fixation/extinction occupancy, retries, query total and
  decoupling counts.
* On any validity failure only `ROUTE_CLOSED_INVALID.json` is written; no estimate is computed.
* `ANALYSIS_LOCK.json` in the production directory makes analysis single-shot.

## Output budget

Records total 4,229,120,000 bytes (~3.94 GiB) = 63,897,600×48 (updates, 3,067,084,800) +
249,600×176 (paths, 43,929,600) + 41,600×96 (blocks, 3,993,600) + 12,582,912×88 (audit rows,
1,107,296,256) + 16,384×416 (audit permutations, 6,815,744); receipts are kilobytes. The driver
computes this projection from compiled constants (`kTotalRecordBytes`), refuses to start if it
exceeds half of the 100 GiB ceiling, and marks the manifest as a failure if the written total differs.
