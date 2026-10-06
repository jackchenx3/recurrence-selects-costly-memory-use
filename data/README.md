# Data layout

Unprefixed directories hold Study 001 (PHASE2-MUTABLE-MEMORY-001) exactly as released in v1.0.0. `study002/` holds Study 002 (PHASE2-PERFORMANCE-CONVERSION-002) exactly as released in v1.1.0. `study003/` holds Study 003 (PHASE2-TORUS-MEMORY-003), new in v1.2.0.

| Study 001 | Study 002 | Study 003 | Content |
|---|---|---|---|
| `analysis/` | `study002/analysis/` | `study003/analysis/` | Saved estimate records (22; 19; 22) and decision records |
| `audit/` | `study002/audit/` | `study003/audit/` | Internal audit receipts; for Study 003 also the failed first attempt and the normalization records |
| `acceptance/` | `study002/acceptance/` | `study003/acceptance/` | Acceptance records (Study 003 also: post-audit source-patch receipt) |
| `compact-production/` | `study002/compact-production/` | `study003/compact-production/` | Compact production records, byte-identical to the originals |
| (none) | `study002/prospective-records/` | `study003/prospective-records/` | Design GO and execution or implementation records |
| (none) | (none) | `study003/production-authentication/` | Outcome-blind authentication, manifest and runner receipt whose embedded inventory lists all 1,962 original production files |

The compact production directories omit the much larger per-update, candidate-level and audit records. These are in the separate complete-outcomes archives, `phase2-mutable-memory-complete-outcomes-v1.0.0.tar.zst`, `phase2-performance-conversion-002-complete-outcomes-v1.1.0.tar.zst` and `phase2-torus-memory-003-complete-outcomes-v1.2.0.tar.zst`, which are release assets outside this Git repository. Zenodo archival publication pending.

**Original and derived Study 003 audit data.** The three original audit files (`audit/audit_candidates.t3c`, `audit/audit_context.t3x`, `audit/audit_entries.t3e`) carry malformed `chunk_index = 0` headers and are preserved unchanged as ORIGINAL data. The normalized copies are DERIVED data used only for the recovery audit; they differ only at bytes 48-51 of each file. Both sets are in the Study 003 complete-outcomes archive (`original-production/` and `derived-normalized-audit/`); this repository holds their hashes and receipts.

All numerical data are abstract computational outputs under CC BY 4.0. Scientific binary records are byte-identical to the authenticated records. Some JSON or text receipts are public copies whose administrative absolute paths were normalized. For each one, `provenance/PUBLIC_EXPORT_TRANSFORMATIONS.json` lists the authenticated source SHA-256, the public SHA-256 and every replacement. No scientific value was changed. The 63 saved estimate records are three separate prespecified families. They are not independent studies or replications.
