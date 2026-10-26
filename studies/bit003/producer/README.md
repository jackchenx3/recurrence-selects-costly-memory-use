BIT003 producer/analyzer candidate v2 — SOURCE ONLY

This package authors brs3_produce, brs3_analyze, brs3_fixture and the role-owned
brs3_fixture_support. It authors no independent auditor. No source acceptance,
compilation, program execution, fixture comparison, timing or result is claimed.
Do not invoke build.sh or any generated executable before genuine nonauthor
SOURCE GO on the exact frozen package. All later execution is FRCE Slurm only.

Intended build inside the authorized FRCE allocation, using absolute paths:

    bash /ABS/candidate-source-v2/build.sh /ABS/NEW_BUILD FROZEN_SOURCE_PACKAGE_SHA256

The second argument is SHA256 of the exact canonical SOURCE_INVENTORY.json bytes.
The inventory excludes only itself and lists every other file, including both
diffs. build.sh verifies that identity, canonical JSON, all listed bytes/hashes
and exact file closure before compiling. Its explicit /usr/bin/python3 build-time
verifier uses standard library only; it never imports candidate code. Build from
the complete frozen package on an isolated review/build staging path; deploy
only the answer-free runtime subset after building. build.sh compiles the new main with a distinct fixed command macro;
no old main or old executable is selected. It does not execute project programs.
Record build exit/stderr and the exact compiler, Boost headers, libraries,
loader, CPU class and filesystem. See DEPENDENCIES.txt. A missing dependency
fails; there is no installer or interpreter fallback.

Runtime staging is mandatory before invocation. ROOT authenticates all 210
AUTHORITATIVE_BUNDLE files (including expectations) on its coordinator against
the accepted external G1F manifest. ROOT separately prepares exactly:

* SCIENCE_VIEW: PROTOCOL.txt, ENGINEERING_CONTRACT.txt, KEYS.tsv, KAT.txt,
  PUBLIC_INPUT_INDEX.json. No subdirectory or sixth file.
* FIXTURE_VIEW: those five, BUNDLE_INVENTORY.json, FIXTURE_SPEC.txt,
  LEGACY_KEYS.tsv, INTERVAL_CERTIFICATE.json, FIXIN_BUNDLE/INDEX.json, its 96
  inputpath files and FIXIN_BUNDLE/literals/SYNTHETIC_RECORD.bin. Exactly107files.

All files must be readable regular readonly files, with no symlinks or symlink
path components. Runtime sources authenticate only these projections. An absent
delegated file is never probed. No expected tree or coordinator original bundle
is deployed. ROOT closes answer handles, sanitizes inherited FDs/environment and
uses an answer-free cwd. Deploy the complete runtime src/ tree, build.sh, dependency/license notices,
build artifacts and the appropriate view. Keep review diff, author reports and
private_expectations on the coordinator. Do not deploy inputs/ or the baseline
author bundle.
No process gets an expected-output location or a full-bundle fallback path.
The program neither reads environment variables nor traverses cwd for inputs.

Intended commands, only after their respective gates:

    /ABS/NEW_BUILD/brs3_produce --inputs /ABS/SCIENCE_VIEW --tier fixture-r1 --output /ABS/NEW_FIXTURE_DATA --threads 32
    /ABS/NEW_BUILD/brs3_analyze --inputs /ABS/SCIENCE_VIEW --tier fixture-r1 --data /ABS/NEW_FIXTURE_DATA --results /ABS/NEW_ANALYZER_RESULTS --threads 32
    /ABS/NEW_BUILD/brs3_fixture --inputs /ABS/FIXTURE_VIEW --case /ABS/FIXTURE_VIEW/FIXIN_BUNDLE/in/OP_HANDTRACE_LOCAL.json --output /ABS/NEW_FIXOUT.json

ROOT runs all96 authenticated cases, preserving actual exits, output bytes,
stderr and hashes. Expected comparison occurs solely on the coordinator after
retrieval, with no expected metadata used to choose the program's output.
Every destination must be new and its parent already exist. Scientific stdout
is empty; stderr never includes endpoint totals. The analyzer derives all
follow-up sums from stored states, validates new-key targets and initialization,
and performs no turnover replay. Its validation is not independent assurance.
Full independent replay by the separately frozen auditor remains mandatory.

Own staging checks and later full fixtures:

    /ABS/NEW_BUILD/brs3_fixture_support --mode invariance --inputs /ABS/SCIENCE_VIEW --threads 1 --output /ABS/NEW_OWN_CHECK_01.json

Repeat this fixture-only interface at every threads value1..32 after SOURCE GO,
using fresh output files. Compare fixture_records_sha256 across all reports;
the thread field deliberately records the invocation. Each run computes all16
fixture records in forward and reversed arm/completion order. No production
keys or outcomes enter this check. It also exercises exact classification
equality. These are producer-owned self-checks, not independence.

Run the separate opaque emission fixture once after exact SOURCE GO, on FRCE:

    /ABS/NEW_BUILD/brs3_fixture_support --mode packing --inputs /ABS/FIXTURE_VIEW --output /ABS/NEW_PACKING_REPORT.json

This mode does no science or RNG computation. It supplies unmodified authenticated
SYNTHETIC_RECORD.bin bytes as opaque completions at every production chunk index,
in forward and reversed chunk order. Both use ChunkEmission from the actual
producer, including chunk bytes, actual atomic file publication and manifest/seal
publication. Exact closure, each filename and every emitted byte are compared;
independently read inventories and their SHA256 must agree. The blob is opaque,
so these trees are fixture evidence, never scientific DATA. The helper retains
NEW_PACKING_REPORT.json.packing-evidence/{forward,reverse}; approximately
8,328,665,600 chunk bytes plus manifests require reserved fixture storage.
It records no timing and supplies no scientific benchmark. Its report has actual
source/binary and emitted-inventory hashes, emitted only upon success.

Freeze own full fixture DATA, analyzer results, all96 FIXOUTs, sources and final
binaries before permitted cross-access. Keep private expectations in this role.

Post-G4 construction, from pristine agreed fixture DATA each time:

    /ABS/NEW_BUILD/brs3_fixture_support --mode corrupt --inputs /ABS/SCIENCE_VIEW --data /ABS/AGREED_FIXTURE_DATA --recipe bitflip --output /ABS/NEW_CORRUPTION_DATA

Supported recipe names exactly match all16 ids in POST_G4_REQUIREMENTS.json.
The strong recipe changes S256.x0 and recomputes BOTH full follow-up branches,
all four sums, record/file/manifest hashes. Earlier history and retries remain
unchanged. It provides a coherent forgery construction, not a rejection verdict.
ROOT invokes the independently authored auditor after its G4a freeze and records
actual rejection exits/codes, absent valid outputs and all failures. Additional
filesystem/symlink/output-collision tests remain ROOT runtime responsibilities.
No helper is invoked by a scientific executable.

After independent correctness/readiness gates, use benchmark-r1 exactly once
for the complete64-block producer/analyzer/full-independent-audit/IO/hash/seal
workflow, with identical frozen CPU class, flags and binaries. The whole bound
is23.04seconds, projection625, with no tuning/repeat after failure. No benchmark
endpoint totals may be displayed. This package supplies no scheduler submission
controller and makes no speed claim. ROOT owns the single job's all-step status,
legitimate cgroup/native memory accounting and original-job seal/agreement.

Only with G7 exact authority may the same science commands use production-r1
and new paths in the one original32CPU/one-node/8h allocation. Initial request
96G is not a hard cap. Outer ceiling700,000,000,000bytes and16GiB whole output
apply. No resume/retry/replacement or postproduction program rerun. Partial
output stays preserved, without a complete inventory seal. Numerical result
files remain quarantined until independent execution acceptance.

V2 review repairs and historical evidence
CORRECTION_V2_REPORT.txt is the current source-only correction account.
ENGINEERING_REPORT.txt is retained byte-for-byte as the historical V1 author
report; its implementation assertions predate the independent FIX findings.
docs/V1_SOURCE_REVIEW.txt preserves that full authenticated FIX review, including
location references that are data only. All five material findings are addressed
in V2 source, subject to new independent review and later runtime verification.
Both diffs exclude generated diff files and SOURCE_INVENTORY.json to break
self-reference; their complete comparison scopes are documented in the report.
This package grants no SOURCE GO or runtime/science acceptance.
