Let me carefully review this frozen specification against the criteria laid out in the request.

**1. Is the recurrence-gated shared bit permutation a defensible cost- and distance-matched directional-information intervention?**

The NONINFO arm uses `x XOR pi_(block,t)(c XOR x)` at recurrent updates (where `R_t=1` and `t>=3`). This:
- Preserves the Hamming distance between the probe and parent: `popcount(pi(c XOR x)) = popcount(c XOR x)` because a permutation of bit positions preserves Hamming weight.
- Preserves the one-proposal displacement (it's still one XOR mask applied to the parent).
- Removes the directional alignment: the original displacement `c XOR x` has a specific pattern of which bits differ, which encodes information about `T_(t-2)`. The permuted version has the same weight but different bit positions, breaking the alignment with the target.
- Is shared across all parents and all six cells in a block/update, so it doesn't add per-parent independent randomness beyond the common coordinate relabeling.

On non-recurrent updates (`R_t=0` or `t<3`), NONINFO uses the true cache `c`, identical to INFO. So the only difference is at recurrent updates.

This is a clean, well-matched intervention. The shared permutation is a design choice that avoids introducing per-parent independent noise (which would change the competitive landscape differently). The fact that it preserves pairwise overlaps of displacement masks (as noted in fixture 6) means the competitive geometry among siblings is partially preserved, though not the cached endpoint.

The weight-0 and weight-32 cases are explicitly acknowledged: when `c = x` (weight 0) or `c XOR x = all ones` (weight 32), the permutation is the identity on the displacement, so NONINFO and INFO are identical at those coordinates. This is honest and doesn't undermine the interpretation—it just means the intervention is a no-op at those specific points, which is a known and bounded limitation.

**Verdict on point 1:** Defensible. It's a well-matched directional knockout.

**2. Does any unmatched feature make `Delta_P` scientifically uninterpretable rather than merely limited?**

Unmatched features:
- Realized label/usage trajectories diverge between arms (total effect, not per-use).
- Sibling cache identity: in INFO, siblings sharing a parent share the same cache `c`. In NONINFO at recurrent updates, siblings sharing the same parent get the same permuted displacement `pi(c XOR x)` (since the permutation is shared). Wait—actually, if two siblings share the same parent `x` and the same cache `c`, then in NONINFO they both get `x XOR pi(c XOR x)`, which is the same. So sibling identity IS preserved at recurrent updates for siblings of the same parent.

Wait, let me re-read. The permutation `pi_(block,t)` is shared by all six cells and all parents in that block/update. So if parent A has cache `c_A` and parent B has cache `c_B`, then:
- INFO: parent A gets probe `c_A`, parent B gets probe `c_B`.
- NONINFO: parent A gets probe `x_A XOR pi(c_A XOR x_A)`, parent B gets probe `x_B XOR pi(c_B XOR x_B)`.

These are different for different parents (unless `c_A XOR x_A = c_B XOR x_B`). So the sibling structure is: children of the same parent share the same probe in both arms. That's preserved.

What's NOT matched: the probe's distances to non-parent genotypes. In INFO, the probe `c` has specific distances to other parents' genotypes. In NONINFO, the permuted probe has different distances to those same genotypes. This affects local-child donor competition.

The CLAUDE-020 review notes: "The unmatched sibling diversity probably gives NONINFO slightly more exploration, which would bias the contrast toward the null."

Wait, actually re-reading more carefully: siblings of the same parent share the same cache in INFO. In NONINFO, they also share the same permuted displacement (same parent, same cache, same permutation). So sibling identity IS preserved.

The "sibling diversity" issue mentioned in CLAUDE-020 refers to different parents having different caches, and after permutation, the probes from different parents may be more or less similar to each other than in INFO. This affects the local-child donor competition (candidate 4 picks the lowest-mismatch among candidates 1-3 for each parent).

Actually wait, the local child is per-parent: "choose the lowest-mismatch member of candidates 1–3 for that parent." So donor competition is within a parent's own candidates, not across parents. The global scout and parent are also per-parent. So the local child doesn't depend on other parents' genotypes.

Let me re-read: "Local child: choose the lowest-mismatch member of candidates 1–3 for that parent, break ties by the fixed keyed donor rule, and apply independent per-bit mutation probability 1/32."

Yes, it's per-parent. So the unmatched feature is about the probe's distance to the target (which is the whole point—the information), and about the probe's distance to other parents' genotypes (which affects... what exactly? The survival is based on mismatch to target, not to other genotypes. The only cross-parent interaction is through the shared target and the survival selection.)

Actually, I think the cross-parent geometry is relevant because survival selects 32 out of 128 candidates. If in NONINFO the probes are more spread out (or less spread out) relative to each other, the composition of survivors changes differently. But this is a second-order effect and is part of the "total effect" interpretation.

The key unmatched feature is: **the probe's directional relationship to the target is removed in NONINFO at recurrent updates**. That's the whole point. Everything else (distance to parent, one-proposal displacement, candidate count, selection rule) is matched.

The residual unmatched features (probe distances to other genotypes, which affects competitive dynamics through survival) are part of the total effect and are acknowledged. They don't make `Delta_P` uninterpretable—they make it a total effect rather than a per-use effect, which is explicitly stated.

**Verdict on point 2:** No unmatched feature makes `Delta_P` scientifically uninterpretable. The total effect framing is correct and honest.

**3. Arithmetic check:**

- Blocks: 41,600
- Cells per block: 3 arms × 2 starts = 6
- Total paths: 41,600 × 6 = 249,600 ✓
- Updates per path: 256
- Total updates (paths × updates): 249,600 × 256 = 63,897,600 ✓
- Candidates per parent per update: 4
- Parents per update: 32
- Candidates per update per path: 4 × 32 = 128
- Objective queries per update: 128 (one mismatch evaluation per candidate)
- Total objective queries: 63,897,600 × 128 = 8,178,892,800 ✓

Primary records:
- `Delta_P`, `D_INFO`, `D_NONINFO` = 3 ✓

Secondary allele records:
- `E_INFO`, `E_NONINFO` = 2 ✓

Secondary performance records:
- `B_INFO`, `B_NONINFO` = 2 ✓

Absolute cell means: 6 cells × 2 (M frequency and accuracy) = 12 ✓

Total saved estimates: 3 + 2 + 2 + 12 = 19 ✓

Audit blocks: 0–63 = 64 blocks
Audit paths: 64 × 6 = 384 ✓
Audit candidates: 384 × 256 × 128 = 384 × 32,768 = 12,582,912 ✓

Now the Hoeffding half-widths:

For a bounded random variable in [a, b], Hoeffding's inequality gives:
P(|X̄ - μ| ≥ ε) ≤ 2 exp(-2nε²/(b-a)²)

For two-sided with confidence 1-α:
ε = (b-a) × sqrt(ln(2/α) / (2n))

Primary family:
- Range length = 2 (accuracy is in [0,1], so range is 1... wait)

Wait. Let me think about this more carefully.

`P(arm,start)` is mean true population accuracy over updates 193-256. Accuracy is the fraction of correct bits, so it's in [0,1]. The mean over 64 updates is also in [0,1].

`P_INFO = [P(INFO,F) + P(INFO,M)]/2` is in [0,1].
`Delta_P = P_INFO - P_NONINFO` is in [-1, 1], so range length = 2. ✓

For each block, `Delta_P^(b)` is in [-1,1], range = 2.

Hoeffding: P(|mean - μ| ≥ ε) ≤ 2 exp(-2nε²/R²) where R = range = 2.

For two-sided confidence 1-α_each:
ε = R × sqrt(ln(2/α_each) / (2n))

α_each = 0.05/3 ≈ 0.016667

ln(2/α_each) = ln(2/0.016667) = ln(120) ≈ 4.7875

ε = 2 × sqrt(4.7875 / (2 × 41600)) = 2 × sqrt(4.7875 / 83200) = 2 × sqrt(0.00005754) = 2 × 0.007586 = 0.015171

The spec says 0.0151712845. Let me be more precise.

ln(120) = ln(8×15) = ln(8) + ln(15) = 3ln(2) + ln(15) = 3(0.693147) + (2.708050) = 2.079442 + 2.708050 = 4.787492

ε = 2 × sqrt(4.787492 / 83200) = 2 × sqrt(0.0000575420) = 2 × 0.00758565 = 0.01517130

The spec says 0.0151712845. Close enough—the small difference is from rounding in ln(120). Let me check more carefully.

Actually, α_each = 0.05/3 exactly. So 2/α_each = 2/(0.05/3) = 60/0.05 = 1200.

Wait! 2/α_each = 2/(0.05/3) = 2×3/0.05 = 6/0.05 = 120.

Yes, 120. So ln(120) is correct.

Let me compute more carefully:
ln(120) = 4.78749174278...

4.78749174278 / 83200 = 0.000057542...

sqrt(0.000057542) = 0.0075856...

2 × 0.0075856 = 0.0151712...

The spec says 0.0151712845. This is consistent (the last digits depend on exact floating point). ✓

Secondary allele family:
- `E_INFO = A_INFO - A_NONINFO`: A is M frequency, in [0,1]. So E_INFO is in [-1,1], range = 2.
  - α_each = 0.025
  - 2/α_each = 80
  - ln(80) = 4.38202663467...
  - ε = 2 × sqrt(4.38202663467 / 83200) = 2 × sqrt(0.000052669...) = 2 × 0.0072573... = 0.0145146...
  - Spec says 0.0145146256. ✓

- `E_NONINFO = A_NONINFO - 1/2`: A_NONINFO is in [0,1], so E_NONINFO is in [-1/2, 1/2], range = 1.
  - ε = 1 × sqrt(4.38202663467 / 83200) = sqrt(0.000052669...) = 0.0072573...
  - Spec says 0.0072573128. ✓

Secondary performance family:
- `B_INFO = P_INFO - P_SHAM`: both in [0,1], so B_INFO in [-1,1], range = 2.
  - Same as E_INFO: half-width 0.0145146256. ✓
- `B_NONINFO = P_NONINFO - P_SHAM`: same range = 2, same half-width. ✓

All arithmetic checks out.

**4. Are all randomness, event order, cache rules, extreme displacement weights and SHAM identities specified enough for independent implementation?**

Randomness:
- Philox4x32-10 with canonical extraction rules.
- New namespace: `PHASE2-PERFORMANCE-CONVERSION-002|production-r1|<purpose>`
- Purposes listed: `INITIAL_GENOTYPE`, `TARGET_INNOVATION`, `TARGET_COPY`, `FRESH_MASK`, `SCOUT_MASK`, `LOCAL_BIT`, `DONOR_KEY`, `SURVIVAL_UNIFORM`, `POLICY_MUTATION`, `DECOY_PERMUTATION`.
- `DECOY_PERMUTATION` counter: `(block, update, entity=i, subindex=retry)` for Fisher-Yates step i=31,...,1.
- 64-bit draw: `word0 | (word1<<32)`.
- Mapping to [0,i] via Lemire multiply-high rejection.
- Generated unconditionally for every block/update.

This is sufficiently specified for independent implementation, assuming the "accepted extraction rules" and "canonical Random123 Philox4x32-10" are defined in the referenced 001 document. The counter structure is explicit.

Event order:
- Section 6: "Survivors inherit the producing parent's policy, mutate M<->F independently with probability 1/32, then update cache as defined in section 2."
- Section 2: "post-mutation F invalidates it and post-mutation M stores the producing parent's pre-update genotype."

So the order is: selection → policy inheritance → policy mutation → cache update. This is explicit.

The candidate evaluation order: 128 candidates, ordered by `(family, parent_slot)` in family order parent, policy probe, global scout, local child. This is explicit.

Extreme displacement weights:
- Weight 0: `c = x`, so `c XOR x = 0`, permutation of 0 is 0, NONINFO probe = x. Same as INFO.
- Weight 32: `c XOR x = all ones`, permutation of all ones is all ones, NONINFO probe = x XOR all ones = ~x. Same as INFO.
- Both cases are explicitly acknowledged: "When the displacement has weight 0 or 32, scrambling cannot change it; this retained information is explicit."

SHAM identities:
- N1: SHAM genotype, mismatch, accuracy and survivor trajectories are bit-identical between starts. This makes sense because SHAM never reads labels or caches in genotype construction—the probe is always `x XOR fresh_mask` regardless of label.
- N2: SHAM labels are exact complements after each update. If ALL_F starts with all F and ALL_M starts with all M, and mutation is symmetric (M→F and F→M both with probability 1/32), then by symmetry the labels should be complements. This is a deterministic identity that can be verified.

Wait, is N2 actually guaranteed? Let me think...

If we start with ALL_F (all labels F) and ALL_M (all labels M), and the mutation rule is "mutate M<->F independently with probability 1/32", then:
- A survivor that inherited F from an ALL_F parent stays F with prob 31/32, becomes M with prob 1/32.
- A survivor that inherited M from an ALL_M parent stays M with prob 31/32, becomes F with prob 1/32.

But the survivors themselves depend on the genotypes, which depend on the labels (through the policy probe). In SHAM, the policy probe is always `x XOR fresh_mask` regardless of label. So the genotypes are identical between ALL_F and ALL_M starts (given the same random numbers). The survivors are therefore the same individuals (same slots) in both starts.

Now, the label of a survivor in slot s:
- In ALL_F start: the parent in slot s has label F (initially), inherits F, mutates to M with prob 1/32.
- In ALL_M start: the parent in slot s has label M (initially), inherits M, mutates to F with prob 1/32.

But wait—the survivors are the same slots in both starts (because genotypes are identical). So the survivor in slot s in the ALL_F start has label F (mutated with prob 1/32 to M), and the survivor in slot s in the ALL_M start has label M (mutated with prob 1/32 to F).

For N2 to hold (exact complements), we need: whenever the ALL_F start has label M in slot s, the ALL_M start has label F in slot s, and vice versa.

The mutation draws are the same in both starts (shared random coordinates). So if the mutation draw in slot s is "mutate" (probability 1/32), then:
- ALL_F: F → M
- ALL_M: M → F

If the mutation draw is "no mutate" (probability 31/32):
- ALL_F: F → F
- ALL_M: M → M

So yes, the labels are exact complements at every step. N2 holds. ✓

But wait—there's a subtlety. The policy mutation draws are per-survivor, keyed by slot. Since the survivors are the same slots in both starts, and the mutation draws are the same, the complementarity holds.

Actually, I need to be more careful. The policy mutation is "survivors inherit the producing parent's policy, mutate M<->F independently with probability 1/32." The key question is: is the mutation draw keyed by the parent slot or the survivor slot?

If it's keyed by the parent slot (or equivalently the survivor slot, since survivors are in fixed slots 0-31), then the same draw is used in both starts, and complementarity holds.

The spec says "post-mutation `F` invalidates it and post-mutation `M` stores the producing parent's pre-update genotype." This is about the cache, not the label. The label mutation is "mutate M<->F independently with probability 1/32" for each survivor.

I think the specification is clear enough: each survivor gets an independent mutation draw, keyed by (block, path, update, slot) or similar. Since the slots are the same in both starts (same genotypes → same survivors), the draws are the same, and complementarity holds.

The spec says "The exact integer Plackett–Luce survival, candidate order, mutation/cache event order and all unchanged operators are those in `PHASE2-MUTABLE-MEMORY-001-REV1.md`." So the exact keying is in the referenced document. As long as that document specifies the keying, this is sufficient.

**Verdict on point 4:** Sufficiently specified for independent implementation, assuming the referenced 001 document defines the extraction rules and coordinate keying.

**5. Do positive, null and adverse branches remain paper-level without overclaiming?**

Looking at section 11 (Interpretive branches) and section 15 (Claim limits):

- Positive + meaningful B_INFO: "directional cache information produces a one-bit benefit relative to both the matched decoy and SHAM in this fixed model." — Limited to "this fixed model." Good.
- Positive without meaningful B_INFO: "directional information offsets some displacement cost; no demonstrated one-bit absolute benefit." — Honest. Good.
- Bounded/adverse + meaningful E_INFO: "directional information selects for policy use without converting into one-bit population performance, sharpening the selection–performance separation." — This is a within-law statement. Good.
- Bounded/adverse without meaningful E_INFO: "the fixed information intervention does not explain the accepted enrichment; close this conversion route." — Honest. Good.

Section 15 explicitly states: "Even a positive result is specific to one supplied cache, one HALF law, one displacement cost, one parent-distance-matched directional scramble and a total effect through evolved use."

The claim limits are well-stated and don't overclaim.

**Verdict on point 5:** Paper-level, no overclaiming.

**6. Is one production run plus one independent audit a finite adequate test?**

The design specifies:
- 41,600 blocks with 6 cells each = 249,600 paths.
- 8.18 billion objective queries.
- 19 saved estimates with Hoeffding confidence intervals.
- Full candidate audit for 64 blocks (384 paths, 12.58M rows).
- Independent audit that reconstructs all 19 estimates, verifies N1/N2 over all blocks, verifies query counts, and replays all 384 audit paths.
- 10 deterministic fixtures.

This is a finite, bounded computation with a hard resource ceiling (32 CPUs, 64 GiB RAM, 6 hours, 100 GiB output). The independent audit is a complete reconstruction from the audit blocks plus verification of global counts and identities.

The Hoeffding intervals are distribution-free and valid for any bounded variable, so the statistical inference is sound regardless of the underlying distribution. The block-level randomization (independent blocks with shared coordinates within a block) provides the needed independence for Hoeffding's inequality.

One potential concern: the 193-256 late window (64 updates) is used to compute the per-block accuracy. Within a block, the 64 updates are not independent (they're sequential). However, Hoeffding's inequality is being applied across blocks (n=41,600), and within each block the value is a fixed (deterministic given the random seed) quantity. So the blocks are independent, and Hoeffding applies to the block-level statistics. This is correct.

**Verdict on point 6:** Finite and adequate.

**Now let me check for any blocking defects:**

1. The `DECOY_PERMUTATION` purpose: the counter is `(block, update, entity=i, subindex=retry)`. The Fisher-Yates algorithm needs one draw per step i=31 down to 1, so 31 draws per (block, update). With retries for Lemire rejection, the subindex handles that. This is clear.

2. The spec says "Starting from [0,...,31], perform Fisher–Yates for i=31,...,1; choose j exactly uniformly from [0,i] with the purpose-separated 64-bit stream and Lemire rejection, then swap positions i and j. A source bit s maps to destination pi[s]."

This defines pi as a permutation where pi[s] is the destination of source bit s. Let me verify: if we start with array [0,1,...,31] and do Fisher-Yates (swap position i with position j), then after the algorithm, the array is a permutation. If we interpret the array as "the array at position s gives the destination of source bit s," then pi[s] = array[s] after Fisher-Yates.

Actually, let me re-read: "A source bit s maps to destination pi[s]." So if the displacement mask has a 1 at position s, after permutation it has a 1 at position pi[s]. This means the permuted mask is obtained by: for each bit position s, if the original mask has a 1 at s, the permuted mask has a 1 at pi[s].

In terms of the array: if we start with array = [0, 1, 2, ..., 31] and perform Fisher-Yates to get a permuted array P, then pi[s] = P[s]. The permuted mask at position d is 1 iff there exists s with P[s] = d and original mask at s is 1. Equivalently, permuted_mask = original_mask with bits moved: bit s goes to position P[s].

This is a well-defined permutation and is standard. The spec is clear.

3. One thing to check: the spec says the permutation is "shared by all six cells and all parents in that block/update." This means the same permutation is used in INFO (where it's not needed—INFO uses the true cache), NONINFO (where it scrambles), and SHAM (where it's not needed). The key point is that it's generated unconditionally and the random coordinate is consumed in all arms, so the random tape is the same across arms. This ensures that the only difference between INFO and NONINFO is the operator rule, not the random numbers.

4. The spec says "The experimental operator may read R_t; the evolving policy cannot." This is important: the gating on R_t is part of the operator definition, not something the policy learns. The policy just says "use cache" or "use fresh mask"—the operator itself decides whether to scramble based on R_t. This is correctly specified.

5. Let me check the decision order for completeness:
   - Step 1: INVALID (mechanical failure)
   - Step 2: START-DEPENDENT (gate failure)
   - Step 3: POSITIVE (lower bound > delta)
   - Step 4: ADVERSE (upper bound < -delta)
   - Step 5: BOUNDED (upper bound ≤ delta)
   - Step 6: UNRESOLVED (otherwise)

   These are mutually exclusive and exhaustive:
   - If not invalid and gates pass:
     - If LB > delta: step 3
     - Else if UB < -delta: step 4
     - Else if UB ≤ delta: step 5
     - Else: step 6 (this means -delta ≤ UB and LB ≤ delta, but UB > delta and UB ≥ -delta)

   Wait, step 5 says "Upper bound of Delta_P is at or below delta." If step 3 and 4 don't apply, we have LB ≤ delta and UB ≥ -delta. Step 5 applies when UB ≤ delta. Step 6 applies when UB > delta (and LB ≤ delta, and UB ≥ -delta from step 4 not applying). So step 6 is: -delta ≤ UB and LB ≤ delta and UB > delta. This is the case where the interval straddles delta from below. Correct.

   Are steps 3 and 4 mutually exclusive? Step 3: LB > delta. Step 4: UB < -delta. Since LB ≤ UB always, if LB > delta then UB > delta > -delta, so step 4 can't apply. Conversely if UB < -delta then LB < -delta < delta, so step 3 can't apply. ✓

6. The half-width 0.0151712845 is below delta/2 = 1/64 ≈ 0.015625. Let me verify: 0.01517 < 0.015625. ✓ This means the interval width is 0.03034, which is below delta = 0.03125. So the interval is narrow enough to potentially distinguish from zero at the one-bit scale. Good.

7. One more check: the spec says "Each block variable has range length 2." For Delta_P, this is correct (range [-1,1]). For D_INFO and D_NONINFO (M frequency differences), M frequency is in [0,1], so the difference is in [-1,1], range = 2. ✓

**Potential issues I want to flag as non-blocking:**

1. The shared permutation preserves pairwise overlaps of displacement masks but not the cached endpoint. This means that in NONINFO, the probes from different parents may have different mutual distances than in INFO. This is a second-order effect on competitive dynamics and is part of the total effect. It's acknowledged in the spec.

2. The weight-0 and weight-32 cases retain full information. In practice, weight 0 means the cache equals the parent (no displacement, no information to scramble), and weight 32 means the cache is the bitwise complement of the parent. The latter is a rare and somewhat degenerate case. The spec acknowledges this honestly.

3. The late window (193-256) is 64 updates. The spec doesn't justify why 193 specifically, but this is carried over from the accepted 001 study, so it's consistent.

**Final assessment:**

I don't find any blocking defect. The specification is:
- Internally complete and consistent.
- The primary contrast is well-defined and supports the stated narrow interpretation.
- All arithmetic is correct.
- Randomness, event order, and identities are specified sufficiently.
- Claims are appropriately limited.
- The test is finite and adequate.

There are minor non-blocking limitations (sibling diversity, weight-0/32 retention, total effect vs. per-use), but these are all acknowledged in the spec and don't change the scientific interpretation.

My verdict: DESIGN_GO.



# Verdict

The frozen specification is internally complete, arithmetically correct, and its primary contrast supports the stated narrow interpretation. No blocking scientific or reproducibility defect was identified.

# Blocking defects

None.

# Nonblocking limitations

1. **Sibling competitive geometry.** The shared permutation preserves Hamming weight and pairwise overlaps of displacement masks under one common coordinate relabeling, but it does not preserve the cached endpoint or the probes' distances to non-parent genotypes. This can shift local-child donor competition and survivor composition second-order relative to INFO. The effect is subsumed in the total-effect estimand and is acknowledged in the specification. It does not render `Delta_P` uninterpretable.

2. **Weight-0 and weight-32 retention.** At these two extreme displacements the permutation is a no-op, so NONINFO retains the full cache direction. The specification states this explicitly and reports the counts. In practice weight 0 means the cache duplicates the parent (no directional information to remove) and weight 32 is a degenerate complement. The retained information is bounded and does not change the interpretation of the contrast.

3. **Total effect, not per-use value of information.** The arms share policy rules but not realized label or usage trajectories. `Delta_P` conflates the direct informational value of the probe with any downstream selection on the allele, cache use frequency, and survivor composition. The specification is explicit that this is a total effect and that per-use value, retention-vs-retrieval separation, and spontaneous origin are out of scope. This is a limitation on generality, not a defect in identifiability.

4. **Single law, single displacement cost.** The HALF recurrence law is fixed to align the one-generation cache delay with a two-update target return. No lag, probability, or cost variation is tested. A positive or negative result is an existence (or non-existence) statement for this one operationalization. The claim limits in section 15 are correctly scoped.

# Arithmetic check

| Quantity | Computation | Value in spec | Correct? |
|---|---|---|---|
| Total paths | 41 600 × 6 | 249 600 | ✓ |
| Total updates | 249 600 × 256 | 63 897 600 | ✓ |
| Objective queries | 63 897 600 × 128 | 8 178 892 800 | ✓ |
| Primary + secondary + absolute records | 3 + 2 + 2 + 12 | 19 | ✓ |
| Audit paths | 64 × 6 | 384 | ✓ |
| Audit candidate rows | 384 × 256 × 128 | 12 582 912 | ✓ |
| Primary half-width | 2·√(ln 120 / 83 200) | 0.0151712845 | ✓ (≈0.015171) |
| Allele `E_INFO` half-width | 2·√(ln 80 / 83 200) | 0.0145146256 | ✓ (≈0.014515) |
| Allele `E_NONINFO` half-width | 1·√(ln 80 / 83 200) | 0.0072573128 | ✓ (≈0.007257) |
| Performance half-widths | same as `E_INFO` | 0.0145146256 | ✓ |
| Primary half-width < δ/2 | 0.015171 < 1/64 ≈ 0.015625 | — | ✓ |

Decision-order branches (steps 1–6) are mutually exclusive and exhaustive given the inequalities on the Hoeffding interval endpoints.

# Permitted claim

Under the single aligned HALF recurrence law, in this fixed 32-bit model with one supplied one-record cache and one parent-distance-matched directional scramble at recurrent updates, a statistically resolved `Delta_P` identifies the total population-accuracy effect of directional cache information beyond the parent and matched displacement length, mediated through evolved policy use. It does not identify a per-use value of information, separate retention from retrieval, establish spontaneous memory origin, or generalize to other lags, costs, models, or biology.

DESIGN_GO
