# Data and code availability

## Status

The public reproducibility package is maintained at <https://github.com/jackchenx3/recurrence-selects-costly-memory-use>. The repository release links the archival record and its complete-outcomes archive.

## Licenses for the future public package

- Manuscript text, figures and data: Creative Commons Attribution 4.0 International (CC BY 4.0).
- Original code: MIT License.

## Planned contents

- Frozen specification PHASE2-MUTABLE-MEMORY-001 revision 1, with its literature boundary.
- Prospective timing records: the terminal design-review record and receipt, the production execution authority, the production job submission and the acceptance record (hashes and times below).
- Literature-search archive: search script, API responses, deduplicated records and title screen. If the literature-review narrative is released, it will be annotated to correct its author metadata for doi:10.1098/rstb.2024.0109 (Leadbeater and Watrobska, not Leadbeater and Hollis).
- Producer source, configuration, build environment and SHA-256 manifests.
- Philox implementation and version, key schema, namespace, collision receipt and regeneration command.
- Per-update binary records for all 332,800 paths, per-path late-window summaries and final-state hashes, and per-block primary and performance variables with N1/N2 flags.
- Candidate-level audit rows for blocks 0-63.
- Analyzer outputs: the 22 estimate records, decision record and descriptive diagnostics.
- Receipts from the separately implemented internal replay and record-verification audits, and the production inventory audit.
- Job requests, accounting, logs, exit status and delivery manifests.
- This manuscript source package, including the figure, statistics-checking and PDF scripts, with build receipts that record the SHA-256 of the checked documents, figures and PDFs and the matplotlib and reportlab versions used.

## Authenticated outputs used by the manuscript

| Output | Location recorded in supplied records | SHA-256 |
|---|---|---|
| 22 estimate records | `postproduction-stage/attempt1/analysis/estimates_22.json` | `83828a49936bb8364813e2634f11cb28c4de2212b4b162d96e616ddbc7b4f8c0` |
| Decision record | `postproduction-stage/attempt1/analysis/decision.json` | `5b3aea3c18c4530b2be9f29d5c64fdd8173b6b323823bf9cc3361bd4e7010fc0` |
| Descriptive diagnostics | not recorded in supplied files | `51554620a5e302416edf8de8a073c6962abbc2c8967da80b91b83e4c3d5b685c` |
| Replay receipt | not recorded in supplied files | `9b40322d0d09b90f0a6862533c1efdc7aff21bd699a882b9b9555c5f7c4b4857` |
| Record-verification receipt | not recorded in supplied files | `afd96dc0a5b190401c035fc66c8af8fd4d4603e7185d05bff73fb05e698cff5a` |
| Production inventory audit | not recorded in supplied files | `622fa9e01a40c44ba545926e54b06e102d20b7fcf9e9c8e645ddaf9f7212e09c` |
| Production run manifest | `production-stage/attempt1/production/run_manifest.json` | `3aa50dc2b9c740341c21d996d1356e55319144a58803d1878b13ab762795d9a6` |
| Acceptance record | `PHASE2_STUDY_001_ACCEPTED.json` | `2eb3aaf5d754f4570a75522aa9e1f38358715c06c674d5a267732a1357926e6f` |

## Prospective design records

All times are UTC on 2026-10-01. The frozen design and its terminal GO review precede the sole production authorization and submission. These records establish design provenance only and add or modify no scientific outcome.

| Record | Location recorded in supplied records | Time | SHA-256 |
|---|---|---|---|
| Frozen specification, revision 1 | `design/PHASE2-MUTABLE-MEMORY-001-REV1.md` | reviewed at 01:55:25 | `c17a3a1e9ac9cf2260f6743eaf08e4c2808080a767af16d2f9e9d2b9ad3e821d` |
| Terminal design review (CLAUDE-003, non-implementing AI reviewer, GO) | `results/claude_003/terminal_design_review.md` | 01:55:25 | `787cf32a20312f2682c761243f3dec04c5eb8036d14d6efc444e9830db37960a` |
| Terminal design-review receipt | `results/claude_003/terminal_design_review.receipt.json` | 01:55:25 | `6277757f9ab3dfa611515c161776c17b561738eba6fa478ae0f8634e13c03176` |
| Production execution authority | `PRODUCTION_EXECUTION_AUTHORITY.json` | 12:53:04 | `ee3cb68bed5d212e6ea10ece542e2e90c32cb9cea07d85fceda10fc56b375b1d` |
| Production job submission (job 53530597; no automatic retry; no parameter changes) | `PRODUCTION_JOB_SUBMISSION.json` | 12:55:19 | not supplied |
| Acceptance record | `PHASE2_STUDY_001_ACCEPTED.json` | 13:33:13 | `2eb3aaf5d754f4570a75522aa9e1f38358715c06c674d5a267732a1357926e6f` |

Locations are relative to the study's internal working directory and are not public addresses.
