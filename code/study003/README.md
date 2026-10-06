# Study 003 source

- `producer/`: release producer/analyzer source. It differs from the source that generated the accepted outputs in exactly one line of `src/torus/chunk_io.cpp`, which now writes the schema-required non-chunk sentinel `0xFFFFFFFF` in audit-file headers. This correction was neither compiled nor executed; no scientific rerun was performed.
- `history/original-malformed-header-source/phase2_torus_memory_v1/`: the accepted producer/analyzer source that generated the Study 003 outputs, unchanged.
- `auditor/`: the independently written auditor source.

Compiled executables, logs, prompts, caches and scratch files are excluded and listed in `provenance/PUBLIC_EXPORT_TRANSFORMATIONS.json`.
