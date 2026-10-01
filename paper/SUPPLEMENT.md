# Supplementary Information: Environmental recurrence selects a costly memory-use allele without a resolved one-bit population benefit

**Jack Chen**

Research preprint draft; not externally peer reviewed

## S1. Status and scope

This supplement documents the frozen design, analysis and audit of the prospective study PHASE2-MUTABLE-MEMORY-001 (specification revision 1). The accepted primary decision is **SUPPORTS RECURRENCE-ATTRIBUTABLE SELECTIVE ENRICHMENT**; the secondary classifications are **BOUNDED BELOW THE POSITIVE ONE-BIT SCALE** for `P_abs` and **UNRESOLVED** for `P_rec`. Every value below is read from the saved analyzer, diagnostics and audit records listed in Section S15. The complete scope is 41,600 independent paired blocks, eight cells per block, 332,800 paths, 85,196,800 path-updates and 10,905,190,400 objective queries.

## S2. State and full update order

Fixed state: 32 haploid individuals in slots 0-31; one unsigned 32-bit genotype per individual; one policy allele, `M` or `F`, per individual; and, for `M`, a private cache that is invalid or holds one unsigned 32-bit genotype. `F` individuals hold no valid cache. Lineage identifiers are audit metadata and are never read by scientific operators.

Initialization (update 0) for each block: 32 independent uniform 32-bit genotypes shared by all eight cells; all caches invalid; every label `F` in ALL_F cells and every label `M` in ALL_M cells.

At each update t = 1, ..., 256 the following steps occur in this order:

1. Generate the target innovation `I_t` and recurrence bit `R_t` and set the target `T_t` according to the cell's law (Section S3).
2. For each parent slot i = 0, ..., 31, construct four candidates: parent, policy probe, global scout and local child (Section S4). All masks, local-bit draws and donor keys are generated for every coordinate whether used or not.
3. Evaluate all 128 candidates by Hamming mismatch to `T_t`. Candidate order is (family, parent slot), with family order parent, policy probe, global scout, local child.
4. Draw 32 survivors sequentially without replacement with exact integer weights `2^(32-h)` (Section S5). Survivor slots are assigned in draw order.
5. Each survivor inherits its producing parent's policy; then each survivor slot independently flips its policy with probability 1/32 (Section S6).
6. Assign caches: post-mutation `F` survivors receive an invalid cache; post-mutation `M` survivors receive their producing parent's pre-update genotype.
7. Record memory-use count, total population mismatch, valid cache-probe use, cache-probe survivors, `F -> M` and `M -> F` counts and the query count, all after step 6.

True population accuracy at update t is `1 - (mean survivor mismatch)/32`. The late window is updates 193-256 inclusive.

## S3. Target laws

For every block and update, an indexed uniform innovation `I_t` and an indexed fair recurrence bit `R_t` are generated whether used or not.

- ZERO: `T_t = I_t` for all t.
- HALF: `T_1 = I_1`, `T_2 = I_2`; for t >= 3, `T_t = T_(t-2)` if `R_t = 1` and `T_t = I_t` otherwise.

ZERO and HALF use the same innovation at every non-recurrent update. HALF is a prior, outcome-informed choice that exactly matches the one-generation cache delay to a two-update target return, and therefore favors retrieval by construction. No other lag, copy probability or target law was tested.

## S4. Candidate construction and the fixed budget

For parent slot i with genotype `x_i` at update t:

1. **Parent:** `x_i`.
2. **Policy probe:** for `F`, `x_i XOR fresh_mask(t, i)` with an independent uniform 32-bit mask; for a valid `M` in ACTIVE, the cached genotype; for an invalid `M` in ACTIVE, the same fresh construction as `F`; for any label in SHAM, the same fresh construction as `F`.
3. **Global scout:** `x_i XOR scout_mask(t, i)` with an independent uniform 32-bit mask.
4. **Local child:** the lowest-mismatch member of candidates 1-3 for that parent, ties broken by an independent keyed 64-bit donor key (lower wins) and then by family index, XOR a local mask in which each of the 32 bits flips independently with probability 1/32.

Every parent therefore makes exactly four evaluated candidates and every update exactly 128 objective queries. Because XOR with a uniform word yields a uniform word, the fresh probe and the scout are each effectively uniform proposals. A valid ACTIVE-M carrier replaces exactly one of these two uniform proposals with its cache; this replacement is the entire cost of memory use. There is no score penalty, no tunable cost coefficient and no deduplication: a cache equal to the unchanged parent is evaluated as a separate candidate. The magnitude of the cost is conditional on the frozen fresh-probe distribution and is not a general cost scale. An invalid-cache `M` carrier, which can occur only at update 1, uses exactly the same fresh draw as `F` and SHAM, so ACTIVE and SHAM first diverge only at valid-M slots.

## S5. Survival by exact integer-weighted sequential sampling

Candidate j with raw mismatch `h_j` receives weight `w_j = 2^(32-h_j)`, an exact integer proportional to `2^(-h_j)`. Thirty-two candidates are selected sequentially without replacement. At survivor draw d = 0, ..., 31:

1. Retain unselected candidates in candidate-index order and sum their weights to `W_d`.
2. Obtain an unbiased integer `Z_d` uniform on `[0, W_d)` from the keyed 64-bit survival stream by multiply-high rejection: for retry r = 0, 1, ..., read a 64-bit word x, form the 128-bit product `m = x * W_d`, set `threshold_d = (2^64 - W_d) mod W_d`, reject if `low64(m) < threshold_d`, and otherwise set `Z_d = high64(m)`.
3. Select the first remaining candidate whose cumulative weight exceeds `Z_d`.

The maximum weight sum is `128 * 2^32 = 2^39`, so unsigned 64-bit totals and unsigned 128-bit products suffice. The procedure is an exact Plackett-Luce sample without replacement using no floating-point arithmetic. No label, cache, lineage identifier or penalized score enters it.

## S6. Policy mutation and cache inheritance and use

Each selected candidate initially inherits the policy of the parent that produced it. Then, independently for every survivor slot, the policy flips `M <-> F` with probability mu = 1/32. After mutation, a post-mutation `F` survivor's cache is invalid and a post-mutation `M` survivor's cache becomes the producing parent's pre-update genotype. Consequences:

- from update 2 onward every `M` carrier holds a valid one-step lineage cache;
- a new `F -> M` mutant first uses its record on the next update;
- an `M -> F -> M` history carries no older hidden record;
- the producing parent was itself a survivor of selection against the previous target, so a cache used at update t+1 holds a genotype that survived selection against `T_(t-1)`.

Under ACTIVE, the cache is read only to construct the policy probe of a valid `M` carrier. Under SHAM it is never read.

The allele therefore jointly determines whether the one-step record is retained and whether it is retrieved. Every individual has a cache slot, but only `M` carriers receive a valid record, so this design cannot separate selection on retention from selection on retrieval.

## S7. Coordinate-keyed random numbers

All random values come from canonical Random123 Philox4x32-10 with multipliers `D2511F53` and `CD9E8D57` and Weyl constants `9E3779B9` and `BB67AE85`. For each purpose, the two 32-bit key words are bytes 0-3 and 4-7 (little-endian) of SHA-256 of the UTF-8 text `PHASE2-MUTABLE-MEMORY-001|production-r1|<purpose>`. The four little-endian 32-bit counter words are exactly (block, update, entity, subindex). Block runs over 0-41599 and update is 0 at initialization and 1-256 otherwise. Output words 2-3 are unused.

| Purpose | Entity | Subindex | Extraction |
|---|---|---|---|
| `INITIAL_GENOTYPE` | parent slot | 0 | word 0 |
| `TARGET_INNOVATION` | 0 | 0 | word 0 |
| `TARGET_COPY` | 0 | 0 | word 0 bit 0 |
| `FRESH_MASK` | parent slot | 0 | word 0 |
| `SCOUT_MASK` | parent slot | 0 | word 0 |
| `LOCAL_BIT` | parent slot | bit 0-31 | flip iff `word0 & 31 == 0` |
| `DONOR_KEY` | `3*parent_slot + family_index` (family 0-2) | 0 | `word0 OR (word1 << 32)`; lower wins |
| `SURVIVAL_UNIFORM` | survivor draw 0-31 | rejection retry 0, 1, ... | `word0 OR (word1 << 32)` |
| `POLICY_MUTATION` | survivor slot 0-31 | 0 | flip iff `word0 & 31 == 0` |

The implementation aborts if any coordinate leaves its range or a survival retry exceeds the 32-bit subindex. A mechanical enumeration of the declared coordinate schemas and a purpose-separated collision audit passed before any population path. Because every draw is addressed by coordinates and generated regardless of branch, arm, law or label, all eight cells of a block share their random values wherever the coordinates coincide.

## S8. SHAM identities and the neutral derivation

In SHAM the policy probe is always the fresh construction, so the genotype process never reads a label or cache. With the same keyed draws:

- **N1.** SHAM genotype, mismatch, accuracy and survivor trajectories are bit-identical between ALL_F and ALL_M.
- **N2.** Policy labels in the two SHAM starts are exact complements after every update, because both starts share the genealogy and the same mutation draws and differ only in their initial labels. Their memory-use frequencies sum to 1 at every update, and their start-averaged late-window frequency is exactly 1/2 in every block.

Derivation of the neutral expectation: because the genealogy is independent of labels in SHAM, the label of a survivor at update t equals its founding ancestor's label passed through t independent flips, each with probability mu. With r = 1 - 2mu = 15/16, the probability of an odd number of flips is (1 - r^t)/2. Hence `P(M at t | ALL_F) = [1 - r^t]/2` and `P(M at t | ALL_M) = [1 + r^t]/2`, and the expected ALL_M minus ALL_F difference is r^t. At update 256 this is r^256 = 6.678005283720385e-08. Averaged over updates 193-256 it is `(1/64) sum_(t=193)^(256) r^t = r^193(1 - r^64)/(64(1 - r))` = 9.57983820600146e-07. These expectations justify the late window for SHAM; they do not establish convergence of ACTIVE paths.

Observed descriptive consistency: SHAM memory-use frequencies were 0.500083 and 0.499917 under ZERO and 0.499089 and 0.500911 under HALF, and the exact saved fractions in each pair sum to 1. SHAM accuracy was 0.607310 under ZERO and 0.619018 under HALF from both starts, with identical exact fractions. The record verifier confirmed both identities block by block in all 41,600 blocks.

What SHAM does not control: SHAM is an exact neutral benchmark for the label process (N1, N2; expected start-averaged frequency 1/2) and a paired control for query budget and keyed random draws, but ACTIVE genotypes and genealogies diverge from SHAM at the first valid-M retrieval, so SHAM does not hold realised drift or genotype state fixed for ACTIVE paths and ACTIVE-SHAM contrasts are arm-level comparisons within the model, not path-level counterfactuals. It also does not separate the lag-two information in the cache from the loss of one fresh proposal (the ZERO law provides that comparison through `C_rec`); it does not represent alternative caches, costs or retrieval rules; and it says nothing about ACTIVE convergence or long-run behavior.

## S9. Estimands and interval construction

For block b, let `L_b(arm, law, start)` be the mean memory-use frequency over the 64 late-window updates and `Acc_b(arm, law, start)` the corresponding mean accuracy. Block variables:

- `A_HALF,b = [L_b(ACTIVE, HALF, ALL_F) + L_b(ACTIVE, HALF, ALL_M)]/2`, and `A_ZERO,b` analogously;
- `C_abs,b = A_HALF,b - 1/2` (range length 1);
- `C_rec,b = A_HALF,b - A_ZERO,b` (range length 2);
- `D_HALF,b = L_b(ACTIVE, HALF, ALL_M) - L_b(ACTIVE, HALF, ALL_F)` (range length 2);
- `D_ZERO,b = L_b(ACTIVE, ZERO, ALL_M) - L_b(ACTIVE, ZERO, ALL_F)` (range length 2);
- with `P_b(arm, law)` the start-averaged accuracy, `P_abs,b = P_b(ACTIVE, HALF) - P_b(SHAM, HALF)` (range length 2);
- `P_rec,b = [P_b(ACTIVE, HALF) - P_b(SHAM, HALF)] - [P_b(ACTIVE, ZERO) - P_b(SHAM, ZERO)]` (range length 4).

Each estimate is the mean of its block variable over n = 41,600 independent blocks. Because each estimand is linear in cell means, each saved estimate equals the same linear combination of the saved descriptive cell means; the statistics-checking script verifies these identities exactly in rational arithmetic.

Hoeffding's inequality for the mean of n independent variables each confined to an interval of length R gives `Pr(|mean - expectation| >= h) <= 2 exp(-2 n h^2 / R^2)`. Setting the right side to alpha_each gives `h = R sqrt[ln(2/alpha_each)/(2n)]`. The primary family of four estimands uses alpha_each = 0.05/4 = 0.0125, giving 95% familywise coverage by Bonferroni's inequality; the secondary family of two uses alpha_each = 0.05/2 = 0.025. The resulting half-widths are 0.00781023 (`C_abs`), 0.01562046 (`C_rec`, `D_HALF`, `D_ZERO`), 0.01451463 (`P_abs`) and 0.02902925 (`P_rec`). The primary range-2 half-width lies below Delta/2 = 0.015625, so the gate can be passed only if a start contrast is estimated near zero. Each interval is `[estimate - h, estimate + h]`, computed from the exact rational estimate and a high-precision half-width. No empirical variance, normal approximation, within-path independence, pilot or favorable pairing is used.

## S10. Frozen decision rules and their application

Primary rules, applied in order with Delta = 1/32 = 0.03125:

1. Failure of a deterministic identity, seed or counter audit, saved-output reconstruction or 128-query count: **INVALID**. Not triggered.
2. Interval for `D_HALF` or `D_ZERO` not wholly inside (-Delta, +Delta): **START-DEPENDENT; SCIENTIFIC QUESTION UNRESOLVED**. Not triggered: `D_HALF` [-0.014898, 0.016343] and `D_ZERO` [-0.015717, 0.015524] both lie inside.
3. Lower bounds of both `C_abs` and `C_rec` above Delta: **SUPPORTS RECURRENCE-ATTRIBUTABLE SELECTIVE ENRICHMENT**. Triggered: `C_abs` lower bound 0.302482 and `C_rec` lower bound 0.426015.
4. Lower bound of `C_abs` above Delta and upper bound of `C_rec` at or below Delta: SELECTIVE ENRICHMENT NOT ATTRIBUTABLE TO RECURRENCE AT THE FIXED SCALE. Not reached.
5. Upper bound of `C_abs` at or below Delta: BOUNDED NEGATIVE AT THE ONE-INDIVIDUAL SCALE, with a statement of whether the upper bound is below -Delta. Not reached; the adverse-selection field is therefore null in the decision record.
6. Every other pattern: UNRESOLVED. Not reached.

The decision record lists decision rule 3 as applied. Secondary rules, applied in order to each of `P_abs` and `P_rec` with Delta_P = 1/32: lower bound above Delta_P, meaningful positive performance; upper bound below -Delta_P, meaningful adverse performance; upper bound at or below Delta_P, **BOUNDED BELOW THE POSITIVE ONE-BIT SCALE**; otherwise **UNRESOLVED**. `P_abs` has upper bound 0.023892, at or below 0.03125, and is BOUNDED BELOW THE POSITIVE ONE-BIT SCALE. `P_rec` has lower bound -0.017669 and upper bound 0.040390 and is UNRESOLVED. Secondary classifications cannot alter the primary decision. No replicate, horizon, target-law, mutation-rate or cost change follows any branch.

## S11. All 22 saved estimate records

Inferential records carry frozen bounds; descriptive records do not. Exact values are the saved rational estimates; decimal values are rounded to six places.

| # | Record | Exact | Estimate | Interval |
|---|---|---|---|---|
| 1 | `C_abs` | 5287181/17039360 | 0.310292 | [0.302482, 0.318102] |
| 2 | `C_rec` | 75251823/170393600 | 0.441635 | [0.426015, 0.457256] |
| 3 | `D_HALF` | 30793/42598400 | 0.000723 | [-0.014898, 0.016343] |
| 4 | `D_ZERO` | -8213/85196800 | -0.000096 | [-0.015717, 0.015524] |
| 5 | `P_abs` | 51129277/5452595200 | 0.009377 | [-0.005138, 0.023892] |
| 6 | `P_rec` | 61943359/5452595200 | 0.011360 | [-0.017669, 0.040390] |
| 7 | Frequency, ACTIVE, ZERO, ALL_F | 314125/851968 | 0.368705 | descriptive |
| 8 | Frequency, ACTIVE, ZERO, ALL_M | 31404287/85196800 | 0.368609 | descriptive |
| 9 | Frequency, ACTIVE, HALF, ALL_F | 8625439/10649600 | 0.809931 | descriptive |
| 10 | Frequency, ACTIVE, HALF, ALL_M | 34532549/42598400 | 0.810654 | descriptive |
| 11 | Frequency, SHAM, ZERO, ALL_F | 42605507/85196800 | 0.500083 | descriptive |
| 12 | Frequency, SHAM, ZERO, ALL_M | 42591293/85196800 | 0.499917 | descriptive |
| 13 | Frequency, SHAM, HALF, ALL_F | 42520779/85196800 | 0.499089 | descriptive |
| 14 | Frequency, SHAM, HALF, ALL_M | 42676021/85196800 | 0.500911 | descriptive |
| 15 | Accuracy, ACTIVE, ZERO, ALL_F | 1650292613/2726297600 | 0.605324 | descriptive |
| 16 | Accuracy, ACTIVE, ZERO, ALL_M | 1650309069/2726297600 | 0.605330 | descriptive |
| 17 | Accuracy, ACTIVE, HALF, ALL_F | 1713218809/2726297600 | 0.628405 | descriptive |
| 18 | Accuracy, ACTIVE, HALF, ALL_M | 428290869/681574400 | 0.628385 | descriptive |
| 19 | Accuracy, SHAM, ZERO, ALL_F | 827853941/1363148800 | 0.607310 | descriptive |
| 20 | Accuracy, SHAM, ZERO, ALL_M | 827853941/1363148800 | 0.607310 | descriptive |
| 21 | Accuracy, SHAM, HALF, ALL_F | 210953313/340787200 | 0.619018 | descriptive |
| 22 | Accuracy, SHAM, HALF, ALL_M | 210953313/340787200 | 0.619018 | descriptive |

Derived start averages used in the main text: ACTIVE frequency 0.810292 under HALF and 0.368657 under ZERO; ACTIVE accuracy 0.628395 under HALF and 0.605327 under ZERO. The ZERO ACTIVE minus SHAM accuracy contrast, -0.001983, is descriptive. The analyzer's descriptive relation labels are DISAGREEMENT for `P_abs` and UNCERTAINTY for `P_rec`.

## S12. Descriptive diagnostics

Totals over all blocks and updates, from the descriptive diagnostics record. These counts support interpretation only and carry no inferential bounds. Cache-probe use counts valid ACTIVE-M retrievals; cache-probe survivors counts retrieved candidates that were selected.

| Cell | `F -> M` | `M -> F` | Cache-probe uses | Cache-probe survivors |
|---|---|---|---|---|
| ACTIVE, ZERO, ALL_F | 7,009,928 | 3,640,445 | 119,306,059 | 23,380,466 |
| ACTIVE, ZERO, ALL_M | 6,382,195 | 4,268,178 | 138,178,075 | 27,148,189 |
| ACTIVE, HALF, ALL_F | 2,226,924 | 8,423,449 | 262,246,633 | 76,047,092 |
| ACTIVE, HALF, ALL_M | 1,743,217 | 8,907,156 | 276,722,365 | 79,823,372 |
| SHAM, ZERO, ALL_F | 5,656,667 | 4,993,706 | 0 | 0 |
| SHAM, ZERO, ALL_M | 4,993,706 | 5,656,667 | 0 | 0 |
| SHAM, HALF, ALL_F | 5,659,540 | 4,990,833 | 0 | 0 |
| SHAM, HALF, ALL_M | 4,990,833 | 5,659,540 | 0 | 0 |

The SHAM mutation counts are exchanged between starts, as label complementarity (N2) implies. Under HALF, ACTIVE cache probes were used more often and survived more often than under ZERO. The same diagnostics record (SHA-256 in Section S15) also archives fixation, extinction and late-window occupancy counters; they are not interpreted here. The diagnostics record lists 10,905,190,400 total objective queries.

## S13. Seeds, fixtures, outputs and audit structure

**Cohort.** One production namespace (`production-r1`) and 41,600 blocks; no prior genotype, preparation, target, trajectory, random tape or outcome was reused. Production ran as one allocation (job 53530597), followed by the frozen analyzer (job 53530688) and the separately implemented internal audit job (53530689). The prospective timing of the specification, design review and production submission is given in Section S15.

**Deterministic fixtures (all passed before production; none contains a sampled scientific outcome).** (1) One- and two-update hand trace covering all four candidate families, donor choice, exact sequential weighted survival, policy mutation and cache transitions; (2) memory-better case (lag-two target, cache mismatch 0, fresh mismatch 16); (3) memory-worse case (fresh target, cache mismatch 28, fresh mismatch 4); (4) duplicate case with the cache equal to the unchanged candidate and 128 queries; (5) invalid-cache case in which ACTIVE-M uses the same fresh draw as `F` and SHAM; (6) full 256-update SHAM paths from ALL_F, ALL_M and a mixed assignment verifying N1 and N2; (7) target-law pairing at innovations, recurrent updates and the t = 1/2 boundary; (8) an exact weighted-selection fixture with known weights and draws, mismatches 0 and 32, a forced rejection and retry, duplicate genotypes and exact survivor order; (9) random-coordinate invariance under changes to labels, arm, law and start; (10) regeneration of a non-audit path from its block identifier and source and configuration hashes to bit-identical summaries.

**Saved outputs.** Frozen specification, source, build environment, configuration and SHA-256 manifests; Philox implementation and version, key schema, namespace and collision receipt; per-update binary records for every path; per-path late summaries and final-state hashes; per-block primary and performance variables and N1/N2 flags; full candidate rows for audit blocks 0-63 (512 paths, 16,777,216 rows) including genotype, family, producing parent, labels, cache, mismatch, integer weight, selection draw, retry and integer, selected rank, target and recurrence indicator; all 22 estimate records with bounds and classifications; and job request, accounting, logs, exit status and delivery manifests.

**Replay audit.** The C++ tool MMEM-AUDIT-REPLAY-1.0.1 (executable SHA-256 `8cf650a89e690af7e65629bee155ea08b9b985c4d504e70faf75875e1ebddc6e`), run in production mode with production-namespace keys only, replayed blocks 0-63. Its known-answer tests (three FIPS 180 SHA-256 vectors, three Random123 Philox4x32-10 vectors, a 64-by-64-bit product, the multiply-high rejection threshold, popcount and coordinate-schema injectivity) all passed. It checked 64 blocks, 512 path records, 131,072 update records and 16,777,216 audit rows, each equal to the expected count, and reported status PASS, zero mismatches and no first discrepancy. The receipt states that it reports no estimate and is not a scientific result.

**Record audit.** The Python tool MMEM-AUDIT-RECORDS-1.2.0 (source SHA-256 `8e551ab55cbe5dc08b94b3fa6152da5f282993d81c00b383e7c8bf43f9d66d7b`) generated no random draws. It checked 41,600 block records (all with both SHAM identities), 332,800 path records, 85,196,800 update records, 512 reconstructed audit-path hashes, 16,777,216 audit rows, 97 manifest files, 22 estimates and one decision file, each equal to the expected count, and reported status PASS and zero mismatches. Its self-checks, including the six frozen half-widths, all passed. The records it compared include the estimate file with SHA-256 `83828a49936bb8364813e2634f11cb28c4de2212b4b162d96e616ddbc7b4f8c0` and the decision file with SHA-256 `5b3aea3c18c4530b2be9f29d5c64fdd8173b6b323823bf9cc3361bd4e7010fc0`.

Neither tool imports producer code. Both are internal reproducibility checks performed within the project with AI assistance; neither is external peer review or an independent scientific replication. Producer and audit tools were both written with AI assistance from the same specification; the audits detect implementation divergence, not a shared misreading of the specification.

## S14. Exact claim limits

The result concerns selection on use of a supplied private cache under one prospectively aligned recurrent law and one fixed opportunity cost; meaningful population benefit was not demonstrated. Specifically:

- **Supported:** under the HALF law and the one-proposal opportunity cost, the late-window start-averaged frequency of the mutation-generated memory-use allele exceeded both its exact neutral expectation and its ZERO-law frequency by more than one expected individual (95% familywise), and both late-window start contrasts passed the prespecified finite-horizon ±1/32 start-state gate, that is, they were bounded within one expected individual (95% familywise) over the 256-update horizon.
- **Bounded:** the recurrent-law ACTIVE minus SHAM accuracy contrast is below one correct bit per survivor (95% familywise within the secondary family); its sign is not resolved.
- **Unresolved:** the recurrence interaction in accuracy.
- **Descriptive only:** all 16 cell means, the ZERO allele deficit, the ZERO adverse accuracy contrast, and all diagnostics.
- **Not supported:** origin of memory architecture or capacity; separate selection on cache retention versus retrieval; general start-independence; rare-mutant invasion; fixation probability; stationarity; equilibrium; evolutionary stability; arbitrary recurrence or any other lag, copy probability or target law; any other or general cost scale; population benefit; biological, ecological or neural extrapolation; priority or global novelty; and replication of earlier internal private-memory studies.

## S15. Provenance of quoted values

Every number in the manuscript and this supplement that is registered in `provenance/QUOTED_STATISTICS.json` is checked by `scripts/check_statistics.py` against the saved records below, which also recomputes the six interval endpoints, the 16 descriptive identities, the decision labels, the scope counts and the audit mismatch counts. The SHA-256 values are those authenticated in the acceptance record of 2026-10-01.

| Source | SHA-256 |
|---|---|
| Decision record | `5b3aea3c18c4530b2be9f29d5c64fdd8173b6b323823bf9cc3361bd4e7010fc0` |
| 22 estimate records | `83828a49936bb8364813e2634f11cb28c4de2212b4b162d96e616ddbc7b4f8c0` |
| Descriptive diagnostics | `51554620a5e302416edf8de8a073c6962abbc2c8967da80b91b83e4c3d5b685c` |
| Replay receipt | `9b40322d0d09b90f0a6862533c1efdc7aff21bd699a882b9b9555c5f7c4b4857` |
| Record-verification receipt | `afd96dc0a5b190401c035fc66c8af8fd4d4603e7185d05bff73fb05e698cff5a` |
| Production inventory audit | `622fa9e01a40c44ba545926e54b06e102d20b7fcf9e9c8e645ddaf9f7212e09c` |
| Acceptance record | `2eb3aaf5d754f4570a75522aa9e1f38358715c06c674d5a267732a1357926e6f` |

The first six values are authenticated in the acceptance record; the acceptance-record SHA-256 itself is recomputed and enforced by the statistics-checking script.

Prospective timing records (all times UTC, 2026-10-01). These establish design provenance only and add or modify no scientific outcome.

| Record | Time | SHA-256 |
|---|---|---|
| Frozen specification, `design/PHASE2-MUTABLE-MEMORY-001-REV1.md` (revision 1) | not separately timestamped; subject of the 01:55:25 review | `c17a3a1e9ac9cf2260f6743eaf08e4c2808080a767af16d2f9e9d2b9ad3e821d` |
| Terminal design review, non-implementing AI reviewer CLAUDE-003, verdict GO | 01:55:25 | `787cf32a20312f2682c761243f3dec04c5eb8036d14d6efc444e9830db37960a` |
| Terminal design-review receipt | 01:55:25 | `6277757f9ab3dfa611515c161776c17b561738eba6fa478ae0f8634e13c03176` |
| Production execution authority | 12:53:04 | `ee3cb68bed5d212e6ea10ece542e2e90c32cb9cea07d85fceda10fc56b375b1d` |
| Sole production submission, job 53530597 (no automatic retry, no parameter changes) | 12:55:19 | not supplied |
| Acceptance record | 13:33:13 | `2eb3aaf5d754f4570a75522aa9e1f38358715c06c674d5a267732a1357926e6f` |
