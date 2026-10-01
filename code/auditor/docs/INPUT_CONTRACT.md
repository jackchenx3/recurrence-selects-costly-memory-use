# Input contract of the independent MMEM auditor

This file maps every parsed field and every operator to the supplied frozen texts only:

- **SPEC**: `PHASE2-MUTABLE-MEMORY-001-REV1.md` (SHA-256 `c17a3a1e…e821d`), cited by section (§).
- **CFG**: `frozen_config.json`, cited by key path.
- **FMT**: `OUTPUT_FORMATS.md`, cited by heading.
- **BIN**: `binary_records.json`, cited by record name.
- **ARCH**: `AUDITOR_ARCHITECTURE_DECISION.md`.
- **IFACE**: `PRODUCER_OUTPUT_INTERFACE_CONTRACT_R2.json` (record
  `PHASE2-MUTABLE-MEMORY-001-PRODUCER-OUTPUT-INTERFACE-2`, revision 2, frozen before scientific production,
  bound to the authenticated producer-source hashes; supersedes revision 1). It is the exact authority for
  every JSON path, key, type, label, rule and Decimal operation order parsed in §5–§6; the auditor read it,
  not the producer sources.

No producer source, header, Python module, generated constant or helper was read, copied,
translated or called. Components: `src/` = C++ replay verifier (`mmem_audit_replay`);
`python/mmem_record_verifier.py` = record/estimate verifier.

## 1. Random numbers (C++ only; `src/philox.hpp`)

| Operator | Auditor implementation | Source |
|---|---|---|
| Philox4x32-10 round, multipliers `D2511F53`, `CD9E8D57`, Weyl `9E3779B9`, `BB67AE85`, 10 rounds | `philox4x32_10` | SPEC §7 ¶1; CFG `rng.generator`, `rng.multipliers_hex`, `rng.weyl_hex`, `rng.rounds`; ARCH "Evidence considered" (multiply-high/low, XOR, Weyl additions; no rotations) |
| Known-answer rows (zero, all-ones, pi digits) | `kPhiloxKnownAnswers` | task statement; ARCH "Selected architecture" item 1 |
| SHA-256 | `src/sha256.hpp` (FIPS 180-4), self-tested on three FIPS vectors | SPEC §7 ¶1 |
| Key text `PHASE2-MUTABLE-MEMORY-001|<namespace>|<purpose>`; key word 0 = digest bytes 0–3 LE, word 1 = bytes 4–7 LE | `key_text`, `KeyedStream` | SPEC §7 ¶1; CFG `rng.key_text_template`, `rng.key_derivation` |
| Namespace `production-r1` (production) / `fixture-r1-nonscientific` (fixture); one namespace per process | `claim_namespace` | CFG `rng.key_namespace`, `rng.fixture_key_namespace_non_scientific`; task statement |
| Counter = (block, update, entity, subindex) | `KeyedStream::draw` | SPEC §7 ¶1; CFG `rng.counter_words` |
| Coordinate ranges per purpose; abort on violation, including survival retry > 2^32−1 | `validate_coordinate` | SPEC §7 "Coordinates and extraction"; CFG `rng.coordinate_schemas`, `rng.block_range`, `rng.max_survival_retry_subindex` |
| Purpose-key separation (9 keys pairwise distinct), donor entity `3*slot+family` injective | `check_purpose_keys`, KAT `coordinate_schema_ranges_and_donor_injectivity` | SPEC §7 last ¶ |
| Extraction: word0; copy bit `word0&1`; local flip and policy flip iff `(word0&31)==0`; 64-bit keys/x = `word0 | word1<<32` | `BlockDraws` | SPEC §7; CFG `rng.coordinate_schemas.*.extraction` |
| All draws generated for every coordinate regardless of cell/arm/law/label | `BlockDraws` constructor | SPEC §5 ¶3, §7 last ¶ |

## 2. Model operators (C++ only; `src/replay.hpp`)

| Operator | Auditor implementation | Source |
|---|---|---|
| 32 slots, initial genotypes shared by 8 cells, caches invalid, ALL_F/ALL_M labels | `begin_cell` | SPEC §2, §3 |
| Cell index `4*arm+2*law+start` | `begin_cell` | CFG `design.cell_index_formula`; FMT preamble |
| ZERO: `T_t=I_t`; HALF: `T_1=I_1,T_2=I_2`, `t≥3`: `T_{t-2}` if `R_t=1` else `I_t`; recurrence applied = HALF, t≥3, R_t=1 | `BlockDraws` target tables | SPEC §4; CFG `target_laws`; FMT audit row offset 26 |
| Candidates parent / policy probe / scout / local child; probe = cache iff ACTIVE ∧ M ∧ cache valid, else parent XOR fresh mask | `step` | SPEC §5; CFG `candidates.policy_probe_rule` |
| Donor: lowest mismatch among families 0–2, then lowest 64-bit donor key, then lowest family | `step` | SPEC §5 item 4; CFG `candidates.donor_rule` |
| Mismatch = popcount(genotype XOR T_t); weight `2^(32-h)` | `popcount32`, `step` | SPEC §2, §6 ¶1 |
| Sequential survival: W_d over unselected in index order; Lemire threshold `(2^64−W) mod W`, reject `low64 < threshold`, Z = high64; first remaining with cumulative > Z; survivor slot = draw order | `lemire_threshold`, `lemire_accept`, `mul_64x64`, `step` | SPEC §6 steps 1–3; CFG `selection.method` |
| Inherit producing parent label; flip with policy draw of survivor slot; post-M cache = parent's pre-update genotype, post-F cache invalid (0) | `step` | SPEC §6 ¶3–5; CFG `population.cache_rule`, `policy_mutation` |
| Final-state hash preimage | `end_cell` | FMT "Final-state hash" |
| Paired-trajectory hash preimage | `begin_cell`, `step` | FMT "Paired-trajectory hash"; BIN `paired_trajectory_sha256` |
| N1, N2 | `end_block` | SPEC §8 |

## 3. Binary records (both components)

All little-endian, fixed size (FMT preamble; BIN `byte_order`). Order within a block is cell-major,
then update-major, then candidate-major; shard `s` holds blocks `1300*s … 1300*s+1299` (FMT preamble;
CFG `output`).

### 3.1 Update record, 24 bytes, struct `<IHBBHBBBBBBQ` (FMT "Per-update record"; BIN `update`)

| Field | Replay (C++) expected value | Record verifier (Python) checks |
|---|---|---|
| block, update, cell | coordinates | exact order; fatal on disorder |
| m_count | post-mutation M count | ≤32; SHAM pair sums to 32 (N2); equals audit-row count of selected post labels = 1 |
| total_mismatch | sum of survivors' mismatch vs T_t | ≤1024; SHAM pair equal (N1); equals audit selected mismatch sum |
| valid_m_cache | parents M with valid cache at update start, any arm | `0` at t=1, else previous m_count (SPEC §6 last ¶); SHAM pair sums to 32 for t≥2 |
| cache_probe_use | probes taken from cache (ACTIVE only) | ACTIVE: = valid_m_cache; SHAM: 0 (FMT "0 in SHAM") |
| cache_probe_survivors | selected family-1 rows with cache source | ≤ cache_probe_use; SHAM 0 |
| f_to_m, m_to_f | survivors whose inherited label flipped F→M / M→F | sum ≤32; SHAM pair swapped equality (N2) |
| query_count | evaluated candidates (128) | must be 128 (SPEC §10 rule 1) |
| survival_retries_u64 | sum of 32 accepted retry indices | < 2^37; SHAM pair equal (N1) |

### 3.2 Path record, 128 bytes, struct `<IBBBBIIIIIIIQHHHH32sIII32s` (FMT "Per-path summary"; BIN `path`)

Python reconstructs every field 0–16 from the 256 update records of the path; final M count and
final total mismatch from update 256; `total_queries = 32768`; `reserved0 = 0`. Fixation/extinction
= updates with M count 32 / 0, late = updates 193–256. SHAM pairs: equal paired hash, equal late
mismatch, late M sums sum to 2048, final M counts sum to 32, fixation↔extinction swap, F→M↔M→F swap,
equal retries. For audit blocks, the paired-trajectory and final-state hashes are recomputed from audit
rows (FMT hash definitions). C++ compares all 128 bytes with its replay.

### 3.3 Block record, 96 bytes, struct `<IBBBB6i8I8I` (FMT "Per-block record"; BIN `block`)

Both components require `n1_ok = n2_ok = query_ok = 1`, `audit_block = (block < 64)`, numerators
`C_abs = L2+L3−2048`, `C_rec = (L2+L3)−(L0+L1)`, `D_HALF = L3−L2`, `D_ZERO = L1−L0`,
`P_abs = (P6+P7)−(P2+P3)`, `P_rec = P_abs − [(P4+P5)−(P0+P1)]`, and `Lc`, `Pc` equal to the path
records' late sums (SPEC §9, §11; FMT table). Python also requires `L4+L5 = L6+L7 = 2048`,
`P4 = P5`, `P6 = P7` (N1/N2 consequences, SPEC §8).

### 3.4 Audit candidate row, 72 bytes, struct `<IHBBBBBBIIIBBBBQBBBBQQQII` (FMT "Audit candidate row"; BIN `audit_row`)

| Field | Python consistency checks (no draws) |
|---|---|
| block, update, cell, candidate | exact order, candidate = row position |
| family, parent | `candidate>>5`, `candidate&31` |
| parent_label_pre, parent_cache_valid, parent_cache | equal to the reconstructed post-update state of the previous update (t=1: start label, invalid, 0) |
| genotype | family-0 genotype = previous survivor genotype by slot; t=1 identical across all 8 cells |
| target, recurrence_bit, recurrence_applied | constant within update; applied = HALF ∧ t≥3 ∧ R; R equal across cells; targets equal within a law; HALF = T_{t−2} if applied else ZERO target |
| mismatch | popcount(genotype XOR target) |
| probe_source | family 1: 1 iff ACTIVE ∧ M ∧ valid (then genotype = cache); else 0; other families 255 |
| weight | `2^(32−mismatch)` |
| selected_rank | 32 distinct ranks 0–31, else 255 |
| post_label, policy_flip | selected: `post = inherited XOR flip`; unselected: 255 |
| donor_family | family 3: 0–2 and a lowest-mismatch family; else 255 |
| W, x, Z, retry | selected: W = remaining weight, `low64(x*W) ≥ (2^64−W) mod W`, `Z = high64(x*W)`, cumulative interval contains Z; unselected: all 0 |
| reserved | 0 |

C++ compares every byte of every row with its replay (same table: `kAuditFields`).

## 4. File sets

- **Fixture** (task statement; fixture receipt `layout_sample_files_and_sizes`): directory contains
  exactly `layout_sample_updates.bin` (49,152 B), `layout_sample_paths.bin` (1,024 B),
  `layout_sample_block.bin` (96 B), `layout_sample_audit.bin` (18,874,368 B) and
  `layout_sample_README.txt` (hashed, not parsed). The block id is read from the block record and must be
  0–41,599; draws use the fixture namespace.
- **Production** (FMT "Production directory layout"; CFG `output`): `shards/shard_00 … shard_31`, each
  with exactly `updates.bin`, `paths.bin`, `blocks.bin` of 63,897,600 / 1,331,200 / 124,800 bytes;
  `shard_00` additionally `audit_rows_blocks_0000_0063.bin` (1,207,959,552 B). C++ reads only
  shard_00 (blocks 0–63) and hashes the full files. Python `production` requires every file at its exact
  record size before any record or manifest check, then parses all records before the manifest.
- **Manifest interface** (Python `manifest-interface`, NONSCIENTIFIC; IFACE requirement for a bounded
  manifest-interface control and corruption test): the same 97 relative paths and `run_manifest.json`,
  but every file must be a placeholder of at most 4,096 bytes (so this mode cannot stand in for a
  production audit). Each file is hashed as opaque bytes only — no record is parsed, nothing is
  simulated, no random key is derived — and the manifest is checked exactly as in §5. Expected counts:
  97 files hashed and 97 manifest entries matched.

## 5. Manifest (Python; IFACE `run_manifest`; FMT "run_manifest.json … output SHA-256 list")

No key or string search is performed. `run_manifest.json` must be a strict-JSON object (§6 item 7) with:

- exactly the 24 IFACE root keys `manifest, study_id, status, config_path, config_sha256,
  specification_sha256, terminal_review_sha256, key_namespace, compiler_version, cplusplus, threads,
  shards, blocks, started_utc, finished_utc, wall_seconds, total_objective_queries,
  expected_total_objective_queries, n1_failed_blocks, n2_failed_blocks, query_failed_blocks,
  total_output_bytes, regeneration_command, outputs` (a missing key is named; an undocumented key is
  INVALID and is not echoed);
- exact values (type-exact, so `true` ≠ `1`): `manifest = "MMEM-RUN-MANIFEST-1"`,
  `study_id = "PHASE2-MUTABLE-MEMORY-001"`, `status = "COMPLETE"`, `key_namespace = "production-r1"`,
  `shards = 32`, `blocks = 41600`, `total_objective_queries = expected_total_objective_queries =
  10905190400`, `n1_failed_blocks = n2_failed_blocks = query_failed_blocks = 0`;
- typed fields: `config_path`, `compiler_version`, `started_utc`, `finished_utc`, `regeneration_command`
  strings; `config_sha256`, `specification_sha256`, `terminal_review_sha256` lowercase 64-hex;
  `cplusplus` integer; `threads` integer 1–32; `wall_seconds` nonnegative number;
  `total_output_bytes` nonnegative integer; `outputs` array. These typed fields are not compared with
  any other field;
- `outputs[*]`: each entry an object with exactly the keys `path` (string), `bytes` (nonnegative
  integer) and `sha256` (lowercase 64-hex). The array must hold exactly 97 entries whose paths are
  exactly the unique set `shards/shard_XX/updates.bin|paths.bin|blocks.bin` for XX = 00…31 plus
  `shards/shard_00/audit_rows_blocks_0000_0063.bin`. Each `bytes` must equal the byte count and each
  `sha256` the SHA-256 the verifier computed while streaming that file. An undocumented, duplicate or
  missing path is INVALID. Entry diagnostics read `run_manifest.json outputs entry <i> field <field> …`
  and may name the relative path, never a byte count or hash value.

## 6. Analysis JSON (Python; IFACE `analysis`; FMT "Analysis outputs"; SPEC §9–§11; CFG `inference`)

Exact expected values from the n block records (n = 41,600 in production):
`C_abs = ΣC_abs_num/(4096n)`, `C_rec = Σ/(4096n)`, `D_* = Σ/(2048n)`, `P_* = Σ/(131072n)`,
cell M frequency `ΣLc/(2048n)`, cell accuracy `1 − ΣPc/(65536n)`.

Decimal recomputation reproduces the IFACE `numeric_encoding` operation order (each step in its own
local context of precision 60, ROUND_HALF_EVEN; verifier `dec`, `producer_half_width`,
`producer_bounds`):

1. `estimate_decimal = dec(Fraction) = Decimal(p)/Decimal(q)`;
2. in one context: `inner = (Decimal(2)/Decimal(alpha_each)).ln() / (Decimal(2)*Decimal(n))`, then
   `half_width = Decimal(range_length) * inner.sqrt()` (n = 41,600 in production);
3. in a separate context: `lower = estimate_decimal − half_width`, `upper = estimate_decimal + half_width`.

In production each half-width must also round (8 decimals) to its frozen value. Decision rules are
applied in order (CFG `decision_rules_in_order`; IFACE `secondary_classification_rule`) to the
precision-60 bounds, with `Delta = Delta_P = 1/32`; the adverse flag is `upper(C_abs) < −Delta`.
IFACE `family_and_relation_rules` give each P record's relation: for `P_abs`, pos = primary ∈ {rule 3,
rule 4 label}, neg = primary = rule 5 label; for `P_rec`, pos = primary = rule 3 label, neg = primary =
rule 4 label; relation = `UNCERTAINTY` if the classification is `UNRESOLVED` or neither pos nor neg,
else `AGREEMENT` iff (classification = `MEANINGFUL POSITIVE PERFORMANCE`) = pos, else `DISAGREEMENT`.

Exact parsing rules:

1. `ROUTE_CLOSED_INVALID.json` present → INVALID without opening `estimates_22.json` or `decision.json`
   and without computing or reporting any estimate comparison.
2. `estimates_22.json` root is an array of exactly 22 objects; element i is record i+1.
3. Exact key sets (no missing and no extra key):
   - records 1–4 (`C_abs`, `C_rec`, `D_HALF`, `D_ZERO`; family `PRIMARY_ALLELE_ENRICHMENT`):
     `record, name, family, estimate_exact, estimate, range_length, alpha_each, n_blocks, half_width,
     lower, upper, family_decision`; `alpha_each = "0.0125"`;
   - records 5–6 (`P_abs`, `P_rec`; family `SECONDARY_POPULATION_PERFORMANCE`): the same plus
     `classification, relation_to_allele_enrichment_descriptive, cannot_alter_primary_decision` instead
     of `family_decision`; `alpha_each = "0.025"`; `cannot_alter_primary_decision = true`;
   - records 7–14 (`M_FREQUENCY_LATE|<cell>`) and 15–22 (`ACCURACY_LATE|<cell>`), cells in the order
     `ACTIVE|ZERO|ALL_F … SHAM|HALF|ALL_M` (family `ABSOLUTE_CELL_MEAN_DESCRIPTIVE`):
     `record, name, family, estimate_exact, estimate, lower, upper, classification`; `lower = upper =
     null`; `classification = "DESCRIPTIVE; NO INFERENTIAL BOUND FROZEN"`.
4. Field rules: `record` integer = i+1; `name`, `family` exact strings; `n_blocks` integer = n;
   `range_length` the exact JSON integer 1, 2, 2, 2, 2, 4 for records 1–6. `estimate_exact` is the only
   exact-fraction field: a string equal to `'%d/%d' % (numerator, denominator)` of the reduced
   reconstructed fraction (signed numerator, positive denominator; `"/1"` is written even when the
   denominator is 1). `estimate`, `half_width`, `lower`, `upper` are strings holding a finite decimal
   numeral whose Decimal value must equal the precision-60 recomputation exactly (no tolerance).
5. Records 1–4: `family_decision` = the recomputed primary-decision label. Records 5–6:
   `classification` = the recomputed secondary label of that P interval;
   `relation_to_allele_enrichment_descriptive` = the recomputed relation (above);
   `cannot_alter_primary_decision` = `true`. Mismatches are category `DECISION` (`ANALYSIS_FIELD` for
   `cannot_alter_primary_decision`).
6. `decision.json` root is an object with exactly `primary_decision`, `decision_rule_applied`,
   `adverse_selection_upper_C_abs_below_minus_Delta`, `secondary_classifications`,
   `interpretation_labels`: `decision_rule_applied` integer = recomputed rule (2–6); `primary_decision`
   = the IFACE label of that rule; the adverse field is a boolean equal to the recomputed flag when the
   rule is 5 and `null` otherwise; `secondary_classifications` is an object with exactly `P_abs` and
   `P_rec` equal to the recomputed labels (`MEANINGFUL POSITIVE PERFORMANCE`, `MEANINGFUL ADVERSE
   PERFORMANCE`, `BOUNDED BELOW THE POSITIVE ONE-BIT SCALE`, `UNRESOLVED`); `interpretation_labels` is
   an array equal, element by element and in order, to the six IFACE `interpretation_labels_exact`
   strings (verifier `INTERPRETATION_LABELS`). The labels are never searched.
7. Strict JSON for all three files: duplicate keys, NaN/Infinity, or invalid UTF-8 → INVALID
   (`ANALYSIS_SCHEMA` for analysis files, `MANIFEST_SCHEMA` for the manifest).

Schema failures use `ANALYSIS_SCHEMA`/`MANIFEST_SCHEMA`; value disagreements use `ANALYSIS_ORDER`,
`ANALYSIS_FIELD`, `ESTIMATE` (estimate_exact), `ESTIMATE_DISPLAY` (estimate), `BOUND` (half_width,
lower, upper), `DECISION`, `MANIFEST_FIELD`, `MANIFEST_SIZE`, `MANIFEST_HASH`. All are INVALID.

## 7. Receipts and failure semantics (ARCH "Failure semantics"; IFACE `audit_receipt_redaction`)

Create-exclusive (`O_CREAT|O_EXCL`, mode 0444), refused inside an input directory. Fields: input byte
counts and SHA-256, tool/version/executable or source SHA-256, mode, namespace, checked vs expected
counts, KAT/self-check results, first discrepancy (≤480 characters), mismatch count (capped at
1,000,000), start/end UTC, `PASS`/`INVALID`.

Redaction: a mismatch diagnostic names only the category, the record index or block/cell/update/
candidate/draw coordinates or manifest entry index/relative path, and the field name (for example
`estimates_22.json record 3 field estimate_exact mismatch`, `block=12 cell=0 path_record.late_m_sum
mismatch`, `run_manifest.json outputs entry 37 field sha256 mismatch for shards/shard_12/updates.bin`).
It never contains an observed or recomputed estimate, fraction, bound, decision label, per-block
numerator or sum, genotype, hash field value, manifest byte count or hash, file bytes or other outcome
bytes (each receipt's `inputs` list does carry the computed SHA-256 and size of every input file as
provenance, as in every mode); C++ `compare_fields` emits `<record>.<field> mismatch` only, and
Python reports unexpected exceptions and JSON decode failures by exception type only. PASS receipts
carry no estimate or decision either.

Exit status (both tools): 0 PASS; 1 INVALID with receipt (any checked evidence mismatch); 2 usage error
or unusable receipt path, with no receipt; 3 INVALID but the receipt could not be written.
