# Independent replay and record auditor — PHASE2-MUTABLE-MEMORY-001 revision 1

Status: **SOURCE ONLY. Nothing here has been compiled, run, tested or benchmarked** (including the
CLAUDE-013 interface-revision-2 completion). No result in this package is evidence of correctness until
the gates below have been run and reviewed.

Two independently written components, derived only from the frozen specification, frozen
configuration and documented binary schemas (field-by-field mapping: `docs/INPUT_CONTRACT.md`):

| Component | Path | Role |
|---|---|---|
| C++17 replay verifier | `src/*.hpp`, `src/main.cpp` → `mmem_audit_replay` | SHA-256 key derivation, Philox4x32-10, coordinate validation, every draw, target laws, candidates, exact weights, Lemire rejection, survivor selection, mutation, caches, final-state and paired-trajectory hashes; byte-exact streaming comparison of audit rows, update, path and block records for the selected blocks |
| Python 3.6 record/estimate verifier | `python/mmem_record_verifier.py` | Parses the four record types; counts, order, sizes, ranges, reserved fields; reconstructs path and block fields; audit-row arithmetic consistency; N1/N2 paired-hash identities in all 41,600 blocks; validates `run_manifest.json` (exact 24-key root, exact fields and the exact 97-entry `outputs[*].path/bytes/sha256` array); recomputes all 22 estimates (precision-60 Decimal in the producer's revision-2 operation order), the decision, family decisions, secondary classifications and relations, and compares them with `estimates_22.json` and `decision.json` (including the six exact interpretation labels) at the exact paths of the frozen producer-output interface contract, revision 2. A separate NONSCIENTIFIC `manifest-interface` mode checks only the manifest schema, entry uniqueness, byte counts and hashes over tiny placeholder files. Generates no random draw and does not simulate. |
| Corruption gate | `tests/fixture_gate.py` | Controls plus five deliberate corruptions on temporary copies (three layout, one estimate record, one manifest `outputs[i].sha256`); checks every INVALID receipt names the intended category/field without the changed value |

Both fail closed: any checked defect gives exit 1 and a create-exclusive `INVALID` receipt with a
bounded first discrepancy and mismatch count. Neither edits an input or reports an estimate: failure
diagnostics name only a category, coordinates, record index or manifest entry index/path, and a field
name — never an observed or recomputed estimate, fraction, bound, decision, per-block value, genotype,
hash value or file bytes (`docs/INPUT_CONTRACT.md` §7).

## Exact future commands

Paths below use the study root
`ROOT=$PROJECT_ROOT`,
`AUD=$ROOT/auditor/phase2_mutable_memory_auditor_v1`, and a new, not-yet-existing output directory
`OUT=$ROOT/auditor-stage/attempt1` (create it with `mkdir -p "$OUT"` first; every receipt path must
not already exist).

1. **Build** (pinned GCC 8.5.0; refuses any other version and any in-tree `BUILD`):
   ```
   make -C "$AUD" BUILD="$OUT/build" all
   ```
   Compiler line: `g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread $AUD/src/main.cpp -o $OUT/build/mmem_audit_replay`.

2. **KAT-only check** (SHA-256 FIPS vectors, three Random123 Philox rows, 128-bit product, Lemire
   threshold/rejection, popcount, coordinate schemas; reads no input, derives no key):
   ```
   "$OUT/build/mmem_audit_replay" --kat-only --receipt "$OUT/kat_receipt.json"
   ```

3. **Fixture-layout audit** (fixture namespace `fixture-r1-nonscientific` only):
   ```
   LAYOUT=$ROOT/fixture-stage/fix1_attempt1/layout_sample
   "$OUT/build/mmem_audit_replay" --mode fixture --layout-dir "$LAYOUT" --receipt "$OUT/fixture_replay_receipt.json"
   python3 -B "$AUD/python/mmem_record_verifier.py" fixture --layout-dir "$LAYOUT" --receipt "$OUT/fixture_records_receipt.json"
   ```

4. **Corruption gate** (works on copies inside a new directory; verifies the source layout is unchanged;
   also writes the synthetic estimate fixture and the 97-file manifest-interface fixture under
   `$OUT/gate` and runs the record verifier's `synthetic-estimates` and `manifest-interface` modes):
   ```
   python3 -B "$AUD/tests/fixture_gate.py" --layout-dir "$LAYOUT" \
     --replay-bin "$OUT/build/mmem_audit_replay" \
     --record-verifier "$AUD/python/mmem_record_verifier.py" --work-dir "$OUT/gate"
   ```

5. **Production replay** (blocks 0–63 of `shards/shard_00`; namespace `production-r1`; only after a
   production run exists and is separately authorized):
   ```
   "$OUT/build/mmem_audit_replay" --mode production --run-dir PRODUCTION_DIR --receipt "$OUT/production_replay_receipt.json"
   ```

6. **Full record and estimate verification** (all 41,600 blocks, manifest, analysis JSON):
   ```
   python3 -B "$AUD/python/mmem_record_verifier.py" production --run-dir PRODUCTION_DIR \
     --analysis-dir ANALYSIS_DIR --receipt "$OUT/production_records_receipt.json"
   ```

Exit codes (both tools): `0` PASS, `1` INVALID with receipt (checked evidence mismatch), `2` usage
error or unusable receipt path (no receipt), `3` INVALID and the receipt could not be written. The gate
exits 0 only if every expectation holds and always writes `$OUT/gate/FIXTURE_GATE_RECEIPT.json` once
`$OUT/gate` has been created (status `FAIL` on any failed expectation or unexpected exception; exit 3 if
that receipt cannot be written; exit 2 if `$OUT/gate` cannot be created). No command here measures time;
a supervisor-owned runner may wrap each command with `/usr/bin/time -v`.

## Conservative expected resources (estimates, not measurements)

| Step | CPUs | Memory | Wall time (conservative ceiling) | Reads |
|---|---|---|---|---|
| Build | 1 | < 1 GiB | 2 min | sources |
| KAT-only | 1 | < 64 MiB | 10 s | none |
| Fixture replay (C++) | 1 | < 256 MiB | 2 min | ~19 MB |
| Fixture records (Python) | 1 | < 1 GiB | 10 min | ~19 MB |
| Corruption gate (12 verifier runs, incl. 2 synthetic-estimate and 2 manifest-interface) | 1 | < 1 GiB | 1 h | ~120 MB copies |
| Production replay (C++) | 1 | < 512 MiB | 2 h | 1.27 GB (shard_00 incl. audit) |
| Production records (Python) | 1 | < 4 GiB | 8 h | 3.30 GB records + JSON |

Basis: ~21 million Philox evaluations and 16.8 million 72-byte row comparisons for the C++ replay;
CPython loops over 85.2 million update records and 16.8 million audit rows for the Python verifier.
These ceilings must be replaced by fixture-measured throughput (ARCH gate 5) before any scheduling.

## Scope limits

- Replay covers only blocks 0–63 (production) or the single layout block (fixture). Blocks 64–41,599
  are checked only for record consistency, N1/N2 and estimate reconstruction, not replayed.
- The Python verifier checks audit rows for internal arithmetic consistency (popcount, weights,
  Lemire, selection, inheritance, lineage, hash reconstruction) but cannot validate a random draw; draw
  correctness rests on the C++ replay alone.
- `final_state_sha256` outside the 64 audit blocks cannot be checked without simulation and is not.
- `run_manifest.json`, `estimates_22.json` and `decision.json` are parsed only at the exact paths,
  keys, types, labels, rules and precision-60 Decimal operation order of
  `PRODUCER_OUTPUT_INTERFACE_CONTRACT_R2.json` (revision 2; `docs/INPUT_CONTRACT.md` §5–§6); no
  heuristic key or string search exists, and the six interpretation labels are compared for exact
  identity and order only. Exact Decimal agreement assumes the producer follows the contract's stated
  operation order; only producer-generated output can confirm the last digit.
- The `manifest-interface` mode is NONSCIENTIFIC: it accepts only placeholder files of at most 4,096
  bytes, hashes them as opaque bytes and checks the manifest; it never parses a record, simulates or
  derives a key, and its PASS says nothing about a production run. Production mode is unchanged: it
  enforces every record size and count before a manifest PASS can count.
- The synthetic gate fixtures exercise the analysis and manifest schemas; both are written from the
  same contract reading as the verifier, so they are not fully independent of it.
- `diagnostics_descriptive.json`, `preflight/*`, job accounting and stdout/stderr are not verified.
- A PASS receipt certifies agreement only; it reports no estimate, decision or scientific result.
- The fixture layout README is hashed but not parsed.
