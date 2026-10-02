# Interpretation limits that must stay visible (PHASE2-PERFORMANCE-CONVERSION-002)

These are copied into `decision.json` (`interpretation_labels`) by `tools/merge_analyze.py` and are
also recorded in `config/frozen_config.json` (`claim_limits`).

1. **Total effect.** `Delta_P` is the total population-accuracy effect of directional cache
   information beyond the parent and matched displacement length, mediated through evolved policy
   use. The arms share policy rules, not realized policy states.
2. **Not per-use.** It is not a per-use value-of-information estimate, does not separate retention
   from retrieval, and does not demonstrate spontaneous memory origin.
3. **Control geometry.** The fixed shared permutation matches parent distance and one common
   coordinate relabeling of displacement masks. It does not preserve a common cached endpoint among
   siblings or every cross-genotype relation. Displacement weights 0 and 32 cannot be scrambled and
   retain direction; their counts are recorded (`decoy_w0`, `decoy_w32`) and reported.
4. **Scope.** One supplied cache, one HALF law, one fixed displacement cost and one
   parent-distance-matched directional-scramble control. No generalization to other lags,
   probabilities, costs, horizons, models or biology.
5. **Decision order.** INVALID first; then the start-dependence gate (D_INFO and D_NONINFO wholly
   inside (−1/32, +1/32)); then positive, adverse, bounded (with the "wholly inside" statement) and
   unresolved, in that order.
6. **Secondary families.** E_INFO/E_NONINFO and B_INFO/B_NONINFO are separate 95%
   Hoeffding–Bonferroni families and cannot alter the primary decision. A positive primary does not
   establish benefit over no-memory use unless lower(B_INFO) > 1/32; otherwise wording is limited
   to information offsetting displacement cost relative to NONINFO.
7. **Neutral baseline.** E_NONINFO is measured against exactly 1/2, which SHAM attains in every
   block by N2.
8. **No extension.** A null, bounded or adverse outcome is reportable and must not trigger a
   numerical extension, sample-size change, parameter change, alternate endpoint or fallback decision.
9. **Terminal INVALID.** Any INVALID found after production is terminal. Only defects found by
   fixtures before production may be corrected, and only without changing the frozen specification.
