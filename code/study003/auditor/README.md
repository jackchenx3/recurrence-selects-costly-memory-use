# phase2_torus_memory_auditor_v1: independent PHASE2-TORUS-MEMORY-003 auditor

**Status: source only. Not compiled, not executed, not tested. No check has passed.** Worker CLAUDE-034 built this package with Read, Edit and Write only. It is a single recovery after CLAUDE-033 failed before writing anything; see `AUDITOR_NOT_RUN.json`. Every command below is for a later, separately authorized gate.

## Contents

| Path | Role |
|---|---|
| `src/sha256.hpp` | Independent SHA-256 (FIPS 180-4) |
| `src/threefry.hpp` | Independent Threefry4x64-20, purpose keys, range-checked draw schema |
| `src/records.hpp` | Byte layout, fail-closed readers, manifest, input identity, receipts |
| `src/replay.hpp` | Torus loss, target laws, four candidate families, donor, local child, tournaments, mutation/cache transition, expected rows and hashes |
| `src/main.cpp` | `torus_audit`: `selftest`, `replay`, `emit-fixture` |
| `python/torus_record_verifier.py` | Python 3.8 standard-library verifier for all seven record types, 650 chunks, N1/N2 and the 22 estimates |
| `tests/fixture_gate.py` | Future non-scientific corruption/self-test harness |
| `docs/INPUT_CONTRACT.md` | Accepted inputs, checks, and **interpretations needing review** |
| `Makefile` | Out-of-tree GCC 8.5 / C++17 build |
| `AUDITOR_NOT_RUN.json` | Honest not-run record |

## Independence

The package was written into an empty seed, using only the frozen specification, the frozen config, `BINARY_SCHEMAS.md`, the auditor input interface and the saved fixture key-derivation record.

- No producer, analyzer, prior-study or prior-auditor source was read, imported, copied, translated or invoked.
- No Random123 code is used. Threefry is checked against the two official known-answer vectors stated in the design.
- The SHA-256 purpose-key rule is also cross-checked against the eleven saved `fixture-r1` digests and key words. These are saved records, not code.
- Neither tool calls a producer binary.

## Future commands (each needs its own authority)

```
# 1. Build (out of tree; refuses a BUILD_DIR inside the package; requires g++ 8.5.x)
make -f <pkg>/Makefile BUILD_DIR=/abs/build/torus_auditor

# 2. Fixture gate (non-scientific, namespace fixture-r1, temporary inputs only)
python3 <pkg>/tests/fixture_gate.py --binary /abs/build/torus_auditor/torus_audit \
    --source-dir <pkg> --receipt <out>/fixture_gate_receipt.json [--blocks 2]

# 3. C++ independent replay of audit blocks 0-63 (production-r1)
/abs/build/torus_auditor/torus_audit replay --root <records> --manifest <manifest.sha256> \
    --source-dir <pkg> --receipt <out>/cpp_replay_receipt.json

# 4. Python full record verification (all 41,600 blocks, 22 estimates)
python3 <pkg>/python/torus_record_verifier.py verify --root <records> \
    --manifest <manifest.sha256> --receipt <out>/py_verify_receipt.json --profile production
```

`torus_audit selftest --source-dir <pkg> --receipt <file>` runs the KAT and hand traces on their own. `replay` always runs them first.

### Exit codes

| Code | Meaning |
|---|---|
| 0 | PASS |
| 1 | INVALID (receipt written) or gate FAIL |
| 2 | Usage error or refusal, no receipt (existing receipt path, receipt inside the input root, bad namespace or block count, symlinked binary) |
| 3 | The create-exclusive receipt could not be written |

## Conservative resource estimates (not measured)

| Gate | CPU | RAM | Disk | Wall request |
|---|---|---|---|---|
| Build | 1 core | ≤ 1 GiB | < 50 MB | 15 min |
| Fixture gate (K = 2) | 1 core | ≤ 1 GiB | ≤ 0.5 GB temp | 1 h (expected minutes) |
| C++ replay | 1 core | ≤ 1 GiB | reads ~2.6 GB three times | 4 h (expected < 1 h) |
| Python verify | 1 core | ≤ 4 GiB | reads ~4.7 GB three times | 12 h (expected 2–6 h) |

- **Fixture gate:** each corruption case copies about 82 MB and is deleted afterwards.
- **C++ replay:** the ~2.6 GB is 1.75 GB of candidates, 0.27 GB of entries, 0.56 GB of context and 3.3 MB of chunk 0. It is read for the pre-hash, the replay and the post-hash. The replay itself makes about 10.5 M Threefry calls and 537 M coordinate-loss terms.
- **Python verify:** 85.2 M update rows and 33.6 M audit rows are parsed in pure Python.

The receipts record only verification status. They contain no classification codes or estimate values.

## Outcome handling

The tools reconstruct and check the saved records. They do not choose, alter or reinterpret outcomes. Every null, adverse, START-DEPENDENT and UNRESOLVED classification is checked exactly like support. Any discrepancy is INVALID. Under the frozen design, that closes the route; it never changes blocks, cells, updates, parameters, bounds or decision order. No production runner is in this package.

## Residual risks for source review

- The code has never been compiled or run. There may be compile errors under GCC 8.5, or logic slips that only the KAT, hand trace and fixture gate would expose.
- Several layout details the interface leaves open are resolved by interpretation (`docs/INPUT_CONTRACT.md` §7). A wrong reading makes honest records fail closed.
- The fixture gate's baseline comes from this auditor's own emitter. It proves corruption detection and internal consistency. Independent agreement with the producer is only shown by the real replay.
- The Python estimate rendering used to build fixtures shares code with the Python checker.
