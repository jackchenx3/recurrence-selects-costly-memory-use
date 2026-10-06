# Supplementary Information: Environmental recurrence selects a costly memory-use policy across two operator bundles without a resolved one-bit population benefit

**Jack Chen**

Research preprint draft, version 1.2.0; not externally peer reviewed

## S1. Status and scope

This supplement documents three prospective studies in two operator bundles. Part A (Sections S1-S15) documents Study 001; its content, values and provenance are unchanged from version 1.0 apart from this introduction and cross-references to Parts B and C. Part B (Sections S16-S26) documents Study 002, a post-v1.0 prospective extension added in version 1.1.0, and is unchanged apart from cross-references. Part C (Sections S27-S39) documents Study 003, a post-v1.1 prospective extension added in version 1.2.0 that poses the Study 001 question again in an independently implemented alternative operator bundle. The 22 Study 001, 19 Study 002 and 22 Study 003 records are three separately accepted estimate sets, 63 saved estimate records in total; none is pooled with another.

## Part A. Study 001 (PHASE2-MUTABLE-MEMORY-001)

Part A documents the frozen design, analysis and audit of the prospective study PHASE2-MUTABLE-MEMORY-001 (specification revision 1). The accepted primary decision is **SUPPORTS RECURRENCE-ATTRIBUTABLE SELECTIVE ENRICHMENT**; the secondary classifications are **BOUNDED BELOW THE POSITIVE ONE-BIT SCALE** for `P_abs` and **UNRESOLVED** for `P_rec`. Every value below is read from the saved analyzer, diagnostics and audit records listed in Section S15. The complete scope is 41,600 independent paired blocks, eight cells per block, 332,800 paths, 85,196,800 path-updates and 10,905,190,400 objective queries.

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

What SHAM does not control: SHAM is an exact neutral benchmark for the label process (N1, N2; expected start-averaged frequency 1/2) and a paired control for query budget and keyed random draws, but ACTIVE genotypes and genealogies diverge from SHAM at the first valid-M retrieval, so SHAM does not hold realised drift or genotype state fixed for ACTIVE paths and ACTIVE-SHAM contrasts are arm-level comparisons within the model, not path-level counterfactuals. It also does not separate the lag-two information in the cache from the loss of one fresh proposal (the ZERO law provides that comparison through `C_rec`, and Study 002 provides a within-law comparison; Section S16); it does not represent alternative caches, costs or retrieval rules; and it says nothing about ACTIVE convergence or long-run behavior.

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

## S15. Provenance of quoted values (Study 001)

Every number in the manuscript and this supplement that is registered in `provenance/QUOTED_STATISTICS.json` is checked by `scripts/check_statistics.py` against the saved records below (Study 001), in Section S26 (Study 002) and in Section S39 (Study 003), which also recomputes the six interval endpoints, the 16 descriptive identities, the decision labels, the scope counts and the audit mismatch counts. The SHA-256 values are those authenticated in the acceptance record of 2026-10-01.

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

## Part B. Study 002 (PHASE2-PERFORMANCE-CONVERSION-002)

## S16. Status, question and relation to Study 001

Study 002 is a post-v1.0 prospective extension specified in PHASE2-PERFORMANCE-CONVERSION-002 revision 1 (SHA-256 `58ee414d753fab042e6cccee1a4e6b95dd7f167633b4f33c1491687d9afcb895`). Its frozen question was: under the single aligned HALF recurrence law, does directional information in the supplied one-record cache produce a late-window population-accuracy gain exceeding one correct bit when compared with a control that preserves the one-proposal displacement and the cache-parent distance but scrambles cache direction at recurrent updates? It is a within-law operator intervention. It does not test the origin of memory capacity, a per-use value of information, arbitrary recurrence, equilibrium, invasion, biological memory or generality across model classes.

The accepted primary decision is **BOUNDED BELOW THE POSITIVE ONE-BIT SCALE** (rule 5; interval wholly inside (-1/32, +1/32)). The secondary classifications are **MEANINGFUL POSITIVE AT THE ONE-BIT SCALE** for `E_INFO` and `E_NONINFO`, and **BOUNDED BELOW THE POSITIVE ONE-BIT SCALE** for `B_INFO` and `B_NONINFO`. The complete scope is 41,600 new independent paired blocks, six cells per block, 249,600 paths, 63,897,600 path-updates, 8,178,892,800 objective queries and 19 saved estimate records.

Relation to Study 001. All operators other than the policy probe are those of Sections S2-S6. The Study 002 producer extends the authenticated Study 001 producer; the specification required unchanged operators to remain behaviorally and fixture-identical. The cohort, namespace and every random tape are new, and no Study 001 genotype, target, trajectory, block or outcome was reused. The INFO probe rule equals the Study 001 ACTIVE rule. The 22 Study 001 records and the 19 Study 002 records are separate accepted estimate sets, 41 saved estimate records in total, with separate familywise controls; they are not pooled and are not independent replications of each other.

## S17. Arms, shared state and full event order

Fixed state is as in Section S2. Initialization (update 0) for each block: 32 independent uniform 32-bit genotypes shared by all six cells; all caches invalid; every label `F` in ALL_F cells and every label `M` in ALL_M cells.

At each update t = 1, ..., 256:

1. Generate the innovation `I_t` and recurrence bit `R_t`, and set the HALF target: `T_1 = I_1`, `T_2 = I_2`, and for t >= 3, `T_t = T_(t-2)` if `R_t = 1` and `T_t = I_t` otherwise. Generate the shared permutation `pi_(b,t)` (Section S18). All are generated whether or not they are used.
2. For each parent slot i = 0, ..., 31 with genotype `x` and cache `c`, construct four candidates: the parent `x`; the policy probe (table below); the global scout `x XOR scout_mask`; and the local child, the lowest-mismatch member of the first three candidates (keyed donor tie-break) with each bit flipped independently with probability 1/32.
3. Evaluate all 128 candidates by Hamming mismatch to `T_t`, in (family, parent slot) order. Duplicates are evaluated separately.
4. Draw 32 survivors sequentially without replacement with exact integer weights `2^(32-h)` (Section S5). No label, cache, lineage or arm enters the weights.
5. Each survivor inherits its producing parent's policy; each survivor slot then flips `M <-> F` independently with probability 1/32.
6. Post-mutation `F` survivors receive an invalid cache; post-mutation `M` survivors receive the producing parent's pre-update genotype.
7. Record memory-use count, total mismatch, valid probe use, true-cache use, decoy use, probe survivors, policy mutations and the query count, all after step 6.

Policy probe by arm:

| Label and cache state at update t | INFO | NONINFO | SHAM |
|---|---|---|---|
| `F`, or `M` with invalid cache | `x XOR fresh_mask` | `x XOR fresh_mask` | `x XOR fresh_mask` |
| valid `M`, t < 3 or `R_t = 0` (innovation update) | `c` | `c` | `x XOR fresh_mask` |
| valid `M`, t >= 3 and `R_t = 1` (recurrent update) | `c` | `x XOR pi_(b,t)(c XOR x)` | `x XOR fresh_mask` |

The experimental operator reads `R_t`; the evolving policy cannot. At innovation updates INFO and NONINFO apply the identical rule, so they remain bit-identical until the first recurrent valid-`M` coordinate, where only the policy-probe genotype may differ before downstream selection. Both arms displace one fresh proposal at every valid-`M` use.

## S18. The shared decoy permutation

**Construction.** For each block b and update t, `pi_(b,t)` is built from the identity array `[0, ..., 31]` by a Fisher-Yates shuffle: for i = 31, ..., 1, draw j exactly uniformly from `[0, i]` and swap positions i and j. A source bit s maps to destination `pi[s]`. Each j is drawn by multiply-high (Lemire) rejection from the purpose-separated stream `DECOY_PERMUTATION`, whose Philox counter is (block, update, entity = i, subindex = retry) and whose unsigned 64-bit draw is `word0 OR (word1 << 32)`; output words 2-3 are unused. Every step is generated for every block and update regardless of branch or arm, and one permutation is shared by all six cells and all 32 parents of that block and update.

**Random coordinates.** Study 002 uses canonical Random123 Philox4x32-10 with the key and counter construction of Section S7 under the UTF-8 namespace `PHASE2-PERFORMANCE-CONVERSION-002|production-r1|<purpose>`. The purposes are those of Section S7 (`INITIAL_GENOTYPE`, `TARGET_INNOVATION`, `TARGET_COPY`, `FRESH_MASK`, `SCOUT_MASK`, `LOCAL_BIT`, `DONOR_KEY`, `SURVIVAL_UNIFORM`, `POLICY_MUTATION`) plus `DECOY_PERMUTATION`. All coordinates are addressable, the implementation aborts on range overflow, and the specification required a purpose-separated collision audit and a constructed exact-permutation frequency fixture before production.

**Geometry of the control.**

- *Distance matched.* Permuting bit positions preserves Hamming weight, so `popcount(pi(c XOR x)) = popcount(c XOR x)` and every NONINFO decoy lies exactly as far from its parent as the cache does, pathwise.
- *One common relabeling.* Because one permutation is shared by all parents of a block and update, overlaps among the displacement masks of different parents are preserved under one common coordinate relabeling.
- *Not preserved.* Other cross-genotype relations are not preserved; in particular, siblings that share one cached genotype do not share one decoy endpoint.
- *Uniform subset law.* For a fixed parent and displacement weight w, the decoy displacement is a uniformly random w-subset of the 32 positions.
- *Retained direction.* Displacements of weight 0 or 32 are unchanged by any permutation and therefore retain direction. Their counts are reported in Section S23.

## S19. SHAM identities in Study 002

SHAM never reads labels or caches in genotype construction, so identities N1 and N2 of Section S8 hold: SHAM genotype, mismatch, accuracy and survivor trajectories are bit-identical between starts, and SHAM labels are exact complements after every update, so the start-averaged memory-use frequency is exactly 1/2 in every block. The neutral derivation of Section S8 applies unchanged. Observed: SHAM memory-use frequencies were 0.499678 (21285489/42598400) and 0.500322 (21312911/42598400), which sum exactly to 1, and SHAM accuracy was 0.619009 (1687602263/2726297600) from both starts. The record verifier confirmed both identities in all 41,600 blocks. SHAM is both the neutral allele benchmark for `E_NONINFO` and the no-memory performance reference for `B_INFO` and `B_NONINFO`; as in Study 001, its genotypes and genealogies diverge from INFO and NONINFO at the first valid-`M` retrieval, so contrasts with SHAM are arm-level comparisons, not path-level counterfactuals.

## S20. Estimands and interval families

For block b, let `L_b(arm, start)` be the mean memory-use frequency over updates 193-256 and `P_b(arm, start)` the corresponding mean population accuracy. With equal-weight start averages `A_b(arm)` and `P_b(arm)`:

- `Delta_P,b = P_b(INFO) - P_b(NONINFO)` (range length 2);
- `D_INFO,b = L_b(INFO, ALL_M) - L_b(INFO, ALL_F)` and `D_NONINFO,b` analogously (range length 2);
- `E_INFO,b = A_b(INFO) - A_b(NONINFO)` (range length 2);
- `E_NONINFO,b = A_b(NONINFO) - 1/2` (range length 1);
- `B_INFO,b = P_b(INFO) - P_b(SHAM)` and `B_NONINFO,b = P_b(NONINFO) - P_b(SHAM)` (range length 2).

Each estimate is the mean of its block variable over n = 41,600 independent blocks and equals the same linear combination of the saved descriptive cell means; the statistics-checking script verifies these identities exactly in rational arithmetic. `Delta_P` is the total population-accuracy effect of directional cache information beyond the parent and matched displacement length, mediated through evolved policy use: INFO and NONINFO share policy rules but not realized policy states, so `Delta_P` includes any divergence in policy frequency, cache use, survivor composition and descendants. It is not a per-use value-of-information estimate and does not separate retention from retrieval.

Intervals use the Hoeffding-Bonferroni construction of Section S9, `h = R sqrt[ln(2/alpha_each)/(2n)]`, with exact rational estimates and high-precision half-widths. No empirical variance, normal approximation or pilot is used.

| Family | Estimands | alpha_each | Range length | Half-width |
|---|---|---|---|---|
| Primary | `Delta_P`, `D_INFO`, `D_NONINFO` | 0.05/3 | 2 | 0.01517128 |
| Secondary allele | `E_INFO` | 0.025 | 2 | 0.01451463 |
| Secondary allele | `E_NONINFO` | 0.025 | 1 | 0.00725731 |
| Secondary performance | `B_INFO`, `B_NONINFO` | 0.025 | 2 | 0.01451463 |

The primary half-width lies below Delta/2 = 0.015625, so the gate can be passed only if a start contrast is estimated near zero. Each family has 95% familywise coverage; coverage is not controlled jointly across the three Study 002 families or with the Study 001 families.

## S21. Frozen decision order and its application

Primary rules, applied in order with delta = 1/32 = 0.03125:

1. Any deterministic identity, coordinate, query-count, saved-output or independent-audit failure: **INVALID**. Not triggered.
2. Either gate interval not wholly within (-delta, +delta): **START-DEPENDENT; PRIMARY UNRESOLVED**. Not triggered: `D_INFO` [-0.015042, 0.015301] and `D_NONINFO` [-0.016439, 0.013903] both lie inside.
3. Lower bound of `Delta_P` above delta: meaningful positive directional-information effect relative to NONINFO. Not triggered: the lower bound is -0.006521.
4. Upper bound of `Delta_P` below -delta: meaningful adverse directional-information effect relative to NONINFO. Not triggered.
5. Upper bound of `Delta_P` at or below delta: **BOUNDED BELOW THE POSITIVE ONE-BIT SCALE**, with a separate statement of whether the interval is wholly inside (-delta, +delta). Triggered: the upper bound is 0.023821, and the interval [-0.006521, 0.023821] lies wholly inside (-delta, +delta).
6. Otherwise: UNRESOLVED. Not reached.

The decision record lists decision rule 5 as applied and records that the bounded interval lies wholly inside (-delta, +delta). Each secondary estimand was classified, in order, as meaningful positive if its lower bound exceeds delta, meaningful adverse if its upper bound is below -delta, bounded below the positive one-bit scale if its upper bound is at or below delta, and unresolved otherwise. `E_INFO` (lower bound 0.169688) and `E_NONINFO` (lower bound 0.118825) are **MEANINGFUL POSITIVE AT THE ONE-BIT SCALE**; `B_INFO` (upper bound 0.023896) and `B_NONINFO` (upper bound 0.015246) are **BOUNDED BELOW THE POSITIVE ONE-BIT SCALE**. Secondary classifications cannot alter the primary decision, and a positive primary would not have established benefit over no-memory use unless the lower bound of `B_INFO` also exceeded delta.

Prespecified interpretive branches:

| Pattern | Frozen interpretation | Applied |
|---|---|---|
| Positive primary and meaningful positive `B_INFO` | One-bit benefit relative to both the matched decoy and SHAM in this fixed model | No |
| Positive primary without meaningful `B_INFO` | Directional information offsets some displacement cost; no demonstrated one-bit absolute benefit | No |
| Bounded or adverse primary with meaningful positive `E_INFO` | Directional information selects for policy use without converting into one-bit population performance, sharpening the selection-performance separation. | Yes |
| Bounded or adverse primary without meaningful `E_INFO` | The information intervention does not explain the accepted enrichment | No |

All branches were to be retained as reported. No parameter, lag, probability, cost, horizon or cohort follow-up follows any branch.

## S22. All 19 saved estimate records

Inferential records carry frozen bounds; descriptive records do not. Exact values are the saved rational estimates; decimal values are rounded to six places.

| # | Record | Family | Exact | Estimate | Interval |
|---|---|---|---|---|---|
| 1 | `Delta_P` | Primary | 5895597/681574400 | 0.008650 | [-0.006521, 0.023821] |
| 2 | `D_INFO` | Primary | 2757/21299200 | 0.000129 | [-0.015042, 0.015301] |
| 3 | `D_NONINFO` | Primary | -54019/42598400 | -0.001268 | [-0.016439, 0.013903] |
| 4 | `E_INFO` | Secondary allele | 1569351/8519680 | 0.184203 | [0.169688, 0.198718] |
| 5 | `E_NONINFO` | Secondary allele | 10741847/85196800 | 0.126083 | [0.118825, 0.133340] |
| 6 | `B_INFO` | Secondary performance | 25576047/2726297600 | 0.009381 | [-0.005133, 0.023896] |
| 7 | `B_NONINFO` | Secondary performance | 1993659/2726297600 | 0.000731 | [-0.013783, 0.015246] |
| 8 | Frequency, INFO, ALL_F | Descriptive | 69028243/85196800 | 0.810221 | descriptive |
| 9 | Frequency, INFO, ALL_M | Descriptive | 69039271/85196800 | 0.810351 | descriptive |
| 10 | Frequency, NONINFO, ALL_F | Descriptive | 26697133/42598400 | 0.626717 | descriptive |
| 11 | Frequency, NONINFO, ALL_M | Descriptive | 13321557/21299200 | 0.625449 | descriptive |
| 12 | Frequency, SHAM, ALL_F | Descriptive | 21285489/42598400 | 0.499678 | descriptive |
| 13 | Frequency, SHAM, ALL_M | Descriptive | 21312911/42598400 | 0.500322 | descriptive |
| 14 | Accuracy, INFO, ALL_F | Descriptive | 856541369/1363148800 | 0.628355 | descriptive |
| 15 | Accuracy, INFO, ALL_M | Descriptive | 856636941/1363148800 | 0.628425 | descriptive |
| 16 | Accuracy, NONINFO, ALL_F | Descriptive | 1689605501/2726297600 | 0.619744 | descriptive |
| 17 | Accuracy, NONINFO, ALL_M | Descriptive | 1689586343/2726297600 | 0.619737 | descriptive |
| 18 | Accuracy, SHAM, ALL_F | Descriptive | 1687602263/2726297600 | 0.619009 | descriptive |
| 19 | Accuracy, SHAM, ALL_M | Descriptive | 1687602263/2726297600 | 0.619009 | descriptive |

Saved-precision values of the primary estimand and the secondary contrasts:

| Record | Estimate | Lower | Upper |
|---|---|---|---|
| `Delta_P` | 0.00864996836735652 | -0.006521316095375593 | 0.023821252830088634 |
| `E_INFO` | 0.18420304518479567 | 0.16968841954593594 | 0.19871767082365541 |
| `E_NONINFO` | 0.12608275193434495 | 0.11882543911491509 | 0.13334006475377482 |
| `B_INFO` | 0.009381238130422739 | -0.005133387508436994 | 0.023895863769282472 |
| `B_NONINFO` | 0.0007312697630662184 | -0.013783355875793514 | 0.015245895401925951 |

Derived start averages used in the main text: memory-use frequency 0.810286 for INFO and 0.626083 for NONINFO (exactly 1/2 for SHAM); accuracy 0.628390 for INFO, 0.619740 for NONINFO and 0.619009 for SHAM. In percentage points, `Delta_P` is +0.865 (-0.652 to +2.382), `E_INFO` 18.420 (16.969 to 19.872), `E_NONINFO` 12.608 (11.883 to 13.334), `B_INFO` +0.938 (-0.513 to +2.390) and `B_NONINFO` +0.073 (-1.378 to +1.525).

## S23. Descriptive diagnostics

Totals over all blocks and updates, from the Study 002 descriptive diagnostics record. These counts support interpretation only and carry no inferential bounds. Valid probe use counts policy probes built by valid-`M` carriers in INFO or NONINFO; in NONINFO it is the sum of true-cache uses (innovation updates) and decoy uses (recurrent updates).

| Cell | `F -> M` | `M -> F` | Valid probe uses | Valid probe survivors | Valid-`M` parents at displacement weight 0 |
|---|---|---|---|---|---|
| INFO, ALL_F | 2,222,248 | 8,425,691 | 262,335,134 | 76,070,068 | 83,507,588 |
| INFO, ALL_M | 1,745,959 | 8,901,980 | 276,680,738 | 79,832,243 | 87,969,874 |
| NONINFO, ALL_F | 4,298,024 | 6,349,915 | 200,347,494 | 47,690,642 | 84,354,781 |
| NONINFO, ALL_M | 3,687,279 | 6,960,660 | 218,682,953 | 51,883,813 | 91,669,182 |
| SHAM, ALL_F | 5,653,523 | 4,994,416 | 0 | 0 | 50,037,470 |
| SHAM, ALL_M | 4,994,416 | 5,653,523 | 0 | 0 | 56,000,609 |

| Cell | True-cache uses | True-cache survivors | Decoy uses | Decoy survivors | Decoys of weight 0 | Decoys identical to cache |
|---|---|---|---|---|---|---|
| INFO, ALL_F | 262,335,134 | 76,070,068 | 0 | 0 | 0 | 0 |
| INFO, ALL_M | 276,680,738 | 79,832,243 | 0 | 0 | 0 | 0 |
| NONINFO, ALL_F | 100,189,242 | 19,329,879 | 100,158,252 | 28,360,763 | 42,175,734 | 42,478,033 |
| NONINFO, ALL_M | 109,946,211 | 21,266,901 | 108,736,742 | 30,616,912 | 45,696,359 | 46,024,351 |

The SHAM mutation counts are exchanged between starts, as label complementarity (N2) implies. Every cell contains 5,284,414 recurrent block-updates. No displacement of weight 32 occurred in any arm, and no survival or permutation retry occurred. Weight-0 displacements, where the cache equals the parent and the decoy necessarily equals the cache, made up 42.1% (ALL_F) and 42.0% (ALL_M) of NONINFO decoy applications; these retain direction trivially. Decoys identical to the cache slightly outnumber weight-0 decoys because a permutation can also leave a nonzero displacement unchanged. In every one of the 41,600 blocks, NONINFO paths diverged from INFO paths from both starts. The diagnostics record (SHA-256 in Section S26) also archives fixation, extinction and late-window occupancy counters; they are not interpreted here. It lists 8,178,892,800 total objective queries.

## S24. Execution history, fixtures and independent audit

**Failed original production submission (infrastructure only).** The original Study 002 production submission did not run the model. It stopped at command-line validation because the required `--design-go` argument was missing, and exited with status 64. It created no production output, generated no random number, executed no model and produced no outcome; no value from it exists or was used. The specification states that a failed or uncertain submission is never duplicated. The authenticated failure record has SHA-256 `289bf3188286476698136d499c12eb3a8c9e4e6fc5cde18fcb15718395f4afaa` and was accepted at 06:28:45 UTC on 2026-10-02. A prospective recovery addendum (SHA-256 `db12c0143b6ba2c0b8cd3771ad18b9afe12feb069fe2a97d15227f4fad836e83`) was issued at 06:29:59 before the corrected run. The corrected run used the frozen production namespace (`production-r1`) and the frozen scientific counts and wrote to a separate attempt-2 production directory.

**Jobs.** Production job 53553084 (run manifest SHA-256 `b7aceaf6638e8f06246c8fb9b182c280a8ed98c4444d93a4a60decdbafb80136`; runner receipt `6ea7f69901c17005a017cf134fd8804a7bdd0c1f4b43de53f9e28e0044de8087`); frozen analyzer job 53553256 (runner receipt `c6bb16016b0133bea52dc3a8e4db5b8fe13189bdd0c79a231181c4669b321570`); separately implemented audit job 53553294 (runner receipt `32e3ffff29b3de9f20158af58a67c2c130cb7456ec8c7287f67354897928a8ed`). The specification allowed one production allocation of at most 32 CPUs, 64 GiB RAM and six hours.

**Deterministic fixtures required before production (none contains a sampled production outcome).** (1) Study 001 candidate, survival, mutation and cache hand traces bit-identical when the new arm is absent; (2) Fisher-Yates and rejection fixtures with forced retries and exact known permutations; (3) INFO and NONINFO bit-identical through every innovation update while their states are identical; (4) at the first recurrent valid-`M` coordinate, only the policy-probe genotype may differ before downstream selection; (5) decoy distance equal to cache distance for all 33 possible weights, with weights 0 and 32 unchanged and reported; (6) preservation of Hamming weights and pairwise overlaps of displacement masks under the shared permutation; (7) the uniform subset law, verified analytically and by exhaustive small-bit analogues; (8) SHAM N1 and N2 for full 256-update paths from both starts and a mixed-label diagnostic; (9) unconditional generation of all random purposes, with coordinate-collision and branch-invariance audits; (10) bit-identical regeneration of a non-audit path from its block identifier and source and configuration hashes.

**Replay audit.** The C++ tool PCONV-AUDIT-REPLAY-1.0.0 (executable SHA-256 `48fff49ee3c704adb0ea46f57823fa7830e7f1fa5ec9d7575dde9aca730e4ac3`), run in production mode with production-namespace keys only, replayed blocks 0-63. Its known-answer tests (three FIPS 180 SHA-256 vectors, three Random123 Philox4x32-10 vectors, the key-word layout and Study 002 key texts including `DECOY_PERMUTATION`, a 64-by-64-bit product, the survival and Fisher-Yates rejection thresholds, popcount, published FNV-1a vectors, coordinate-schema ranges and donor injectivity, identity and rotation Fisher-Yates cases with source-to-destination orientation, a forced rejection retry, exhaustive 4- and 5-position analogues of the uniform subset law, decoy distance matching at weights 0 to 32 with retained weights 0 and 32 and preserved overlaps, the arm-specific probe rule and the HALF law) all passed. It checked 64 blocks, 384 path records, 98,304 update records, 12,582,912 audit rows and 16,384 permutation records, each equal to the expected count, and reported status PASS, zero mismatches and no first discrepancy. The receipt states that it reports no estimate and is not a scientific result.

**Record audit.** The Python tool PCONV-AUDIT-RECORDS-1.0.0 (source SHA-256 `e700ce2621def54db2b2f14fb077dced6cff0e57eb6a3439ac2cfe870e1b5ad9`) generated no random draws. It checked 41,600 block records (all with both SHAM identities), 249,600 path records, 63,897,600 update records, 63,897,600 path-updates with 128 queries, 384 reconstructed audit-path hashes, 12,582,912 audit rows, 16,384 permutation records, 98 manifest files, 19 estimates and one decision file, each equal to the expected count, and reported status PASS and zero mismatches. Its self-checks, including the seven frozen half-widths and a Fisher-Yates replay check that detects a wrongly accepted draw, all passed. The records it compared include the estimate file with SHA-256 `5efa213cdfeb985a20fd7bbc65fb36dd505c568670ad350b3223c2139eb0b68d` and the decision file with SHA-256 `3f28b82d0cb40357a1068c464d1c5158c36ba8fbfe775026ce22c70113b3b84a`.

The specification required a reviewer that did not implement the producer to reconstruct the estimates and replay the audit paths without importing producer code. Both audits are nevertheless internal reproducibility checks performed within the project; neither is external peer review or an independent scientific replication, and they detect implementation divergence, not a shared misreading of the specification.

## S25. Exact claim limits for Study 002 and the two-study result

The result concerns the total population-accuracy and allele-frequency effects of directional information in a supplied one-record cache under one HALF law aligned to the cache delay, one fixed one-proposal opportunity cost, one mutation rate, one finite horizon and one model class. Specifically:

- **Supported:** relative to the recurrence-gated, parent-distance-matched NONINFO control, directional cache information raised the late-window start-averaged memory-use frequency by more than one expected individual (95% familywise), and NONINFO itself exceeded exact neutrality by more than one expected individual; both Study 002 start contrasts passed the prespecified finite-horizon ±1/32 gate.
- **Bounded:** the total population-accuracy effect of directional information relative to NONINFO (`Delta_P`) lies below one correct bit per survivor, wholly inside ±1/32; `B_INFO` and `B_NONINFO` lie below one correct bit; no sign is resolved.
- **Descriptive only:** all 12 cell means, all diagnostics, and the close agreement between the Study 002 INFO and Study 001 ACTIVE-HALF cell means.
- **Joint result:** recurrence selected a costly memory-use policy (Study 001), and directional cache information strongly amplified that selection (Study 002), without a resolved one-bit population-performance gain in either study.
- **Not supported:** a per-use value-of-information or per-use causal effect; separate selection on retention versus retrieval; spontaneous origin of memory architecture or capacity; general population utility; any other lag, law, cost, mutation rate, horizon or model class; equilibrium, invasion, stability or fixation; biological, ecological or neural relevance; priority or global novelty; independent replication of Study 001 or independent model-class reproduction.

The control matches parent distance and one common coordinate relabeling but not every cross-genotype relation, and weight-0 displacements retain direction. No grid, cost sweep, new cohort, lag variant or numerical extension follows from either study.

## S26. Provenance of Study 002 quoted values

Every registered Study 002 value in the manuscript and this supplement is checked by `scripts/check_statistics.py` against the saved records below, which also recomputes the seven interval endpoints, the exact linear identities, the decision and classification labels, the SHAM identities, the scope counts, the diagnostic consistency relations and the audit mismatch counts. The checker requires the 22 Study 001, 19 Study 002 and 22 Study 003 records and authenticates each set against its own acceptance record.

| Source | SHA-256 |
|---|---|
| Decision record | `3f28b82d0cb40357a1068c464d1c5158c36ba8fbfe775026ce22c70113b3b84a` |
| 19 estimate records | `5efa213cdfeb985a20fd7bbc65fb36dd505c568670ad350b3223c2139eb0b68d` |
| Descriptive diagnostics | `304843eb5d4a38761a6ce371c7cc125260041dfd910b29d5ba3cc377a6ac3553` |
| Replay receipt | `cbee79527876745e8918576f73a5d302377e4463d959318ca2f5feffa66416d4` |
| Record-verification receipt | `09f5dccf4921c8d842ef60fb31aca712bbb582e87a2011c76c02efb658539197` |
| Production run manifest | `b7aceaf6638e8f06246c8fb9b182c280a8ed98c4444d93a4a60decdbafb80136` |
| Acceptance record (accepted 2026-10-02 07:10:51 UTC) | `fc7777239cd4442c35c01d6261fd71e6fbf7e7f6fd6e11ddcc5d24876ce2f4a2` |
| Frozen specification, revision 1 | `58ee414d753fab042e6cccee1a4e6b95dd7f167633b4f33c1491687d9afcb895` |
| Terminal design review (`DESIGN_GO`) | `76b0c6172e61f60befecef183e83d9e43ea50c1b6bc1e93299668b76f55be60f` |
| Design-GO record (frozen 2026-10-01 18:59:00 UTC) | `f4a6180109e0d212cbe784a7722a64576e5e7cb0fbfd4b23c757bb5e7ab84d6a` |
| Original production authority (issued 2026-10-02 06:02:41 UTC) | `31c2100dd73de0090ded3059525aacd139565917086d2c19880b01e319c98992` |
| Attempt-1 failure record (accepted 2026-10-02 06:28:45 UTC) | `289bf3188286476698136d499c12eb3a8c9e4e6fc5cde18fcb15718395f4afaa` |
| Prospective recovery addendum (issued 2026-10-02 06:29:59 UTC) | `db12c0143b6ba2c0b8cd3771ad18b9afe12feb069fe2a97d15227f4fad836e83` |
| Recovery execution authority (issued 2026-10-02 06:32:55 UTC) | `c4cb2e4c162df92a761b2fff391b6e81e000055db88d7cbaeb954b0750b4e085` |

The first six values are authenticated in the Study 002 acceptance record; the acceptance-record SHA-256 is recomputed and enforced by the statistics-checking script; the specification hash is quoted from the source-binding record and is not recomputed by the checker. The remaining provenance records establish prospective order and preserve the disclosed infrastructure-only failure; they add or modify no scientific outcome.

## Part C. Study 003 (PHASE2-TORUS-MEMORY-003)

## S27. Status, question and relation to Studies 001 and 002

Study 003 is a post-v1.1 prospective extension specified in PHASE2-TORUS-MEMORY-003 revision 1 (SHA-256 `0ff662dc31807b0b2cdcae371d884473ed398e2583272d94331bac8c8f9ff0b0`; design-GO record `e5dc485dd858f18eb9652cfe6b63f142e2d09fb6b069d640438cd8dbe63a786d`, both bound in the frozen decision record). Its frozen question was: does the Study 001 classification of recurrence-attributable enrichment of a mutation-generated private-cache-use allele under a one-proposal opportunity cost reproduce in one independently implemented, large-alphabet, graded-loss, tournament-survival model with entirely new cohorts? The fixed class name is "32-locus, 2^16-allele torus with graded circular loss and tournament survival". It is a test of one alternative operator bundle: alphabet, loss, local-mutation semantics and survival all differ from the bit, Hamming, XOR and Plackett-Luce bundle of Studies 001 and 002. It is not a continuous-state, gradual-search or small-step model. It does not test memory-capacity origin, arbitrary recurrence, biological memory, equilibrium, evolutionary stability, rare-mutant invasion, per-use value of information or the Study 002 directional-versus-displacement contrast.

The accepted primary decision is **SUPPORTS RECURRENCE-ATTRIBUTABLE SELECTIVE ENRICHMENT IN THIS ALTERNATIVE OPERATOR BUNDLE**. The performance classifications are **BOUNDED BELOW THE POSITIVE ONE-BIT SCALE** for `P_abs` (saved class `BOUNDED_BELOW_POSITIVE`; in Study 003 the label refers to the positive 1/32 performance scale of Section S34, not to a bit) and **UNRESOLVED** for `P_rec`. The frozen crossed interpretation is **SELECTION/PERFORMANCE SEPARATION RECURS IN THIS OPERATOR BUNDLE**. The complete scope is 41,600 new independent paired blocks, eight cells per block, 332,800 paths, 85,196,800 path-updates, 10,905,190,400 objective queries and 22 saved estimate records.

Relation to Studies 001 and 002. The producer is a new implementation from the specification and imports or adapts no producer, analyzer or auditor source from Studies 001 or 002; the only permitted external random-number dependency is the pinned Random123 reference of Section S32. The specification required the auditor to be built, without importing producer code, by a worker who did not implement the producer, and to implement Threefry and circular loss independently. No genotype, target, random tape, trajectory, cohort, seed namespace or outcome from any earlier study was reused. No numerical effect is pooled with Studies 001 or 002, and reproduction is assessed only by the prespecified within-bundle classification. The 22 Study 001, 19 Study 002 and 22 Study 003 records are three separately accepted estimate sets, 63 in total.

The two operator bundles compare as follows.

| Element | Studies 001 and 002 | Study 003 |
|---|---|---|
| Individual state | one unsigned 32-bit genotype | 32 circular loci, each an unsigned 16-bit integer in Z/2^16 Z |
| Raw objective | Hamming mismatch to a 32-bit target | exact integer loss `sum_i d_i^2`, with `d_i` the circular distance at locus i |
| Fresh probe and global scout | parent XOR an independent uniform 32-bit mask | independent uniform 32-coordinate vectors |
| Local child | best of candidates 1-3; each bit flipped with probability 1/32 | best of candidates 1-3; each coordinate replaced by a uniform 16-bit value with probability 1/32 |
| Survival | 32 exact integer-weighted draws without replacement, weights `2^(32-h)` | 32 independent four-entry tournaments with replacement; rank only |
| Random numbers | Random123 Philox4x32-10 | Random123 Threefry4x64-20 |
| Population accuracy | `1 - (mean survivor mismatch)/32` | `(2^40 - total survivor loss)/2^40` |
| Performance scale 1/32 | one correct bit per survivor | 1/32 of maximal total loss |
| Implementation and cohorts | Study 001 producer; Study 002 extends it | new implementation, new random addressing, new cohorts |

Shared by both bundles: 32 haploid individuals; the supplied one-record lineage cache; the `M`/`F` policy allele, which couples retention and retrieval; symmetric policy mutation at rate 1/32; four evaluated candidates per parent and 128 objective queries per update; replacement of exactly one fresh proposal by a valid ACTIVE-`M` cache; the ZERO and HALF target laws; 256 updates with the late window 193-256; the eight paired cells; 41,600 blocks; and the Hoeffding-Bonferroni families of Section S9.

## S28. State, horizon and exact torus loss

Fixed state: 32 haploid individuals in deterministic slots 0-31; phenotype `x = (x_0, ..., x_31)` with each coordinate an unsigned 16-bit integer in Z/2^16 Z; policy allele `M` (use a valid private cache in ACTIVE) or `F` (fresh exploration); and a private cache that is invalid or holds one 32-coordinate phenotype from the immediate lineage. Updates run from 1 to 256 inclusive, and the late window is the post-mutation states at updates 193-256 inclusive.

For one coordinate, let `u = (x_i - t_i) mod 2^16`, an integer from 0 to 2^16 - 1, and `d_i = min(u, 2^16 - u)`. The antipode has distance 2^15. Individual loss is the exact integer `Q(x, t) = sum_i d_i^2`. The maximal coordinate loss is 2^30, the maximal individual loss is 32 × 2^30 = 2^35, and the maximal population loss is 32 × 2^35 = 2^40. After survival, true population accuracy is stored as the exact rational `(2^40 - sum over survivors of Q)/2^40`. No floating-point arithmetic enters loss, ranking, saved accuracy numerators or inferential reconstruction. A uniformly random coordinate has expected squared distance 715827883/2, a fraction 715827883/2147483648 of its maximum, slightly above one-third; the expected accuracy of a uniformly random population is therefore slightly below 2/3.

## S29. Blocks, cells, initial state and target laws

There are 41,600 independent random blocks, and every block contains the same eight paired cells, arm (ACTIVE, SHAM) x target law (ZERO, HALF) x policy start (ALL_F, ALL_M). In each block the 32 initial phenotypes have independent uniform 16-bit coordinates and are shared by all eight cells; every cache starts invalid; ALL_F labels every individual `F` and ALL_M labels every individual `M`. Target innovations and copy bits, every candidate draw, donor keys, tournament entries, candidate tie keys and policy-mutation draws take the same coordinate-keyed values in every compatible cell. Blocks are the independent units for all intervals.

For every block and update t, an indexed uniform 32-coordinate innovation vector `I_t` and an indexed fair recurrence bit `R_t` are generated whether used or not. ZERO: `T_t = I_t` for every t. HALF: `T_1 = I_1`, `T_2 = I_2`, and for t >= 3, `T_t = T_(t-2)` when `R_t = 1`, including chained copies, and `T_t = I_t` otherwise. ZERO and HALF share every innovation. HALF is fixed because a one-generation lineage cache is aligned to a two-update target return; it favors retrieval by construction. No other lag, copy probability or target law was tested.

## S30. Candidate families and the opportunity cost

At update t, parent slot i has pre-update phenotype `x_i`. Candidate order is (family, parent slot), with family order parent, policy probe, scout, local child, and candidate index `32 * family + parent_slot` runs from 0 to 127.

1. **Parent:** exactly `x_i`.
2. **Policy probe:** for `F`, an independent uniform 32-coordinate vector `fresh(t, i)`; for a valid `M` in ACTIVE, its cached phenotype; for an invalid `M` in ACTIVE, exactly `fresh(t, i)`; for either label in SHAM, exactly `fresh(t, i)`.
3. **Global scout:** an independent uniform 32-coordinate vector `scout(t, i)`.
4. **Local child:** among families 0-2 for this parent, the smallest-loss donor, an equal-loss tie being broken by the lower unsigned 64-bit donor key and then by the lower family index. For each coordinate independently, the donor coordinate is replaced by an independent uniform 16-bit value when the low five bits of the replacement flag are zero, and retained otherwise.

Every fresh and scout vector, local replacement flag and local replacement value is generated unconditionally. A replacement value may equal the donor value, so a coordinate is flagged with probability exactly 1/32 and actually changes with probability exactly `(1/32)(1 - 2^-16)`. This is a large-alphabet resampling operator, not a small torus step. A valid ACTIVE-`M` carrier replaces exactly one uniform fresh proposal with its cache. There is no penalty coefficient and no other cost. Duplicate phenotypes, including a cache equal to the parent, remain separate candidates, and each incurs one objective query, so every update makes exactly 128 queries.

## S31. Four-entry tournament survival, tie semantics and full event order

For each survivor slot s = 0, ..., 31, four candidate indices are drawn independently with replacement from 0-127, in the same fixed candidate order in every cell. A candidate may appear more than once within a tournament and may win several survivor slots. The tournament winner is the entry with (1) the lowest exact loss; then (2) the lowest label-blind unsigned 64-bit candidate tie key, generated once per candidate and update and reused in every tournament and cell; then (3) the lowest candidate index. The policy label, cache, lineage and entry position never enter the tie break.

Tournament size four was frozen prospectively because 32 survivor slots x four entries gives exactly 128 entries per update, the size of the candidate pool, so every candidate has one tournament entry in expectation while independent with-replacement tournaments permit zero, one or several descendants. It was not calibrated to the survival strength of Study 001, and every result is conditional on this intensity. For total ordered candidate rank r = 1, ..., 128, best first, the exact probability of winning one survivor slot is `p_r = [(129 - r)^4 - (128 - r)^4]/128^4 = N_r/2^28`, expected offspring is `32 p_r = N_r/2^23`, and the probability of no offspring is `(1 - p_r)^32`; the probabilities sum to 1. The unique best candidate has expected offspring 8290815/8388608 and leaves no offspring with probability above one-third; the worst has expected offspring 1/8388608. The complete exact 128-row rank table is Appendix A of the frozen specification and is part of the planned package.

At each update t = 1, ..., 256 the following steps occur in this order:

1. Generate `I_t` and `R_t` and set `T_t` by the cell's law (Section S29).
2. For each parent slot, construct the four candidates (Section S30), generating every declared draw.
3. Evaluate all 128 candidates by exact integer loss against `T_t`.
4. For each survivor slot, draw four tournament entries and assign the winner to that slot.
5. Each survivor inherits the producing parent's pre-update policy label.
6. Independently in each survivor slot, flip only that label `M <-> F` with probability mu = 1/32; there is no post-survival phenotype mutation.
7. A post-mutation `F` gets an invalid cache; a post-mutation `M` caches the producing parent's pre-update phenotype, even when the winning candidate was that parent's cache probe. Separate survivor slots descending from one candidate keep separate cache copies. SHAM writes caches by the same rule but never reads them, and an `M -> F -> M` history carries no hidden record.
8. Record memory-use count, exact population-loss numerator, valid cache-probe uses, cache-probe winners, `F -> M` and `M -> F` counts, tournament duplicate count and query count, all after step 7.

## S32. Coordinate-keyed Threefry4x64-20 addressing

Random values come from canonical Random123 Threefry4x64-20 pinned to the official D. E. Shaw Research repository commit `9545ff6413f258be2f04c1d319d99aaef7521150` (2022-01-17), with reference `include/Random123/threefry.h` SHA-256 `4c210b32b5ba605b059c54d5edd6f01bf04190de49a0abeecec76420cd072a72` and official `tests/kat_vectors` SHA-256 `aab5ebabf40003f63d6d87b24cbd2c8a02652e00cf8bad64226fd50586929183`. The 64-bit four-word rotation pairs, in round-cycle order, are (14, 16), (52, 57), (23, 40), (5, 37), (25, 33), (46, 12), (58, 22) and (32, 32), and the key-schedule parity constant is `0x1BD11BDAA9FC1A22`. Producer and auditor were each required to pass at least the official 20-round known-answer vectors: counter `(0,0,0,0)` with key `(0,0,0,0)` gives `(09218ebde6c85537, 55941f5266d86105, 4bd25e16282434dc, ee29ec846bd2e40b)`, and the all-ones counter with the all-ones key gives `(29c24097942bba1b, 0371bbfb0f6f4e11, 3c231ffa33f83a1c, cd29113fde32d168)`.

Key. For each purpose, the SHA-256 digest of the exact UTF-8 text `PHASE2-TORUS-MEMORY-003|production-r1|<PURPOSE>` is split into byte ranges 0-7, 8-15, 16-23 and 24-31, each read as a little-endian unsigned 64-bit key word. Counter. Every call uses the four unsigned 64-bit counter words (block, update, entity, subindex). Lanes. A 32-coordinate vector uses two calls with subindex q = 0 and 1. In each output word, four 16-bit lanes are extracted from least to most significant; output words 0-3 and lanes 0-3 are enumerated in order, and coordinate `16q + 4 × word + lane` receives `(output_word >> (16 × lane)) & 0xffff`.

| Purpose | Counter | Extraction |
|---|---|---|
| `INITIAL_VECTOR` | (block, 0, parent, q), parent 0-31, q 0-1 | 32-coordinate vector |
| `TARGET_INNOVATION_VECTOR` | (block, t, 0, q), t 1-256, q 0-1 | 32-coordinate vector |
| `TARGET_COPY` | (block, t, 0, 0), every t 1-256 | output word 0, bit 0 |
| `FRESH_VECTOR` | (block, t, parent, q), q 0-1 | 32-coordinate vector |
| `SCOUT_VECTOR` | (block, t, parent, q), q 0-1 | 32-coordinate vector |
| `LOCAL_REPLACE_FLAG` | (block, t, parent, q), q 0-1 | same lane order; replace iff `lane & 31 == 0` |
| `LOCAL_REPLACE_VALUE` | (block, t, parent, q), q 0-1 | 32-coordinate vector |
| `DONOR_KEY` | (block, t, 3 * parent + family, 0), family 0-2 | output word 0 |
| `TOURNAMENT_ENTRY` | (block, t, survivor_slot, entry), entry 0-3 | output word 0 `& 127` |
| `CANDIDATE_TIE_KEY` | (block, t, candidate_index, 0) | output word 0 |
| `POLICY_MUTATION` | (block, t, survivor_slot, 0) | flip iff output word 0 `& 31 == 0` |

Purpose-separated keys make identical counters across purposes distinct. All declared draws are addressable and generated regardless of arm, law, start, label or branch; `TARGET_COPY` is generated at t = 1 and t = 2 although unused. The implementation aborts on an out-of-range coordinate. Before any path, the specification required a mechanical enumeration of every declared schema proving that no `(purpose, key, counter, extraction)` address is duplicated, and a check of invariance under cell evaluation order and thread count.

## S33. SHAM identities and the neutral derivation in Study 003

SHAM never reads a policy label or cache when constructing phenotypes. With shared random coordinates, N1: SHAM phenotype, loss, accuracy, candidate and winner trajectories are bit-identical between ALL_F and ALL_M; and N2: labels in the two SHAM starts are exact complements after every update, so their memory-use frequencies sum to 1 at every update and their start-averaged late-window frequency is exactly 1/2 in every block. Because tournament winners and their producing parents do not depend on labels in SHAM, the neutral derivation of Section S8 applies unchanged, with r = 1 - 2mu = 15/16 and the same expected start differences at update 256 and over the late window. These are analytic neutral checks, not evidence of ACTIVE convergence.

Observed: SHAM memory-use frequencies were 0.499608 (42565009/85196800) and 0.500392 (42631791/85196800) under ZERO and 0.499756 (21288793/42598400) and 0.500244 (21309607/42598400) under HALF; each pair sums exactly to 1. SHAM accuracy was 0.729671 under ZERO and 0.735785 under HALF, with identical exact fractions from both starts. The recovery record verifier confirmed both identities in all 41,600 blocks. As in Studies 001 and 002, ACTIVE and SHAM phenotypes and genealogies diverge at the first valid-`M` retrieval, so ACTIVE-SHAM contrasts are arm-level comparisons, not path-level counterfactuals.

## S34. Estimands and interval families

For block b, let `L_b(arm, law, start)` be the mean memory-use frequency over post-transition updates 193-256 and `P_b(arm, law, start)` the late-window mean exact normalized population accuracy, with start averages where the start is omitted. The block variables are those of Section S9: `A_HALF,b` and `A_ZERO,b`; `C_abs,b = A_HALF,b - 1/2` (range length 1); `C_rec,b = A_HALF,b - A_ZERO,b`, `D_HALF,b` and `D_ZERO,b` (range length 2); `P_abs,b = P_b(ACTIVE, HALF) - P_b(SHAM, HALF)` (range length 2); and `P_rec,b = [P_b(ACTIVE, HALF) - P_b(SHAM, HALF)] - [P_b(ACTIVE, ZERO) - P_b(SHAM, ZERO)]` (range length 4). Each estimate is the mean of its block variable over n = 41,600 independent blocks; each saved record carries the exact sum of block numerators and the block denominator, and each estimate equals the same linear combination of the saved descriptive cell means, which the statistics-checking script verifies exactly in rational arithmetic.

The meaningful allele scale is Delta = 1/32, one expected individual in a 32-member population; it is a late-window expected excess, not persistence of one named carrier. The meaningful performance scale is Delta_P = 1/32 of maximal total loss. It equals moving one coordinate per individual from the torus antipode to exact agreement on average and is roughly the removal of three typical random-coordinate losses per individual. It is not the share of the random-to-perfect range that the 1/32 scale represents in Studies 001 and 002, so effect magnitudes are not compared across bundles; only prespecified within-bundle classifications are.

Intervals use the construction of Section S9, `h = R sqrt[ln(2/alpha_each)/(2n)]`, with 95% familywise coverage in each family. The primary allele family uses alpha_each = 0.05/4 = 0.0125: `C_abs` has half-width 0.00781023, and `C_rec`, `D_HALF` and `D_ZERO` have half-width 0.01562046, below Delta/2 = 0.015625. The performance family uses alpha_each = 0.05/2 = 0.025: `P_abs` has half-width 0.01451463 and `P_rec` 0.02902925. No empirical variance, normal approximation, within-path independence or outcome-adaptive choice enters these bounds. The two Study 003 families are controlled separately from each other and from every Study 001 and Study 002 family.

## S35. Frozen decision order, application and crossed interpretation

Primary rules, applied in order with Delta = 1/32 = 0.03125:

1. Failure of a deterministic identity, RNG collision audit, saved-output reconstruction or 128-query count: **INVALID**. Not triggered; the decision record lists zero identity failures. The audit-header defect of Section S37 is a nonscientific interface failure, not one of these conditions.
2. Either `D_HALF` or `D_ZERO` interval not wholly inside (-Delta, +Delta): **START-DEPENDENT; SCIENTIFIC QUESTION UNRESOLVED**. Not triggered: `D_HALF` [-0.015822, 0.015419] and `D_ZERO` [-0.015861, 0.015380] both lie inside.
3. Lower bounds of both `C_abs` and `C_rec` above Delta: **SUPPORTS RECURRENCE-ATTRIBUTABLE SELECTIVE ENRICHMENT IN THIS ALTERNATIVE OPERATOR BUNDLE**. Triggered: `C_abs` lower bound 0.275067 and `C_rec` lower bound 0.408801.
4. Lower bound of `C_abs` above Delta and upper bound of `C_rec` at or below Delta: SELECTIVE ENRICHMENT NOT ATTRIBUTABLE TO RECURRENCE AT THE FIXED SCALE. Not reached.
5. Upper bound of `C_abs` at or below Delta: BOUNDED NEGATIVE AT THE ONE-INDIVIDUAL SCALE, with a statement of whether the upper bound is below -Delta. Not reached; the adverse-selection field is null in the decision record.
6. Every other pattern: UNRESOLVED. Not reached.

Only rule 3 reproduces the Study 001 classification; a start-dependent or unresolved result would have been no answer rather than evidence against the mechanism. Performance rules, applied in order to each of `P_abs` and `P_rec` with Delta_P = 1/32: lower bound above Delta_P, meaningful positive; upper bound below -Delta_P, meaningful adverse; upper bound at or below Delta_P, bounded below the positive scale; otherwise unresolved. `P_abs` has upper bound 0.019571, at or below 0.03125, and is **BOUNDED BELOW THE POSITIVE ONE-BIT SCALE** (saved class `BOUNDED_BELOW_POSITIVE`), with an interval containing zero. `P_rec` has lower bound -0.022743 and upper bound 0.035316 and is **UNRESOLVED**. This family cannot alter the primary decision.

Frozen crossed interpretation:

| Pattern | Frozen interpretation | Applied |
|---|---|---|
| Primary support and `P_abs` bounded below positive Delta_P | The selection/performance separation recurs in this operator bundle | Yes |
| Primary support and meaningfully positive `P_abs` | The separation is class-specific at the declared scales | No |
| All other combinations | Reported literally, including null, bounded, adverse and unresolved outcomes | No |

The decision record states the applied branch as **SELECTION/PERFORMANCE SEPARATION RECURS IN THIS OPERATOR BUNDLE**. No branch triggers added cohorts, a different tournament, another operator class or any component-isolating follow-up.

## S36. All 22 saved Study 003 estimate records

Inferential records carry frozen bounds; descriptive records do not. Exact values are the saved rational estimates; decimal values are rounded to six places. Record numbers follow the saved index plus one.

| # | Record | Exact | Estimate | Interval |
|---|---|---|---|---|
| 1 | `C_abs` | 48200517/170393600 | 0.282878 | [0.275067, 0.290688] |
| 2 | `C_rec` | 9039837/21299200 | 0.424421 | [0.408801, 0.440042] |
| 3 | `D_HALF` | -687/3407872 | -0.000202 | [-0.015822, 0.015419] |
| 4 | `D_ZERO` | -20481/85196800 | -0.000240 | [-0.015861, 0.015380] |
| 5 | `P_abs` | 284666892836763/56294995342131200 | 0.005057 | [-0.009458, 0.019571] |
| 6 | `P_rec` | 36804952557685197/5854679515581644800 | 0.006286 | [-0.022743, 0.035316] |
| 7 | Frequency, ACTIVE, ZERO, ALL_F | 30549551/85196800 | 0.358576 | descriptive |
| 8 | Frequency, ACTIVE, ZERO, ALL_M | 234839/655360 | 0.358336 | descriptive |
| 9 | Frequency, ACTIVE, HALF, ALL_F | 33353623/42598400 | 0.782978 | descriptive |
| 10 | Frequency, ACTIVE, HALF, ALL_M | 66690071/85196800 | 0.782777 | descriptive |
| 11 | Frequency, SHAM, ZERO, ALL_F | 42565009/85196800 | 0.499608 | descriptive |
| 12 | Frequency, SHAM, ZERO, ALL_M | 42631791/85196800 | 0.500392 | descriptive |
| 13 | Frequency, SHAM, HALF, ALL_F | 21288793/42598400 | 0.499756 | descriptive |
| 14 | Frequency, SHAM, HALF, ALL_M | 21309607/42598400 | 0.500244 | descriptive |
| 15 | Accuracy, ACTIVE, ZERO, ALL_F | 2132393394645606663/2927339757790822400 | 0.728441 | descriptive |
| 16 | Accuracy, ACTIVE, ZERO, ALL_M | 533099455361384907/731834939447705600 | 0.728442 | descriptive |
| 17 | Accuracy, ACTIVE, HALF, ALL_F | 1084347749061327307/1463669878895411200 | 0.740842 | descriptive |
| 18 | Accuracy, ACTIVE, HALF, ALL_M | 1668226509797077/2251799813685248 | 0.740841 | descriptive |
| 19 | Accuracy, SHAM, ZERO, ALL_F | 533998851474226017/731834939447705600 | 0.729671 | descriptive |
| 20 | Accuracy, SHAM, ZERO, ALL_M | 533998851474226017/731834939447705600 | 0.729671 | descriptive |
| 21 | Accuracy, SHAM, HALF, ALL_F | 2153892302001915681/2927339757790822400 | 0.735785 | descriptive |
| 22 | Accuracy, SHAM, HALF, ALL_M | 2153892302001915681/2927339757790822400 | 0.735785 | descriptive |

Saved-precision values of the six inferential records, rounded from the saved 40-place display values to 19 places:

| Record | Estimate | Lower | Upper |
|---|---|---|---|
| `C_abs` | 0.2828775083101712740 | 0.2750672787822218728 | 0.2906877378381206753 |
| `C_rec` | 0.4244214336688701923 | 0.4088009746129713898 | 0.4400418927247689949 |
| `D_HALF` | -0.0002015920785757212 | -0.0158220511344745237 | 0.0154188669773230814 |
| `D_ZERO` | -0.0002403963529146635 | -0.0158608554088134660 | 0.0153800627029841391 |
| `P_abs` | 0.0050566998204140212 | -0.0094579258184457116 | 0.0195713254592737541 |
| `P_rec` | 0.0062864162691967155 | -0.0227428350085227502 | 0.0353156675469161812 |

Derived start averages used in the main text: ACTIVE frequency 0.782878 under HALF and 0.358456 under ZERO; ACTIVE accuracy 0.740842 under HALF and 0.728441 under ZERO; SHAM accuracy 0.735785 under HALF and 0.729671 under ZERO. The ZERO ACTIVE minus SHAM accuracy contrast, -0.001230, and the ZERO frequency deficit below 1/2 are descriptive. No aggregate diagnostic record was among the accepted Study 003 analysis outputs used for this revision, so no Study 003 diagnostic count (mutation, cache-probe use or tournament duplicates) is reported or interpreted.

## S37. Execution, audit-header defect, recovery and audit provenance

**Production and outcome-blind authentication.** One production allocation (job 53561601; ceiling 32 CPU cores, 64 GiB RAM, 12 hours and 150 GiB of new output) ran the frozen namespace `production-r1` for all 41,600 blocks and 332,800 paths. A separate job (53562156) then authenticated, without running analysis or audit and without parsing any scientific value, all 1,962 output files and 4,694,355,040 bytes against an independent inventory, the production manifest (SHA-256 `fb91f91676b84babbcb09ee6cd9129e58580249e21d3b223d71880170873bf4c`) and the completion marker (SHA-256 `ea9763c421161fe8044f6c489b09183f743f4cd8514ab9bab67424f88c1949ff`); every member hash and size matched, and the recorded counts were 41,600 blocks, 332,800 paths, 85,196,800 path-updates, 10,905,190,400 objective queries, 16,777,216 audit candidate rows and 16,777,216 audit tournament-entry rows. The frozen analyzer (job 53562267) computed the 22 estimate records and the decision record.

**Audit attempt 1 (preserved failure).** The first audit attempt (job 53562358; failure record SHA-256 `a54f2842774f38af0cab5a5ee6564b8ac295baf25549b7695b9f4d0500c45531`) stopped at a header-interface check. In the three non-chunk audit files, `audit/audit_candidates.t3c`, `audit/audit_context.t3x` and `audit/audit_entries.t3e`, the authenticated production outputs had written `chunk_index = 0` (bytes `00000000`), whereas the frozen binary schema requires the non-chunk sentinel `0xFFFFFFFF` (bytes `ffffffff`). The cause is in the producer's `FileAuditSink::open`, which constructed the audit `FileHeader` with chunk index 0. Before the stop, the Python verifier had already reconstructed all 22 estimates and checked all 41,600 block records, including N1 and N2, 332,800 path records and 85,196,800 update records, and the C++ tool had passed its ten self-tests; replay of the candidate, tournament-entry and context payloads had not started and remained unverified. The record classifies the event as a preserved nonscientific header-interface failure; production and analysis were unchanged, there was no automatic retry, and attempt 1 is immutable.

**Normalization-only recovery.** Under a prospectively frozen recovery, a derived audit tree was built (normalization receipt SHA-256 `b66baa25585d2a7d0cb4a1ca3fdc002ed8bc7f3ca9f92b6c48475e64da786b5a`). The three audit files were copied and in each exactly bytes 48-51, the `chunk_index` field, were changed from `00000000` to `ffffffff`, 12 differing bytes in total; for every file the receipt proves that all payload bytes from offset 64 to the end are identical to the original. The other files of the tree are hard links to the unmodified production outputs (derived manifest SHA-256 `93a033ed000ad0aacab5dbf83486b8b41f26903eb4f1ec207362caec183126ce`). No scientific value was parsed, and no source, production output or analysis output was modified. The original malformed files remain part of the authenticated production outputs.

**Recovery audit (PASS).** The same accepted auditor then ran once on the derived tree (job 53562452; runner receipt SHA-256 `6ef9d18b51125253b57d59d32c1694a33bda428f54fc83351da0a7d62fee0d5e`). The C++ replay tool `torus_audit` (binary SHA-256 `6b46c3e77045a1992f482e387e6829368052fca3f87082ed857def45aa893626`; receipt SHA-256 `497970f5243313a6cd945b69ce9c7eaba7449ceb33ef6d0dfd0d8873811ec5cc`) passed ten self-tests, including FIPS SHA-256 vectors, purpose-key derivation and separation, the official Threefry known-answer vectors, counter order, lane extraction, range refusal and schema count, circular loss and the rank table, target-law pairing, tournament and donor order, and one- and two-update hand traces. Without invoking producer code or modifying inputs, it replayed blocks 0-63 and checked 64 blocks, 512 paths, 131,072 path updates, 16,777,216 candidate rows, 16,777,216 tournament-entry rows, 131,072 context rows, 131,072 update rows, 512 path rows and 64 block rows, reading the normalized audit files by their normalized SHA-256 values. The Python verifier (receipt SHA-256 `151f88a11c42d36bc9e64c97d98d83afe087e579bfbde4e00762ed13ae73ab11`) checked all 41,600 block records, all N1/N2 blocks, 332,800 path rows, 85,196,800 update rows, all 22 estimates, 16,777,216 candidate rows, 16,777,216 tournament-entry rows and 131,072 context rows. Both exited with status 0 and reported PASS with zero mismatches. These are internal reproducibility audits performed within the project with AI assistance; they are not external peer review or an independent scientific replication, and they detect implementation divergence, not a shared misreading of the specification.

**Release-source correction.** After acceptance, a source-only patch (record of 20:56:31 UTC on 2026-10-02) changed one line of `src/torus/chunk_io.cpp` so that `FileAuditSink::open` constructs the header with the non-chunk sentinel `kNoChunk` instead of 0. The patch was neither compiled nor executed, the original production outputs were not modified, and the scientific model, random streams, event order, records, analyzer and fixtures are unchanged. Because the defect affected only a header field that no scientific computation reads, and the recovery proved the audit payloads byte-identical, no scientific rerun was warranted. The original authenticated outputs, the failed attempt, the normalization receipt and the derived audit tree are all preserved.

## S38. Exact claim limits for Study 003 and the three-study result

The result concerns recurrence-attributable enrichment of a mutable supplied-cache-use policy in one alternative large-alphabet torus/tournament operator bundle, under one HALF law aligned to the cache delay, one ZERO law, one fixed one-proposal opportunity cost, one mutation rate and one finite horizon. Specifically:

- **Supported:** under HALF and the one-proposal opportunity cost, the late-window start-averaged frequency of the memory-use allele exceeded both its exact neutral expectation and its ZERO-law frequency by more than one expected individual (95% familywise), and both late-window start contrasts passed the prespecified finite-horizon ±1/32 gate; the frozen primary decision supports recurrence-attributable selective enrichment in this alternative operator bundle.
- **Bounded:** the recurrent-law ACTIVE minus SHAM performance contrast lies below the positive 1/32 performance scale (95% familywise within its family); its sign is not resolved.
- **Unresolved:** the recurrence interaction in performance.
- **Descriptive only:** all 16 cell means, the ZERO allele deficit and the ZERO adverse accuracy contrast.
- **Three-study result:** recurrence selected a costly memory-use policy in the original bundle (Study 001), directional cache information amplified that selection (Study 002), and recurrence-attributable enrichment recurred in one independently implemented alternative bundle (Study 003), without a resolved population-performance gain at the declared scale in any study; the frozen crossed interpretation is that the selection/performance separation recurs in this operator bundle.
- **Not supported:** universal replication or generality across operator bundles; continuous, gradual or small-step search; attribution of the outcome separately to alphabet, graded loss, resampling or mutation semantics, or tournament survival; any other tournament size, cost, mutation rate, lag, law or horizon; separate selection on retention versus retrieval; spontaneous origin of memory architecture or capacity; general population utility; biological, ecological or neural relevance; equilibrium, invasion, stability or fixation; priority or global novelty; component attribution; a path-level counterfactual; numerical comparison or pooling of effect magnitudes across bundles; and the Study 002 directional-versus-displacement contrast in this bundle.

The three original audit-file headers were malformed; only their four-byte `chunk_index` fields were normalized, in a derived audit tree used for verification. No grid, rate or cost sweep, alternative tournament, new cohort, lag variant, top-up or numerical extension follows from Study 003, and none is proposed.

## S39. Provenance of Study 003 quoted values

Every registered Study 003 value in the manuscript and this supplement is checked by `scripts/check_statistics.py` against the saved records below. The checker recomputes the six interval endpoints and half-widths, the exact block-numerator and cell-mean identities, the decision order, the decision predicates, the performance classes and the crossed interpretation. It also checks the SHAM identities, the scope counts, the outcome-blind production authentication, the preserved attempt-1 failure, the normalization receipt, the recovery receipts and the source-patch receipt.

| Source | SHA-256 |
|---|---|
| Decision record (`decisions.json`) | `84f88bb17c20593abd222b8e099e2981e725b28968c650dbf306f43441892f09` |
| 22 estimate records (`estimates.json`) | `65a31d9f8c73459aa29c3bc4388ed22a156952a18d71388440bffb38f4c073c4` |
| Production-output authentication | `21f8e4147b68ba8099b7d0c293f512ec51e269b40e4b816c2c9b81b826e2a865` |
| Audit attempt-1 failure record | `a54f2842774f38af0cab5a5ee6564b8ac295baf25549b7695b9f4d0500c45531` |
| Audit normalization receipt | `b66baa25585d2a7d0cb4a1ca3fdc002ed8bc7f3ca9f92b6c48475e64da786b5a` |
| Recovery C++ replay receipt | `497970f5243313a6cd945b69ce9c7eaba7449ceb33ef6d0dfd0d8873811ec5cc` |
| Recovery Python verification receipt | `151f88a11c42d36bc9e64c97d98d83afe087e579bfbde4e00762ed13ae73ab11` |
| Recovery runner receipt | `6ef9d18b51125253b57d59d32c1694a33bda428f54fc83351da0a7d62fee0d5e` |
| Production manifest | `fb91f91676b84babbcb09ee6cd9129e58580249e21d3b223d71880170873bf4c` |
| Production runner receipt | `f6737bf03d217f8cde8375f7f6ae5e459d6aa8aeb33a65ed2b85284db540fbc6` |
| Analysis runner receipt | `100c0d26627d20808cfdb5b8e0e950b0f307e576cad3ccf765cf046fed976204` |
| Acceptance record (accepted 2026-10-02 20:54:51 UTC) | `2ac18b3f6c22bc6f11ebbf21ffd8614dbcf24f8f3d028c169e92b2d046b02d5d` |
| Frozen specification, revision 1 | `0ff662dc31807b0b2cdcae371d884473ed398e2583272d94331bac8c8f9ff0b0` |
| Design-GO record | `e5dc485dd858f18eb9652cfe6b63f142e2d09fb6b069d640438cd8dbe63a786d` |

The first eleven values are authenticated in the Study 003 acceptance record. The acceptance-record SHA-256 is bound by the post-audit source-patch receipt and is enforced by the statistics-checking script. The specification and design-GO hashes are bound in the frozen decision record and are not recomputed from the files by the checker. The source-patch receipt post-dates acceptance and has no frozen SHA-256 of its own; the checker verifies that it binds the supplied acceptance and failure records. These provenance records add or modify no scientific outcome.
