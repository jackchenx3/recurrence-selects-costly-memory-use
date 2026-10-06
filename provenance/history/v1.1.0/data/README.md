# Data layout

Unprefixed directories hold Study 001 (PHASE2-MUTABLE-MEMORY-001) exactly as released in v1.0.0. `study002/` holds Study 002 (PHASE2-PERFORMANCE-CONVERSION-002).

| Study 001 | Study 002 | Content |
|---|---|---|
| `analysis/` | `study002/analysis/` | Saved estimate records (22; 19), frozen decision and descriptive diagnostics |
| `audit/` | `study002/audit/` | The two internal audit receipts (replay and record verification) |
| `acceptance/` | `study002/acceptance/` | Acceptance record and accepted result note |
| `compact-production/` | `study002/compact-production/` | Run manifest, preflight, and every shard `blocks.bin` and `paths.bin` |
| (none) | `study002/prospective-records/` | Design GO, the failed original submission (infrastructure only), execution recovery and stage acceptances |

The compact production directories omit the much larger per-update and candidate-level records. These are in the separate complete-outcomes archives, `phase2-mutable-memory-complete-outcomes-v1.0.0.tar.zst` (Study 001) and `phase2-performance-conversion-002-complete-outcomes-v1.1.0.tar.zst` (Study 002). The archives are release assets outside this Git repository. Zenodo archival publication pending.

All numerical data are abstract computational outputs under CC BY 4.0. Scientific binary records are byte-identical to the authenticated records. Some JSON or text receipts are public copies whose administrative absolute paths were normalized. For each one, `provenance/PUBLIC_EXPORT_TRANSFORMATIONS.json` lists the authenticated source SHA-256, the public SHA-256 and every replacement. No scientific value was changed. The 41 saved estimate records are two separate prespecified families. They are not independent studies or replications.
