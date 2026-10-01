# PHASE2-MUTABLE-MEMORY-001: prospective specification, revision 1

Status: **TERMINAL REVIEW CANDIDATE — NO IMPLEMENTATION OR SCIENTIFIC OUTCOME AUTHORIZED**

This is the single design revision allowed by the prior gate. A non-implementing skeptical reviewer must return GO before implementation. Missing requirements produce STOP, not another scientific revision.

## 1. Scientific question

> Does lag-two environmental recurrence selectively enrich a mutable private-memory-use allele above its neutral mutation–drift balance when retrieval displaces one fresh exploratory proposal?

The model supplies the retrieval operator. It tests mutation-originated use of that operator and enrichment at a finite-time mutation–selection balance. It does not test the origin of memory capacity, rare-mutant invasion, stationarity, evolutionary stability, biological memory or recurrence in general.

The general evolution of memory, learning and anticipatory effects is established in prior theory. The possible contribution here is narrower: a matched finite search budget, an exact neutral label control, a fixed one-for-one exploration opportunity cost, and a prospective separation of allele enrichment from collective accuracy. `LITERATURE_BOUNDARY.md` records the bounded review. No claim of first discovery or global novelty is permitted.

## 2. Fixed state space and inherited architecture

- Population: 32 haploid individuals in deterministic slots 0–31.
- Search state: one unsigned 32-bit genotype per individual.
- Policy allele: `M` for private-memory use or `F` for fresh exploration.
- Private cache: for `M`, invalid or one unsigned 32-bit immediate-parent genotype; `F` holds no valid cache.
- Lineage identifiers are audit metadata and are never read by scientific operators.
- Updates: 1–256 inclusive.
- Late window: frequencies measured after policy mutation at updates 193–256 inclusive.
- Every parent produces four evaluated candidates every update, for exactly 128 objective queries.
- Raw objective: Hamming mismatch to the current 32-bit target; true accuracy is `1 - mean_mismatch/32` after survival.

The population size, bit length, four-candidate architecture, lag-two law and selection weights are inherited from the private-memory work so that the changed scientific question is identifiable. The implementation must be new and may import no producer source from studies 001–064.

## 3. New cohorts and initial state

There are 41,600 independent random blocks. Every block contains the same eight paired cells:

`information_arm in {ACTIVE, SHAM}` × `target_law in {ZERO, HALF}` × `policy_start in {ALL_F, ALL_M}`.

For each block:

- 32 initial genotypes are independent uniform 32-bit words and are shared by all eight cells;
- all caches are invalid;
- `ALL_F` labels every individual `F`; `ALL_M` labels every individual `M`;
- target innovations, recurrence bits, candidate masks, donor tie keys, survival uniforms and policy-mutation draws use the same coordinate-keyed random values in every compatible cell.

No prior genotype, preparation, target, trajectory, random tape or outcome is reused.

## 4. Target laws

For every block and update `t`, generate an indexed uniform innovation `I_t` and an indexed fair recurrence bit `R_t`, whether used or not.

- `ZERO`: `T_t = I_t` for all t.
- `HALF`: `T_1 = I_1`, `T_2 = I_2`; for `t >= 3`, `T_t = T_(t-2)` when `R_t=1`, otherwise `T_t = I_t`.

Thus ZERO and HALF use the same innovation at every non-recurrent update. The HALF law is a prior outcome-informed choice. It exactly matches a one-generation cache delay to a two-update target return. No other lag, copy probability or target law is tested.

## 5. Candidate operators

At update t, for each parent slot i with genotype `x_i`:

1. **Parent:** `x_i`.
2. **Policy probe:**
   - `F`: `x_i XOR fresh_mask(t,i)`, where the mask is an independent uniform 32-bit word;
   - valid `M` in ACTIVE: its cached genotype;
   - invalid `M` in ACTIVE: the same fresh-probe construction as F;
   - any label in SHAM: the same fresh-probe construction as F.
3. **Global scout:** `x_i XOR scout_mask(t,i)`, with an independent uniform 32-bit mask for every parent and update.
4. **Local child:** choose the lowest-mismatch member of candidates 1–3 for that parent, breaking a mismatch tie by an independent keyed 64-bit donor key and then by family index. XOR its genotype with a local mask in which each of the 32 bits flips independently with probability 1/32.

Fresh masks, scout masks, local-bit draws and donor keys are generated unconditionally for every coordinate in every cell. No label or branch changes later random values.

The opportunity cost is fixed: a valid ACTIVE-M carrier replaces exactly one uniform fresh proposal with its cache. There is no score penalty or tunable cost coefficient. The magnitude and interpretation of this cost are conditional on the frozen fresh-probe distribution.

The cache can equal the unchanged parent candidate. Both copies remain separately evaluated; no genotype deduplication occurs.

## 6. Survival, mutation and cache event order

Evaluate all 128 candidates. Candidate ordering is `(family, parent_slot)`, with family order parent, policy probe, global scout, local child.

For candidate j with raw mismatch `h_j`, define the exact integer weight `w_j = 2^(32-h_j)`, which is proportional to `2^(-h_j)`. Select 32 candidates sequentially without replacement. At survivor draw d:

1. retain unselected candidates in candidate-index order and sum their weights to `W_d`;
2. obtain an unbiased integer `Z_d` uniformly in `[0,W_d)` from the keyed 64-bit survival stream using Lemire multiply-high rejection: for retry r, form the 128-bit product `m=x*W_d`, set `threshold_d = (2^64 - W_d) mod W_d`, reject when `low64(m) < threshold_d`, otherwise take `Z_d=high64(m)`;
3. select the first remaining candidate whose cumulative weight is greater than `Z_d`.

Assign survivor slots in draw order. The maximum weight sum is `128 * 2^32 = 2^39`, so unsigned 64-bit totals and unsigned 128-bit products are sufficient. This is an exact Plackett–Luce weighted sample without replacement with the inherited weights; it uses no floating arithmetic. No label, cache, lineage or penalized score enters it.

Each selected candidate initially inherits its producing parent's policy. Then, independently for every survivor slot, flip `M <-> F` with probability `mu = 1/32`.

After mutation:

- post-mutation `F`: cache invalid;
- post-mutation `M`: cache becomes the producing parent's pre-update genotype.

Consequently, from update 2 onward every M carrier has a valid one-step lineage cache. A new F→M mutant first uses that record on the next update. An M→F→M history carries no older hidden record. Frequency and accuracy are recorded after this mutation/cache transition.

## 7. Coordinate-keyed random numbers

Use canonical Random123 Philox4x32-10 with multipliers `D2511F53` and `CD9E8D57` and Weyl constants `9E3779B9` and `BB67AE85`. For each purpose, compute SHA-256 of the UTF-8 text `PHASE2-MUTABLE-MEMORY-001|production-r1|<purpose>`; interpret digest bytes 0–3 and 4–7 as little-endian unsigned 32-bit Philox key words. The four little-endian unsigned 32-bit counter words are exactly `(block, update, entity, subindex)`. Fixed purposes are:

`INITIAL_GENOTYPE`, `TARGET_INNOVATION`, `TARGET_COPY`, `FRESH_MASK`, `SCOUT_MASK`, `LOCAL_BIT`, `DONOR_KEY`, `SURVIVAL_UNIFORM`, and `POLICY_MUTATION`.

Coordinates and extraction are fixed:

- block is 0–41,599;
- update is 0 for initialization and 1–256 otherwise;
- initial genotype: entity=parent slot, subindex=0, output word 0;
- target innovation/copy: entity=0, subindex=0, output word 0; the copy bit is word0 bit0;
- fresh/scout mask: entity=parent slot, subindex=0, output word 0;
- local mutation: entity=parent slot, subindex=bit 0–31; flip iff `word0 & 31 == 0`;
- donor key: entity=`3*parent_slot+family_index` for family index 0–2, subindex=0; the unsigned 64-bit key is `word0 | (word1<<32)` and lower wins;
- survival integer: entity=survivor draw 0–31, subindex=rejection retry 0,1,…; the unsigned 64-bit x is `word0 | (word1<<32)`;
- policy mutation: entity=survivor slot 0–31, subindex=0; flip iff `word0 & 31 == 0`.

Words 2–3 are unused. The implementation must abort if any coordinate exceeds its range or a survival retry exceeds the unsigned 32-bit subindex. A mechanical enumeration of the declared coordinate schemas and a purpose-separated collision audit must pass before any population path. All draws are addressable and generated regardless of branch, arm, law or label.

## 8. Exact SHAM identities and neutral derivation

SHAM never reads a policy label or cache when constructing genotypes. With the same keyed draws:

- **N1:** SHAM genotype, mismatch, accuracy and survivor trajectories are bit-identical between ALL_F and ALL_M starts.
- **N2:** policy labels in the two SHAM starts are exact complements after every update. Therefore their M frequencies sum to 1 at every update, and their start-averaged late-window frequency is exactly 1/2 in every block.

For symmetric mutation, with `r = 1 - 2*mu = 15/16`:

- `P(M at t | ALL_F) = [1 - r^t]/2`;
- `P(M at t | ALL_M) = [1 + r^t]/2`;
- expected ALL_M minus ALL_F difference at update 256 is `r^256 = 6.678005283720385e-08`;
- expected difference averaged over t=193…256 is `r^193*(1-r^64)/(64*(1-r)) = 9.57983820600146e-07`.

These formulas justify the neutral window in expectation. They do not establish ACTIVE convergence.

## 9. Primary estimands and familywise intervals

For each block, let `L(arm,law,start)` be the mean M frequency over updates 193–256. Define

- `A_HALF = [L(ACTIVE,HALF,ALL_F) + L(ACTIVE,HALF,ALL_M)]/2`;
- `A_ZERO = [L(ACTIVE,ZERO,ALL_F) + L(ACTIVE,ZERO,ALL_M)]/2`;
- `C_abs = A_HALF - 1/2`;
- `C_rec = A_HALF - A_ZERO`;
- `D_HALF = L(ACTIVE,HALF,ALL_M) - L(ACTIVE,HALF,ALL_F)`;
- `D_ZERO = L(ACTIVE,ZERO,ALL_M) - L(ACTIVE,ZERO,ALL_F)`.

The meaningful scale is `Delta = 1/32`, one expected individual in the 32-member population. This refers to a late-window expected excess; it is not literal persistence of one named carrier.

The four-estimand family is controlled at 95% familywise coverage by two-sided Hoeffding intervals with Bonferroni `alpha_each = 0.05/4`. For a block variable with range length R and n=41,600 blocks, the half-width is

`h = R * sqrt[ln(2/alpha_each)/(2n)]`.

- `C_abs` has range length 1 and h=0.00781023.
- `C_rec`, `D_HALF` and `D_ZERO` have range length 2 and h=0.01562046, below `Delta/2 = 0.015625`.

No empirical variance, normal approximation, within-path independence, pilot or favorable pairing is used for these decision bounds.

## 10. Primary decision table, applied in order

1. Failure of a deterministic identity, seed/counter audit, saved-output reconstruction or 128-query count: **INVALID**. Correct a mechanical defect only if the frozen scientific specification and all existing outcomes remain hidden; otherwise stop.
2. The intervals for either `D_HALF` or `D_ZERO` are not wholly inside `(-Delta,+Delta)`: **START-DEPENDENT; SCIENTIFIC QUESTION UNRESOLVED**.
3. Lower bounds for both `C_abs` and `C_rec` exceed `Delta`: **SUPPORTS RECURRENCE-ATTRIBUTABLE SELECTIVE ENRICHMENT**.
4. The lower bound for `C_abs` exceeds `Delta` and the upper bound for `C_rec` is at or below `Delta`: **SELECTIVE ENRICHMENT NOT ATTRIBUTABLE TO RECURRENCE AT THE FIXED SCALE**.
5. The upper bound for `C_abs` is at or below `Delta`: **BOUNDED NEGATIVE AT THE ONE-INDIVIDUAL SCALE**. Also state whether the upper bound is below `-Delta`, indicating adverse selection.
6. Every other pattern: **UNRESOLVED**.

No replicate, horizon, target-law, mutation-rate or cost change follows any branch.

## 11. Fixed population-performance secondary family

Let `P(arm,law,start)` be late-window mean true population accuracy. Define start-averaged

- `P_abs = P(ACTIVE,HALF) - P(SHAM,HALF)`;
- `P_rec = [P(ACTIVE,HALF)-P(SHAM,HALF)] - [P(ACTIVE,ZERO)-P(SHAM,ZERO)]`.

The meaningful performance scale is one correct bit on average, `Delta_P = 1/32`. Use a separate 95% Hoeffding–Bonferroni family for these two estimands. Report agreement, disagreement or uncertainty relative to allele enrichment; this family cannot alter the primary decision.

With `alpha_each = 0.05/2` and 41,600 blocks, `P_abs` has range length 2 and half-width 0.01451463; `P_rec` has range length 4 and half-width 0.02902925. For each estimand, classify a lower bound above `Delta_P` as meaningful positive performance, an upper bound below `-Delta_P` as meaningful adverse performance, an upper bound at or below `Delta_P` as bounded below the positive one-bit scale, and every other interval as unresolved. Apply those rules in the stated order. These classifications are secondary and cannot alter the primary allele-enrichment decision.

Descriptive diagnostics include absolute cell means, mutation counts, cache-probe use and survival, fixation/extinction occupancy and query totals. They support interpretation only.

## 12. Fixed counts and resource ceiling

- Independent blocks: 41,600.
- Paths per block: 8.
- Total paths: 332,800.
- Updates per path: 256.
- Total path-updates: 85,196,800.
- Candidate evaluations per update: 128.
- Total scientific objective queries: 10,905,190,400.
- Primary inferential records: 4.
- Population-performance inferential records: 2.
- Absolute cell means: 16, comprising M frequency and accuracy for all 8 cells.
- Total prespecified saved estimate records: 22.
- Full candidate-level audit blocks: 0–63, fixed before outcomes.
- Full audit paths: 512.
- Candidate-level audit rows: 16,777,216.

One production allocation only: at most 32 CPUs, 64 GiB RAM, six hours wall time and 100 GiB of new output. A fixture-only timing benchmark may optimize implementation without generating a production target or outcome. Block count and scientific parameters never change. No second scientific job is automatic.

## 13. Required deterministic fixtures

1. One- and two-update hand trace covering all four candidate families, donor choice, exact sequential integer-weighted survival, policy mutation and cache transitions.
2. Memory-better case: lag-two target, cache mismatch 0, fresh mismatch 16.
3. Memory-worse case: fresh target, cache mismatch 28, fresh mismatch 4.
4. Duplicate case: cache equals unchanged candidate; both are evaluated and query count remains 128.
5. Invalid-cache case: ACTIVE-M uses exactly the same fresh draw as F and SHAM; ACTIVE and SHAM first diverge only at valid-M slots.
6. Full 256-update SHAM paths from ALL_F, ALL_M and an arbitrary mixed label assignment verifying N1 and N2.
7. Target-law pairing at innovations, recurrent updates and the t=1/2 boundary.
8. Exact sequential-weighted-selection fixture with known weights and 64-bit draws, mismatch 0 and 32, a forced rejection/retry, duplicate genotypes and exact survivor order.
9. Random-coordinate invariance under changes to labels, arm, law and start.
10. Regeneration of a non-audit path from block ID and source/config hashes to bit-identical saved summaries.

All fixtures and exact query/counter checks pass before production. They contain no randomly sampled scientific outcome.

## 14. Required outputs and independent audit

Save:

- frozen specification, source, build environment, configuration and SHA-256 manifests;
- Philox implementation/version, key schema, namespace, counter collision receipt and regeneration command;
- per-update binary records for every path: M count, total population mismatch, valid/cache-probe use, cache-probe survivors, F→M count, M→F count and query count;
- per-path late summaries and final-state hash;
- per-block four primary variables, two performance variables and N1/N2 flags;
- full candidate rows for audit blocks 0–63, including genotype, family, producing parent, inherited/post-mutation labels where applicable, cache, mismatch, exact integer weight, selection draw/retry/integer where applicable, selected rank, target and recurrence indicator;
- all 22 estimate records with bounds and decision classifications;
- job request, accounting, stdout/stderr, exit status and delivery manifests.

After completion, a reviewer who did not implement the producer must reconstruct all 22 estimates from saved path outputs, verify N1/N2 over all blocks, and independently replay all 512 audit paths without importing producer code. Any discrepancy expands only the affected audit and never changes scientific counts.

## 15. Claim limits and terminal route rule

Even a positive result cannot support rare invasion, mechanism origin, arbitrary recurrence, a general cost scale, stationarity, equilibrium, evolutionary stability, population benefit, biological extrapolation, global novelty or replication of studies 056–060.

This revision proceeds to implementation only after one fresh non-implementing Claude review returns GO against this exact file and its SHA-256. If it returns STOP or identifies a missing required item, the route closes. No further scientific revision, parameter grid or fallback experiment follows automatically.
