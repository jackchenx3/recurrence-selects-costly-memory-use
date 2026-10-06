# Data and code availability

## Status

A public reproducibility package is being prepared. **No DOI, repository address or release date has been assigned yet.** None is claimed in the manuscript.

## Licenses for the future public package

- Manuscript text, figures and data: Creative Commons Attribution 4.0 International (CC BY 4.0).
- Original code: MIT License.

## Scope

Version 1.1.0 of the manuscript reports two prospective studies: Study 001 (PHASE2-MUTABLE-MEMORY-001; 22 saved estimate records) and Study 002 (PHASE2-PERFORMANCE-CONVERSION-002; 19 saved estimate records), a post-v1.0 prospective extension. The two record sets are separate accepted estimate sets and are released and checked separately. The Study 001 entries below are unchanged from version 1.0.

## Planned contents: Study 001

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

## Planned contents: Study 002

- Frozen specification PHASE2-PERFORMANCE-CONVERSION-002 revision 1.
- The producer extension, configuration, build environment and SHA-256 manifests; the production namespace `production-r1`, the key schema including `DECOY_PERMUTATION`, and the collision receipt.
- Per-update binary records for all 249,600 paths, per-path summaries and final-state hashes, and per-block records.
- Candidate-level audit rows and shared permutations for blocks 0-63.
- Analyzer outputs: the 19 estimate records, decision record and descriptive diagnostics.
- Receipts from the separately implemented internal replay and record-verification audits, and the runner receipts.
- The record of the failed original production submission (missing required `--design-go` argument; exit status 64; no production output, random draw, model execution or outcome) and the prospective recovery addendum that preceded the corrected run.
- The design-review record, production authorization and acceptance record.

## Authenticated outputs used by the manuscript: Study 001

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

## Authenticated outputs used by the manuscript: Study 002

| Output | Location recorded in supplied records | SHA-256 |
|---|---|---|
| 19 estimate records | `analysis-stage/performance002_attempt1/results/estimates_19.json` | `5efa213cdfeb985a20fd7bbc65fb36dd505c568670ad350b3223c2139eb0b68d` |
| Decision record | `analysis-stage/performance002_attempt1/results/decision.json` | `3f28b82d0cb40357a1068c464d1c5158c36ba8fbfe775026ce22c70113b3b84a` |
| Descriptive diagnostics | `analysis-stage/performance002_attempt1/results/diagnostics_descriptive.json` | `304843eb5d4a38761a6ce371c7cc125260041dfd910b29d5ba3cc377a6ac3553` |
| Replay receipt | `audit-stage/performance002_production_attempt1/production_replay_receipt.json` | `cbee79527876745e8918576f73a5d302377e4463d959318ca2f5feffa66416d4` |
| Record-verification receipt | `audit-stage/performance002_production_attempt1/production_records_receipt.json` | `09f5dccf4921c8d842ef60fb31aca712bbb582e87a2011c76c02efb658539197` |
| Audit runner receipt | `audit-stage/performance002_production_attempt1/AUDIT_RUNNER_RECEIPT.json` | `32e3ffff29b3de9f20158af58a67c2c130cb7456ec8c7287f67354897928a8ed` |
| Production run manifest (corrected run, job 53553084) | `production-stage/performance002_attempt2/production/run_manifest.json` | `b7aceaf6638e8f06246c8fb9b182c280a8ed98c4444d93a4a60decdbafb80136` |
| Frozen specification, revision 1 | `design/PHASE2-PERFORMANCE-CONVERSION-002-REV1.md` | `58ee414d753fab042e6cccee1a4e6b95dd7f167633b4f33c1491687d9afcb895` |
| Acceptance record (2026-10-02T07:10:51Z) | `PHASE2_STUDY_002_ACCEPTED.json` | `fc7777239cd4442c35c01d6261fd71e6fbf7e7f6fd6e11ddcc5d24876ce2f4a2` |

The following Study 002 provenance records establish prospective order and preserve the infrastructure-only failed submission. They add or modify no scientific outcome.

| Record | Location | Time (UTC) | SHA-256 |
|---|---|---|---|
| Terminal design review (`DESIGN_GO`) | `results/qwen_014/performance_design_terminal_review.md` | before design-GO record | `76b0c6172e61f60befecef183e83d9e43ea50c1b6bc1e93299668b76f55be60f` |
| Design-GO record | `DESIGN_002_GO.json` | 2026-10-01 18:59:00 | `f4a6180109e0d212cbe784a7722a64576e5e7cb0fbfd4b23c757bb5e7ab84d6a` |
| Original production execution authority | `PERFORMANCE_002_PRODUCTION_EXECUTION_AUTHORITY.json` | 2026-10-02 06:02:41 | `31c2100dd73de0090ded3059525aacd139565917086d2c19880b01e319c98992` |
| Attempt-1 failure record (job 53552434; no scientific execution) | `PERFORMANCE_002_PRODUCTION_ATTEMPT1_FAILURE.json` | 2026-10-02 06:28:45 | `289bf3188286476698136d499c12eb3a8c9e4e6fc5cde18fcb15718395f4afaa` |
| Prospective recovery addendum, revision 1 | `design/PHASE2-PERFORMANCE-CONVERSION-002-EXECUTION-RECOVERY-REV1.md` | 2026-10-02 06:29:59 | `db12c0143b6ba2c0b8cd3771ad18b9afe12feb069fe2a97d15227f4fad836e83` |
| Recovery execution authority | `PERFORMANCE_002_RECOVERY_EXECUTION_AUTHORITY.json` | 2026-10-02 06:32:55 | `c4cb2e4c162df92a761b2fff391b6e81e000055db88d7cbaeb954b0750b4e085` |

Locations are relative to the study's internal working directory and are not public addresses.

## Prospective design records: Study 001

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
