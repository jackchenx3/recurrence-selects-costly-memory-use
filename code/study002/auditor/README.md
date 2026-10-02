# Independent replay and record auditor — PHASE2-PERFORMANCE-CONVERSION-002 revision 1

Status: **SOURCE ONLY. Nothing here has been compiled, run, tested or benchmarked.** The package was built
in two source-only passes: CLAUDE-022, which was interrupted by the session limit after editing nine files,
and CLAUDE-023, which completed it. Neither pass ran a command. No result in this package is evidence of
correctness until the gates below have been run and reviewed.

**Provenance.** This package was adapted from an authenticated copy of the accepted independent study-001
auditor (itself derived without producer source). The adaptation used only the frozen study-002
specification (`PHASE2-PERFORMANCE-CONVERSION-002-REV1.md`, SHA-256 `58ee414d…afcb895`), `frozen_config.json`
(`PCONV-FROZEN-CONFIG-1`), `OUTPUT_FORMATS.md` and `binary_records.json` (`PCONV-BINARY-RECORDS-1`). No producer
source, header, analyzer, generated constant or helper was read, imported, copied, translated or called. Every key
text, record layout, count and estimand belongs to study 002. Study-001 key texts are rebuilt in exactly one place:
the `--kat-only` disjointness check, which proves that no study-002 key equals a study-001 key. No draw is ever
generated from a study-001 key. The field-by-field source mapping is in `docs/INPUT_CONTRACT.md`.

| Component | Path | Role |
|---|---|---|
| C++17 replay verifier | `src/*.hpp`, `src/main.cpp` → `pconv_audit_replay` | SHA-256 key derivation, Philox4x32-10 (three official Random123 rows), coordinate validation of all ten purposes, and every draw. Six paired cells (INFO / NONINFO / SHAM × ALL_F / ALL_M) and the HALF target law. Recurrence-gated NONINFO decoy `x XOR π(c XOR x)` with one shared permutation per (block, update), built by Fisher–Yates with Lemire retries. Candidates, donor-key tie breaks, exact weights, Lemire survival retries, mutation and caches. Its own N1, N2 and C1 identities, and the final-state and paired-trajectory hashes. Byte-exact streaming comparison of all five binary record types for blocks 0–63. |
| Python 3.6 record/estimate verifier | `python/mmem_record_verifier.py` (tool id `pconv_record_verifier`; the seed file name is kept so the file set is unchanged) | Exact layout of 32 shards and 98 record files, with sizes, order, ranges and reserved fields. Rebuilds path and block records from update records. Checks arm and probe-source counter identities, cross-cell sharing of the permutation evidence, N1/N2 (including SHAM paired-hash equality) and record-level C1 in all 41,600 blocks, and 128 queries in all 63,897,600 path-updates. For blocks 0–63 it checks audit-row and permutation-record consistency. It validates `run_manifest.json`, then recomputes all 19 estimates as exact fractions, the frozen half-widths, the bounds, the primary decision, the secondary classifications and the interpretive branch. These are compared with `estimates_19.json` and `decision.json`. It generates no random draw and does not simulate. |
| Corruption gate | `tests/fixture_gate.py` | Controls plus five deliberate corruptions, each on a temporary copy: a candidate row, a permutation row, a paired path hash, a reserved field, and a synthetic estimate record. Every INVALID receipt must name the intended category or field and must not contain the changed value. |

## What the replay covers (production)

Blocks 0–63 of `shards/shard_00` in namespace `production-r1`. This means 64 block records, 384 paths
(64 × 6 cells), 98,304 update records (384 × 256), 12,582,912 candidate rows (98,304 × 128) and 16,384
permutation rows (64 × 256). Every random value is regenerated: the initial genotypes; the target innovations
and recurrence bits; the fresh, scout and local masks; the donor keys; the survival uniforms, including every
Lemire retry; the policy mutations; and every `DECOY_PERMUTATION` draw at (block, update, step, retry). Each draw
is generated once per block and shared by the six cells, whether or not a cell uses it.

## Exact future commands

The supervisor sets three variables. `ROOT` is the study-002 root assigned by the supervisor. `AUD` is
`$ROOT/auditor/phase2_performance_conversion_auditor_v1`, an authenticated copy of this package. `OUT` is a new,
not-yet-existing output directory: create it with `mkdir -p "$OUT"` first, and make sure no receipt path already
exists. `LAYOUT` is the accepted study-002 fixture-namespace layout sample (see *Pre-production prerequisites*).

1. **Build** (pinned GCC 8.5.0; refuses any other version and any in-tree `BUILD`):
   ```
   make -C "$AUD" BUILD="$OUT/build" all
   ```
   Compiler line: `g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread $AUD/src/main.cpp -o $OUT/build/pconv_audit_replay`.

2. **KAT-only check.** Covers three SHA-256 FIPS vectors, the three official Random123 Philox4x32-10 rows,
   key-word byte order, and the literal production and fixture key texts. It checks that 38 key words are
   distinct across purposes, namespaces and study 001. It also covers the 128-bit product, the Lemire survival
   and Fisher–Yates thresholds, forced rejection and reachability, popcount, FNV-1a, and the coordinate schemas
   including `DECOY_PERMUTATION`. Further cases: the identity, rotation and forced-retry permutations; the
   exhaustive 4- and 5-position analogues and the uniform subset law; decoy distance matching for weights 0–32
   (weights 0 and 32 are retained); overlap preservation; the probe-rule table; and the HALF law. It reads no
   input and constructs no stream.
   ```
   "$OUT/build/pconv_audit_replay" --kat-only --receipt "$OUT/kat_receipt.json"
   ```

3. **Fixture-layout audit** (namespace `fixture-r1-nonscientific` only):
   ```
   "$OUT/build/pconv_audit_replay" --mode fixture --layout-dir "$LAYOUT" --receipt "$OUT/fixture_replay_receipt.json"
   python3 -B "$AUD/python/mmem_record_verifier.py" fixture --layout-dir "$LAYOUT" --receipt "$OUT/fixture_records_receipt.json"
   ```

4. **Corruption gate.** It works on copies inside a new directory and checks that the source layout is unchanged.
   It also writes the four-block NONSCIENTIFIC synthetic estimate fixture under `$OUT/gate` and runs the record
   verifier's `synthetic-estimates` mode.
   ```
   python3 -B "$AUD/tests/fixture_gate.py" --layout-dir "$LAYOUT" \
     --replay-bin "$OUT/build/pconv_audit_replay" \
     --record-verifier "$AUD/python/mmem_record_verifier.py" --work-dir "$OUT/gate"
   ```

5. **Production replay.** Covers blocks 0–63 of `shards/shard_00` in namespace `production-r1`. Run it only after
   a production run exists and is separately authorized:
   ```
   "$OUT/build/pconv_audit_replay" --mode production --run-dir PRODUCTION_DIR --receipt "$OUT/production_replay_receipt.json"
   ```

6. **Full record and estimate verification** (all 41,600 blocks, the manifest and the analysis JSON):
   ```
   python3 -B "$AUD/python/mmem_record_verifier.py" production --run-dir PRODUCTION_DIR \
     --analysis-dir ANALYSIS_DIR --receipt "$OUT/production_records_receipt.json"
   ```

**Exit codes (both tools):**

| Code | Meaning |
|---|---|
| 0 | PASS |
| 1 | INVALID, receipt written (checked evidence mismatch) |
| 2 | usage error or unusable receipt path; no receipt |
| 3 | INVALID, and the receipt could not be written |

The gate exits 0 only if every one of its 12 expectations holds. Once `$OUT/gate` exists, it always writes
`$OUT/gate/FIXTURE_GATE_RECEIPT.json`, with status `FAIL` on any failed expectation or unexpected exception. It
exits 3 if that receipt cannot be written, and exits 2 if `$OUT/gate` cannot be created. No command here measures
time; a supervisor-owned runner may wrap each command with `/usr/bin/time -v`. This package contains no
production runner.

**Receipts** are create-exclusive (mode 0444) and refused inside an input directory. Inputs are opened read-only.
A failure names only a category, coordinates or record index, and a field. It never contains an observed or
recomputed estimate, fraction, bound, decision label, numerator, sum, genotype, permutation, hash value or file
bytes. PASS receipts carry no estimate or decision (`docs/INPUT_CONTRACT.md` §7).

## Pre-production prerequisites

Before the fixture gate, each of the following must be confirmed against a frozen study-002 producer-output
interface, or corrected by a new source-only revision:
- the **PROVISIONAL** items of `docs/INPUT_CONTRACT.md`;
- the six fixture layout file names (§4);
- the `run_manifest.json` root keys (§5);
- the `estimates_19.json` and `decision.json` keys and encodings (§6).

Each of these fails closed if the producer differs. Every other step, in order, must also pass:
- build;
- KAT-only;
- fixture replay and fixture records;
- the gate;
- an independent review of the receipts.

Finally, the resource ceilings below must be replaced by throughput measured on the fixtures before any
scheduling.

## Conservative expected resources (estimates, not measurements)

| Step | CPUs | Memory | Wall time (conservative ceiling) | Reads |
|---|---|---|---|---|
| Build | 1 | < 1 GiB | 2 min | sources |
| KAT-only | 1 | < 64 MiB | 10 s | none |
| Fixture replay (C++) | 1 | < 256 MiB | 2 min | ~17.5 MB |
| Fixture records (Python) | 1 | < 1 GiB | 10 min | ~17.5 MB |
| Corruption gate (12 verifier runs) | 1 | < 1 GiB | 1 h | ~90 MB copies |
| Production replay (C++) | 1 | < 512 MiB | 2 h | 1.21 GB (shard_00 incl. audit) |
| Production records (Python) | 1 | < 2 GiB | 8 h | 4.23 GB records + JSON |

Basis: about 21 million Philox evaluations (64 blocks × 256 updates × about 1,280 draws, plus retries) and
12.6 million 88-byte row comparisons for the C++ replay. For the Python verifier, CPython loops over 63.9 million
update records and 12.6 million audit rows.

## Scope limits

- Replay covers only blocks 0–63 (production) or the single layout block (fixture). Blocks 64–41,599 are
  checked for record consistency, N1/N2, record-level C1, query counts and estimate reconstruction, but they are
  not replayed.
- The Python verifier checks audit and permutation rows for internal consistency only. That includes popcount,
  weights, probe-source rule, decoy masks, Lemire acceptance, Fisher–Yates replay of the accepted draws,
  selection, inheritance, caches and hash reconstruction. It cannot validate that a draw is the correct Philox
  output, or that a lower retry was rejected. Draw correctness and donor-key tie breaks rest on the C++ replay
  alone.
- `final_state_sha256` outside the 64 audit blocks cannot be checked without simulation and is not checked.
- The analysis and manifest interfaces are read only at their PROVISIONAL exact key names. Nothing is searched.
  Estimates are compared exactly as fractions. Decimal strings are compared within `1e-40` against a 110-digit
  recomputation, because no producer Decimal operation order is frozen. The text of `interpretation_labels` is
  not verified.
- The synthetic gate fixture exercises the estimate comparison only. It and the verifier read the same
  provisional interface, so the fixture is not independent of it.
- `diagnostics_descriptive.json`, `preflight/*`, job accounting and stdout/stderr are not verified. The fixture
  layout README is hashed but not parsed.
- A PASS receipt certifies agreement only. It reports no estimate, decision or scientific result.

## Interpretation limits (unchanged from the frozen design)

`Delta_P` is the total population-accuracy effect of directional cache information beyond the parent and the
matched displacement length. That effect is mediated through evolved policy use. It is not a per-use value of
information. It does not separate retention from retrieval, and it does not establish spontaneous memory
origin. The shared permutation does not preserve every cross-genotype relation. Displacement weights 0 and 32
cannot be scrambled and keep their direction; they are counted and reported. A null, bounded or adverse outcome
is valid and reportable, and it does not trigger an extension. The result is limited to one supplied cache, one
HALF law, one displacement cost and one directional-scramble control. The auditor neither reports nor
interprets any outcome.
