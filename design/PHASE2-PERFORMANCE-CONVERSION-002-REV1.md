# PHASE2-PERFORMANCE-CONVERSION-002: prospective specification, revision 1

Status: **FROZEN FOR ONE TERMINAL INDEPENDENT REVIEW — NO IMPLEMENTATION OR SCIENTIFIC OUTCOME AUTHORIZED**

## 1. Scientific decision

> Under the single aligned HALF recurrence law, does directional information in a supplied one-record cache produce a terminal population-accuracy gain exceeding one correct bit when compared with a control that preserves the one-proposal displacement and the cache–parent distance but scrambles cache direction at recurrent updates?

This is a within-law operator intervention. It asks whether information beyond the current parent and matched displacement length changes population performance after evolved policy use. It does not test the origin of memory capacity, a per-use value of information, arbitrary recurrence, equilibrium, invasion, biological memory or generality across model classes.

The primary estimand is a total effect through any resulting divergence in policy frequency, cache use, survivor composition and descendants. The arms share policy rules; they are not forced to share realized policy states.

## 2. Fixed model and state

- Population: 32 haploid individuals in deterministic slots 0–31.
- Genotype and target: unsigned 32-bit words; mismatch is Hamming distance.
- Policy allele: `M` or `F`, with symmetric post-selection mutation probability 1/32.
- Supplied private cache: invalid or one immediate-lineage parental genotype; post-mutation `F` invalidates it and post-mutation `M` stores the producing parent's pre-update genotype.
- Updates: 1–256; late window: post-mutation states at updates 193–256.
- Four candidates per parent and exactly 128 objective queries per update.
- Exact integer Plackett–Luce survival, candidate order, mutation/cache event order and all unchanged operators are those in `PHASE2-MUTABLE-MEMORY-001-REV1.md`.

The accepted producer may be extended only after source authentication. Unchanged operators must remain behaviorally and fixture-identical. The new namespace and every production cohort are independent of study 001.

## 3. Cohorts and six paired cells

There are 41,600 independent blocks. Each contains:

`arm in {INFO, NONINFO, SHAM}` × `policy_start in {ALL_F, ALL_M}`.

For each block, all six cells share the same 32 independent uniform initial genotypes, invalid caches, target innovations, target-copy bits, candidate masks, donor keys, survival uniforms, policy-mutation draws and decoy permutations at compatible coordinates. `ALL_F` and `ALL_M` set all initial labels accordingly.

No prior genotype, target, trajectory, block, random tape or outcome is reused.

## 4. Single target law

Only HALF is used. Generate an independent uniform innovation `I_t` and fair recurrence bit `R_t` for every update whether used or not:

- `T_1=I_1`, `T_2=I_2`;
- for `t>=3`, `T_t=T_(t-2)` when `R_t=1`, otherwise `T_t=I_t`.

The law is fixed because it aligns the one-generation cache delay to a two-update target return. There is no ZERO arm, lag/probability variation, cost variation or subsequent grid.

## 5. Candidate operators

For parent genotype `x` and valid cache `c`:

1. **Parent:** `x`.
2. **Policy probe:**
   - `F`, or invalid `M`, in INFO or NONINFO: `x XOR fresh_mask`;
   - valid `M` in INFO: `c`;
   - valid `M` in NONINFO when `t<3` or `R_t=0`: `c`;
   - valid `M` in NONINFO when `t>=3` and `R_t=1`: `x XOR pi_(block,t)(c XOR x)`;
   - either label in SHAM: `x XOR fresh_mask`.
3. **Global scout:** `x XOR scout_mask`.
4. **Local child:** choose the lowest-mismatch member of candidates 1–3 for that parent, break ties by the fixed keyed donor rule, and apply independent per-bit mutation probability 1/32.

All candidates remain separately evaluated, including duplicates.

`pi_(block,t)` is one exact uniform random permutation of the 32 bit positions, shared by all six cells and all parents in that block/update and generated unconditionally. Starting from `[0,...,31]`, perform Fisher–Yates for `i=31,...,1`; choose `j` exactly uniformly from `[0,i]` with the purpose-separated 64-bit stream and Lemire rejection, then swap positions `i` and `j`. A source bit `s` maps to destination `pi[s]`.

At recurrent updates NONINFO therefore preserves `popcount(c XOR x)` pathwise. The shared permutation preserves overlaps among cache-displacement masks under one common coordinate relabeling. It does not preserve a common cached endpoint among siblings. When the displacement has weight 0 or 32, scrambling cannot change it; this retained information is explicit.

The experimental operator may read `R_t`; the evolving policy cannot. On innovation updates INFO and NONINFO use the identical cache candidate at the same state. The only rule difference is directional scrambling of valid cache displacement at recurrent updates.

## 6. Survival, mutation and cache order

Evaluate 128 candidates. Candidate ordering is `(family,parent_slot)` in family order parent, policy probe, global scout, local child. For mismatch `h`, integer weight is `2^(32-h)`. Select 32 without replacement by the exact sequential Lemire-based procedure in the accepted design. No policy, cache, lineage or arm enters the weight.

Survivors inherit the producing parent's policy, mutate `M<->F` independently with probability 1/32, then update cache as defined in section 2. Record accuracy and frequencies after this transition.

## 7. Random coordinates

Use canonical Random123 Philox4x32-10 and the accepted extraction rules under the new UTF-8 namespace

`PHASE2-PERFORMANCE-CONVERSION-002|production-r1|<purpose>`.

Existing purposes are `INITIAL_GENOTYPE`, `TARGET_INNOVATION`, `TARGET_COPY`, `FRESH_MASK`, `SCOUT_MASK`, `LOCAL_BIT`, `DONOR_KEY`, `SURVIVAL_UNIFORM`, and `POLICY_MUTATION`.

Add `DECOY_PERMUTATION`. Its counter is `(block,update,entity=i,subindex=retry)` for Fisher–Yates step `i=31,...,1`; its unsigned 64-bit draw is `word0 | (word1<<32)`. Map it exactly into `[0,i]` by Lemire multiply-high rejection. Generate every step for every block/update independent of branch or arm. Words 2–3 are unused.

All coordinates are addressable. Abort on range overflow. A purpose-separated collision audit and constructed exact-permutation frequency fixture must pass before production.

## 8. SHAM identities

SHAM never reads labels or caches in genotype construction.

- **N1:** SHAM genotype, mismatch, accuracy and survivor trajectories are bit-identical between starts.
- **N2:** SHAM labels are exact complements after each update, so start-averaged M frequency is exactly 1/2 in every block.

These identities provide the neutral allele benchmark and mechanical validation. SHAM is also the no-memory performance reference.

## 9. Primary family

For block `b`, let `P(arm,start)` be mean true population accuracy over updates 193–256 and `L(arm,start)` the corresponding mean M frequency. Define start averages with equal weight:

- `P_INFO=[P(INFO,F)+P(INFO,M)]/2`;
- `P_NONINFO=[P(NONINFO,F)+P(NONINFO,M)]/2`;
- `Delta_P=P_INFO-P_NONINFO`;
- `D_INFO=L(INFO,M)-L(INFO,F)`;
- `D_NONINFO=L(NONINFO,M)-L(NONINFO,F)`.

The meaningful scale is `delta=1/32`, one correct bit in mean accuracy. The family contains `Delta_P`, `D_INFO`, and `D_NONINFO`. Each block variable has range length 2. Two-sided Hoeffding intervals use Bonferroni `alpha_each=0.05/3`, `n=41,600`, and fixed half-width 0.0151712845, below `delta/2`.

Decision order:

1. Any deterministic identity, coordinate, query-count, saved-output or independent-audit failure: **INVALID**.
2. Either gate interval is not wholly within `(-delta,+delta)`: **START-DEPENDENT; PRIMARY UNRESOLVED**.
3. Lower bound of `Delta_P` exceeds `delta`: **MEANINGFUL POSITIVE DIRECTIONAL-INFORMATION EFFECT RELATIVE TO NONINFO**.
4. Upper bound of `Delta_P` is below `-delta`: **MEANINGFUL ADVERSE DIRECTIONAL-INFORMATION EFFECT RELATIVE TO NONINFO**.
5. Upper bound of `Delta_P` is at or below `delta`: **BOUNDED BELOW THE POSITIVE ONE-BIT SCALE**; separately state whether the interval is wholly inside `(-delta,+delta)`.
6. Otherwise: **UNRESOLVED**.

No sample extension or model change follows any branch.

## 10. Secondary families

These cannot change the primary decision.

### Allele family

Let `A_arm=[L(arm,F)+L(arm,M)]/2`.

- `E_INFO=A_INFO-A_NONINFO`, the total allele-frequency effect of cache direction relative to NONINFO;
- `E_NONINFO=A_NONINFO-1/2`, selection under the decoy operator relative to exact SHAM neutrality.

Use two-sided Hoeffding–Bonferroni `alpha_each=0.025`. `E_INFO` has range length 2 and half-width 0.0145146256; `E_NONINFO` has range length 1 and half-width 0.0072573128. Compare both with `1/32` without changing the primary result.

### No-memory performance family

Let `P_SHAM=[P(SHAM,F)+P(SHAM,M)]/2`.

- `B_INFO=P_INFO-P_SHAM`;
- `B_NONINFO=P_NONINFO-P_SHAM`.

Both have range length 2 and use two-sided Hoeffding–Bonferroni `alpha_each=0.025`, half-width 0.0145146256. A positive primary does not establish benefit over no-memory use unless the lower bound of `B_INFO` also exceeds 1/32. Otherwise the wording is limited to information offsetting displacement cost relative to NONINFO.

## 11. Interpretive branches

- Positive primary plus meaningful positive `B_INFO`: directional cache information produces a one-bit benefit relative to both the matched decoy and SHAM in this fixed model.
- Positive primary without meaningful `B_INFO`: directional information offsets some displacement cost; no demonstrated one-bit absolute benefit.
- Bounded or adverse primary with meaningful positive `E_INFO`: directional information selects for policy use without converting into one-bit population performance, sharpening the selection–performance separation.
- Bounded or adverse primary without meaningful `E_INFO`: the fixed information intervention does not explain the accepted enrichment; close this conversion route.

All adverse and unresolved results are retained. No parameter, lag, probability, cost, horizon or cohort follow-up is automatic.

## 12. Counts and resource ceiling

- Blocks: 41,600.
- Cells/paths per block: 6.
- Total paths: 249,600.
- Updates: 63,897,600.
- Objective queries: 8,178,892,800.
- Primary records: 3.
- Secondary allele records: 2.
- Secondary performance records: 2.
- Absolute cell means: 12 (M frequency and accuracy for six cells).
- Total saved estimate records: 19.
- Full candidate audit blocks: 0–63.
- Audit paths: 384.
- Candidate audit rows: 12,582,912.

One production allocation only: at most 32 CPUs, 64 GiB RAM, six hours wall time and 100 GiB new output. Fixture-only timing may optimize code without producing a scientific target or outcome. A failed or uncertain submission is never duplicated.

## 13. Required deterministic fixtures

1. The accepted candidate, survival, mutation and cache hand traces remain bit-identical when the new arm is absent.
2. Fisher–Yates and Lemire fixtures include forced retries and exact known permutations.
3. INFO and NONINFO are bit-identical through every innovation update while their scientific states are identical.
4. At the first recurrent valid-M coordinate, only the policy-probe genotype may differ before downstream selection.
5. Decoy distance to parent equals cache distance for all 33 possible weights; weights 0 and 32 remain unchanged and are reported.
6. Applying the shared permutation preserves Hamming weights and pairwise overlaps of displacement masks.
7. For fixed parent and displacement weight, enumeration over all 32! permutations gives the stated uniform subset law analytically; constructed small-bit exhaustive analogues verify implementation.
8. SHAM N1/N2 hold for full 256-update paths from both starts and a mixed-label diagnostic.
9. All random purposes are generated unconditionally and pass coordinate collision and branch-invariance audits.
10. A non-audit path regenerates bit-identically from block ID and source/config hashes.

Fixtures contain no sampled production outcome.

## 14. Outputs and independent audit

Save frozen specification, source/config/build manifests, namespace and coordinate receipt; per-update path records for M count, total mismatch, valid probe use, true-cache use, decoy use, probe survival, policy mutations and query totals; per-path summaries and final hashes; all 19 estimates; and full candidate rows for audit blocks 0–63.

Candidate audit rows include target, recurrence indicator, parent, true cache, true displacement mask, shared permutation identifier, actual policy probe, parent distance, candidate family, mismatch, exact weight, donor choice, survival draw/retry/integer, survivor rank and pre/post-mutation labels.

A reviewer that did not implement the producer reconstructs all 19 estimates, verifies N1/N2 over all blocks, verifies all 6 × 41,600 × 256 query counts, and independently replays all 384 audit paths without importing producer code. Expand only an actual discrepancy; never change scientific counts after outcomes.

## 15. Claim limits and terminal rule

Even a positive result is specific to one supplied cache, one HALF law, one displacement cost, one parent-distance-matched directional scramble and a total effect through evolved use. The control retains displacement length and cannot erase direction at weights 0 or 32. It does not identify a per-use effect, separate retention from retrieval, establish spontaneous memory origin, or generalize to other models or biology.

This exact file proceeds only if one fresh non-implementing review returns GO against its SHA-256. STOP closes this design; there is no recursive redesign, outcome pilot, parameter grid or fallback numerical variant.
