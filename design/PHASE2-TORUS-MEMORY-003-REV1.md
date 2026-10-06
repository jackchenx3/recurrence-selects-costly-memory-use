# PHASE2-TORUS-MEMORY-003: prospective specification, revision 1

Status: **FROZEN FOR ONE TERMINAL INDEPENDENT REVIEW — NO IMPLEMENTATION OR SCIENTIFIC OUTCOME AUTHORIZED**

This specification resolves the prospective `TORUS_DESIGN_REVISE` critique before any implementation or outcome exists. A fresh non-implementing reviewer must return `TORUS_DESIGN_GO` against this exact file and its SHA-256 before source construction. A terminal `TORUS_DESIGN_STOP` closes this route. There is no outcome pilot, parameter revision, fallback class or automatic follow-up.

## 1. Scientific decision and class name

> Does the Study 001 classification of recurrence-attributable enrichment of a mutation-generated private-cache-use allele under a one-proposal opportunity cost reproduce in one independently implemented, large-alphabet, graded-loss, tournament-survival model with entirely new cohorts?

The fixed class name is **32-locus, 2^16-allele torus with graded circular loss and tournament survival**. Each locus is a 16-bit circular coordinate. This is a test of an alternative operator bundle: alphabet, graded loss, local-mutation semantics and survival all differ from the bit/Hamming/XOR/Plackett–Luce bundle. It is not a continuous-state, gradual-search or small-step model. Torus geometry enters proposal choice through graded loss; the fresh proposals themselves are uniform and tournament survival uses rank only.

The supplied one-record cache, lag-two target law, one-for-one proposal cost, policy mutation, finite horizon and paired ACTIVE/SHAM logic retain the scientific meaning of Study 001. The study does not test memory-capacity origin, arbitrary recurrence, biological memory, equilibrium, evolutionary stability, rare-mutant invasion, per-use value of information, or the Study 002 directional-versus-displacement contrast. A non-supporting result cannot identify whether alphabet, loss, mutation semantics or survival caused the difference.

No numerical effect is pooled with Studies 001 or 002. Reproduction is assessed by the prespecified within-class classification only.

## 2. Independent implementation and new cohorts

- The producer is a new implementation from this specification. It may not import or adapt producer, analyzer or auditor source from Studies 001 or 002.
- The only permitted external random-number dependency is the pinned public Random123 reference in section 8. Scientific operators must be implemented anew.
- A second worker who did not implement the producer must build the independent auditor without importing producer code. The auditor must implement Threefry and torus loss independently.
- No genotype, target, random tape, trajectory, cohort, seed namespace or outcome from any prior study is reused.
- Implementation, fixtures and production each require separate frozen authority. This document alone authorizes none of them.

## 3. State, horizon and exact loss

- Population: 32 haploid individuals in deterministic slots 0–31.
- Phenotype: `x=(x_0,...,x_31)` with each coordinate an unsigned 16-bit integer in `Z/2^16 Z`.
- Policy allele: `M` uses a valid private cache in ACTIVE; `F` takes fresh exploration.
- Private cache: invalid or one 32-coordinate phenotype from the immediate lineage.
- Updates: 1–256 inclusive.
- Late window: post-mutation states at updates 193–256 inclusive.
- Each parent produces four separately evaluated candidates every update, exactly 128 objective queries.

For one coordinate, compute `u=(x_i-t_i) mod 2^16` as an integer in 0–65,535 and

`d_i=min(u, 65,536-u)`.

The antipode therefore has distance 32,768. Individual loss is the exact integer

`Q(x,t)=sum_i d_i^2`.

Maximum coordinate loss is `2^30`, maximum individual loss is `32*2^30=2^35`, and maximum population loss is `32*2^35=2^40`. After survival, true population accuracy is stored as the exact rational

`accuracy=(2^40-sum_survivor Q)/2^40`.

No floating arithmetic enters loss, ranking, saved accuracy numerators or inferential reconstruction.

## 4. Blocks, cells and initial state

There are 41,600 independent random blocks. Every block contains the same eight paired cells:

`arm in {ACTIVE, SHAM}` × `target_law in {ZERO, HALF}` × `policy_start in {ALL_F, ALL_M}`.

For each block:

- the 32 initial phenotypes contain independent uniform 16-bit coordinates and are shared by all eight cells;
- every cache starts invalid;
- `ALL_F` labels every individual `F`, and `ALL_M` labels every individual `M`;
- target innovations and copy bits, every candidate draw, donor keys, tournament entries, candidate tie keys and policy-mutation draws use the same coordinate-keyed values in every compatible cell.

Pairing is fixed before outcomes. Blocks are the independent units for all intervals.

## 5. Target laws

For every block and update `t`, generate an indexed uniform innovation vector `I_t` and an indexed fair recurrence bit `R_t`, whether used or not.

- `ZERO`: `T_t=I_t` for every t.
- `HALF`: `T_1=I_1`, `T_2=I_2`; for `t>=3`, `T_t=T_(t-2)` when `R_t=1`, including chained copies, and `T_t=I_t` otherwise.

ZERO and HALF share every innovation. The HALF law is fixed because a one-generation lineage cache is aligned to a two-update target return. No other lag, copy probability or target law is tested.

## 6. Four candidate families and the opportunity cost

At update t, parent slot i has pre-update phenotype `x_i`. Candidate order is `(family,parent_slot)` with family order parent, policy probe, scout, local child; candidate index is `32*family+parent_slot` in 0–127.

1. **Parent:** exact `x_i`.
2. **Policy probe:**
   - `F`: an independent uniform 32-coordinate vector `fresh(t,i)`;
   - valid `M` in ACTIVE: its cached phenotype;
   - invalid `M` in ACTIVE: exactly `fresh(t,i)`;
   - either label in SHAM: exactly `fresh(t,i)`.
3. **Global scout:** an independent uniform 32-coordinate vector `scout(t,i)`.
4. **Local child:** among families 0–2 for this parent, choose the smallest-loss donor. Break an equal-loss tie by the lower unsigned donor key and then lower family index. For each coordinate independently, replace the donor coordinate with an independent uniform 16-bit value when the replacement flag's low five bits are zero; otherwise retain the donor value.

Every fresh/scout vector, local replacement flag and local replacement value is generated unconditionally. A replacement value may equal the donor value, so a coordinate is flagged with probability exactly `1/32` and actually changes with probability exactly `(1/32)*(1-2^-16)`. This is a large-alphabet resampling operator, not a small torus step.

A valid ACTIVE-M carrier replaces exactly one uniform fresh proposal with its cache. There is no penalty coefficient and no other cost. Duplicate phenotypes remain separate candidates and each incurs one objective query.

## 7. Four-entry tournament survival, mutation and cache event order

For each survivor slot s=0–31, draw four candidate indices independently with replacement from 0–127. The four entries use the same fixed candidate order in every cell. A candidate may appear more than once within a tournament and may win several survivor slots.

The tournament winner is the entry with:

1. lowest exact loss;
2. then lowest label-blind unsigned 64-bit candidate tie key, which is generated once for each candidate and update and reused in every tournament and cell;
3. then lowest candidate index.

The policy label, cache, lineage and tournament-entry position never enter the tie break.

Tournament size four is frozen prospectively because 32 survivor slots × four entries gives exactly 128 entries per update, equal to the candidate-pool size. Thus every candidate has one tournament entry in expectation while independent with-replacement tournaments permit zero, one or multiple descendants and give a distinct, auditable, rank-based survival regime. This choice is not calibrated to the original survival strength. A null or adverse result is conditional on this intensity.

For total ordered candidate rank `r=1,...,128`, best first, the exact probability of winning one survivor slot is

`p_r=[(129-r)^4-(128-r)^4]/128^4`,

expected offspring is `32*p_r`, and zero-offspring probability is `(1-p_r)^32`. The complete exact rank table is Appendix A. In particular, the unique best candidate has expected offspring `0.988342165946960...` and zero-offspring probability `0.366437715922037...`; survival is deliberately much weaker than near-deterministic preservation of a clearly best candidate.

Assign each tournament winner to its tournament's survivor slot. Then execute this event order exactly:

1. the survivor inherits the producing parent's pre-update policy label;
2. independently flip only that policy label `M<->F` with probability `mu=1/32`; there is no post-survival phenotype mutation;
3. post-mutation `F` gets an invalid cache;
4. post-mutation `M` caches the producing parent's pre-update phenotype, even when the winning candidate was the parent's cache probe.

Separate survivor slots descending from one candidate keep separate cache copies. SHAM writes caches by the same rule but never reads them. An `M->F->M` history carries no hidden record. Frequency and accuracy are recorded after this mutation/cache transition.

## 8. Coordinate-keyed Threefry4x64-20 randomness

Use canonical Random123 Threefry4x64-20 pinned to the official D. E. Shaw Research repository commit `9545ff6413f258be2f04c1d319d99aaef7521150` (2022-01-17):

- reference `include/Random123/threefry.h` SHA-256 `4c210b32b5ba605b059c54d5edd6f01bf04190de49a0abeecec76420cd072a72`;
- official `tests/kat_vectors` SHA-256 `aab5ebabf40003f63d6d87b24cbd2c8a02652e00cf8bad64226fd50586929183`;
- source: `https://github.com/DEShawResearch/random123/tree/9545ff6413f258be2f04c1d319d99aaef7521150`.

The 64-bit four-word rotation pairs, in round-cycle order, are `(14,16)`, `(52,57)`, `(23,40)`, `(5,37)`, `(25,33)`, `(46,12)`, `(58,22)` and `(32,32)`. The key-schedule parity constant is `0x1BD11BDAA9FC1A22`. Both producer and auditor must pass at least these official 20-round known-answer vectors:

- counter `(0,0,0,0)`, key `(0,0,0,0)` -> `(09218ebde6c85537,55941f5266d86105,4bd25e16282434dc,ee29ec846bd2e40b)`;
- counter `(ffffffffffffffff,...,ffffffffffffffff)`, key likewise all ones -> `(29c24097942bba1b,0371bbfb0f6f4e11,3c231ffa33f83a1c,cd29113fde32d168)`.

For each purpose, compute SHA-256 of the exact UTF-8 text

`PHASE2-TORUS-MEMORY-003|production-r1|<PURPOSE>`

and interpret digest byte ranges 0–7, 8–15, 16–23 and 24–31 as four little-endian unsigned 64-bit key words. Every call uses the four unsigned 64-bit counter words `(block,update,entity,subindex)`.

For a 32-coordinate vector, call Threefry twice with subindex q=0,1. In each output word, extract four 16-bit numeric lanes from least to most significant; enumerate output words 0–3 and lanes 0–3. Coordinate `16*q+4*word+lane` receives `(output_word >> (16*lane)) & 0xffff`. The exact purpose schemas are:

| Purpose | Counter use | Extraction |
|---|---|---|
| `INITIAL_VECTOR` | `(block,0,parent,q)`, parent 0–31, q 0–1 | 32-coordinate vector |
| `TARGET_INNOVATION_VECTOR` | `(block,t,0,q)`, t 1–256, q 0–1 | 32-coordinate vector |
| `TARGET_COPY` | `(block,t,0,0)`, every t 1–256 | output word 0 bit 0 |
| `FRESH_VECTOR` | `(block,t,parent,q)`, q 0–1 | 32-coordinate vector |
| `SCOUT_VECTOR` | `(block,t,parent,q)`, q 0–1 | 32-coordinate vector |
| `LOCAL_REPLACE_FLAG` | `(block,t,parent,q)`, q 0–1 | same lane order; replace iff lane `&31==0` |
| `LOCAL_REPLACE_VALUE` | `(block,t,parent,q)`, q 0–1 | 32-coordinate vector |
| `DONOR_KEY` | `(block,t,3*parent+family,0)`, family 0–2 | output word 0 |
| `TOURNAMENT_ENTRY` | `(block,t,survivor_slot,entry)`, entry 0–3 | output word 0 `&127` |
| `CANDIDATE_TIE_KEY` | `(block,t,candidate_index,0)` | output word 0 |
| `POLICY_MUTATION` | `(block,t,survivor_slot,0)` | flip iff output word 0 `&31==0` |

Purpose-separated keys make identical counters across purposes distinct. All declared draws are addressable and generated regardless of arm, law, start, label or branch. `TARGET_COPY` is generated at t=1 and t=2 although unused. The implementation must abort on an out-of-range coordinate. Before any path, a mechanical enumeration of every declared schema must prove no duplicate `(purpose,key,counter,extraction)` address and must confirm invariance under cell evaluation order and thread count.

## 9. Exact SHAM identities and neutral derivation

SHAM never reads a policy label or cache when constructing phenotypes. With shared random coordinates:

- **N1:** SHAM phenotype, loss, accuracy, candidate and winner trajectories are bit-identical between ALL_F and ALL_M starts.
- **N2:** labels in the two SHAM starts are exact complements after every update. Their M frequencies sum to 1 at every update and their start-averaged late-window frequency is exactly 1/2 in every block.

For symmetric mutation, `r=1-2*mu=15/16`:

- `Pr(M at t | ALL_F)=[1-r^t]/2`;
- `Pr(M at t | ALL_M)=[1+r^t]/2`;
- expected ALL_M-minus-ALL_F difference at update 256 is `r^256=6.678005283720385e-08`;
- expected difference averaged over t=193...256 is `r^193*(1-r^64)/(64*(1-r))=9.57983820600146e-07`.

These are analytic neutral checks, not evidence of ACTIVE convergence.

## 10. Primary allele estimands and familywise intervals

For each block, let `L(arm,law,start)` be mean M frequency over post-transition updates 193–256. Define

- `A_HALF=[L(ACTIVE,HALF,ALL_F)+L(ACTIVE,HALF,ALL_M)]/2`;
- `A_ZERO=[L(ACTIVE,ZERO,ALL_F)+L(ACTIVE,ZERO,ALL_M)]/2`;
- `C_abs=A_HALF-1/2`;
- `C_rec=A_HALF-A_ZERO`;
- `D_HALF=L(ACTIVE,HALF,ALL_M)-L(ACTIVE,HALF,ALL_F)`;
- `D_ZERO=L(ACTIVE,ZERO,ALL_M)-L(ACTIVE,ZERO,ALL_F)`.

The meaningful allele scale is `Delta=1/32`, one expected individual in a 32-member population. It is a late-window expected excess, not persistence of one named carrier.

Use two-sided Hoeffding intervals with 95% familywise coverage and Bonferroni `alpha_each=0.05/4`. For block-variable range length R and n=41,600,

`h=R*sqrt[ln(2/alpha_each)/(2*n)]`.

- `C_abs` has range length 1 and `h=0.007810229527949401`.
- `C_rec`, `D_HALF` and `D_ZERO` have range length 2 and `h=0.015620459055898803`, below `Delta/2=0.015625`.

No empirical variance, normal approximation, within-path independence or outcome-adaptive choice enters these bounds.

## 11. Primary decision table, applied in order

1. Failure of a deterministic identity, RNG collision audit, saved-output reconstruction or 128-query count: **INVALID**. A hidden-outcome mechanical defect closes the route; it does not change the model.
2. Either `D_HALF` or `D_ZERO` interval is not wholly inside `(-Delta,+Delta)`: **START-DEPENDENT; SCIENTIFIC QUESTION UNRESOLVED**.
3. Lower bounds for both `C_abs` and `C_rec` exceed `Delta`: **SUPPORTS RECURRENCE-ATTRIBUTABLE SELECTIVE ENRICHMENT IN THIS ALTERNATIVE OPERATOR BUNDLE**.
4. The lower bound for `C_abs` exceeds `Delta` and the upper bound for `C_rec` is at or below `Delta`: **SELECTIVE ENRICHMENT NOT ATTRIBUTABLE TO RECURRENCE AT THE FIXED SCALE**.
5. The upper bound for `C_abs` is at or below `Delta`: **BOUNDED NEGATIVE AT THE ONE-INDIVIDUAL SCALE**; also report whether the upper bound is below `-Delta`, indicating adverse selection.
6. Every other pattern: **UNRESOLVED**.

Only branch 3 reproduces the Study 001 classification. Every other classification is a non-reproduction in this class, but START-DEPENDENT or UNRESOLVED is no answer rather than evidence against the mechanism. No branch triggers added cohorts, a different tournament, another operator class or any component-isolating follow-up.

## 12. Population-performance secondary family

Let `P(arm,law,start)` be late-window mean exact normalized population accuracy and use a start average where `start` is omitted. Define

- `P_abs=P(ACTIVE,HALF)-P(SHAM,HALF)`;
- `P_rec=[P(ACTIVE,HALF)-P(SHAM,HALF)]-[P(ACTIVE,ZERO)-P(SHAM,ZERO)]`.

The meaningful performance scale is `Delta_P=1/32` of maximum total loss. It equals moving one coordinate per individual from the torus antipode to exact agreement on average. A uniformly random coordinate has expected squared loss `715827883/2`, or `715827883/2147483648 = 0.33333333348855376...` of its maximum, so the same threshold is roughly the removal of three typical random-coordinate losses per individual. This differs from the share of the random-to-perfect range in the bit model. Effect magnitudes must not be compared across classes; only prespecified within-class classifications may be compared.

Use a separate 95% Hoeffding–Bonferroni family with `alpha_each=0.05/2`:

- `P_abs` has range length 2 and half-width `0.014514625638859732`;
- `P_rec` has range length 4 and half-width `0.029029251277719464`.

For each interval, apply in order: lower bound above `Delta_P` = meaningful positive; upper bound below `-Delta_P` = meaningful adverse; upper bound at or below `Delta_P` = bounded below the positive scale; otherwise unresolved. This family cannot alter the primary allele decision.

Crossed interpretation is frozen:

- primary support plus `P_abs` bounded below positive `Delta_P`: the selection/performance separation recurs in this operator bundle;
- primary support plus meaningfully positive `P_abs`: the separation is class-specific at the declared scales;
- all other combinations are reported literally, including null, bounded, adverse and unresolved outcomes.

## 13. Fixed counts and finite resource rule

- Blocks: 41,600.
- Cells per block: 8.
- Paths: 332,800.
- Updates per path: 256.
- Path-updates: 85,196,800.
- Candidates/objective queries per update: 128.
- Total objective queries: 10,905,190,400.
- Primary allele records: 4.
- Performance records: 2.
- Absolute cell means: 16, M frequency and accuracy for all eight cells.
- Total prespecified estimate records: 22.
- Full audit blocks: 0–63, fixed before outcomes.
- Full audit paths: 512.
- Audit candidate rows: 16,777,216.
- Audit tournament-entry rows: 16,777,216.

The single production ceiling is 32 CPU cores, 64 GiB RAM, 12 hours wall time and 150 GiB of new output. No H200 is needed for scientific simulation. A fixture-only timing test may run solely in a distinct non-production namespace after all deterministic fixtures pass. It may confirm or tighten the resource request; it may not create a production target or outcome, inspect an inferential record, or alter blocks, cells, updates, operators or saved fields. If the fixed study does not fit the ceiling, the route stops.

## 14. Required deterministic fixtures before production

1. Official Threefry all-zero and all-one known-answer vectors, independently passed by producer and auditor.
2. Complete declared-address enumeration, collision audit and thread/cell-order invariance check.
3. Circular-distance wrap cases `(0,65535)`, `(65535,0)`, equality and the 32,768 antipode.
4. Maximum coordinate, individual and population loss and exact accuracy numerator cases.
5. One- and two-update hand trace covering all candidate families, tournament draws, label mutation and cache transitions.
6. Target-law pairing at t=1/2, new innovations, direct copies and chained copies.
7. Memory-better torus case and memory-worse torus case with exact integer losses.
8. Duplicate candidate case in which cache equals parent; both are evaluated and the query count stays 128.
9. Invalid-cache case: ACTIVE-M uses the exact same fresh vector as F and SHAM.
10. Donor tie fixture covering equal losses, lower donor key and lower family index.
11. Local-child fixture in which a flagged replacement equals the donor value, verifying flagged and actual-change counts.
12. Tournament fixture with duplicate entries, cross-label equal-loss candidates, a forced candidate-key tie, candidate-index fallback and one candidate winning several slots.
13. Exact analytic rank table checks for all 128 ranks and the total `sum_r p_r=1`.
14. Full 256-update SHAM paths from ALL_F, ALL_M and a mixed label assignment verifying N1 and N2 under tournament survival.
15. Random-coordinate invariance under changes to labels, arm, law, start, cell evaluation order and thread count.
16. Regeneration of a non-audit path from block ID and frozen source/config hashes to bit-identical saved summaries.

Fixtures are constructed without randomly sampling a scientific block or revealing an outcome. A fresh source review and separate fixture authority precede execution.

## 15. Required saved records and independent audit

Save:

- this specification, arithmetic records, producer/auditor source, pinned Random123 provenance, build environment, configuration and SHA-256 manifests;
- key derivation, counter/extraction schemas, collision receipt and regeneration commands;
- per-update binary records for every path: M count, exact population-loss numerator, valid/cache-probe uses, cache-probe winners, F-to-M and M-to-F counts, tournament duplicate count and query count;
- per-path late summaries and final-state hash;
- per-block four primary variables, two performance variables and N1/N2 flags;
- for audit blocks 0–63, all candidate phenotypes, family, producer, inherited/post-mutation labels where applicable, cache validity/value, exact loss, donor key/choice, candidate tie key, tournament entries and winners, target and recurrence indicator;
- all 22 estimate records with exact inputs, intervals and classifications;
- job request, accounting, stdout/stderr, exit status and delivery manifests.

The independent auditor must reconstruct all 22 estimates from saved path records, verify N1/N2 over all 41,600 blocks, and independently replay the fixed 512 audit paths: 131,072 path-updates, 16,777,216 candidate rows and 16,777,216 tournament entries. It must implement Threefry and circular loss independently and may share only this specification, official known-answer vectors and saved records. Any discrepancy expands only the affected audit; it never changes scientific counts or model parameters.

## 16. Interpretation and terminal route rule

A primary support result means only that recurrence-attributable enrichment survives this one alternative large-alphabet, graded-loss, tournament-survival operator bundle. It does not establish universal replication, continuous or gradual search, general memory evolution, arbitrary costs, biological relevance, equilibrium, invasion or priority.

A non-support result establishes contingency on the tested operator bundle at the declared scale. It cannot attribute contingency to alphabet, loss, resampling or tournament survival and does not invalidate Study 001. The fixed no-extension rule leaves that component ambiguity unresolved.

Study 002's directional-information versus matched-displacement contrast is not tested. Null, bounded, adverse and unresolved outcomes remain public reportable results. No favorable retry, seed extension, horizon extension, tournament change, rate sweep, cohort top-up, fallback class or automatic paper is permitted.

Normative arithmetic is in `TORUS_003_DESIGN_ARITHMETIC.json`, generated by `torus_003_design_arithmetic.py`. The full tournament table is embedded below and also saved as `TORUS_003_TOURNAMENT_RANK_TABLE.md`.

## Appendix A. Exact four-entry tournament rank table

For every row, `p_r=N_r/2^28` and `E[offspring]=N_r/2^23` before fraction reduction.

<!-- Generated by torus_003_design_arithmetic.py; no outcomes accessed. -->
| Rank r | Exact N_r in p_r=N_r/2^28 | E[offspring]=N_r/2^23 | Decimal E[offspring] |
|---:|---:|---:|---:|
| 1 | 8290815 | 8290815/8388608 | 0.988342165947 |
| 2 | 8097265 | 8097265/8388608 | 0.965269207954 |
| 3 | 7906751 | 7906751/8388608 | 0.942558169365 |
| 4 | 7719249 | 7719249/8388608 | 0.920206189156 |
| 5 | 7534735 | 7534735/8388608 | 0.898210406303 |
| 6 | 7353185 | 7353185/8388608 | 0.876567959785 |
| 7 | 7174575 | 7174575/8388608 | 0.855275988579 |
| 8 | 6998881 | 6998881/8388608 | 0.83433163166 |
| 9 | 6826079 | 6826079/8388608 | 0.813732028008 |
| 10 | 6656145 | 6656145/8388608 | 0.793474316597 |
| 11 | 6489055 | 6489055/8388608 | 0.773555636406 |
| 12 | 6324785 | 6324785/8388608 | 0.753973126411 |
| 13 | 6163311 | 6163311/8388608 | 0.734723925591 |
| 14 | 6004609 | 6004609/8388608 | 0.71580517292 |
| 15 | 5848655 | 5848655/8388608 | 0.697214007378 |
| 16 | 5695425 | 5695425/8388608 | 0.67894756794 |
| 17 | 5544895 | 5544895/8388608 | 0.661002993584 |
| 18 | 5397041 | 5397041/8388608 | 0.643377423286 |
| 19 | 5251839 | 5251839/8388608 | 0.626067996025 |
| 20 | 5109265 | 5109265/8388608 | 0.609071850777 |
| 21 | 4969295 | 4969295/8388608 | 0.592386126518 |
| 22 | 4831905 | 4831905/8388608 | 0.576007962227 |
| 23 | 4697071 | 4697071/8388608 | 0.55993449688 |
| 24 | 4564769 | 4564769/8388608 | 0.544162869453 |
| 25 | 4434975 | 4434975/8388608 | 0.528690218925 |
| 26 | 4307665 | 4307665/8388608 | 0.513513684273 |
| 27 | 4182815 | 4182815/8388608 | 0.498630404472 |
| 28 | 4060401 | 4060401/8388608 | 0.484037518501 |
| 29 | 3940399 | 3940399/8388608 | 0.469732165337 |
| 30 | 3822785 | 3822785/8388608 | 0.455711483955 |
| 31 | 3707535 | 3707535/8388608 | 0.441972613335 |
| 32 | 3594625 | 3594625/8388608 | 0.428512692451 |
| 33 | 3484031 | 3484031/8388608 | 0.415328860283 |
| 34 | 3375729 | 3375729/8388608 | 0.402418255806 |
| 35 | 3269695 | 3269695/8388608 | 0.389778017998 |
| 36 | 3165905 | 3165905/8388608 | 0.377405285835 |
| 37 | 3064335 | 3064335/8388608 | 0.365297198296 |
| 38 | 2964961 | 2964961/8388608 | 0.353450894356 |
| 39 | 2867759 | 2867759/8388608 | 0.341863512993 |
| 40 | 2772705 | 2772705/8388608 | 0.330532193184 |
| 41 | 2679775 | 2679775/8388608 | 0.319454073906 |
| 42 | 2588945 | 2588945/8388608 | 0.308626294136 |
| 43 | 2500191 | 2500191/8388608 | 0.298045992851 |
| 44 | 2413489 | 2413489/8388608 | 0.287710309029 |
| 45 | 2328815 | 2328815/8388608 | 0.277616381645 |
| 46 | 2246145 | 2246145/8388608 | 0.267761349678 |
| 47 | 2165455 | 2165455/8388608 | 0.258142352104 |
| 48 | 2086721 | 2086721/8388608 | 0.248756527901 |
| 49 | 2009919 | 2009919/8388608 | 0.239601016045 |
| 50 | 1935025 | 1935025/8388608 | 0.230672955513 |
| 51 | 1862015 | 1862015/8388608 | 0.221969485283 |
| 52 | 1790865 | 1790865/8388608 | 0.213487744331 |
| 53 | 1721551 | 1721551/8388608 | 0.205224871635 |
| 54 | 1654049 | 1654049/8388608 | 0.197178006172 |
| 55 | 1588335 | 1588335/8388608 | 0.189344286919 |
| 56 | 1524385 | 1524385/8388608 | 0.181720852852 |
| 57 | 1462175 | 1462175/8388608 | 0.174304842949 |
| 58 | 1401681 | 1401681/8388608 | 0.167093396187 |
| 59 | 1342879 | 1342879/8388608 | 0.160083651543 |
| 60 | 1285745 | 1285745/8388608 | 0.153272747993 |
| 61 | 1230255 | 1230255/8388608 | 0.146657824516 |
| 62 | 1176385 | 1176385/8388608 | 0.140236020088 |
| 63 | 1124111 | 1124111/8388608 | 0.134004473686 |
| 64 | 1073409 | 1073409/8388608 | 0.127960324287 |
| 65 | 1024255 | 1024255/8388608 | 0.122100710869 |
| 66 | 976625 | 976625/8388608 | 0.116422772408 |
| 67 | 930495 | 930495/8388608 | 0.110923647881 |
| 68 | 885841 | 885841/8388608 | 0.105600476265 |
| 69 | 842639 | 842639/8388608 | 0.100450396538 |
| 70 | 800865 | 800865/8388608 | 0.0954705476761 |
| 71 | 760495 | 760495/8388608 | 0.0906580686569 |
| 72 | 721505 | 721505/8388608 | 0.0860100984573 |
| 73 | 683871 | 683871/8388608 | 0.0815237760544 |
| 74 | 647569 | 647569/8388608 | 0.0771962404251 |
| 75 | 612575 | 612575/8388608 | 0.0730246305466 |
| 76 | 578865 | 578865/8388608 | 0.0690060853958 |
| 77 | 546415 | 546415/8388608 | 0.0651377439499 |
| 78 | 515201 | 515201/8388608 | 0.0614167451859 |
| 79 | 485199 | 485199/8388608 | 0.0578402280807 |
| 80 | 456385 | 456385/8388608 | 0.0544053316116 |
| 81 | 428735 | 428735/8388608 | 0.0511091947556 |
| 82 | 402225 | 402225/8388608 | 0.0479489564896 |
| 83 | 376831 | 376831/8388608 | 0.0449217557907 |
| 84 | 352529 | 352529/8388608 | 0.042024731636 |
| 85 | 329295 | 329295/8388608 | 0.0392550230026 |
| 86 | 307105 | 307105/8388608 | 0.0366097688675 |
| 87 | 285935 | 285935/8388608 | 0.0340861082077 |
| 88 | 265761 | 265761/8388608 | 0.0316811800003 |
| 89 | 246559 | 246559/8388608 | 0.0293921232224 |
| 90 | 228305 | 228305/8388608 | 0.0272160768509 |
| 91 | 210975 | 210975/8388608 | 0.025150179863 |
| 92 | 194545 | 194545/8388608 | 0.0231915712357 |
| 93 | 178991 | 178991/8388608 | 0.021337389946 |
| 94 | 164289 | 164289/8388608 | 0.019584774971 |
| 95 | 150415 | 150415/8388608 | 0.0179308652878 |
| 96 | 137345 | 137345/8388608 | 0.0163727998734 |
| 97 | 125055 | 125055/8388608 | 0.0149077177048 |
| 98 | 113521 | 113521/8388608 | 0.0135327577591 |
| 99 | 102719 | 102719/8388608 | 0.0122450590134 |
| 100 | 92625 | 92625/8388608 | 0.0110417604446 |
| 101 | 83215 | 83215/8388608 | 0.00992000102997 |
| 102 | 74465 | 74465/8388608 | 0.0088769197464 |
| 103 | 66351 | 66351/8388608 | 0.00790965557098 |
| 104 | 58849 | 58849/8388608 | 0.00701534748077 |
| 105 | 51935 | 51935/8388608 | 0.00619113445282 |
| 106 | 45585 | 45585/8388608 | 0.00543415546417 |
| 107 | 39775 | 39775/8388608 | 0.00474154949188 |
| 108 | 34481 | 34481/8388608 | 0.004110455513 |
| 109 | 29679 | 29679/8388608 | 0.00353801250458 |
| 110 | 25345 | 25345/8388608 | 0.00302135944366 |
| 111 | 21455 | 21455/8388608 | 0.00255763530731 |
| 112 | 17985 | 17985/8388608 | 0.00214397907257 |
| 113 | 14911 | 14911/8388608 | 0.00177752971649 |
| 114 | 12209 | 12209/8388608 | 0.00145542621613 |
| 115 | 9855 | 9855/8388608 | 0.00117480754852 |
| 116 | 7825 | 7825/8388608 | 0.000932812690735 |
| 117 | 6095 | 6095/8388608 | 0.000726580619812 |
| 118 | 4641 | 4641/8388608 | 0.000553250312805 |
| 119 | 3439 | 3439/8388608 | 0.000409960746765 |
| 120 | 2465 | 2465/8388608 | 0.000293850898743 |
| 121 | 1695 | 1695/8388608 | 0.000202059745789 |
| 122 | 1105 | 1105/8388608 | 0.000131726264954 |
| 123 | 671 | 671/8388608 | 7.99894332886e-05 |
| 124 | 369 | 369/8388608 | 4.39882278442e-05 |
| 125 | 175 | 175/8388608 | 2.08616256714e-05 |
| 126 | 65 | 65/8388608 | 7.7486038208e-06 |
| 127 | 15 | 15/8388608 | 1.78813934326e-06 |
| 128 | 1 | 1/8388608 | 1.19209289551e-07 |
