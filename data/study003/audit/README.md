# Study 003 audit records

1. `01-attempt1-failed-original-headers/`: the preserved failure record of audit attempt 1 (job 53562358). Three ORIGINAL audit files written by production encode the non-chunk `chunk_index` as `0` rather than `0xFFFFFFFF`; the auditor stopped at the header interface before any payload replay. The ORIGINAL files are unchanged and are archive members `original-production/scientific_record/audit/...`.
2. `02-normalization-derived-headers/`: the normalization receipt and the DERIVED audit-tree manifest. In each of the three files exactly bytes 48-51 changed from `00000000` to `ffffffff` (12 bytes in total); bytes 64 to end of file are identical. All other derived-tree files are hard links to the unmodified production outputs. The three DERIVED files are archive members `derived-normalized-audit/audit/...`.
3. `03-recovery-audit/`: recovery runner, C++ replay and Python verification receipts (job 53562452; PASS; zero mismatches).

| File | Bytes | ORIGINAL SHA-256 | DERIVED SHA-256 |
|---|---|---|---|
| `audit/audit_candidates.t3c` | 1744830528 | `fc18a84754a4084b53061ff1d0374117c88f7995b748384beafdc263c5856613` | `89720a9624fbb2813229dc8174b6a930f171c14661c77622296c6f2de4b2d338` |
| `audit/audit_context.t3x` | 555745344 | `1a666a93df839c8cc1ed1833b36118ab6a2a600238739db0a17fda7febf6b730` | `080f904bf1404c1cb36e63ea5abfc9b2c54f33afd1cc2643c38dea2e2d961414` |
| `audit/audit_entries.t3e` | 268435520 | `6fc6a0e4c78d8e9e945a8ad1ddbb7c46b16fea287d620ef365d89ae98865f806` | `3bd36dfc2e278a7663fa4416e0806f5cd8e70f39ece5697b27d161d7a1c841c2` |
