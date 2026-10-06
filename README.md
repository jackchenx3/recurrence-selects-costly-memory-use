# Environmental recurrence selects a costly memory-use policy across two operator bundles without a resolved one-bit population benefit

Research preprint and reproducibility package, version **1.2.0** (2026-10-02). Not externally peer reviewed.

**Author:** Jack Chen\
**Affiliation:** Frederick Sequencing and Genomics Core; Advanced Biomedical Computational Science (ABCS); Frederick National Lab for Cancer Research; National Institutes of Health

Version 1.2.0 revises the paper by integrating one accepted post-v1.1 prospective extension in an independently implemented alternative operator bundle. The package covers three accepted prospective studies:

- **Study 001**, PHASE2-MUTABLE-MEMORY-001: 22 saved estimate records (unchanged since v1.0.0).
- **Study 002**, PHASE2-PERFORMANCE-CONVERSION-002: 19 saved estimate records (unchanged since v1.1.0).
- **Study 003**, PHASE2-TORUS-MEMORY-003: 22 saved estimate records (new in v1.2.0).

## Result

In an abstract 32-individual model, lag-two environmental recurrence strongly enriched a mutation-generated allele for using a supplied one-record private cache, even though each retrieval replaced one of two fresh proposals (Study 001). Under that recurrent law, the ACTIVE-minus-SHAM population-accuracy contrast was bounded below the prespecified one-correct-bit scale with its sign unresolved, and the recurrence interaction in accuracy was unresolved. The descriptive ZERO-law accuracy contrast was adverse and is retained.

Study 002 showed that directional cache information further amplified selection on the memory-use allele relative to a recurrence-gated, parent-distance-matched noninformative control, while its total population-accuracy effect through evolved policy use (`Delta_P`) was bounded below the prespecified positive one-bit scale with its sign unresolved.

Study 003 (PHASE2-TORUS-MEMORY-003) repeated the question in one independently implemented alternative operator bundle: a 32-locus, 2^16-allele torus with graded circular loss and four-entry tournament survival. Its frozen primary decision was "SUPPORTS RECURRENCE-ATTRIBUTABLE SELECTIVE ENRICHMENT IN THIS ALTERNATIVE OPERATOR BUNDLE". The saved contrasts `C_abs`, 0.282878 (interval 0.275067 to 0.290688), and `C_rec`, 0.424421 (interval 0.408801 to 0.440042), had intervals wholly above zero. The population-accuracy contrast `P_abs`, 0.005057 (interval -0.009458 to 0.019571), lay inside the prespecified ±1/32 maximal-loss band with its sign unresolved, so it was bounded below the positive 1/32 scale; `P_rec`, 0.006286 (interval -0.022743 to 0.035316), was unresolved. Values are on the saved records' own scale.

Taken together, selection on a costly supplied-cache-use policy separated from any demonstrated population-performance benefit in both operator bundles at the declared scales ("SELECTION/PERFORMANCE SEPARATION RECURS IN THIS OPERATOR BUNDLE").

## Study 003 audit history

The first Study 003 audit attempt (job 53562358) stopped before any audit-payload replay because three audit-file headers written by production encoded the non-chunk `chunk_index` as `0` instead of the schema-required `0xFFFFFFFF`. A derived audit tree changed only bytes 48-51 of those three files (12 bytes in total); bytes 64 to end of file are identical to the originals, and every other file of the derived tree is a hard link to the unmodified production output. The recovery audit (job 53562452) passed with zero mismatches. The original outputs, the failed attempt, the normalization receipt and the derived tree are all preserved. The release producer source (`code/study003/producer/`) corrects the header sentinel in one line of `src/torus/chunk_io.cpp`; that correction was neither compiled nor executed and no scientific rerun was performed. The producer source that actually generated the Study 003 outputs is preserved at `code/study003/history/original-malformed-header-source/`.

## Scope and limits

- The one-record cache architecture was supplied. No study shows that memory architecture, or memory itself, originated or evolved.
- One allele jointly controls whether a valid one-step record is retained and whether it is retrieved, so retention and retrieval are not separated.
- One HALF recurrent law, deliberately aligned to the cache delay, was tested with one fixed one-proposal opportunity cost, one mutation rate, one finite horizon and one model class per operator bundle.
- `Delta_P` is a total effect through evolved policy use, not a per-use value-of-information effect.
- The 22 Study 001, 19 Study 002 and 22 Study 003 estimate records are separately accepted families with separate familywise control. They are not pooled, and the 63 records are not independent studies or replications.
- Study 003 limits, as accepted:
  - one alternative operator bundle only; no universal replication claim
  - large-alphabet torus proposals are uniform, not continuous or gradual search
  - operator-bundle result cannot attribute effects to alphabet, graded loss, mutation/resampling, or tournament survival separately
  - supplied cache architecture; policy allele couples retention and retrieval
  - one HALF law aligned to cache delay, one ZERO law, fixed cost and finite horizon
  - no biological, equilibrium, invasion, general memory-origin or priority claim
  - the three original audit-file headers were malformed; only their four-byte chunk_index fields were normalized in a derived audit tree for verification
- All production runs were checked by separately implemented internal auditors with zero mismatches; Study 003 needed one normalization-only audit recovery, described below. These internal audits and the AI-assisted reviews are not external peer review or independent scientific replication.
- The findings make no claim about memory origin in general, biological systems, equilibrium, invasion probability, general recurrence effects or priority.

## Package

Study 001 keeps its v1.0.0 layout and Study 002 its v1.1.0 layout byte for byte. Study 003 material is under `study003/` subdirectories.

| Path | Content |
|---|---|
| `paper/` | Manuscript and supplement (Part A: Study 001; Part B: Study 002; Part C: Study 003) in Markdown and PDF |
| `figures/` | Five source-bound figures in PNG, PDF and SVG, plus `FIGURE_DATA.json` |
| `design/` | Frozen specifications for all three studies and the Study 002 execution-recovery addendum |
| `code/producer/`, `code/auditor/` | Study 001 producer/analyzer and separately implemented auditor source |
| `code/study002/` | Study 002 producer/analyzer and separately implemented auditor source |
| `code/study003/producer/` | Study 003 release producer/analyzer source with the one-line audit-header correction |
| `code/study003/history/original-malformed-header-source/` | The accepted Study 003 producer source that generated the outputs, unchanged |
| `code/study003/auditor/` | Study 003 independently written auditor source |
| `data/analysis/`, `data/audit/`, `data/acceptance/`, `data/compact-production/` | Study 001 records |
| `data/study002/` | Study 002 records, compact production records and prospective records |
| `data/study003/analysis/` | All 22 Study 003 saved estimate records (JSON and binary), decisions and analysis runner receipt |
| `data/study003/acceptance/` | Study 003 acceptance record and post-audit source-patch receipt |
| `data/study003/production-authentication/` | Outcome-blind authentication of the original production outputs, run manifest, completion marker and runner receipt with its embedded output inventory |
| `data/study003/audit/` | `01-` failed attempt 1 (original headers), `02-` normalization receipt and derived-tree manifest, `03-` recovery audit receipts |
| `data/study003/compact-production/` | Selected compact production records copied byte for byte from the original outputs |
| `data/study003/prospective-records/` | Design GO, Random123 provenance and source/build/fixture/review records of the independent implementation |
| `literature/` | Bounded literature-search archive from v1.0.0; not a systematic or exhaustive review |
| `provenance/` | Claim bindings, quoted statistics, the v1.2 final seal, source authentication, build receipt, visual QA, formatting-only build failures and patch receipts, the 2,998-check statistics report, QWEN-037 transport failure, QWEN-037R MANUSCRIPT_GO review, receipt and acceptance, Study 003 records, the v1.1 records, and public-export records |
| `provenance/history/v1.0.0/`, `provenance/history/v1.1.0/` | Release records superseded in later versions, byte for byte |
| `docs/` | AI-assistance and data-availability statements |

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
  --s3-estimates data/study003/analysis/estimates.json \
  --s3-decisions data/study003/analysis/decisions.json \
  --s3-accepted data/study003/acceptance/PHASE2_STUDY_003_ACCEPTED.json \
  --s3-production-authentication data/study003/production-authentication/TORUS_003_PRODUCTION_OUTPUT_AUTHENTICATION.json \
  --s3-audit-failure data/study003/audit/01-attempt1-failed-original-headers/TORUS_003_AUDIT_ATTEMPT1_FAILURE.json \
  --s3-normalization-receipt data/study003/audit/02-normalization-derived-headers/TORUS_003_AUDIT_NORMALIZATION_RECEIPT.json \
  --s3-recovery-runner-receipt data/study003/audit/03-recovery-audit/TORUS_003_AUDIT_RECOVERY_RUNNER_RECEIPT.json \
  --s3-cpp-replay-receipt data/study003/audit/03-recovery-audit/cpp_replay_receipt.json \
  --s3-source-patch-receipt data/study003/acceptance/TORUS_003_POST_AUDIT_SOURCE_PATCH_RECEIPT.json \
  --public-transformations provenance/PUBLIC_EXPORT_TRANSFORMATIONS.json \
  --draft-root . --report _rebuilt/STATISTICS_CHECK.json

python3 scripts/make_figures.py \
  --estimates data/analysis/estimates_22.json \
  --decision data/analysis/decision.json \
  --s2-estimates data/study002/analysis/estimates_19.json \
  --s2-decision data/study002/analysis/decision.json \
  --s3-estimates data/study003/analysis/estimates.json \
  --s3-decisions data/study003/analysis/decisions.json \
  --output-dir _rebuilt/figures

python3 scripts/build_pdfs.py \
  --figures-dir _rebuilt/figures --output-dir _rebuilt/paper
```

The checker covers all three accepted studies. The accepted v1.2.0 build passed all 2,998 checks with SHA-256 enforcement (`provenance/MANUSCRIPT_V1_2_STATISTICS_CHECK_2998.json`). Public receipts may differ from the authenticated receipts only by recorded administrative path normalization. `--public-transformations` maps the exact SHA-256 of each such public receipt back to its authenticated source SHA-256. It changes no arithmetic or decision check, and any other byte difference still fails.

The accepted release build used Python 3, matplotlib 3.9.4 and reportlab 5.0.1 (`requirements.txt`). The producers and auditors require a C++17 compiler. Rerunning the full analyzers or replay audits from raw records requires the separate complete-outcomes archives, `phase2-mutable-memory-complete-outcomes-v1.0.0.tar.zst`, `phase2-performance-conversion-002-complete-outcomes-v1.1.0.tar.zst` and `phase2-torus-memory-003-complete-outcomes-v1.2.0.tar.zst`. The Study 003 archive may be published as numbered parts (`phase2-torus-memory-003-complete-outcomes-v1.2.0.tar.zst.part-000`, ...) because of the release-asset size limit; its archive receipt gives the full-stream SHA-256, the ordered part hashes and the reconstruction command.

## Availability and citation

Repository: https://github.com/jackchenx3/recurrence-selects-costly-memory-use\
Archival record: Zenodo archival publication pending. No DOI is cited for this version until the archival record is published.

The complete-outcomes archives are separate release assets and are not stored in this Git repository. See `CITATION.cff`. Internal AI-assisted checks are documented in `docs/AI_ASSISTANCE.md`. They are not external peer review or independent scientific replication.

## Licenses

Manuscript, figures and data: [CC BY 4.0](LICENSE-CONTENT.md). Original code: [MIT](LICENSE-CODE). Third-party dependencies (including the Random123 reference) and cited works retain their own terms.
