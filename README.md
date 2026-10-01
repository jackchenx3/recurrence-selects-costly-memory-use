# Environmental recurrence selects a costly memory-use allele without a resolved one-bit population benefit

Research preprint and reproducibility package, version **1.0.0**. Not externally peer reviewed.

**Author:** Jack Chen  
**Affiliation:** Frederick Sequencing and Genomics Core; Advanced Biomedical Computational Science (ABCS); Frederick National Lab for Cancer Research; National Institutes of Health

## Result

In an abstract 32-individual model, lag-two environmental recurrence strongly enriched a mutation-generated allele for using a supplied one-record private cache, even though each retrieval replaced one of two fresh proposals. The recurrent ACTIVE-minus-SHAM population-accuracy contrast was bounded below the prespecified one-correct-bit scale with its sign unresolved, and the recurrence interaction in accuracy was unresolved. The result separates selection on a policy allele from demonstrated population performance.

This study does not show that memory architecture evolved. The allele jointly controls whether a valid one-step record is retained and whether it is retrieved, so those mechanisms are not separated. HALF was deliberately aligned to the cache delay. The findings do not establish equilibrium, invasion probability, general recurrence effects, biological effects, or a priority claim.

## Package

- `paper/`: manuscript and supplement in Markdown and PDF.
- `figures/`: three source-bound figures in PNG, PDF and SVG plus plotted data.
- `design/`: frozen prospective specification.
- `code/producer/`: authenticated C++17 producer and Python analyzer source.
- `code/auditor/`: separately implemented C++17 replay and Python record-verification source.
- `data/analysis/`: all 22 saved estimate records, decision and descriptive diagnostics.
- `data/compact-production/`: all 41,600 block records and 332,800 path records, plus preflight and manifest metadata. Per-update and candidate-level audit records are in the separate complete-outcomes archive deposited with the preprint.
- `literature/`: bounded OpenAlex and Europe PMC search archive. It is not a systematic or exhaustive review.
- `provenance/`: claim bindings, statistic checks, visual QA and public-export transformations.

## Reproduce the saved statistics and figures

```bash
python3 scripts/check_statistics.py \
  --estimates data/analysis/estimates_22.json \
  --decision data/analysis/decision.json \
  --diagnostics data/analysis/diagnostics_descriptive.json \
  --accepted data/acceptance/PHASE2_STUDY_001_ACCEPTED.json \
  --replay-receipt data/audit/production_replay_receipt.json \
  --records-receipt data/audit/production_records_receipt.json \
  --public-transformations provenance/PUBLIC_EXPORT_TRANSFORMATIONS.json \
  --draft-root . --report _rebuilt/STATISTICS_CHECK.json

python3 scripts/make_figures.py \
  --estimates data/analysis/estimates_22.json \
  --decision data/analysis/decision.json \
  --output-dir _rebuilt/figures

python3 scripts/build_pdfs.py \
  --figures-dir _rebuilt/figures --output-dir _rebuilt/paper
```

The accepted release build used Python 3.9.6, matplotlib 3.9.4 and reportlab 5.0.1. The producer and auditor require a C++17 compiler. The separate complete-outcomes archive is needed to rerun the full analyzer and replay audit from raw per-update and candidate-level records.

## Availability and citation

Repository: https://github.com/jackchenx3/recurrence-selects-costly-memory-use  
The version DOI and complete-outcomes archive are listed on the GitHub release page and in the archival record.

See `CITATION.cff`. Internal AI-assisted checks are documented in `docs/AI_ASSISTANCE.md`; they are not external peer review or independent scientific replication.

## Licenses

Manuscript, figures and data: [CC BY 4.0](LICENSE-CONTENT.md). Original code: [MIT](LICENSE-CODE). Third-party dependencies and cited works retain their own terms.
