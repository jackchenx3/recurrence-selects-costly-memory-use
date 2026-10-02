# AI assistance

This project used AI systems under the direction of the author, Jack Chen. The author made the scientific decisions and is responsible for the content.

## Roles of AI assistance

AI systems assisted with:

- **design criticism:** skeptical review of the prospective specification, including a non-implementing AI review of revision 1 (terminal GO review, 2026-10-01T01:55:25Z) before production was authorized;
- **implementation:** writing producer, analyzer and audit code under the frozen specification;
- **code review:** reviewing implementation and audit code;
- **analysis checking:** checking saved records, receipts and decision logic;
- **literature organization:** organizing a structured search of open scholarly metadata and the resulting comparison; and
- **drafting:** drafting this manuscript, supplement, provenance files and support scripts.

## What AI review is not

Internal reviews performed by AI models are **not external peer review**. The separately written replay and record-verification tools are internal reproducibility audits. They are not an independent scientific replication. Producer and audit tools were both written with AI assistance from the same specification; the audits detect implementation divergence, not a shared misreading of the specification. This manuscript is a research preprint draft and has not been externally peer reviewed.

## This source package

The source files in this package (task CLAUDE-017) were drafted by Claude from the supplied records only. Claude did not run any script, statistical check, figure build, PDF build or scientific computation while constructing the package (see `CONSTRUCTION_NOT_RUN.json`). Every quoted number is registered in `provenance/QUOTED_STATISTICS.json` so that it can be checked mechanically by `scripts/check_statistics.py` against the authenticated records before release.

Claude then applied one bounded source-only correction patch (task CLAUDE-019) from the supervisor's records: the CLAUDE-018 claim and reproducibility audit, which was itself AI-assisted, the visual QA, the reference-metadata corrections and the prospective timing bindings. While patching, Claude did not run any script, build any figure or PDF, use the network or change any scientific result. `CONSTRUCTION_NOT_RUN.json` remains the unchanged CLAUDE-017 record. Funding, competing-interest and acknowledgment statements are not drafted by AI and are not asserted in this package.

For version 1.1.0 (task CLAUDE-025), Claude integrated the accepted Study 002 (PHASE2-PERFORMANCE-CONVERSION-002) into the existing manuscript, supplement, provenance files and support scripts from the supplied Study 002 records only. While doing so, Claude did not run any script, statistics check, figure build or PDF build, did not execute any scientific model, did not use the network, did not inspect unsupplied upstream paths, and did not create or remove any file; no scientific result was changed. Every new quoted number is registered in `provenance/QUOTED_STATISTICS.json` for mechanical checking against both studies' authenticated records before release. `CONSTRUCTION_NOT_RUN.json` remains the unchanged CLAUDE-017 record.

## Ethics

This is an abstract computational model. It involves no human participants, no animals and no personal data.
