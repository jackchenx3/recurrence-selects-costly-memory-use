# PHASE2-MUTABLE-MEMORY-001 revision 1: implementation package (CLAUDE-004)

**Nothing in this package has been compiled, run, tested, benchmarked or submitted.** No fixture
result, timing, random value or scientific outcome exists. Production execution is **not
authorized**. A separate review must grant each later authority (build, fixtures, production).

* Specification SHA-256: `c17a3a1e9ac9cf2260f6743eaf08e4c2808080a767af16d2f9e9d2b9ad3e821d`
* Terminal review SHA-256: `787cf32a20312f2682c761243f3dec04c5eb8036d14d6efc444e9830db37960a`

The source is new. It imports, copies, calls and loads no producer code from studies 001–064.

## Architecture

| Path | Role |
|---|---|
| `config/frozen_config.json` | All frozen counts, operators, RNG schemas, inference, decision rules, outputs, both input hashes |
| `src/mm/constants.hpp` | Compiled frozen constants and `static_assert` count identities |
| `src/mm/sha256.hpp` | Self-contained SHA-256 (purpose keys, final-state hashes, manifests) |
| `src/mm/philox.hpp`, `kat.hpp` | Philox4x32-10 and the literal Random123 / FIPS known-answer vectors |
| `src/mm/keys.hpp`, `coords.hpp` | Purpose-key derivation; counter schemas `(block, update, entity, subindex)` with range aborts |
| `src/mm/draws.hpp` | Per-(block, update) exogenous draws generated for every coordinate; `(word0 & 31U) == 0U`; widened `word1` |
| `src/mm/selection.hpp` | Exact integer Plackett–Luce with Lemire rejection; portable 64×64→128 product |
| `src/mm/model.hpp` | State, target laws, four candidate families, event order, mutation, cache transition |
| `src/mm/block_runner.hpp` | The eight paired cells of a block in lockstep; N1/N2 after every update; encodes records |
| `src/mm/records.hpp` | Little-endian record encoders, final-state hash and audit-only paired-trajectory hash |
| `src/mm/collision_audit.hpp` | Schema enumeration and purpose-separated collision audit |
| `src/mm/fixtures.hpp` | K0 (Philox KAT, first), S0, KEYS, COLLISION, F1–F10 |
| `src/fixtures_main.cpp` | `mmem_fixtures` |
| `src/production_main.cpp` | `mmem_production` (single run; refuses without acknowledgement) |
| `src/regenerate_main.cpp` | `mmem_regenerate` (post-production block regeneration receipt) |
| `tools/merge_analyze.py` | Validity checks, exact 22 records, primary decision, secondary family |
| `tools/collision_schema_check.py` | Independent Python enumeration of the declared schemas |
| `tools/preflight.py`, `tools/manifest.py`, `tools/mmem_common.py` | Config/hash/key checks; source manifest; shared layouts |
| `tests/test_merge_analyze.py` | Analyzer decision-table tests and C++/Python layout cross-check |
| `slurm/launcher_template.slurm` | Inert resource template (32 CPUs, 64 GiB, 6 h) |
| `docs/OUTPUT_FORMATS.md`, `schemas/binary_records.json` | Output formats |
| `docs/INTERPRETATION_AND_LIMITS.md` | Labels that must stay visible |

Event order per update (spec §6): build families 0–2 (SHAM never reads labels or caches when
constructing genotypes) → evaluate 96 → donor = lowest mismatch, then lowest 64-bit donor key, then
lowest family → local child = donor XOR local mask → evaluate (128 queries, asserted) → weights
2^(32−h) → 32 sequential Lemire/Plackett–Luce draws, survivor slot = draw order → inherit producing
parent's label → flip with policy-mutation draw of the survivor slot → cache := producing parent's
pre-update genotype for post-mutation M, invalid for F → record.

## Build (system compiler, GCC 8.5.0; not yet attempted)

The Makefile pins the compiler and flags below with `:=`; environment variables do not replace them.

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
python3 tools/preflight.py --config config/frozen_config.json --spec SPEC.md --review REVIEW.md
./build/mmem_fixtures --timing-benchmark 20   # fixture namespace only; prints timing, no outcome
```

K0 compares Philox output with the literal Random123 vectors before any other check; if K0 fails no
population path runs. F6 and F10 run full paths only with the non-scientific key namespace
`fixture-r1-nonscientific`, so they never reproduce a production draw. The production driver runs
the entire fixture suite and the collision audit again as preflight and refuses to start if any fails.

## Production command syntax (NOT AUTHORIZED)

```
./build/mmem_production \
  --production I_ACKNOWLEDGE_SINGLE_FROZEN_PRODUCTION_RUN_PHASE2-MUTABLE-MEMORY-001-R1 \
  --config config/frozen_config.json --spec SPEC.md --review REVIEW.md \
  --out /new/dir/outside/package --threads 32
```

It refuses a wrong acknowledgement, any config mismatch, wrong spec/review hash, threads outside 1–32,
an existing output directory, or an output directory inside this package. Block count, cells and
updates are compiled in; no option changes them, and no logic depends on timing. Blocks go to 32
fixed shards (`shard = block / 1300`); threads only choose which shard to run next, so output bytes do
not depend on the thread count.

After production (separately authorized):

```
python3 tools/merge_analyze.py --config config/frozen_config.json --production-dir DIR --out NEW_DIR
./build/mmem_regenerate --post-production-audit I_ACKNOWLEDGE_POST_PRODUCTION_REGENERATION_AUDIT \
  --config config/frozen_config.json --production-dir DIR --block B --receipt NEW_FILE
```

The analyzer runs every validity check first. Any failure gives INVALID, which is terminal: no
estimate is computed and `ANALYSIS_LOCK.json` forbids re-analysis. The independent reviewer replay of
all 512 audit paths must be written by a non-implementer and is not part of this package.

## Expected resource model (estimate only; nothing has been timed)

* Work: 85,196,800 path-updates. Each needs 128 popcounts, at most 32×128 additions for weight sums
  and scans, and the block shares about 1,250 Philox calls per update across its eight cells
  (about 1.3×10^10 Philox calls in total).
* Rough single-thread estimate: 5–20 µs per path-update, so roughly 0.1–0.5 CPU-hours in total. The
  ceiling is 32 CPUs × 6 h = 691,200 CPU-seconds, about 8.1 ms per path-update. Confirm with the
  fixture-namespace `--timing-benchmark` before any request.
* Memory: about 18 MiB per thread at most (the 8×256×128×72-byte audit buffer for blocks 0–63;
  otherwise about 50 KiB). The collision audit uses tens of MiB once. Well under 1 GiB in total.
* Output: 3,299,274,752 bytes of records (~3.07 GiB). The ceiling is 100 GiB.
* Analysis: single-threaded Python over about 2 GB of update records and 1.2 GB of audit rows.
  It is expected to take tens of minutes and has not been timed.
