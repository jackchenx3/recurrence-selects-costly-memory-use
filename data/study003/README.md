# Study 003 (PHASE2-TORUS-MEMORY-003)

One independently implemented alternative operator bundle: 32-locus, 2^16-allele torus with graded circular loss and four-entry tournament survival.

- `analysis/`: the 22 saved estimate records (`estimates.json` and the binary form), `decisions.json` and the analysis runner receipt, byte-identical where the checker reads them.
- `acceptance/`: `PHASE2_STUDY_003_ACCEPTED.json` (byte-identical) and the post-audit source-patch receipt.
- `production-authentication/`: the outcome-blind authentication of the ORIGINAL production outputs (1,962 files; 4,694,355,040 bytes), the run manifest, completion marker and production runner receipt. The acceptance field `production.output_inventory_sha256` is the canonical digest of the `output_inventory` object embedded in the runner receipt (`runner_inventory_digest`), independently restated by the authentication record (`canonical_inventory_sha256`); it is not the SHA-256 of any file. Inventory paths are relative to the production attempt directory, with scientific records under `scientific_record/`.
- `audit/`: see `audit/README.md` for the ORIGINAL versus DERIVED audit data.
- `compact-production/`: selected ORIGINAL production records, byte-identical; every other production file is listed as excluded in `provenance/PUBLIC_EXPORT_TRANSFORMATIONS.json` and is in the complete-outcomes archive.
- `prospective-records/`: design GO and the independent-implementation records.

Original scientific binaries were never rewritten.
