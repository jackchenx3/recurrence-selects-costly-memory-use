# Output formats

All binary records are fixed-size and little-endian. Python `struct` formats are in
`tools/mmem_common.py`; a machine-readable copy is in `schemas/binary_records.json`.
Cell index = 4*arm + 2*law + start (ACTIVE=0/SHAM=1, ZERO=0/HALF=1, ALL_F=0/ALL_M=1).
Shard s holds blocks 1300*s … 1300*s+1299 in ascending order. Within a block, records are
cell-major, then update-major, then candidate-major.

## Production directory layout (`--out NEW_DIR`)

```
preflight/philox_kat.json        literal Random123 vectors, actual outputs, PASS/FAIL
preflight/fixtures.json          K0, S0, KEYS, COLLISION, F1–F10 results
preflight/purpose_keys.json      nine production key texts and key words
preflight/collision_audit.json   counter collision receipt
shards/shard_XX/updates.bin      per-update records, 1300*8*256 per shard
shards/shard_XX/paths.bin        per-path summaries, 1300*8 per shard
shards/shard_XX/blocks.bin       per-block records, 1300 per shard
shards/shard_00/audit_rows_blocks_0000_0063.bin   candidate rows, blocks 0–63
run_manifest.json                hashes, counts, timing, compiler, output SHA-256 list
```

The Slurm template adds `job_environment.txt`, `job_request.txt`, `python_preflight.json`,
`production_exit_status.txt` and the job stdout/stderr in the caller's directory.

## Per-update record (24 bytes; 85,196,800 records; 2,044,723,200 bytes)

| off | type | field |
|---|---|---|
| 0 | u32 | block |
| 4 | u16 | update (1–256) |
| 6 | u8 | cell |
| 7 | u8 | M count after mutation/cache transition |
| 8 | u16 | total population mismatch of survivors vs T_t (0–1024) |
| 10 | u8 | parents labelled M with valid cache at start of update (diagnostic, any arm) |
| 11 | u8 | cache-probe use (policy probes taken from cache; 0 in SHAM) |
| 12 | u8 | cache-probe survivors |
| 13 | u8 | F→M count |
| 14 | u8 | M→F count |
| 15 | u8 | query count (must be 128) |
| 16 | u64 | survival retries: sum of the 32 accepted u32 retry indices (exact, < 2^37) |

Python struct `<IHBBHBBBBBBQ`; no reserved fields.

## Per-path summary (128 bytes; 332,800 records; 42,598,400 bytes)

| off | type | field |
|---|---|---|
| 0 | u32 | block |
| 4 | u8×4 | cell, arm, law, start |
| 8 | u32 | late M sum over updates 193–256 (L = sum/2048) |
| 12 | u32 | late mismatch sum over 193–256 (accuracy P = 1 − sum/65536) |
| 16 | u32 | total queries (32768) |
| 20–32 | u32×4 | F→M, M→F, cache-probe use, cache-probe survivors (path totals) |
| 36 | u64 | survival retries, path total (exact, < 2^45) |
| 44 | u16×4 | updates with M fixation, M extinction, late fixation, late extinction |
| 52 | 32 bytes | final-state SHA-256 (see below) |
| 84 | u32 | final M count |
| 88 | u32 | final total mismatch |
| 92 | u32 | reserved (0) |
| 96 | 32 bytes | paired-trajectory SHA-256 (see below) |

Python struct `<IBBBBIIIIIIIQHHHH32sIII32s`.

Final-state hash: SHA-256 of ASCII `MMEM-FINAL-STATE-V1`, block u32, cell u8, completed updates
u32, then for each slot genotype u32, label u8, cache-valid u8, cache u32, then T_256 u32, T_255 u32.

Paired-trajectory hash (audit evidence only; never read by a scientific operator): SHA-256 of
ASCII `MMEM-PAIRED-TRAJECTORY-V1`, block u32, arm u8, law u8 (cell and start are excluded), then
after every completed update, all little-endian: update u16, target u32, recurrence bit R_t u8,
recurrence applied u8, total mismatch u16; the 32 survivor candidate indices as u8 in
survivor-slot order; the 32 post-update genotypes as u32 in slot order; one u32 post-update label
mask (bit i = label of slot i, 1 = M), XORed with `0xffffffff` for an ALL_M start and unchanged
for ALL_F. For correct SHAM paths, records 4/5 and 6/7 have identical paired hashes, which jointly
binds the target, survivor-index, genotype and slotwise label-complement trajectories (N1 and N2).
`merge_analyze.py` requires both equalities in every block.

## Per-block record (96 bytes; 41,600 records)

| off | type | field |
|---|---|---|
| 0 | u32 | block |
| 4 | u8 | N1 flag (1 = pass) |
| 5 | u8 | N2 flag |
| 6 | u8 | all 2048 path-updates had 128 queries |
| 7 | u8 | audit block (block < 64) |
| 8 | i32 | C_abs numerator (/4096) = L2+L3−2048 |
| 12 | i32 | C_rec numerator (/4096) = (L2+L3)−(L0+L1) |
| 16 | i32 | D_HALF numerator (/2048) = L3−L2 |
| 20 | i32 | D_ZERO numerator (/2048) = L1−L0 |
| 24 | i32 | P_abs numerator (/131072) = (P6+P7)−(P2+P3) on mismatch sums |
| 28 | i32 | P_rec numerator (/131072) = P_abs_num − [(P4+P5)−(P0+P1)] |
| 32 | u32×8 | late M sums by cell (Lc) |
| 64 | u32×8 | late mismatch sums by cell (Pc) |

## Audit candidate row (72 bytes; blocks 0–63; 16,777,216 rows; 1,207,959,552 bytes)

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
| 12 | u32 | parent cache (0 if invalid) |
| 16 | u32 | candidate genotype |
| 20 | u32 | target T_t |
| 24 | u8 | mismatch |
| 25 | u8 | recurrence bit R_t |
| 26 | u8 | recurrence applied (HALF, t ≥ 3, R_t = 1) |
| 27 | u8 | probe source (family 1: 0 fresh, 1 cache; else 255) |
| 28 | u64 | exact integer weight 2^(32−h) |
| 36 | u8 | selected rank = survivor slot (255 if not selected) |
| 37 | u8 | post-mutation label (selected only, else 255) |
| 38 | u8 | policy flip (selected only, else 255) |
| 39 | u8 | donor family (family 3 only, else 255) |
| 40 | u64 | W_d at the draw that selected this row (else 0) |
| 48 | u64 | accepted 64-bit x (else 0) |
| 56 | u64 | Z_d (else 0) |
| 64 | u32 | accepted retry index (else 0) |
| 68 | u32 | reserved |

## Analysis outputs (`merge_analyze.py --out NEW_DIR`)

* `estimates_22.json`: records 1–4 primary (C_abs, C_rec, D_HALF, D_ZERO) with exact fraction,
  60-digit decimal estimate, half-width, bounds and family decision; records 5–6 secondary
  (P_abs, P_rec) with classification and a descriptive relation to allele enrichment; records
  7–14 late M frequency per cell; records 15–22 late accuracy per cell (descriptive, no bound).
* `decision.json`: primary decision, rule applied, adverse-selection flag, secondary
  classifications, mandatory interpretation labels.
* `diagnostics_descriptive.json`: per-cell mutation counts, cache-probe use/survival,
  fixation/extinction occupancy, retries, query total.
* On any validity failure only `ROUTE_CLOSED_INVALID.json` is written; no estimate is computed.
* `ANALYSIS_LOCK.json` in the production directory makes analysis single-shot.

## Output budget

Records total 3,299,274,752 bytes (~3.07 GiB) = 85,196,800×24 (updates, 2,044,723,200) +
332,800×128 (paths, 42,598,400) + 41,600×96 (blocks, 3,993,600) + 16,777,216×72 (audit rows,
1,207,959,552); receipts are kilobytes. The driver computes this projection from compiled
constants (`kTotalRecordBytes`) and refuses to start if it exceeds half of the 100 GiB ceiling.
