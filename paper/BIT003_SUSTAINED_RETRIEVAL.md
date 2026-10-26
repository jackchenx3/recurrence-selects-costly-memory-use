# Sustained cache access changes policy composition; survivor-accuracy benefit remains unresolved

Jack Chen

Research preprint addendum v1.4.0 | 26 October 2026 | PHASE3-BIT-SUSTAINED-RETRIEVAL-003

## Abstract

A prospective computational intervention tested sustained cached retrieval against repeated fresh-probe substitution while allowing each branch's state to evolve. Exactly 20,000 new independent paired blocks were allocated equally to ALL_F and ALL_M starting populations. Each block shared a 256-turnover history, followed by 256 turnovers under each policy. Sustained cache access increased time-averaged M-policy frequency by 29.319907 percentage points, with a simultaneous Hoeffding interval of [27.342023, 31.297791] points. The normalized survivor-accuracy contrast was +0.941148 points, with interval [-1.360659, 3.242956] points. The primary effect is MATERIAL_POSITIVE; the secondary finding is UNRESOLVED under the frozen 3.125-point thresholds. The results support a total sustained retrieval-intervention effect on policy composition in this model, without resolving an accuracy benefit or equivalence within the declared accuracy margin. Full independent replay covered every new history and both branches before numerical unsealing. This is an internal AI-assisted computational study, not external journal peer review.

## 1. Question and prospective design

The prior BIT002 experiment measured immediate retrieval effects after an identical factual history. That experiment and its v1.3.0 release remain frozen. This separate BIT003 experiment asks how sustained cached retrieval changes policy composition and survivor accuracy when downstream population states are allowed to diverge. It does not pool new blocks with earlier studies or complete the original broader unfinished protocol.

The accepted BIT/PL model contains 32 slots and 32-bit genotypes. F uses a fresh probe; M normally uses a valid supplied cache, with a fresh fallback when needed. Each new paired block begins from one prescribed ALL_F or ALL_M population and follows a common 256-turnover CACHE history. From its common post-history state, the C branch continues CACHE. At each subsequent update, the A branch substitutes a fresh probe for every eligible M-policy retrieval in its own current state. Both branches evolve for 256 further turnovers using their own state feedback and common indexed tapes and targets. All remaining proposal, selection, mutation, inheritance, and cache-update rules stay fixed. The registered operator and address specifications govern all details.

There are exactly 10,000 blocks per starting group. Blocks, rather than correlated slots or turnovers, are the independent inferential units. Every block, including zero contrasts, enters each estimate. There was one outcome-blind 64-block benchmark and one scientific submission, without arrays, requeue, retry, resume, sample-size adjustment, horizon search, or postproduction scientific-program reruns.

## 2. Prespecified endpoints and inference

For block b, let M_C and M_A be the sums of post-update M-policy counts over the 256 follow-up turnovers, each bounded by 8192. Let A_C and A_A be the corresponding sums of survivor matching-bit scores, each bounded by 262144. The primary block contrast is (M_C - M_A)/8192; the secondary is (A_C - A_A)/262144. Estimates are arithmetic means across all 20,000 paired blocks, equally weighting the two starting groups. Direction is CACHE minus fresh substitution.

The two fixed two-sided Hoeffding intervals use alpha 0.04 for the primary and 0.01 for the secondary endpoint. Their joint coverage is at least 95%, conditional on the ideal independent indexed-RNG assumption; this is not a claim of unconditional randomness for a deterministic generator. Frozen outward rational halfwidths are 19778834661/10^12 and 23018074131/10^12. Intervals are clipped to [-1, 1]. Exact rational arithmetic controls classification before outward decimal rounding; no variance estimate or result-dependent fit is used.

For both endpoints the material-effect threshold and within-margin equivalence margin are 1/32, or 3.125 percentage points. The ordered classifications are material positive, material negative, bounded within margin, positive with magnitude unresolved, negative with magnitude unresolved, and unresolved. A lower endpoint exactly at the positive threshold is not material.

## 3. Results

| Prespecified endpoint | Estimate (percentage points) | Simultaneous interval (percentage points) |
|---|---:|---:|
| Primary: time-averaged M-policy frequency | +29.319907 | [27.342023, 31.297791] |
| Secondary: normalized survivor accuracy | +0.941148 | [-1.360659, 3.242956] |

Table 1. CACHE minus fresh substitution. Estimates are rounded to six decimals; interval endpoints are rounded outward. The intervals jointly cover the two registered estimands with at least 95% coverage under the stated RNG idealization.

The primary interval lies wholly above +3.125 points, giving MATERIAL_POSITIVE. The accuracy interval crosses zero, and its upper endpoint exceeds +3.125 points. Its classification is therefore UNRESOLVED, not bounded within margin. Neither a positive accuracy benefit nor within-margin accuracy equivalence is established. Zero, modest accuracy losses, modest gains, and some gains above the declared positive threshold remain compatible with this interval. A material negative accuracy effect below -3.125 points is excluded at the stated coverage, without proving exact absence of adverse effects.

Exact signed integer totals are 48037735/163840000 for policy frequency and 49343283/5242880000 for normalized survivor accuracy. In normalized units, the outward 12-decimal intervals are [0.273420231501, 0.312977900824] and [-0.013606589795, 0.032429558468]. Original A/B result files agree byte-for-byte and by SHA-256; both endpoints and all unresolved implications are retained.

## 4. Interpretation and limits

Maintaining cached retrieval produces a substantial sustained change in M-policy frequency relative to repeated fresh substitution in this specified model. The intervention also changes later proposals, donor-child pathways, competition, inheritance, and branch states. The estimate is their total downstream effect. It is not natural mediation, an isolated retention benefit, a pure selection coefficient, a recurrence intervention, a claim about cache origins, invasion fitness, equilibrium, or biological memory.

The supplied cache and operator bundle are prespecified model components. The equal ALL_F/ALL_M mixture and finite 256-turnover follow-up delimit the estimand. The result does not license conditioning on final policy or cache carriage, selecting a favorable time window, adding subgroup tests, or extrapolating to other operators or horizons. The policy-composition effect and unresolved accuracy finding must be reported together. A conspicuous composition change does not by itself establish a population-performance benefit.

## 5. Execution and reproducibility

The accepted original FRCE Slurm scientific job was 54339475 on cn038: one node, 32 CPUs, a 96G memory request, and an eight-hour allocation limit. Original job, batch, and extern steps all completed with native exit 0:0 in 168 seconds. The job-wide hierarchical memory high-water value was 8375296000 bytes; the request is distinct from the 700000000000-byte user outer ceiling. Original local work, shared archive, terminal native logs, and the retained terminal-log snapshot total 8330494918 bytes, below 16 GiB. Monitoring was unprivileged and non-atomic; native all-step MaxRSS and raw job-root samples were retained.

Independent auditing replayed all 20,000 common histories and both sustained branches: 15380000 states and 15360000 turnovers, with first_mismatch=NONE. Every original chunk and archive-file hash reconciled. Separate independent execution acceptance was authenticated before the original numerical result files were opened. The one benchmark job 54338181 and scientific job 54339475 remain the only submissions; failed review-service capacity attempts produced no extra scientific run.

The companion reproducibility materials bind the accepted protocol, public keys and fixtures, exact producer/analyzer and independent-auditor source and binary identities, original dataset manifest, original raw integer totals, exact rational presentation, and execution/gate evidence. The original accepted evidence remains retained; public path-label redactions, if applied, must be declared and must not change scientific payloads or their original hashes. The focused publication update preserves earlier releases and findings rather than replacing them. No journal submission or acceptance is claimed.

## References and identity

1. Accepted prospective protocol PHASE3-BIT-SUSTAINED-RETRIEVAL-003 v0.1, SHA-256 30754cf9de941f09b8086e8ffb27a2b4af42882c6bfcca484928efa926ae6b98. Exact endpoints, keys, operator inheritance, full replay, fixed intervals, and decision rules are authoritative.
2. [Earlier frozen BIT002 publication, v1.3.0](https://doi.org/10.5281/zenodo.23649496). Its immediate-intervention definitions and outcomes are separate from this sustained-intervention study.

Author name only; no workplace affiliation or work contact is supplied. Internal independent AI-assisted implementation, execution, and claims reviews do not constitute external peer review. The prior broad protocol remains incomplete.
