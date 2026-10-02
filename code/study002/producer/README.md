# PHASE2-PERFORMANCE-CONVERSION-002 revision 1: implementation package (CLAUDE-021)

**Nothing in this package has been compiled, run, tested, benchmarked or submitted.** No fixture
result, timing, random value or scientific value exists for study 002. Production execution is **not
authorized**. A separate review must grant each later authority (build, fixtures, production).

* Specification SHA-256: `58ee414d753fab042e6cccee1a4e6b95dd7f167633b4f33c1491687d9afcb895`
* Terminal design review SHA-256: `76b0c6172e61f60befecef183e83d9e43ea50c1b6bc1e93299668b76f55be60f`
* Design-GO record `PHASE2-PERFORMANCE-CONVERSION-002-DESIGN-GO-1`: SHA-256
  `f4a6180109e0d212cbe784a7722a64576e5e7cb0fbfd4b23c757bb5e7ab84d6a`.
  The supervisor inserted this exact provenance hash deterministically after source construction;
  no code was compiled or run. `mmem_production` checks the record hash and verifies that the
  record's design and review hashes match the two identities above.

This package is the accepted study-001 package (`PHASE2-MUTABLE-MEMORY-001`), adapted in place. No
study-001 code is imported or loaded at runtime. Every key text carries the study-002 identifier,
so no study-001 random value, block or outcome can recur (fixture KEYS checks key disjointness).

## Scientific delta implemented (revision 1 only)

* One HALF law; six cells per block: `INFO`, `NONINFO`, `SHAM` × `ALL_F`, `ALL_M`
  (cell = 2·arm + start). 41,600 blocks, 249,600 paths, 63,897,600 path-updates,
  8,178,892,800 objective queries, 19 saved estimates.
* **INFO** is the accepted ACTIVE operator: a valid-M parent probes its true cache `c`.
* **NONINFO** probes `c` when `t < 3` or `R_t = 0`, and `x XOR pi(c XOR x)` when `t ≥ 3` and `R_t = 1`.
  `pi` is one exact uniform permutation of the 32 bit positions, shared by every parent and all
  six cells for that (block, update) and generated for every update whether or not it is used. A
  source bit `s` moves to destination `pi[s]`. The operator reads `R_t`; the allele never does.
* **SHAM** is the accepted fresh-mask operator and never reads labels or caches when building genotypes.
* `DECOY_PERMUTATION` (purpose 9) counter = `(block, update, entity = i, subindex = retry)` for
  Fisher–Yates steps `i = 31..1`; 64-bit draw `word0 | (word1 << 32)` with `word1` widened first;
  `j` uniform in `[0, i]` by Lemire multiply-high rejection with bound `i+1`; words 2–3 unused.
  Purposes 0–8 keep their accepted semantics. Namespace:
  `PHASE2-PERFORMANCE-CONVERSION-002|production-r1|<purpose>`.
* Unchanged: 32 individuals, 32-bit genotypes, 256 updates, late window 193–256, four candidate
  families and their order, donor rule, integer Plackett–Luce survival with Lemire draws, policy
  mutation, cache transition, target generation and the true-accuracy definition.

## Architecture

| Path | Role |
|---|---|
| `config/frozen_config.json` | All frozen counts, operators, RNG schemas, identities, inference, decision rules, branches, claim limits, outputs, design/review/GO identities |
| `src/mm/constants.hpp` | Compiled frozen constants, six-cell map and `static_assert` count identities |
| `src/mm/sha256.hpp` | Self-contained SHA-256 (unchanged) |
| `src/mm/philox.hpp`, `kat.hpp` | Philox4x32-10 and the literal Random123 / FIPS known-answer vectors (unchanged) |
| `src/mm/keys.hpp`, `coords.hpp` | Ten purpose keys; counter schemas `(block, update, entity, subindex)` with range aborts, including `DECOY_PERMUTATION` |
| `src/mm/draws.hpp` | Per-(block, update) exogenous draws, the `DecoySource` interface, permutation fields, fingerprint |
| `src/mm/selection.hpp` | Accepted Plackett–Luce/Lemire code unchanged; appended `lemire_accept`, Fisher–Yates, `fill_update_all` |
| `src/mm/model.hpp` | State, single HALF law, the three probe rules, `permute_bits`, event order, mutation, cache transition |
| `src/mm/block_runner.hpp` | The six paired cells of a block in lockstep; N1/N2 and C1 after every update; encodes records |
| `src/mm/records.hpp` | Little-endian encoders, final-state hash, paired-trajectory hash, audit permutation record |
| `src/mm/collision_audit.hpp` | Schema enumeration and purpose-separated collision audit over six paired cells |
| `src/mm/fixtures.hpp` | K0 (Philox KAT, first), S0, KEYS, COLLISION, F1–F13 (mapping to spec §13 in the file header) |
| `src/fixtures_main.cpp` | `mmem_fixtures` |
| `src/production_main.cpp` | `mmem_production` (single run; refuses without acknowledgement or verified design-GO) |
| `src/regenerate_main.cpp` | `mmem_regenerate` (post-production block regeneration receipt, incl. audit permutations) |
| `tools/merge_analyze.py` | Validity checks, 64-block candidate replay, exactly 19 records, frozen decision order |
| `tools/collision_schema_check.py` | Independent Python enumeration of the declared schemas over six cells |
| `tools/preflight.py`, `tools/manifest.py`, `tools/mmem_common.py` | Config/hash/GO/key checks; source manifest; shared layouts and replay helpers |
| `tests/test_merge_analyze.py` | Analyzer decision-table tests, operator-consistency tests, C++/Python layout and replay cross-check |
| `slurm/launcher_template.slurm` | Inert resource template (32 CPUs, 64 GiB, 6 h); never calls `sbatch` |
| `docs/OUTPUT_FORMATS.md`, `schemas/binary_records.json` | Output formats and the 64-block replay contract |
| `docs/INTERPRETATION_AND_LIMITS.md` | Labels that must stay visible |

The file set is the study-001 file set; no file was added or removed. Binary names keep the
`mmem_` prefix from the accepted Makefile, which is unchanged.

Event order per update (spec §§5–6): generate every exogenous draw, including the shared
permutation → build families 0–2 (SHAM never reads labels or caches; NONINFO alone reads `R_t`) →
evaluate 96 → donor = lowest mismatch, then lowest 64-bit donor key, then lowest family → local child
= donor XOR local mask → evaluate (128 queries, asserted) → weights 2^(32−h) → 32 sequential
Lemire/Plackett–Luce draws, survivor slot = draw order → inherit the producing parent's label → flip
with the survivor slot's policy-mutation draw → cache := producing parent's pre-update genotype for
post-mutation M, invalid for F → record.

## Run-time identities (any failure is INVALID)

* **N1**: SHAM genotype, target, mismatch, survival-retry and survivor-index trajectories are
  bit-identical between starts (checked every update; bound by the paired-trajectory SHA-256).
* **N2**: SHAM labels are exact slotwise complements after every update.
* **C1** (new; enforces spec fixtures 3–4 in every block): for each start, INFO and NONINFO are
  bit-identical in results and post-update states until the first update at which a NONINFO decoy
  probe differs from the true cache, and that update must be recurrent. The update is recorded per
  block and start.
* Exactly 128 objective queries per path-update; total 8,178,892,800.

## Build (system compiler, GCC 8.5.0; not yet attempted)

```
make                       # or, equivalently:
mkdir -p build
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread -Isrc src/fixtures_main.cpp   -o build/mmem_fixtures   -lstdc++fs
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread -Isrc src/production_main.cpp -o build/mmem_production -lstdc++fs
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread -Isrc src/regenerate_main.cpp -o build/mmem_regenerate -lstdc++fs
```

Python utilities need Python ≥ 3.6 and only the standard library.

## Fixture commands (pending authorization)

```
./build/mmem_fixtures --receipt /path/outside/package/fixtures.json \
                      --emit-layout-sample /path/outside/package/layout_sample   # empty existing dir
python3 tests/test_merge_analyze.py --layout-sample /path/outside/package/layout_sample
python3 tools/collision_schema_check.py --config config/frozen_config.json
python3 tools/preflight.py --config config/frozen_config.json --spec SPEC.md --review REVIEW.md \
                           --design-go DESIGN_002_GO.json
./build/mmem_fixtures --timing-benchmark 20   # fixture namespace only; prints timing, no outcome
```

K0 compares Philox output with the literal Random123 vectors before any other check; if K0 fails no
population path runs. F6, F9, F10 and F13 run full paths only with the non-scientific key namespace
`fixture-r1-nonscientific`, so they never reproduce a production draw. The production driver runs the
entire fixture suite and the collision audit again as preflight and refuses to start if any fails.

## Production command syntax (NOT AUTHORIZED)

```
./build/mmem_production \
  --production I_ACKNOWLEDGE_SINGLE_FROZEN_PRODUCTION_RUN_PHASE2-PERFORMANCE-CONVERSION-002-R1 \
  --config config/frozen_config.json --spec SPEC.md --review REVIEW.md --design-go DESIGN_002_GO.json \
  --out /new/dir/outside/package --threads 32
```

It refuses a wrong acknowledgement, any config mismatch, wrong spec/review hash, a design-GO sentinel
or mismatch, threads outside 1–32, an existing output directory, or an output directory inside this
package. Block count, cells and updates are compiled in; no option changes them, and no logic depends
on timing. Blocks go to 32 fixed shards (`shard = block / 1300`); threads only choose which shard to
run next, so output bytes do not depend on the thread count.

After production (separately authorized):

```
python3 tools/merge_analyze.py --config config/frozen_config.json --production-dir DIR --out NEW_DIR
./build/mmem_regenerate --post-production-audit I_ACKNOWLEDGE_POST_PRODUCTION_REGENERATION_AUDIT \
  --config config/frozen_config.json --production-dir DIR --block B --receipt NEW_FILE
```

The analyzer runs every validity check first. Any failure gives INVALID, which is terminal: no
estimate is computed and `ANALYSIS_LOCK.json` forbids re-analysis. The independent auditor (replay of
all 384 audit paths without producer code) and the production-submission controller are separate
assignments for non-implementing workers and are not part of this package.

## Saved estimates (exactly 19)

| # | Name | Family | Range | Half-width |
|---|---|---|---|---|
| 1 | `Delta_P` = P_INFO − P_NONINFO | primary, α_each = 0.05/3 | 2 | 0.0151712845 |
| 2 | `D_INFO` = L(INFO,M) − L(INFO,F) | primary (gate) | 2 | 0.0151712845 |
| 3 | `D_NONINFO` = L(NONINFO,M) − L(NONINFO,F) | primary (gate) | 2 | 0.0151712845 |
| 4 | `E_INFO` = A_INFO − A_NONINFO | allele, α_each = 0.025 | 2 | 0.0145146256 |
| 5 | `E_NONINFO` = A_NONINFO − 1/2 | allele | 1 | 0.0072573128 |
| 6 | `B_INFO` = P_INFO − P_SHAM | performance, α_each = 0.025 | 2 | 0.0145146256 |
| 7 | `B_NONINFO` = P_NONINFO − P_SHAM | performance | 2 | 0.0145146256 |
| 8–13 | late M frequency per cell | descriptive | — | — |
| 14–19 | late accuracy per cell | descriptive | — | — |

Decision order (δ = 1/32): INVALID → START-DEPENDENT; PRIMARY UNRESOLVED (either gate not wholly
inside (−δ, +δ)) → MEANINGFUL POSITIVE (lower Delta_P > δ) → MEANINGFUL ADVERSE (upper < −δ) →
BOUNDED BELOW THE POSITIVE ONE-BIT SCALE (upper ≤ δ; states whether wholly inside) → UNRESOLVED.

## Claim limits (also in config, decision.json and docs)

* `Delta_P` is the total population-accuracy effect of directional cache information beyond the
  parent and matched displacement length, mediated through evolved policy use.
* It is not a per-use value-of-information estimate, does not separate retention from retrieval, and
  does not demonstrate spontaneous memory origin.
* The fixed shared permutation does not preserve every cross-genotype relation; weights 0 and 32
  retain direction and are counted.
* A null, bounded or adverse outcome is reportable and must not trigger a numerical extension.

## Expected resource model (estimate only; nothing has been timed)

* Work: 63,897,600 path-updates. Each needs 128 popcounts, at most 32×128 additions for weight sums
  and scans, a 32-entry permutation check and, in NONINFO at recurrent updates, one 32-bit
  relabeling per valid-M parent. Each (block, update) shares about 1,281 Philox calls across its six
  cells (about 1.36×10^10 Philox calls in total; the 31 Fisher–Yates calls add about 2.5%).
* Rough single-thread estimate: 5–20 µs per path-update, so roughly 0.1–0.4 CPU-hours in total. The
  ceiling is 32 CPUs × 6 h = 691,200 CPU-seconds, about 10.8 ms per path-update. Confirm with the
  fixture-namespace `--timing-benchmark` before any request.
* Memory: about 17 MiB per thread at most (the 6×256×128×88-byte audit buffer for blocks 0–63;
  otherwise about 80 KiB). The collision audit uses tens of MiB once. Well under 1 GiB in total.
* Output: 4,229,120,000 bytes of records (~3.94 GiB). The ceiling is 100 GiB.
* Analysis: single-threaded Python over about 3.1 GB of update records and 1.1 GB of audit rows,
  with a per-candidate replay. It is expected to take tens of minutes to a few hours and has not been timed.
