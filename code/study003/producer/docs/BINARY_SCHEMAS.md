# PHASE2-TORUS-MEMORY-003 binary record schemas, version 1

All integers are **little-endian**, unsigned unless marked `i32`/`i64`/`i128`
(two's complement). No padding exists except where shown as `zero`. Sources:
`src/torus/records.{hpp,cpp}` (writer) and `tools/torus_schema.py` (reader).

## Common 64-byte file header

| Off | Size | Field | Domain |
|---:|---:|---|---|
| 0 | 8 | magic | ASCII, NUL-padded (table below) |
| 8 | 4 | schema_version | 1 |
| 12 | 4 | endian_marker | 0x01020304 (bytes `04 03 02 01`) |
| 16 | 4 | row_size | per type |
| 20 | 4 | header_size | 64 |
| 24 | 8 | row_count | exact number of rows that follow |
| 32 | 16 | namespace | `production-r1`, `fixture-r1` or `timing-r1`, NUL-padded |
| 48 | 4 | chunk_index | 0..649, or 0xFFFFFFFF if not a chunk file |
| 52 | 4 | first_block | first block covered |
| 56 | 4 | block_count | blocks covered |
| 60 | 4 | reserved | 0 |

File size must equal `64 + row_count * row_size`.

| Type | Magic | Row bytes | Production file(s) | Rows per file |
|---|---|---:|---|---:|
| per-update | `T3UPDATE` | 24 | `updates/chunk_KKKKK.t3u` | 131,072 |
| path | `T3PATH` | 224 | `paths/chunk_KKKKK.t3p` | 512 |
| block | `T3BLOCK` | 136 | `blocks/chunk_KKKKK.t3b` | 64 |
| estimate | `T3ESTIM` | 256 | `estimates.t3s` (analyzer) | 22 |
| candidate audit | `T3CANDID` | 104 | `audit/audit_candidates.t3c` | 16,777,216 |
| tournament-entry audit | `T3ENTRY` | 16 | `audit/audit_entries.t3e` | 16,777,216 |
| audit context | `T3CONTXT` | 4240 | `audit/audit_context.t3x` | 131,072 |

Chunk `k` (0..649) holds blocks `64k .. 64k+63`. Chunk 0 is exactly the fixed
audit set (blocks 0..63). Row order in chunk files is block ascending, then
cell 0..7, then update 1..256. Audit rows are ordered block, update, cell,
then candidate 0..127 / (survivor slot 0..31, entry 0..3).

**Cell index** = `4*arm + 2*law + start`, with ACTIVE=0/SHAM=1, ZERO=0/HALF=1,
ALL_F=0/ALL_M=1: 0 AZF, 1 AZM, 2 AHF, 3 AHM, 4 SZF, 5 SZM, 6 SHF, 7 SHM.
**Candidate index** = `32*family + parent_slot`; family 0 parent, 1 policy
probe, 2 scout, 3 local child. Labels: F=0, M=1.

## Per-update record (24 bytes), one per (block, cell, update)

| Off | Type | Field | Domain |
|---:|---|---|---|
| 0 | u64 | pop_loss — sum over 32 survivors of exact Q against T_t | 0..2^40 |
| 8 | u16 | update | 1..256 |
| 10 | u16 | query_count (objective evaluations) | must be 128 |
| 12 | u16 | local_flagged (coordinates with lane & 31 == 0) | 0..1024 |
| 14 | u16 | local_changed (flagged and value differs from donor) | 0..local_flagged |
| 16 | u8 | m_count after mutation/cache transition | 0..32 |
| 17 | u8 | valid_cache_pre (slots with valid cache before update) | 0..32 |
| 18 | u8 | cache_probe_uses (ACTIVE, M, valid) | 0..32; 0 in SHAM |
| 19 | u8 | cache_probe_winner_slots | 0..32 |
| 20 | u8 | f_to_m label mutations | 0..32 |
| 21 | u8 | m_to_f label mutations | 0..32 |
| 22 | u8 | dup_entry_tournaments (some index repeated within the 4 entries) | 0..32 |
| 23 | u8 | distinct_winners | 1..32 |

Exact population accuracy at update t = `(2^40 - pop_loss) / 2^40`.

## Path record (224 bytes), one per (block, cell)

| Off | Type | Field |
|---:|---|---|
| 0 | u32 | block |
| 4 | u8×4 | cell, arm, law, start |
| 8 | u32 | late_m_sum = Σ m_count over updates 193..256 (0..2048) |
| 12 | u32 | total_queries (must be 32,768) |
| 16 | u64 | late_pop_loss_sum = Σ pop_loss over 193..256 (≤ 2^46) |
| 24 | u64 | all_pop_loss_sum over 1..256 (≤ 2^48) |
| 32 | u32 | all_m_sum |
| 36 | u32 | total cache_probe_uses |
| 40 | u32 | total cache_probe_winner_slots |
| 44 | u32 | total f_to_m |
| 48 | u32 | total m_to_f |
| 52 | u32 | total dup_entry_tournaments |
| 56 | u32 | total local_flagged |
| 60 | u32 | total local_changed |
| 64 | 32 | final_state_sha256 (phenotypes u16 LE, labels, cache flags, caches u16 LE; invalid caches are zero) |
| 96 | 32 | update_records_sha256 = SHA-256 of the path's 256 serialized 24-byte update rows |
| 128 | 32 | label_blind_sha256 = SHA-256 over updates 1..256 of, in this order, (128 candidate phenotypes in candidate index order 0..127, each 32 u16 LE; 128 candidate losses u64 LE in candidate index order; 32 winner candidate indices u8 in survivor-slot order; 32 post-transition survivor phenotypes, each 32 u16 LE, in slot order) |
| 160 | 32 | label_sha256 = SHA-256 over updates of the 32 post-transition labels |
| 192 | 32 | complement_label_sha256 = same with every label complemented |

Late-window mean M frequency `L = late_m_sum / 2048`; late mean accuracy
`P = (2^46 - late_pop_loss_sum) / 2^46`.

## Block record (136 bytes)

| Off | Type | Field |
|---:|---|---|
| 0 | u32 | block |
| 4 | u32 | flags: bit0 N1 ZERO, bit1 N1 HALF, bit2 N2 ZERO, bit3 N2 HALF, bit4 N2 per-update sum 32 and late sum 2048, bit5 all query counts 128; valid iff 0x3F |
| 8 | u32×8 | late_m_sum per cell |
| 40 | u64×8 | late_pop_loss_sum per cell |
| 104 | i32 | C_abs numerator / 4096 = (s2+s3) − 2048 |
| 108 | i32 | C_rec numerator / 4096 = (s2+s3) − (s0+s1) |
| 112 | i32 | D_HALF numerator / 4096 = 2(s3 − s2) |
| 116 | i32 | D_ZERO numerator / 4096 = 2(s1 − s0) |
| 120 | i64 | P_abs numerator / 2^47 = (L6+L7) − (L2+L3) |
| 128 | i64 | P_rec numerator / 2^47 = [(L6+L7) − (L2+L3)] − [(L4+L5) − (L0+L1)] |

`s_c` = late_m_sum, `L_c` = late_pop_loss_sum of cell c.

## Estimate record (256 bytes), written only by `tools/torus_analyze.py`

| Off | Type | Field |
|---:|---|---|
| 0 | u16 | estimate index 0..21 |
| 2 | u8 | family: 0 primary allele, 1 performance, 2 absolute cell mean |
| 3 | u8 | classification code (below) |
| 4 | u32 | n_blocks = 41,600 |
| 8 | i128 | Σ over blocks of the per-block numerator |
| 24 | u128 | per-block denominator (estimate = Σ / (n · denominator)) |
| 40 | u32, u32 | block-variable range length numerator, denominator |
| 48 | char[32] | name |
| 80 | char[44] | estimate, 40 decimal places, half-even (display only) |
| 124 | char[44] | Hoeffding half-width, rounded up (or `NA`) |
| 168 | char[44] | lower bound, rounded down (or `NA`) |
| 212 | char[44] | upper bound, rounded up (or `NA`) |

Order: 0 C_abs, 1 C_rec, 2 D_HALF, 3 D_ZERO, 4 P_abs, 5 P_rec, 6–13 M
frequency for cells 0–7, 14–21 accuracy for cells 0–7. Cell means are
descriptive (no interval). Codes: 0 INVALID; 10 lower>Δ, 11 upper≤Δ,
12 upper<−Δ, 13 neither (C_abs, C_rec); 20 inside (−Δ,Δ), 21 not inside
(D_HALF, D_ZERO); 30 meaningful positive, 31 meaningful adverse, 32 bounded
below positive, 33 unresolved (P_abs, P_rec); 40 descriptive.

## Candidate audit row (104 bytes)

| Off | Type | Field |
|---:|---|---|
| 0 | u32 | block |
| 4 | u16 | update |
| 6 | u8 | cell |
| 7 | u8 | candidate index |
| 8 | u8 | family |
| 9 | u8 | parent (producer) slot |
| 10 | u8 | producing parent's pre-update label |
| 11 | u8 | flags: bit0 parent cache valid before update, bit1 probe read cache, bit2 chosen as local-child donor |
| 12 | u8 | donor family for family-3 rows, else 255 |
| 13 | u8 | local flagged count (family 3), else 0 |
| 14 | u8 | local changed count (family 3), else 0 |
| 15 | u8 | survivor slots won |
| 16 | u64 | exact loss Q |
| 24 | u64 | DONOR_KEY value (families 0–2), else 0 |
| 32 | u64 | CANDIDATE_TIE_KEY |
| 40 | u16×32 | phenotype |

## Tournament-entry audit row (16 bytes)

| Off | Type | Field |
|---:|---|---|
| 0 | u32 | block |
| 4 | u16 | update |
| 6 | u8 | cell |
| 7 | u8 | survivor slot |
| 8 | u8 | entry 0..3 |
| 9 | u8 | entered candidate index |
| 10 | u8 | tournament winner (candidate index) |
| 11 | u8 | inherited (producer's pre-update) label |
| 12 | u8 | post-mutation label |
| 13 | u8 | label flipped |
| 14 | u8 | post-transition cache valid |
| 15 | u8 | zero |

## Audit context row (4240 bytes), one per audit (block, update, cell)

| Off | Type | Field |
|---:|---|---|
| 0 | u32 | block |
| 4 | u16 | update |
| 6 | u8 | cell |
| 7 | u8 | raw TARGET_COPY bit R_t |
| 8 | u8 | 1 iff this cell's law set T_t = T_(t−2) |
| 9 | 7 | zero |
| 16 | u16×32 | target T_t |
| 80 | u16×32×32 | pre-update parent phenotypes |
| 2128 | u8×32 | pre-update labels |
| 2160 | u8×32 | pre-update cache valid flags |
| 2192 | u16×32×32 | pre-update caches (zero when invalid) |
