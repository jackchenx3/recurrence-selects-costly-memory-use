# Input contract of the independent PCONV auditor (PHASE2-PERFORMANCE-CONVERSION-002 r1)

This file maps every parsed field and every operator to the supplied study-002 authorities only:

- **SPEC**: `PHASE2-PERFORMANCE-CONVERSION-002-REV1.md` (SHA-256 `58ee414d…afcb895`), cited by section (§).
- **CFG**: `frozen_config.json` (`PCONV-FROZEN-CONFIG-1`), cited by key path.
- **FMT**: `OUTPUT_FORMATS.md` (study 002), cited by heading.
- **BIN**: `binary_records.json` (`PCONV-BINARY-RECORDS-1`), cited by record name.
- **GO**: `DESIGN_002_GO.json`; **SRD**: `PERFORMANCE_002_SOURCE_REVIEW_DECISION.json`;
  **FIX**: the accepted fixture receipt `fixtures.json` (`PCONV-FIXTURES-1`), used only for the list of
  behaviours a fixture must cover, never as expected values.

Unchanged operators were already present in the accepted independent study-001 auditor seed. That seed was
derived independently of the producer, and this package was adapted from an authenticated copy of it. No
producer source, header, Python module, analyzer, generated constant or helper was read, copied, translated or
called. Identical public constants and record layouts required by the texts above are permitted.
Components: `src/` = C++17 replay verifier (`pconv_audit_replay`);
`python/mmem_record_verifier.py` = record/estimate verifier (tool id `pconv_record_verifier`; seed file name
kept so the file set is unchanged); `tests/fixture_gate.py` = corruption gate.

Items marked **PROVISIONAL** are auditor-side readings of points the supplied texts leave unspecified.
Each is kept in one place in each component. Each fails closed (INVALID) when the producer's output differs.
Each must be confirmed or corrected against a frozen study-002 producer-output interface before the auditor
fixture gate and before production (README "Pre-production prerequisites").

## 1. Random numbers (C++ only; `src/philox.hpp`, `src/replay.hpp`)

| Operator | Auditor implementation | Source |
|---|---|---|
| Philox4x32-10, multipliers `D2511F53`, `CD9E8D57`, Weyl `9E3779B9`, `BB67AE85`, 10 rounds | `philox4x32_10` (seed, unchanged) | SPEC §7; CFG `rng.generator`, `rng.multipliers_hex`, `rng.weyl_hex`, `rng.rounds` |
| Three official Random123 known-answer rows (zero, all-ones, pi digits) | `kPhiloxKnownAnswers` (seed, unchanged) | task statement; FIX K0 |
| SHA-256 (FIPS 180-4), three FIPS vectors | `src/sha256.hpp` (seed, unchanged in substance) | SPEC §7 |
| Key text `PHASE2-PERFORMANCE-CONVERSION-002|<namespace>|<purpose>`; key word 0 = digest bytes 0–3 LE, word 1 = bytes 4–7 LE | `key_text`, `key_words_from_text`, `KeyedStream` | SPEC §7; CFG `rng.key_text_template`, `rng.key_derivation` |
| Namespaces `production-r1` / `fixture-r1-nonscientific`; one stream namespace per process | `claim_namespace` | CFG `rng.key_namespace`, `rng.fixture_key_namespace_non_scientific` |
| Ten purposes in order, `DECOY_PERMUTATION` = 9 | `Purpose`, `purpose_name` | SPEC §7; CFG `rng.purposes_in_order` |
| Counter = (block, update, entity, subindex) | `KeyedStream::draw` | CFG `rng.counter_words` |
| Coordinate ranges; `DECOY_PERMUTATION` update 1–256, entity = step 1–31, subindex = retry ≤ 2^32−1; abort on violation | `validate_coordinate` | SPEC §7; CFG `rng.coordinate_schemas`, `rng.max_decoy_retry_subindex` |
| Extraction: word0; copy bit `word0&1`; local and policy flip iff `(word0&31)==0`; 64-bit donor keys / survival x / decoy x = `word0 | word1<<32`; decoy words 2–3 unused | `BlockDraws`, `join64` | CFG `rng.coordinate_schemas.*.extraction` |
| Every draw generated for every coordinate whatever the cell, arm, label or recurrence; the permutation of every (block, update) generated once and shared by all six cells | `BlockDraws` constructor | SPEC §3, §5 ¶2, §7; CFG `decoy_permutation.shared_by`, `generated_unconditionally` |
| Fisher–Yates: start `[0..31]`; i = 31..1; bound n = i+1; accept iff `low64(x·n) ≥ (2^64−n) mod n`; j = `high64(x·n)`; retry r+1 only after r rejected; swap `perm[i]`, `perm[j]` | `fisher_yates<32>`, `lemire_accept`, `mul_64x64` | SPEC §5 ¶2; CFG `decoy_permutation.construction` |
| Orientation: source bit s → destination bit `perm[s]` | `apply_permutation` | SPEC §5; CFG `decoy_permutation.mapping` |
| Abort if a permutation retry total exceeds its u32 record field (per update and per block) | `BlockDraws`, `end_block` | SPEC §7 "Abort on range overflow"; BIN record widths |

## 2. Model operators (C++ only; `src/replay.hpp`)

| Operator | Auditor implementation | Source |
|---|---|---|
| 32 slots; initial genotypes shared by six cells; caches invalid; ALL_F / ALL_M labels | `begin_cell` | SPEC §2, §3 |
| Cell index `2*arm+start` (INFO 0, NONINFO 1, SHAM 2) | `begin_cell` | CFG `design.cell_index_formula` |
| HALF only: `T_1=I_1`, `T_2=I_2`; `t≥3`: `T_{t−2}` if `R_t=1` else `I_t`; recurrence applied = `t≥3 ∧ R_t=1` | `half_targets`, `recurrence_applied_at` | SPEC §4; CFG `target_law` |
| Policy probe: INFO valid-M → `c`; NONINFO valid-M → `c` off recurrence, `x XOR π(c XOR x)` on recurrence; SHAM, F, invalid-M → `x XOR fresh` | `policy_probe` | SPEC §5 item 2; CFG `candidates.policy_probe_rule` |
| Parent, scout, local child; donor = lowest mismatch of families 0–2, then lowest u64 donor key, then lowest family | `step` | SPEC §5; CFG `candidates.donor_rule` |
| Weights `2^(32−h)`; sequential Lemire survival; survivor slot = draw order | `step` | SPEC §6; CFG `selection.method` |
| Inherit; flip with the survivor slot's policy draw; post-M cache = producing parent's pre-update genotype; post-F invalid | `step` | SPEC §2, §6; CFG `population.cache_rule` |
| Counters: valid-M (any arm), weight-0/32 diagnostics (any arm), true-cache / decoy use and survival, decoy weight-0/32, decoy identical to cache | `step` | FMT "Per-update record" offsets 10–33 |
| Final-state hash (`PCONV-FINAL-STATE-V1`) and paired-trajectory hash (`PCONV-PAIRED-TRAJECTORY-V1`, block, arm; per update update, target, R_t, applied, mismatch, 32 survivor indices, 32 genotypes, label mask XOR `0xffffffff` for ALL_M) | `end_cell`, `begin_cell`, `step` | FMT hash definitions; BIN `paired_trajectory_sha256` |
| N1 (SHAM candidates, survivors, genotypes, mismatch, retries equal between starts), N2 (label complement), SHAM paired hashes equal | `end_block` | SPEC §8; CFG `identities.N1`, `N2` |
| C1: for each start, F = first update with a NONINFO decoy probe ≠ true cache (0 = never); INFO and NONINFO identical in candidates, donors, survivors, genotypes, labels, caches, mismatch, retries for t < F; F recurrent; at F pre-update diagnostics, parent and scout candidates agree, probes differ exactly at the decoy-differs slots, local children agree where neither arm used the probe as donor | `c1_for_start` | CFG `identities.C1`; SPEC §13 items 3–4 |
| Block numerators `Δ_P=(P2+P3)−(P0+P1)`, `D_INFO=L1−L0`, `D_NONINFO=L3−L2`, `E_INFO=(L0+L1)−(L2+L3)`, `E_NONINFO=(L2+L3)−2048`, `B_INFO=(P4+P5)−(P0+P1)`, `B_NONINFO=(P4+P5)−(P2+P3)` | `end_block` | FMT "Per-block record"; SPEC §9–§10 |

## 3. Binary records (both components)

All little-endian and fixed-size (BIN `byte_order`). Within a block the order is cell-major, then
update-major, then candidate-major. Shard `s` holds blocks `1300*s … 1300*s+1299` (FMT preamble). The C++
replay compares every byte of every record it replays, naming only the first differing field from the
`kUpdateFields`, `kPathFields`, `kBlockFields`, `kAuditFields` and `kPermFields` tables (every byte, reserved
bytes included, belongs to a named field).

### 3.1 Update record, 48 bytes, `<IHBBHBBBBBBQBBBBBBBBBBHIII` (FMT "Per-update record")

The Python verifier checks every block in this order:
- **Order**: exact block/update/cell order (any break is fatal).
- **Ranges**: m ≤ 32, mismatch ≤ 1024, valid-M ≤ 32, F→M + M→F ≤ 32, retries < 2^37, `recurrence_applied`
  ∈ {0,1} and 0 for t < 3.
- **Counter consistency**: probe survivors ≤ probe use; true + decoy use = probe use; true + decoy survivors =
  probe survivors (each ≤ its use); weight-0 + weight-32 ≤ valid-M; decoy weight-0 + weight-32 ≤ identical ≤
  decoy use.
- **Reserved fields**: `reserved_u16` = `reserved_u32` = 0.
- **Query count**: 128.
- **Valid-M**: `valid_m_cache` = 0 at t = 1, else the previous `m_count`.
- **Arm identities**:
  - INFO: probe use = true use = valid-M, and no decoy.
  - NONINFO: off recurrence as INFO; on recurrence decoy use = valid-M, true use = 0, decoy weight-0/32 =
    valid-M weight-0/32.
  - SHAM: every probe counter is 0.
- **Shared across cells**: `recurrence_applied`, `decoy_perm_retry_total` and `decoy_perm_fnv1a` are
  identical in all six cells.

### 3.2 Path record, 176 bytes, `<IBBBBIIIIIIIQHHHH32sIII32sIIIIIIIIIIQ` (FMT "Per-path summary")

Python reconstructs from the 256 update records:
- block, cell, arm, start;
- late sums;
- queries (= 32,768);
- F→M, M→F, probe use and survivors, retries;
- fixation, extinction and their late versions;
- final M count and mismatch (from update 256);
- the nine new totals (offsets 128–160), recurrent updates and the u64 permutation retry total.

It requires `reserved_u8` = `reserved_u32` = 0.

SHAM pair (cells 4, 5) must satisfy N1/N2 consequences:
- equal paired hash;
- equal late mismatch, final mismatch and retries;
- late M sums totalling 2048 and final M counts totalling 32;
- fixation ↔ extinction and F→M ↔ M→F swapped.

The same holds per update: equal mismatch, retries and queries; complementary M counts; valid-M totalling 32
for t ≥ 2.

C1 record consequences, per start, with F derived as the first update where `decoy_use > decoy_identical_to_cache`:
- Through F−1, INFO and NONINFO agree on M, mismatch, valid-M, probe use and survivors, F→M, M→F, queries,
  retries and the weight-0/32 diagnostics.
- In the same updates, INFO true use = NONINFO true + decoy use, and likewise for survivors.
- At F, NONINFO is recurrent and the pre-update diagnostics agree.

### 3.3 Block record, 96 bytes, `<IBBBB7i6I6IBBHHHI` (FMT "Per-block record")

Both components require:
- `n1_ok = n2_ok = query_ok = c1_ok = 1`, `audit_block = (block < 64)`, `reserved_u8 = 0`;
- the seven numerators of §2, and `L_c`, `P_c` equal to the path records' late sums;
- `first_decoupling_update_ALL_F/ALL_M` equal to F of cells 2/3;
- `recurrent_updates` and `decoy_perm_retry_total` equal to the sums of the per-update shared fields.

Python additionally requires `L4+L5 = 2048` and `P4 = P5`.

### 3.4 Audit candidate row, 88 bytes, `<IHBBBBBBIIIBBBBQBBBBQQQIIIIBBHI` (FMT "Audit candidate row")

Python checks every row of blocks 0–63 for consistency (it generates no draw):

**Common to every row:**
- exact order;
- `family = j>>5`, `parent = j&31`;
- parent label, cache flag and cache continue the reconstructed previous state;
- `parent_genotype` = the family-0 genotype = the previous survivor;
- target, R and applied constant within an update and identical across the six cells, with
  `applied = (t ≥ 3 ∧ R)` and recurrent targets equal to `T_{t−2}`;
- mismatch = popcount, and the exact weight;
- `true_displacement` = `c XOR x` (0 if invalid);
- `displacement_weight` = popcount (255 if invalid);
- `parent_distance = popcount(genotype XOR x)`;
- `perm_ref = block*256 + (t−1)`;
- reserved = 0.

**By family:**
- Family 0: mask 0.
- Family 1: the probe-source rule of §2.
  - source 0: `genotype = x XOR mask`;
  - source 1: `genotype = c` and `mask = c XOR x`;
  - source 2: `mask = π(c XOR x)` with the replayed shared permutation, `genotype = x XOR mask` and parent
    distance = displacement weight.
- Family 2: `genotype = x XOR mask`.
- Family 3: the donor is a lowest-mismatch family and `genotype = donor XOR mask`.
- Fresh, scout and local masks of one (update, parent) are identical in every cell that uses them.

**Selection and reconstruction:**
- selected / unselected field rules;
- exact Lemire acceptance and the cumulative-interval selection for every survival draw;
- the full update record recomputed from the rows;
- the paired-trajectory and final-state hashes recomputed and compared.

Donor-key tie breaks and every random value are checked only by the C++ replay (SRD finding 5).

### 3.5 Audit permutation record, 416 bytes, `<IHBBI32s31Q31I` (FMT "Audit permutation record")

Python checks, for blocks 0–63 × updates 1–256:
- exact order and `reserved_u8 = 0`;
- each accepted x passes the Lemire test for its step;
- the Fisher–Yates replay of the accepted x equals `perm`;
- `retry_steps` and `retry_total` agree with `accepted_retry`;
- `retry_total` and FNV-1a(`perm`) equal the update records' shared fields.

The C++ replay regenerates each x from `DECOY_PERMUTATION` at (block, update, step, retry) and compares all
416 bytes. It therefore also confirms that every lower retry was rejected.

## 4. File sets

- **Fixture** (one block, fixture namespace). The directory must contain exactly six files:

  | File | Size |
  |---|---|
  | `layout_sample_updates.bin` | 73,728 B |
  | `layout_sample_paths.bin` | 1,056 B |
  | `layout_sample_block.bin` | 96 B |
  | `layout_sample_audit.bin` | 17,301,504 B |
  | `layout_sample_permutations.bin` | 106,496 B |
  | `layout_sample_README.txt` | hashed, not parsed |

  The block id is read from the block record (0–41,599). **PROVISIONAL**: the task names the six roles; these
  names are an analogy to the study-001 fixture and FMT. A different accepted layout fails closed with LAYOUT
  and needs a source-only rename in both verifiers and the gate.
- **Production** (FMT "Production directory layout"). `shards/shard_00 … shard_31` must each contain exactly
  `updates.bin`, `paths.bin` and `blocks.bin` (95,846,400 / 1,372,800 / 124,800 B). `shard_00` must also
  contain `audit_rows_blocks_0000_0063.bin` (1,107,296,256 B) and `audit_permutations_blocks_0000_0063.bin`
  (6,815,744 B).
  - The C++ replay reads only `shard_00` (blocks 0–63) and hashes each file in full.
  - Python checks the layout and every size before reading any record. It then streams every record and
    checks the manifest before any analysis JSON.

## 5. Manifest (Python, production only) — PROVISIONAL key names

`run_manifest.json` is strict JSON (§6 item 6) with root keys `study_id = "PHASE2-PERFORMANCE-CONVERSION-002"`,
`key_namespace = "production-r1"`, `specification_sha256`, `terminal_review_sha256` and `design_go_sha256`.
The three hashes must equal the CFG `input_hashes` values. The root key `outputs` is an array of objects with
exactly `path`, `bytes` and `sha256`. Those entries must cover exactly the unique 98 record files of §4,
with byte counts and SHA-256 equal to those computed while streaming. FMT lists only "hashes, counts, timing,
compiler, outputs", so other root keys are permitted and not read.

## 6. Analysis JSON (Python; FMT "Analysis outputs"; SPEC §9–§11; CFG `inference`)

Exact expected values from the n block records (n = 41,600 in production): estimand = Σ numerator /
(denominator · n) as an exact `Fraction`; cell M frequency `ΣL_c/(2048n)`; cell accuracy `1 − ΣP_c/(65536n)`.
Half-widths are `R·sqrt(ln(2/α)/(2n))` with exact `2/α = 120` (α = 0.05/3) or `80` (α = 0.025). They are
computed in a 110-digit Decimal context, and in production each must round (10 decimals) to its CFG frozen
value. Bounds are estimate ∓ half-width at 110 digits. Decisions apply CFG `decision_rules_in_order`,
`secondary_classification_rules_in_order` and `interpretive_branches` with δ = 1/32, using these bounds.

**PROVISIONAL** keys (no study-002 producer-output interface contract was supplied). Keys are read only at
these names; nothing is searched:

1. `ROUTE_CLOSED_INVALID.json` present → INVALID without opening any other analysis file.
2. `estimates_19.json` is an array of exactly 19 objects in FMT order:
   - records 1–3 (`Delta_P`, `D_INFO`, `D_NONINFO`);
   - records 4–5 (`E_INFO`, `E_NONINFO`);
   - records 6–7 (`B_INFO`, `B_NONINFO`);
   - records 8–13 (`M_FREQUENCY_LATE|<cell>`);
   - records 14–19 (`ACCURACY_LATE|<cell>`), with cells in CFG order.
3. Required keys:
   - records 1–7: `record, name, estimate_exact, estimate, half_width, lower, upper`, plus `family_decision`
     (1–3) or `classification` (4–7);
   - records 8–19: `record, name, estimate_exact, estimate`, and `lower`/`upper`/`half_width` null if present.

   Other keys are permitted and not read.
4. Field rules:
   - `record` must be the exact integer and `name` the exact string.
   - `estimate_exact` is a string `"p/q"` with q > 0, compared by **exact rational value**.
   - `estimate`, `half_width`, `lower` and `upper` are a decimal string or a JSON number. Each must agree
     with the 110-digit recomputation within `1e-40`. This tolerance absorbs any producer precision of ≥ 45
     digits and any representation of α, and is far below every decision scale.
   - `family_decision` must equal the recomputed primary label; `classification` the recomputed secondary
     label.
5. `decision.json` is an object with:
   - `decision_rule_applied`: integer, equal to the recomputed rule 2–6;
   - `primary_decision`: the CFG label of that rule;
   - `bounded_interval_wholly_inside_minus_delta_plus_delta`: boolean equal to the recomputed flag when the rule is 5,
     otherwise null or that boolean;
   - `secondary_classifications`: an object with exactly `E_INFO, E_NONINFO, B_INFO, B_NONINFO`, each equal
     to its recomputed label;
   - `interpretive_branch`: equal to the CFG branch statement;
   - `interpretation_labels`: a non-empty array of strings. Their exact text is not frozen in the supplied
     texts and is **not verified**.
6. Strict JSON for every JSON input: a duplicate key, NaN/Infinity or invalid UTF-8 makes it INVALID.

Categories: `ANALYSIS_SCHEMA`, `ANALYSIS_ORDER`, `ESTIMATE` (`estimate_exact`), `ESTIMATE_DISPLAY`, `BOUND`,
`DECISION`, `CONSTANT`, `MANIFEST_*`. All are INVALID.

## 7. Receipts and failure semantics

Receipts are create-exclusive (`O_CREAT|O_EXCL`, mode 0444) and are refused inside an input directory. They
record:
- input byte counts and SHA-256;
- tool, version, and executable or source SHA-256;
- mode and namespace;
- checked vs expected counts;
- KAT / self-check results;
- the first discrepancy (≤ 480 characters) and the mismatch count (capped at 1,000,000);
- start/end UTC and `PASS`/`INVALID`.

Inputs are opened read-only and never modified. The gate verifies the fixture layout's six SHA-256 values
before and after.

**Redaction.** A diagnostic names only the category, the coordinates (block/cell/update/candidate/draw) or
record index or manifest entry index/path, and the field name. It never contains an observed or recomputed
estimate, fraction, bound, decision label, numerator, sum, genotype, permutation, hash value, size or file
bytes. Python reports unexpected exceptions and JSON decode failures by exception type only. PASS receipts
carry no estimate or decision.

**Exit status** (both tools):

| Code | Meaning |
|---|---|
| 0 | PASS |
| 1 | INVALID, receipt written |
| 2 | usage error or unusable receipt path; no receipt |
| 3 | INVALID, and the receipt could not be written |

**Fail-closed coverage.** The following are all INVALID:
- missing files;
- short files (size check, `SHORT_RECORD`, or C++ "ended early");
- trailing bytes (size check, `TRAILING_BYTES` / C++ "trailing bytes");
- reordered records (`ORDER`, or byte mismatch in C++);
- any inconsistency.
