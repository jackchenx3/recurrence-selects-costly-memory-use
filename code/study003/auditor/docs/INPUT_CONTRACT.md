# Input contract: independent PHASE2-TORUS-MEMORY-003 auditor

Status: **source only, uncompiled, never run.** This document says what the auditor accepts as input and what it checks. Where the frozen saved-record interface (`BINARY_SCHEMAS.md`, SHA-256 `fce058ab…3301`) leaves a detail open, section 7 lists the reading this auditor uses. A source reviewer must confirm or reject each of those readings before any fixture stage. If one is wrong, an honest record set fails closed as INVALID. The auditor never silently accepts it.

## 1. Authorities

| Item | SHA-256 |
|---|---|
| Frozen design `PHASE2-TORUS-MEMORY-003-REV1.md` | `0ff662dc31807b0b2cdcae371d884473ed398e2583272d94331bac8c8f9ff0b0` |
| Design GO record | `e5dc485dd858f18eb9652cfe6b63f142e2d09fb6b069d640438cd8dbe63a786d` |
| Frozen config | `73648abca608d7bffa7344cb456147c856c7130bb57cc269f87cde4acefedb0e` |
| Binary schema contract | `fce058ab01557e861f4d88ffc9e9baa8a05f0f21e5dfc8911068d801b3123301` |
| Auditor input interface | `10b70feae459bcce2f23f484304fc37e745f2bc465658806d947bdd85ac4d8b2` |

The scientific operators come only from the design. The byte layout comes only from the schema contract. The auditor cannot change blocks, cells, updates, parameters, bounds, estimands, decision order or claims.

## 2. Record tree

`--root` names one directory that contains exactly the following:

```
updates/chunk_KKKKK.t3u   KKKKK = 00000..00649   (T3UPDATE, 131,072 rows each)
paths/chunk_KKKKK.t3p                          (T3PATH,   512 rows each)
blocks/chunk_KKKKK.t3b                         (T3BLOCK,  64 rows each)
estimates.t3s                                  (T3ESTIM,  22 rows)
audit/audit_candidates.t3c                     (T3CANDID, 16,777,216 rows)
audit/audit_entries.t3e                        (T3ENTRY,  16,777,216 rows)
audit/audit_context.t3x                        (T3CONTXT, 131,072 rows)
```

Any other entry under `updates/`, `paths/`, `blocks/` or `audit/` is INVALID. So is any symlink, any non-regular file, or a missing, short or trailing file. The fixture profile (`fixture-r1`, one chunk of K blocks with K in 1..64) uses the same names, with chunk 00000 only.

## 3. Manifest

`--manifest` is a sha256sum-format text file. Its rules:

- Each line is `<64 lowercase hex><space><space or *><relative path>`.
- Lines end in LF and the file must end with a final LF.
- One leading `./` on a path is tolerated.
- Paths may use only `[A-Za-z0-9._-]` and `/`, with no `.` or `..` components.
- Duplicate paths are INVALID, and so is an empty manifest.

Every record file in section 2 must appear. Every file listed is opened read-only and hashed. Its hash must match before verification, and its hash, size, mtime and inode must be unchanged afterwards. The manifest's own SHA-256 is recorded and checked again at the end.

## 4. Common header (both tools, byte-exact)

| Field | Value |
|---|---|
| magic | 8-byte NUL-padded magic for the record type |
| version | 1 |
| endian marker | `04 03 02 01` |
| row_size | the size for the record type |
| header_size | 64 |
| row_count | the exact expected count |
| namespace | NUL-padded |
| chunk_index | `k` for chunk files, `0xFFFFFFFF` for audit and estimate files |
| first_block | `64k` for chunk files, 0 for others |
| block_count | 64 for chunk files (K in the fixture profile); 64 or K for audit files; 41,600 or K for `estimates.t3s` |
| reserved | 0 |

The file size must equal `64 + row_count × row_size`.

## 5. Python verifier (`python/torus_record_verifier.py`)

It parses no-simulation records only.

**Per-update rows (all 85,196,800):**

- Order is block, then cell 0..7, then update 1..256.
- `query_count` must be 128.
- Domains:
  - `pop_loss ≤ 2^40`
  - `local_changed ≤ local_flagged ≤ 1024`
  - `m_count ≤ 32`
  - `1 ≤ distinct_winners ≤ 32`
- `valid_cache_pre` equals the previous update's `m_count` (0 at update 1), because every post-mutation M holds a valid cache and every F an invalid one.
- In ACTIVE, `cache_probe_uses = valid_cache_pre`. In SHAM, both cache fields are 0.
- Mutation counts must agree with the inherited-label count. At update 1 the inherited labels are all equal to the start label.

**Path rows:**

- Every summary is recomputed from the 256 update rows.
- `total_queries` is 32,768.
- `update_records_sha256` is recomputed over the 256 serialized 24-byte rows.
- `cell/arm/law/start` follow `cell = 4·arm + 2·law + start`.

**Block rows (all 41,600):**

- N1 for each law: the label-blind digest and the label-blind update columns are identical between SHAM starts.
- N2 for each law: label digest = complement digest of the other start (both ways), and `f_to_m` and `m_to_f` are swapped between the starts.
- Bit 4: per-update SHAM M counts sum to 32 and the late sums total 2048.
- Bit 5: all queries are 128.
- The recomputed flags must equal 0x3F, and the stored row must match byte for byte, including all six numerators.

**Estimates (22):** numerator sums are rebuilt from the block-level integers. Each estimate is `Σ / (n · denominator)`.

| Estimates | Denominator | Range |
|---|---|---|
| C_abs | 4096 | 1 |
| C_rec, D_HALF, D_ZERO | 4096 | 2 |
| P_abs | 2^47 | 2 |
| P_rec | 2^47 | 4 |
| M frequency for cell c (numerator `s_c`) | 2048 | 1 |
| Accuracy for cell c (numerator `2^46 − L_c`) | 2^46 | 1 |

- The Hoeffding half-width is `R·sqrt(ln(2/α)/(2n))`, with α = 1/80 for the primary family and 1/40 for the performance family. It is evaluated with 120-digit `decimal` and bracketed to ±10^-100.
- In production, the four frozen half-widths are checked against the design to 10^-16.
- Classifications follow BINARY_SCHEMAS codes and the design's order:
  - C: 10, 12, 11, 13
  - D: 20, 21
  - P: 30, 31, 32, 33
  - cell means: 40
- Every comparison is exact on rationals against the bracket. A bracket that straddles a threshold is INVALID (undecidable), never guessed.
- Display strings are compared after the exact integer checks. Each must have exactly 40 decimals and match the correct rounding: estimate half-even, half-width up, lower down, upper up. Cell means show `NA` for the interval fields.

**Audit files (no simulation):**

- Ordering checks, plus checks on reserved bytes and domains.
- Randomness is shared across cells: copy bit, ZERO target, scout, fresh, donor keys, tie keys, tournament entries and mutation flips.
- Target laws: ZERO targets are shared; a HALF target either copies its own t−2 target exactly when `copied = (t ≥ 3 ∧ R_t = 1)`, or equals the ZERO innovation.
- Candidate checks:
  - Families 0 and 1 (when the cache is read) match the context pre-state.
  - The donor choice is recomputed from (loss, donor key, family).
  - The local child differs from its donor in exactly `local_changed` coordinates.
- Tournament checks:
  - The winner is recomputed from (loss, tie key, index).
  - The inherited label is the label of the winner's parent.
  - `flipped = inherited ⊕ post` and `cache_valid = post`.
- Transitions: the pre-state at t is the winners' phenotypes at t−1, the labels are the post-labels, and the caches are the producing parents' pre-update phenotypes.
- Every audit (block, cell, update) is cross-checked against its chunk-0 per-update row.

## 6. C++ replay (`torus_audit replay`)

1. Self-tests run first: SHA-256 FIPS vectors, the fixture-r1 key-derivation record values, both official Threefry4x64-20 KAT vectors, key byte order, counter order, lane extraction, purpose and namespace separation, range refusal, schema count 164,672, circular loss, rank table, target laws, tournament/donor order, and a one- and two-update hand trace.
2. The tool independently replays blocks 0–63, all eight cells, from `production-r1` purpose keys.
3. Each regenerated row is compared byte-exactly against the six saved files: chunk-0 update, path and block records, plus all candidate, tournament-entry and audit-context rows, including all hashes.
4. It stops at the first discrepancy and reports `MISMATCH_<TYPE>_<field>` with the file and row. No value is printed.

**Hash definitions used by the replay:**

- `update_records_sha256`: SHA-256 of the 256 serialized update rows.
- `label_blind_sha256`: for each update, the 128 candidate phenotypes (32 × u16 LE each), then 128 losses (u64 LE), then 32 winner indices (u8), then 32 post-transition survivor phenotypes.
- `label_sha256`: the 32 post-transition labels (u8) for each update. `complement_label_sha256` is the same with each label complemented.
- `final_state_sha256`: see section 7, item 1.

## 7. Interpretations needing source-review confirmation

1. **`final_state_sha256` byte order:** the 32 phenotypes in slot order (each 32 × u16 LE), then 32 label bytes, then 32 cache-valid bytes, then the 32 caches (each 32 × u16 LE, zero when invalid).
2. **Candidate `flags`:**
   - bit0 is set on all four rows of a parent whose cache was valid before the update;
   - bit1 is set only on the family-1 row whose probe read the cache;
   - bit2 is set only on the family-0..2 row chosen as donor.
3. **Candidate fields by family:** `donor_key` is 0 on family-3 rows. `local_flagged` and `local_changed` are 0 and `donor_family` is 255 on family-0..2 rows.
4. **Context `law_copied`** is 1 only for HALF cells at t ≥ 3 with R_t = 1. `target_copy_bit` holds the raw R_t at every t, including t = 1 and 2.
5. **Estimate rows:**
   - Numerator/denominator pairs are compared as exact rationals.
   - Range lengths are compared as rationals, and cell means use range 1.
   - The cell-mean accuracy numerator is `2^46 − L_c` (an accuracy, not a loss).
   - Cell-mean names are only required to be printable ASCII. The six inferential names must be exactly `C_abs`, `C_rec`, `D_HALF`, `D_ZERO`, `P_abs`, `P_rec`.
   - The estimate header has chunk `0xFFFFFFFF`, first block 0, and block count n.
6. **Audit file headers:** chunk `0xFFFFFFFF`, first block 0, block count 64.
7. **Manifest:** sha256sum text format, located outside or inside the root. A manifest inside the root does not need to list itself.
8. **SHAM bookkeeping:** `cache_probe_winner_slots` counts survivor slots won by a family-1 candidate whose probe read the cache, so it is always 0 in SHAM.

## 8. Receipts

Both tools write one create-exclusive JSON receipt (mode 0444 from Python) outside the input root. Each receipt contains:

- the tool and source identities (SHA-256 of the C++ sources and Makefile, the binary via `/proc/self/exe`, or the Python file);
- the input hashes and the manifest hash;
- the checked counts;
- the verdict (PASS or INVALID);
- at most one failure, as a code plus a location, with values redacted.

Exit codes: 0 PASS, 1 INVALID, 2 usage/refusal (no receipt written), 3 receipt could not be written.
