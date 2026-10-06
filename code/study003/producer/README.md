# PHASE2-TORUS-MEMORY-003 producer / analyzer / fixture package (source only)

**Status: SOURCE ONLY — NOT COMPILED, NOT RUN, NO FIXTURE PASSED, NO
SCIENTIFIC OUTCOME EXISTS. Scientific execution is not authorized.**
See `IMPLEMENTATION_NOT_RUN.json` for every pending gate.

Implements, without optional modes, the frozen design
`PHASE2-TORUS-MEMORY-003-REV1.md` (SHA-256 `0ff662dc31807b0b2cdcae371d884473ed398e2583272d94331bac8c8f9ff0b0`),
design-GO record SHA-256 `e5dc485dd858f18eb9652cfe6b63f142e2d09fb6b069d640438cd8dbe63a786d`,
terminal review SHA-256 `68ccffc3c9a4d013590e7b5c8e5f4200ce955b77a76554bd58bad4bcc7dcc04a`.
New implementation; no Study 001/002 source is present or used. The only
external dependency is the byte-identical pinned Random123 seed under
`third_party/` (seed receipt `3fe28150…dd53`).

## Fixed study

| Quantity | Value |
|---|---:|
| Blocks × cells | 41,600 × 8 |
| Paths | 332,800 |
| Updates per path / late window | 256 / 193–256 |
| Path-updates | 85,196,800 |
| Objective queries (128 per update) | 10,905,190,400 |
| Estimate records | 22 (4 primary, 2 performance, 16 cell means) |
| Audit blocks / paths / path-updates | 0–63 / 512 / 131,072 |
| Audit candidate rows / tournament-entry rows | 16,777,216 / 16,777,216 |
| Declared Threefry draws | 164,672 per block; 6,850,355,200 total |
| Resource ceiling | 32 CPU, 64 GiB, 12 h, 150 GiB new output, one job |

## Layout

| Path | Contents |
|---|---|
| `config/torus_003_frozen_config.json` | frozen machine-readable binding of every count, purpose, hash, threshold and resource limit; the binaries require its exact key order and values |
| `src/torus/constants.hpp` | compiled constants and identities |
| `src/torus/torus_loss.hpp` | exact circular squared loss |
| `src/torus/threefry_rng.*` | Random123 wrapper, purpose keys, addressing, domain aborts, KATs |
| `src/torus/target_law.*` | ZERO / HALF target laws, initial phenotypes |
| `src/torus/tape.*` | unconditional per-(block, update) random tape |
| `src/torus/candidates.*` | four candidate families, donor choice, local child |
| `src/torus/tournament.*` | 32 four-entry tournaments with replacement |
| `src/torus/transition.*` | label inheritance, mutation, cache event order |
| `src/torus/step.*`, `path_runner.*` | one update; one block of eight paired cells, digests, N1/N2 |
| `src/torus/records.*`, `chunk_io.*` | binary schemas and chunked output |
| `src/torus/collision.*` | declared-address collision audit, order/thread invariance |
| `src/torus/config_check.*` | fail-closed config, identity and authorization checks |
| `src/torus/sha256.*`, `util.*`, `cli.hpp`, `manifest.*` | support |
| `src/apps/torus_produce.cpp` | producer (`production` / non-production `timing`) |
| `src/apps/torus_fixtures.cpp` | 16 deterministic fixture groups (`fixture-r1`) |
| `src/apps/torus_regen.cpp` | single-path / single-block regeneration and byte comparison |
| `tools/torus_analyze.py` | merge/analyze: complete authenticated set → 22 estimates + ordered decisions |
| `tools/torus_manifest.py` | source manifests, verification, seed byte-identity check |
| `tools/torus_preflight.py` | static preflight (no compile, no execution) |
| `tools/torus_schema.py` | binary schema reader shared by the tools |
| `slurm/torus_003_production.slurm.template` | inert one-job template, no submission command |
| `docs/` | schemas, RNG addressing, fixtures, gates, interpretation/limitations |
| `provenance/random123_seed_inventory.json` | the 29 authenticated seed files |

## Intended commands (for later, separately authorized gates; none has been run)

```
make BUILD=build
python3 tools/torus_preflight.py --package-root . --out <new file>
python3 tools/torus_manifest.py seed-check --root . --out <new file>
build/torus_fixtures --package-root . --config config/torus_003_frozen_config.json \
    --expect-config-sha256 <sha> --threads <2..32> --out <new dir>
build/torus_produce --mode timing --namespace timing-r1 --package-root . \
    --config config/torus_003_frozen_config.json --expect-config-sha256 <sha> \
    --fixture-receipt <fixture_receipt.json> --timing-block-begin <b0> \
    --timing-block-end <b1> --threads <n> --out <new dir>
build/torus_regen --namespace fixture-r1 --package-root . --config ... \
    --expect-config-sha256 <sha> --block <b> --cell <0..7|all> \
    --with-audit-rows <yes|no> [--compare-dir <dir>] --out <new dir>
python3 tools/torus_analyze.py --production-dir <dir> --config ... \
    --design-spec <file> --design-go-record <file> --out <new dir>
```

Every executable requires explicit output paths and refuses existing nonempty
output. Production additionally requires the design text, the design-GO
record, a separate execution-authorization record and the production
namespace (`docs/PRODUCTION_GATES.md`). Fixtures and timing use the separate
`fixture-r1` / `timing-r1` namespaces and never emit estimates; the analyzer
accepts only `production-r1` data.

## Exactness

Loss, ranking, saved accuracy numerators and all accumulation are integers.
The analyzer forms exact `Fraction` estimates and decides every threshold by
comparing an exact rational against a rigorously bracketed Decimal Hoeffding
half-width, refining precision until decided. Decimal strings are display only.
