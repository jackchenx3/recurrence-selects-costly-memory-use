# Interpretation limits that must stay visible

These are copied into `decision.json` (`interpretation_labels`) by `tools/merge_analyze.py`.

1. **HALF cache alignment.** The HALF law is a prior, outcome-informed choice. It exactly matches the
   one-generation cache delay to a two-update target return, so it favors retrieval by construction.
   No other lag, copy probability or target law is tested.
2. **Opportunity cost.** A valid ACTIVE-M carrier replaces exactly one uniform fresh proposal with its
   cache. Because the fresh probe and the scout are both effectively uniform words, the cost is the loss
   of one of two random proposals. Its size depends on the frozen fresh-probe distribution; it is not a
   general cost scale.
3. **Start-dependence gate.** Rule 2 is applied before any enrichment classification. If either D
   interval is not wholly inside (−1/32, +1/32), the result is START-DEPENDENT and the question is unresolved.
4. **Adverse outcomes.** Under BOUNDED NEGATIVE, an upper C_abs bound below −1/32 indicates adverse
   selection. Otherwise the only admissible negative is "not enriched by one expected individual".
5. **Secondary population performance.** P_abs and P_rec form a separate 95% Hoeffding–Bonferroni family.
   They cannot alter the primary decision. P_rec has half-width 0.0290 against Δ_P = 0.03125, so it will
   often be unresolved. The agreement/disagreement/uncertainty label is descriptive only.
6. **Neutral baseline.** The ½ baseline is exact for SHAM in every block by N2. The closed-form neutral
   formulas justify the window in expectation only; they do not establish ACTIVE convergence.
7. **Claim limits.** No claim of rare invasion, mechanism origin, arbitrary recurrence, a general cost
   scale, stationarity, equilibrium, evolutionary stability, population benefit, biological
   extrapolation, global novelty, or replication of studies 056–060.
8. **Terminal INVALID.** Any INVALID found after production is terminal. Only defects found by fixtures
   before production may be corrected, and only without changing the frozen specification.
