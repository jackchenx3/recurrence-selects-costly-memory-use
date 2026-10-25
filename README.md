# Environmental recurrence selects a costly memory-use policy: long-run population effects and immediate retrieval effects

Research preprint, version **1.3.0**, October 25, 2026. Not externally peer reviewed.

**Author:** Jack Chen

## Read the paper

- [Manuscript PDF](paper/MANUSCRIPT.pdf) and [editable source](paper/MANUSCRIPT.md).
- [Supplement PDF](paper/SUPPLEMENT.pdf) and [editable source](paper/SUPPLEMENT.md).
- [Versioned release and checksummed assets](https://github.com/jackchenx3/recurrence-selects-costly-memory-use/releases/tag/v1.3.0).
- [Remaining scientific questions](docs/BIT002_REMAINING_SCIENTIFIC_GAPS.txt).

## What this revision reports

Studies 001-003 retain their accepted long-run results: recurrence enriched the costly memory-use policy, directional cache information amplified selection in the first bundle, and the selection/performance separation recurred in one independently implemented alternative bundle. Population-performance contrasts remained bounded below the declared positive scale or unresolved; adverse descriptive findings remain reported.

BIT002 adds a separately frozen identical-state, paired-tape, one-turnover retrieval experiment with 42,000 new BIT/PL histories. At recurrent targets, lineage share and a hashed focal family's unconditional population contribution were materially positive. Immediate normalized accuracy was positive with unresolved magnitude. All three innovation effects were bounded within their prespecified margins, with negative point estimates and intervals containing zero; this does not establish absence. The manuscript reports all six effects, all prespecified transforms, full eligibility histograms and provenance limits.

The long-run and immediate estimands differ. No long-run mediation, effect of changing recurrence, cross-bundle immediate retrieval replication, per-carrier value, general mechanism, biological memory or completion of the original broader Phase 3 protocol is established. Statistical families and cohorts are separate and not pooled. Internal computational reviews are not journal peer review.

## Reproducibility

The original Studies 001-003 code, configurations and compact data remain in code/, design/, data/ and their provenance directories. Earlier complete archives remain available from [release v1.2.0](https://github.com/jackchenx3/recurrence-selects-costly-memory-use/releases/tag/v1.2.0) and [its Zenodo record](https://doi.org/10.5281/zenodo.23085659); that DOI does not identify this revision.

New BIT002 material is in studies/bit002/: protocol, exact accepted result tables, all accepted producer/analyzer and independent-auditor sources, 33 keys/public fixtures, runtime package, Linux binary identities and original execution/audit/gate evidence. The separate BIT002 complete reproducibility ZIP release asset additionally contains all 45 accepted original data files. Its exact SHA-256 is 1616258c046aa94eb00b8589a94896e93d87f608724c3e6ed5fc562752556003. No program was rerun to prepare this revision.

The accepted scientific execution was FRCE Slurm job 54269109. Earlier failed R1 job 54254461 remains invalid, and its partial outputs were not combined with the accepted run. Frozen definitions, endpoints and original results are unchanged. Historical local receipts may identify their original workspaces and earlier draft status; those records are provenance, not instructions or new execution authority.

Rebuild only the PDFs with `python scripts/build_pdfs.py --source-root . --figures-dir figures --output-dir paper` in a ReportLab environment. Archived scientific commands do not authorize overwriting frozen namespaces or another automatic scientific attempt.

## Publication and licenses

This revision has separate independent internal manuscript-content review and final export checks. The author's name is retained without workplace affiliation or work contact in this release. Existing older public tags/records are historical versions and are not rewritten. New current-file checksums are in SHA256SUMS; author-metadata export transformations are separately recorded without changing scientific outputs.

Manuscript, figures and data: CC BY 4.0. Original code: MIT. See LICENSE-CONTENT.md and LICENSE-CODE. AI assistance is disclosed in the manuscript. No external peer-review acceptance or journal-submission status is asserted.
