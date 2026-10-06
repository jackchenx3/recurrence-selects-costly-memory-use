# Interpretation and limitations

No scientific outcome exists. This file restates the frozen interpretation
rules of `PHASE2-TORUS-MEMORY-003-REV1.md` (SHA-256 `0ff662dc…f0b0`) so the
analyzer's output cannot be read more broadly than the design allows.

## What a result can mean

- **Class:** 32-locus, 2^16-allele torus with graded circular loss and
  tournament survival. This is one alternative operator bundle (alphabet,
  graded loss, coordinate-resampling local child, rank-only four-entry
  tournament) — not a continuous-state, gradual-search or small-step model.
- **Primary decision** (applied in order): INVALID; START-DEPENDENT;
  SUPPORTS RECURRENCE-ATTRIBUTABLE SELECTIVE ENRICHMENT IN THIS ALTERNATIVE
  OPERATOR BUNDLE; SELECTIVE ENRICHMENT NOT ATTRIBUTABLE TO RECURRENCE AT THE
  FIXED SCALE; BOUNDED NEGATIVE AT THE ONE-INDIVIDUAL SCALE (with an adverse
  flag); UNRESOLVED. Only SUPPORTS reproduces the Study 001 classification.
  START-DEPENDENT and UNRESOLVED are no answer, not evidence against.
- **Performance family** (P_abs, P_rec) cannot alter the primary decision.
- A non-support result shows contingency on this bundle at the declared
  scale. It cannot attribute that contingency to alphabet, loss, resampling
  or survival, and it does not invalidate Study 001.
- No effect magnitude is pooled or compared with Studies 001/002; only
  within-class classifications are comparable. Study 002's directional versus
  matched-displacement contrast is not tested.
- Not tested: memory-capacity origin, arbitrary recurrence, biological memory,
  equilibrium, evolutionary stability, rare-mutant invasion, per-use value of
  information, arbitrary costs.

## Fixed limitations

- Tournament size 4 is frozen prospectively; survival is weak (best candidate:
  expected offspring 0.988, zero-offspring probability 0.366). Any null or
  adverse result is conditional on this intensity.
- Hoeffding–Bonferroni intervals are conservative; the range-2 primary
  half-width 0.0156205 is just below Δ/2 = 0.015625.
- The HALF law (lag 2, copy probability 1/2) is the only recurrence tested.
- No follow-up: no retry, seed extension, horizon extension, tournament
  change, rate sweep, cohort top-up, fallback class or automatic paper.

## Implementation-specific notes for reviewers

- The 16 absolute cell means are reported as exact descriptive estimates with
  no interval (the design specifies Hoeffding intervals only for the two
  inferential families).
- Crossed interpretation uses the ordered performance class of P_abs:
  "separation recurs" only when P_abs is BOUNDED_BELOW_POSITIVE; a
  MEANINGFUL_ADVERSE P_abs (whose upper bound is also ≤ Δ_P) is reported
  literally. Reviewers should confirm this reading of section 12.
- Under INVALID the analyzer still writes the 22 exact records (with
  classification code 0) and the identity-failure list, so the defect is
  auditable; no scientific classification is made.
