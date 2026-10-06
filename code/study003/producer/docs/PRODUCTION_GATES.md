# Gates before any scientific execution

Scientific execution is **not authorized**. This package is source only.
The gates below are sequential; each requires its own frozen authority.

1. **Fresh independent source review** of this package (next gate).
2. **Compile** with GCC 8.5 (`make BUILD=<dir>`), recording the compiler
   version, flags and binary SHA-256 values.
3. **Static preflight**: `tools/torus_preflight.py` and
   `tools/torus_manifest.py seed-check` / `create` for the frozen source
   manifest.
4. **Fixture authority**, then `torus_fixtures` (all 16 groups must pass).
5. **Timing** (optional, fixture-only): `torus_produce --mode timing
   --namespace timing-r1`, which requires a passing fixture receipt bound to
   the same config SHA-256 and writes only a timing receipt. It may confirm or
   tighten the request; it may not change blocks, cells, updates, operators or
   saved fields. If the study does not fit 32 CPU / 64 GiB / 12 h / 150 GiB,
   the route stops.
6. **Independent auditor** built by a different worker (not in this package).
7. **Production authorization record**, created separately by the supervisor.
   The producer refuses to run unless that file contains all of:
   - `"scientific_execution_authorized": true`
   - `"design_sha256": "0ff662dc31807b0b2cdcae371d884473ed398e2583272d94331bac8c8f9ff0b0"`
   - `"design_go_record_sha256": "e5dc485dd858f18eb9652cfe6b63f142e2d09fb6b069d640438cd8dbe63a786d"`
   - `"config_sha256": "<SHA-256 of config/torus_003_frozen_config.json>"`
   - `"namespace": "production-r1"`
   (exact `"key": value` spelling with one space after the colon).
8. **Production** via the inert template `slurm/torus_003_production.slurm.template`,
   filled and submitted only by a later, separately authorized controller.
9. **Analysis** with `tools/torus_analyze.py` on the complete set; then the
   independent audit of all 22 estimates, N1/N2 on all blocks and replay of
   blocks 0–63.

## What the producer checks before simulating any path

SHA-256 self-test; official Threefry KATs; pinned Random123 header and KAT
file hashes; exact frozen configuration (key order and value tokens) and its
SHA-256 against `--expect-config-sha256`; frozen design SHA-256; design-GO
record SHA-256 and its GO/design/review bindings; the execution authorization
record; production namespace; purpose-key distinctness; the full collision
audit; the cell-order/thread invariance check. Any failure exits with status 2
before the output directory is created.
