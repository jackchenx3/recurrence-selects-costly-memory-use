# Data layout

`analysis/` contains the 22 accepted saved records and frozen decisions. `audit/` contains public path-normalized copies of the two internal audit receipts. `compact-production/` contains every block and path record but omits the much larger per-update and candidate-level audit tables. Those complete saved records are supplied as `phase2-mutable-memory-complete-outcomes-v1.0.0.tar.zst` with the Zenodo deposit.

All numerical data are abstract computational outputs under CC BY 4.0. The public-export transformation record binds each path-normalized text file to its authenticated source hash. Scientific binary records are unchanged.
