# PHASE2-MUTABLE-MEMORY-001: accepted result

Date: 2026-10-01. Frozen prospective study; no post-outcome parameter, cohort, horizon, target-law, mutation-rate or cost change.

## Scientific decision

The fixed primary decision is **SUPPORTS RECURRENCE-ATTRIBUTABLE SELECTIVE ENRICHMENT**. In this 32-individual model, lag-two recurrence strongly enriched the mutable private-memory-use allele despite its fixed opportunity cost of replacing one of two fresh proposals. The corresponding population-performance result is materially weaker: the absolute performance contrast is bounded below the prespecified one-correct-bit scale, while the recurrence interaction in performance is unresolved.

This gives a coherent paper-level result: recurrence can select a costly, mutation-generated memory-use policy without demonstrating a commensurate population-performance advantage at the fixed scale.

## Confirmatory estimates

All values are late-window start-averaged proportions over 41,600 independent paired blocks. Intervals are the frozen two-sided Hoeffding–Bonferroni intervals.

| Estimand | Estimate | 95% familywise interval | Frozen interpretation |
|---|---:|---:|---|
| `C_abs = A_HALF - 1/2` | 0.310292 | [0.302482, 0.318102] | Lower bound exceeds 1/32 |
| `C_rec = A_HALF - A_ZERO` | 0.441635 | [0.426015, 0.457256] | Lower bound exceeds 1/32 |
| `D_HALF` | 0.000723 | [-0.014898, 0.016343] | Entirely within (-1/32,+1/32) |
| `D_ZERO` | -0.000096 | [-0.015717, 0.015524] | Entirely within (-1/32,+1/32) |
| `P_abs` | 0.009377 | [-0.005138, 0.023892] | Bounded below the positive one-bit scale |
| `P_rec` | 0.011360 | [-0.017669, 0.040390] | Unresolved |

Expressed as percentage points, recurrence raised late-window memory-use allele frequency by 44.164 points relative to the nonrecurrent law, and the recurrent-law frequency exceeded the neutral 1/2 baseline by 31.029 points. The start-state contrasts were only +0.072 and -0.010 points and passed the fixed start-dependence gate.

The absolute recurrent-law population-accuracy contrast was +0.938 points, with an upper bound of +2.389 points, below the prespecified +3.125-point one-correct-bit threshold. The recurrence interaction in performance was +1.136 points with an interval from -1.767 to +4.039 points, so its meaningful sign remains unresolved.

## Descriptive outcomes retained

- Under the recurrent HALF law, late-window ACTIVE memory-use frequency was 0.809931 from ALL_F and 0.810654 from ALL_M.
- Under ZERO recurrence, the corresponding values were 0.368705 and 0.368609.
- SHAM frequencies remained near the exact neutral 1/2 benchmark.
- Under HALF, start-averaged ACTIVE accuracy was about 0.628395 versus 0.619018 for SHAM.
- Under ZERO, start-averaged ACTIVE accuracy was about 0.605327 versus 0.607310 for SHAM; this small adverse descriptive contrast is retained.

## Evidence validity

- Production job 53530597 completed 332,800 paths in 70 seconds with exit code 0, 10,905,190,400 objective queries and zero N1, N2 or query-count failures.
- The runner authenticated 124 regular files and 3,299,274,752 bytes of manifest-listed scientific outputs. A separately written deterministic audit reproduced every recorded size and SHA-256.
- Frozen analyzer job 53530688 completed with exit code 0.
- Independent auditor job 53530689 completed with exit code 0.
- The independently implemented C++ replay returned PASS with zero mismatches for blocks 0–63.
- The independently implemented Python verifier returned PASS with zero mismatches over all 41,600 blocks and all 22 saved estimates.

## Claim limits

The HALF law was chosen prospectively to match the one-generation cache delay to a two-update return. The study tested no other lag, copy probability, retrieval cost or target law. The opportunity cost is specific to replacing one effectively uniform fresh proposal. The result concerns selection on use of an already supplied private cache; it does not establish de novo evolution of memory architecture, rare invasion, equilibrium, evolutionary stability, arbitrary recurrence, general cost scales, biological extrapolation or replication of studies 056–060. Population benefit at the one-bit scale was not demonstrated.

## Publication decision

A standalone manuscript is warranted. Its central result should join the positive primary selection result and the weaker performance result rather than presenting either alone. No additional numerical experiment is needed before drafting. Code, frozen design, complete outcomes, analyzer records, independent audit receipts and limitations should be packaged together.
