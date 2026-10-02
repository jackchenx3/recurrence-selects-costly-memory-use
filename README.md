# Environmental recurrence and directional cache information select a costly memory-use allele without a resolved one-bit population benefit

Research preprint and reproducibility package, version **1.1.0** (2026-10-02). Not externally peer reviewed.

**Author:** Jack Chen\
**Affiliation:** Frederick Sequencing and Genomics Core; Advanced Biomedical Computational Science (ABCS); Frederick National Lab for Cancer Research; National Institutes of Health

Version 1.1.0 revises the v1.0.0 paper by integrating one accepted post-v1.0 prospective extension. The package covers two accepted prospective studies of one abstract model:

- **Study 001**, PHASE2-MUTABLE-MEMORY-001: 22 saved estimate records (unchanged from v1.0.0).
- **Study 002**, PHASE2-PERFORMANCE-CONVERSION-002: 19 saved estimate records.

## Result

In an abstract 32-individual model, lag-two environmental recurrence strongly enriched a mutation-generated allele for using a supplied one-record private cache, even though each retrieval replaced one of two fresh proposals (Study 001). Under that recurrent law, the ACTIVE-minus-SHAM population-accuracy contrast was bounded below the prespecified one-correct-bit scale with its sign unresolved, and the recurrence interaction in accuracy was unresolved. The descriptive ZERO-law accuracy contrast was adverse and is retained.

Study 002 compared directional retrieval (INFO) with a recurrence-gated, parent-distance-matched control that scrambles the direction of the retrieved displacement (NONINFO). Directional information further amplified selection on the memory-use allele: INFO minus NONINFO memory-use frequency was 18.420305 points (interval 16.968842 to 19.871767), and NONINFO itself remained 12.608275 points (interval 11.882544 to 13.334006) above the exact neutral half-frequency. The total population-accuracy effect of directional information, mediated through evolved policy use (`Delta_P`), was 0.864997 points (familywise interval -0.652132 to 2.382125). That interval includes zero and lies wholly inside the prespecified ±3.125-point (one correct bit in 32) band, so the effect is bounded below the positive one-bit scale with its sign unresolved. INFO minus SHAM accuracy, 0.938124 points (interval -0.513339 to 2.389586), and NONINFO minus SHAM accuracy, 0.073127 points (interval -1.378336 to 1.524590), were also bounded below the positive one-bit scale with unresolved signs.

Taken together, recurrence selected supplied-cache use and directional cache information further amplified that selection, while the total population-performance effects remained below the prespecified positive one-bit scale with unresolved signs. In this model, selection on a costly memory-use policy allele is separated from any demonstrated population-performance benefit.

## Scope and limits

- The one-record cache architecture was supplied. Neither study shows that memory architecture, or memory itself, originated or evolved.
- One allele jointly controls whether a valid one-step record is retained and whether it is retrieved, so retention and retrieval are not separated.
- One HALF recurrent law, deliberately aligned to the cache delay, was tested with one fixed one-proposal opportunity cost, one mutation rate, one finite horizon and one model class.
- `Delta_P` is a total effect through evolved policy use, not a per-use value-of-information effect. The NONINFO shared permutation matches displacement length and one common coordinate relabeling but not every cross-genotype relation, and zero-weight displacements retain direction.
- The 22 Study 001 and 19 Study 002 estimate records are separate prespecified families with separate familywise control. They are not pooled, and the 41 records are not independent studies or replications.
- Both production runs were checked by separately implemented internal auditors with zero mismatches. These internal audits and the AI-assisted reviews are not external peer review or independent scientific replication.
- The findings make no claim about memory origin in general, biological systems, equilibrium, invasion probability, general recurrence effects or priority.

## Package

Study 001 keeps its v1.0.0 layout byte for byte. Study 002 material is under `study002/` subdirectories.

| Path | Content |
|---|---|
| `paper/` | Manuscript and supplement (Part A: Study 001; Part B: Study 002) in Markdown and PDF |
| `figures/` | Four source-bound figures in PNG, PDF and SVG, plus `FIGURE_DATA.json` |
| `design/` | Frozen specifications for both studies and the prospective Study 002 execution-recovery addendum |
| `code/producer/`, `code/auditor/` | Study 001 authenticated producer/analyzer source and separately implemented auditor source |
| `code/study002/producer/`, `code/study002/auditor/` | Study 002 authenticated producer/analyzer source and separately implemented auditor source |
| `data/analysis/`, `data/audit/`, `data/acceptance/`, `data/compact-production/` | Study 001 estimates, decision, diagnostics, audit receipts, acceptance and compact production records |
| `data/study002/analysis/` | All 19 Study 002 saved estimate records, decision and descriptive diagnostics |
| `data/study002/audit/` | The two Study 002 internal audit receipts |
| `data/study002/acceptance/` | Study 002 acceptance record and accepted result note |
| `data/study002/compact-production/` | Run manifest, preflight and every `blocks.bin` and `paths.bin` from the 32 corrected-run shards |
| `data/study002/prospective-records/` | Design GO, the failed original submission (infrastructure only), the execution recovery and the production, analysis and audit acceptances |
| `literature/` | Bounded literature-search archive from v1.0.0; not a systematic or exhaustive review |
| `provenance/` | Claim bindings, quoted statistics, the v1.1 final seal, source authentication, build receipt, visual QA, provenance patch, the 1,975-check statistics report, the QWEN-019 MANUSCRIPT_GO claim audit and public-export records |
| `docs/` | AI-assistance and data-availability statements |

The original Study 002 production submission failed for infrastructure reasons before producing any scientific output. Its authority, submission and failure records are preserved as an infrastructure-only event. The corrected run followed the prospective execution-recovery addendum, and only the corrected run supplies Study 002 results.

## Reproduce the saved statistics and figures

```bash
python3 scripts/check_statistics.py \
  --estimates data/analysis/estimates_22.json \
  --decision data/analysis/decision.json \
  --diagnostics data/analysis/diagnostics_descriptive.json \
  --accepted data/acceptance/PHASE2_STUDY_001_ACCEPTED.json \
  --replay-receipt data/audit/production_replay_receipt.json \
  --records-receipt data/audit/production_records_receipt.json \
  --s2-estimates data/study002/analysis/estimates_19.json \
  --s2-decision data/study002/analysis/decision.json \
  --s2-diagnostics data/study002/analysis/diagnostics_descriptive.json \
  --s2-accepted data/study002/acceptance/PHASE2_STUDY_002_ACCEPTED.json \
  --s2-replay-receipt data/study002/audit/production_replay_receipt.json \
  --s2-records-receipt data/study002/audit/production_records_receipt.json \
  --public-transformations provenance/PUBLIC_EXPORT_TRANSFORMATIONS.json \
  --draft-root . --report _rebuilt/STATISTICS_CHECK.json

python3 scripts/make_figures.py \
  --estimates data/analysis/estimates_22.json \
  --decision data/analysis/decision.json \
  --s2-estimates data/study002/analysis/estimates_19.json \
  --s2-decision data/study002/analysis/decision.json \
  --output-dir _rebuilt/figures

python3 scripts/build_pdfs.py \
  --figures-dir _rebuilt/figures --output-dir _rebuilt/paper
```

The checker covers both accepted studies. The accepted v1.1.0 build passed all 1,975 checks with SHA-256 enforcement (`provenance/MANUSCRIPT_V1_1_STATISTICS_CHECK_1975.json`). Public audit receipts may differ from the authenticated receipts only by recorded administrative path normalization. `--public-transformations` maps the exact SHA-256 of each such public receipt back to its authenticated source SHA-256. It changes no arithmetic or decision check, and any other byte difference still fails.

The accepted release build used Python 3, matplotlib 3.9.4 and reportlab 5.0.1 (`requirements.txt`). The producers and auditors require a C++17 compiler. Rerunning the full analyzers or replay audits from raw per-update and candidate-level records requires the separate complete-outcomes archives, `phase2-mutable-memory-complete-outcomes-v1.0.0.tar.zst` and `phase2-performance-conversion-002-complete-outcomes-v1.1.0.tar.zst`.

## Availability and citation

Repository: https://github.com/jackchenx3/recurrence-selects-costly-memory-use\
Archival record: Zenodo archival publication pending. No DOI is cited for this version until the archival record is published.

The complete-outcomes archives are separate release assets and are not stored in this Git repository. See `CITATION.cff`. Internal AI-assisted checks are documented in `docs/AI_ASSISTANCE.md`. They are not external peer review or independent scientific replication.

## Licenses

Manuscript, figures and data: [CC BY 4.0](LICENSE-CONTENT.md). Original code: [MIT](LICENSE-CODE). Third-party dependencies and cited works retain their own terms.
