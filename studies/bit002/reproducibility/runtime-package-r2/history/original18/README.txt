BIT002 runtime-package-r1 — V2 INTEGRATED SOURCE CANDIDATE

CURRENT STATUS: V2 source completion only; no independent GO or execution.
SOURCE_AUTHOR_REPORT.txt is the unchanged historical v1 report. Its unknown-CLI,
pending-metadata and interpreter statements are superseded here and in
V2_COMPLETION_REPORT.txt; they are not current blockers.

The two plain Slurm batch scripts implement the accepted v0.4 job sequence.
Nothing in this delivery has been executed, imported, syntax checked, built,
fixture tested, benchmarked, submitted, or scientifically run. Independent source,
engineering, resource, and readiness reviews are still required. This package
neither authorizes a submission nor supplies G5, G6, G7, or G8 acceptance.

Deployment identity

STUDY is exactly:
/mnt/ccrsf-static/Analysis/chenx3/organized-variation-transfer/phase3_bit_retrieval_002_20261024

Root deploys the reviewed exact files to STUDY/runtime-package-r1. That directory
contains benchmark.sh, production.sh, runtime_helper.py, the unchanged
PROTOCOL_V0_4_ACCEPTED.txt, and Root's immutable NODE_TOOLCHAIN_FREEZE.txt and
NODE_TOOLCHAIN_FREEZE.json. There is one configuration path; there are no runtime
arguments, alternate configurations, fallback jobs, rebuilds, or alternate tiers.
Python is the existing /home/chenx3/miniconda3/bin/python3.11, version 3.11.4,
SHA-256 90ebd62efa611953ec39804a827a8df12e743b70b0a7435536bb053e0e1a6ae5.
It is pinned in both scripts, helper and freeze (standard library only); native
hash verification is /usr/bin/sha256sum. Bash, chmod and ordinary Linux /proc,
POSIX rename/fsync, Slurm cgroups, and the shared filesystem are required.
Dependency existence/compatibility is NOT asserted by this source delivery.

benchmark.sh requests norm/x6342, one node/task, 32 CPUs, 64 GiB, 10 minutes.
production.sh requests the identical class/CPU/memory allocation, 8 hours.
Both disable requeue and contain no sbatch, srun, arrays, retry, resume, or new
allocation command. Scheduler stdout/stderr go to /dev/null; bounded evidence
is written under each run's logs/. Root must preserve native accounting even
if early preflight, node loss, timeout, or termination prevents a receipt.

Root records each separate exact one-submission intent BEFORE its sbatch call.
The benchmark requires accepted source/engineering/resource authorization first.
Science additionally requires the binding G5 receipt, independent G6 GO, and
Root's exact-hash G7 authority. None is supplied here. No submission commands are
included in this source-only handoff. Uncertain submission status is inspected
through original native scheduler records; it never authorizes resubmission.

The fixed run paths are STUDY/benchmark-r1 and STUDY/scientific-r1. Preparation
checks identities first, then exclusively commits STUDY/.benchmark-r1.once or
STUDY/.scientific-r1.once and creates the new run parent. Existing runs or markers
cause failure. Markers and partial directories must never be removed to retry.
The protocol's scientific TIER production-r1 is distinct from its specified run
DIRECTORY scientific-r1; there is no production-r1 run directory in this package.

P1 directory mapping

Only results/, audit/, agreement/, and logs/ are precreated. data/ remains absent
until the accepted producer creates its NEW empty output directory and starts
its atomic commits. The new run parent and exclusively owned absent child give
the protocol's empty-data starting condition without violating the actual
producer CLI. There is no producer modification or mkdir of data/ by the script.

Fixed binary paths and SHA-256 (also hardcoded and checked in the helper)

producer: STUDY/build-g2-r1/producer/bin/brc2_produce
bc597d652bfab0f05cab7e9ca52e53ea7ddc365e304a7fe4588d7386fc4031f0
analyzer: STUDY/build-g2-r1/producer/bin/brc2_analyze
0af66a01615414129be30df2d75df9efbfa724660d4c1701b9799982ea367721
auditor: STUDY/auditor-v3-own-r1/auditor/bin/brc2_audit
950ac9ffb49faaca3ec6568c8b55cdd9b8349e1f2782bcfe33c7d07fa3182eca
Protocol: STUDY/runtime-package-r1/PROTOCOL_V0_4_ACCEPTED.txt
71ccc1d065fa4868c30f96588ec34312bdc435e1047214c60b1f5643846c7daa

Authenticated operational freeze and exact interfaces

NODE_TOOLCHAIN_FREEZE.json is a complete actual freeze, not a template. Its
closed schema is NODE_TOOLCHAIN_FREEZE.schema.json. The helper enforces these
same exact keys/values directly without a third-party validator or network.
There are no configurable timing formats, runtime modes or alternate classes.
The immutable TXT copy is byte-identical to Root's accepted metadata freeze.
CPU_METADATA_AUTHENTICATION.json corrects the original combined O2 shorthand
using the native build commands: producer/analyzer O3, Wconversion, Wshadow,
pthread; auditor O2, Wpedantic; GCC 8.5 and no fast-math. Nothing is rebuilt.
Each full binary SHA-256 remains its protocol build ID; original ELF BuildIDs
are also preserved in that authentication copy. Original evidence is unchanged.

Freeze fields (all required, no others):
- schema: BIT002-NODE-TOOLCHAIN-FREEZE-V2.
- hardware_class: the single norm/short-shared-x6342 class, requested_partition
  norm, reported_partitions [norm, short], constraint x6342, exact active_features
  [u467, x6342], and Intel(R) Xeon(R) Gold 6342 CPU @ 2.80GHz.
- filesystem_path: exact STUDY, as printed above. Runtime records realpath and
  mountinfo for provenance; no unavailable mount-server literal is required.
- compiler: exact path, executable hash, GCC version line, and actual per-build
  flags from the authenticated native commands, frozen as identity metadata.
- binaries: exactly producer, analyzer, auditor, each exact path and sha256.
- python: exactly path, version, sha256 of the existing 3.11.4 interpreter.
- protocol: exact accepted package protocol path and sha256.
- package_sha256: exactly benchmark.sh, production.sh, runtime_helper.py and
  NODE_TOOLCHAIN_FREEZE.schema.json, with their final source hashes.
- evidence_sha256: exactly the adjacent immutable TXT, CPU authentication,
  PUBLIC_TIMING_CLI_CONTRACT.json and DEFAULT_PYTHON_IDENTITY_NATIVE.json hashes.
- original_metadata_receipt: exact original receipt path and hash from CPU
  authentication. This is a provenance reference to Root's preserved local
  evidence, not a fabricated remote deployment path or a new runtime input.
  The helper checks its authenticated binding, without opening that original.
- timing_source_sha256: exact accepted producer/analyzer and auditor source
  hashes from the public contract. Source implementation is never loaded.

Root deploys the delivered JSON/schema and four authority copies along with the
package source. JSON and authority copies must have all write bits removed;
this workspace delivers them mode 0444. The JSON hash and SOURCE_INVENTORY hash
must be bound externally in the exact submission intent and independent GO.

Both jobs still REQUEST norm, x6342, 32 CPUs and 64 GiB. Reported norm OR short
is accepted only after exact CPU model, native ActiveFeatures=x6342,u467,
shared norm/short node membership, filesystem PATH, compiler hash/flags and all
three unchanged binary hashes match. Every identity receipt records requested
and reported partitions separately plus the original native node description.
No other model, feature class or reported partition is accepted. Native
scontrol --oneliner show node is a read-only identity observation in P1/B1
preparation, not another allocation. Full observed ISA flag sets and mountinfo
are informational: qualified metadata did not freeze their exact literals.
SLURM_JOB_CONSTRAINTS is checked if exported, and absence is recorded honestly.
Root's native allocation/accounting and independent resource review must confirm
routing and enforcement; the norm/short mismatch is never silently waived.

Before ANY helper execution or syntax check, a genuinely fresh non-author must
return GO on the exact source inventory AND exact job/resource freeze, including
norm/short routing. No such GO is claimed here. After that GO, separately
licensed engineering checks must establish this pinned interpreter, shared
libraries and required standard modules on the representative existing class,
plus Bash, scontrol, sha256sum, fsync and cgroup behavior before the benchmark.
No installation, new CPU metadata allocation, recompile or population probe is
needed or authorized by this package. Missing runtime evidence is not a pass.

Fixed exact timing parsers

Analyzer stdout is exactly (one ASCII LF-terminated line):
  total_ns=<uint64 decimal> K_ns=<uint64 decimal> <PASS|FAIL>\n

Auditor stdout is exactly (one ASCII LF-terminated line):
  status=<VALID|INVALID> total_seconds=<decimal with 9 places> K_seconds=<decimal with 9 places>\n

The displayed escape denotes LF, not two literal backslash/n bytes. Matching is
against raw bytes: CRLF, extra lines/spaces, exponent notation, aliases, banners
and partial output fail. There is no fallback grammar. Analyzer unsigned
integers use canonical decimal as the accepted ostream emits. Values are
normalized by exact Decimal integer-nanoseconds / 1000000000; no float path.
Auditor decimals are read exactly as printed. Original stdout is retained.
Internal VALID requires analyzer PASS+exit0 or auditor VALID+exit0; FAIL+exit1
and INVALID+exit1 stay INVALID. Any status/exit inconsistency or missing stdout
fails; neither failure nor missing K is converted to success or zero.

T_an and T_aud are independent whole-process monotonic measurements. K is the
actual printed duration, never inferred from results or the total. All values
must be finite/nonnegative. Analyzer enforces K <= source wall <= whole wall
exactly in decimal nanoseconds. Auditor checks the same ordering with only a
fixed 2 ns allowance for K versus separately rendered source wall and 1 ns for
rendered source wall versus measured whole wall. A fixed9 nearest decimal
rounding contributes at most 0.5 ns per field; binary64 conversion at <=600 s
has absolute error below 0.00012 ns, so 1 ns per field is conservative. No
clock/scheduler/performance allowance is added. K <= measured whole wall is
also required strictly, keeping the projection's linear remainder nonnegative.
The exact printed K is retained, without clamping or inventing a correction;
there is no projection reduction beyond that actual serialized duration.
Allowances are recorded with each timing receipt. Whole process intervals are
integer monotonic nanoseconds and are not reconstructed from printed totals.

Benchmark sequence and exact timing partition

B1 runs producer --tier benchmark-r1 --out BENCH/data --threads 32 once.
B2 creates BENCH/iovol outside data. It mechanically copies the opaque benchmark
chunk_00.bin (2,088,064 bytes) to exactly io_00.bin through io_41.bin, and the
524,832-byte trace to trace_audit_blocks.bin. Every copy uses exclusive temp,
write, file fsync, rename, directory fsync, then SHA-256. The entire operation,
including input reads, directory setup, and all hashes, is T_ioS. It never decodes
records, numerators, or estimates. These mechanical B2 reads are necessary for
§17.3's copy; substantive benchmark data access is confined to B4–B6.
B3 uses native sha256sum on all 43 iovol files and verifies equality with B2's
hash inventory. Whole wall is T_hashS.
B4 checks exact DC inventory, canonical manifest, expected sizes/build/protocol/
tier metadata, MANIFEST.sha256, and native sha256sum -c for every listed data
file. No bookkeeping file is falsely treated as a manifest payload entry.
B4's whole wall is T_P3. No keys or endpoint values are computed in the helper.
B5/B6 call the exact analyzer/auditor with benchmark-r1 and --timing-only once,
with real --out paths in the separate results/ and audit/ parents. The binaries
perform full A–L/inference/render/hash and discard; the helper requires those
parents to remain empty. It preserves bounded status/timing stdout and stderr,
never result files from a successful benchmark. A violated discard contract
fails the benchmark and preserves evidence without rerunning.

B1, B5 and B6 wall intervals start immediately before process creation and end
after waiting/draining child pipes. Launch/wait/monitor/capture overhead inside
these intervals belongs to their respective T terms. It is conservatively
included once, not subtracted. Log fsync, step metadata serialization, binary
rechecks, fixed timing parsing, source/config hashing, directory preparation, shell
progress, final receipts and final resource sampling outside these intervals
are script work covered by the fixed unscaled 120-second allowance. B2–B4 have
one monotonic start/end each; their step-receipt writes follow that interval.
IDENTITIES records the preparation start; receipts list all measured intervals,
check nonoverlap, and record untimed elapsed through receipt start. Receipt
commit itself is outside that recorded subtotal and inside the same allowance.
No interval is assigned to two measured terms. The formula's deliberately
repeated/scaled terms are exactly the protocol's conservative overcounts.

s = 42000/1024 = 41.015625
C = s*T_prod
IO = T_ioS
V = T_hashS + s*T_P3 + s*(T_an-T_an_K) + s*(T_aud-T_aud_K)
    + T_an_K + T_aud_K + 120
Require 2*(C+IO+V) <= 14400 seconds. Decimal arithmetic uses integer-nanosecond
wall values and actual printed K seconds; no K estimate is synthesized. Each
term, scale, allowance and projection is stored in BENCH_MEASUREMENTS.json.
Missing measurements are null, never zero. All T terms and projection fields
are decimal strings, preserving exact seconds; source wall is evidence only.
That immutable mode-0400 artifact records only engineering status/timing/resource/hash metadata. G5 is NEVER
claimed from inside the allocation.

The projected production output upper bound is the sum of 87,698,688 chunk
bytes, 524,832 trace bytes, 8 KiB manifests, 1 MiB results/report/agreement,
16 MiB logs and 1 MiB receipt/resource/marker overhead, well below 1 GiB.
The benchmark's actual output, including its immutable receipt and once marker,
is measured separately and must be <=1 GiB. Production's actual hard budget is
2 GiB; its readiness projection must still be <=1 GiB. Native post-job sizes
must include all files and any preserved temporary/partial evidence.

Resource and log evidence

A child process has core dumps disabled and a 2,088,064-byte individual-file
limit (the exact largest accepted chunk). stdout and stderr are independently
captured with selectors, capped at 1 MiB each per binary invocation; exceeding
either kills that original child process group, marks failure, and preserves
the bounded prefix and byte counts. There are only three binary invocations
per job. Neither stream is echoed into the scheduler log. The accepted binaries'
public stdout contract contains only status/timing/progress/exit diagnostics;
no result bytes are requested or printed. Child content is never parsed as
scientific results. Unexpected timing stdout fails strict contract matching.
The shell's own job.log contains a finite set of fixed progress lines and safe
helper failure categories. Total logs must be <=16 MiB. sha256sum output is
bounded by the fixed 43-file/manifest inventory and an explicit 1 MiB check.

The child monitor samples output sizes and cgroup high water about every 0.1 s.
It fails/kills on output, log, or 64 GiB allocation-memory limit violations.
This is fail-closed monitoring of trusted accepted writers, not an assertion of
an atomic filesystem-wide quota: independent resource review must assess the
site's cgroup enforcement, accounting and filesystem behavior. Limits exceeded
between samples are failures, never permission to keep or interpret a result.
The fixed accepted file inventories/output sizes and bounded logs are the
normal output bound; there is no fallback disk or alternate output location.

Cgroup discovery uses /proc/self/cgroup plus /proc/self/mountinfo. A readable v2
memory.peak is preferred. A v1 memory.max_usage_in_bytes is used only on an actual
memory-controller mount. An explicitly named Slurm job ancestor is preferred;
otherwise only the process's own batch cgroup is sampled, never a shared user's
or root cgroup. Exact path/version/scope/bytes are recorded. Unavailable cgroup
peaks have available=false, bytes=null, and a reason. No current-RSS estimate or
zero is substituted. Scope is subject to independent resource review, especially
if only a batch cgroup is visible. The final in-job sample precedes the receipt
commit; native all-step accounting must cover finalization and terminal status.

Slurm MaxRSS is deliberately null in all in-job receipts. After terminal state,
Root obtains original job AND all steps' native state, exit code, elapsed, CPUs,
nodes, memory and MaxRSS, preserving native units and normalization to bytes.
For G5 Root finalizes immutable BENCH_RECEIPT.txt from the original measurements
and accounting, using max(cgroup peak, maximum original-step Slurm MaxRSS),
requiring <=32 GiB, <=600-second successful original job, <=1 GiB own/projected
output, both VALID, and the exact projection test. The binding receipt must
include all T definitions/values, s/C/IO/V/total/twice total, node/class/compiler/
filesystem/binary/protocol/config identities, measurement/accounting hashes,
all-step success, output sizes, both peak sources and the decision. A genuinely
unavailable memory/accounting source remains explicit; no missing source is
invented, treated as zero, or silently accepted as a complete max measurement.
Failure or absence of required evidence cannot trigger another benchmark.
No helper or program is rerun to finalize that receipt.

Production and terminal acceptance

P1 verifies the same frozen identities/class before preparing the exclusive
new run. P2 runs the exact producer --tier production-r1 --out RUN/data
--threads 32. P2 failure stops. P3 verifies DC as above, then shell chmod -R a-w
data/. P3/chmod failure stops. There is no aggregation of partial blocks.
P4/P5 each invoke the original analyzer/auditor once, without timing-only, with
separate --out parents; failures are captured so BOTH original calls are
attempted. Their existing outputs, including partial temp files, are sealed
0400 without printing/decoding content. Results/report/agreement combined must
remain below the protocol's 1 MiB bound.

P6 seals outputs, hashes the two results without reading their contents as text,
and atomically commits the exact §18.2 record. It has SIX physical LF lines:
BRC2-AGREEMENT-V3 plus the five fields data_committed, analyzer_exit, auditor_exit,
results_sha256_equal, auditor_status, in that order. Thus the request's '5 line'
wording is interpreted as five FIELD lines, preserving the normative header.
Native subprocess signal returns are negative integers in analyzer_exit and
auditor_exit. Auditor status is CRASH for a signal, exit >=3, or missing auditor
results/report; otherwise exit 0 yields PASS, exit 1/2 yields FAIL. Missing step
receipts use explicit failure sentinel 255, never a fabricated successful exit.
No audit report or result status/estimate is interpreted in the job helper.

Job exit 0 requires original P1–P3 success, both analyzer/auditor exits 0 and
supervisor success, required outputs present, equal result hashes, P6 committed
with PASS, and successful resource/output receipt finalization. JOB_MEASUREMENTS
records resource evidence for independent G8; it is not EA or permission to read
results. Failure writes a content-free FAILURE.txt when finalization is possible.
A timeout/OOM/node loss can prevent any final artifact: missing evidence fails
acceptance and Root preserves native records and all surviving partial files.
There are no post-job producer/analyzer/auditor calls, replacement agreements,
unsealing operations, or result-line readers in the package.

After the original job is terminal, independent G8 must perform EVERY §13.2
check: original job and all steps COMPLETED/0:0; <=28800 seconds; <=32 CPUs on
one node; both memory peaks <=64 GiB; actual total RUN <=2 GiB; independent DC
inventory and native sha256sum -c; original agreement and recomputed result
hash equality; and only then the allowed validity-line check (line 2 of analyzer
results exactly status=VALID). The source author does none of these. Results
stay sealed and uninterpreted unless EA independently holds. Rejected or
partial results yield only an uninterpreted failure/feasibility disposition.

Source inventory

SOURCE_INVENTORY.tsv records bytes and SHA-256 for all payloads and the
completion report. It excludes itself and V2_DIFF.patch to avoid circular
digests; the full diff then includes this inventory and the report. Root separately records the
inventory's SHA-256 when freezing this source review snapshot. The complete
actual NODE_TOOLCHAIN freeze and schema are delivered. See V2_COMPLETION_REPORT
for the acyclic inventory/diff hash scope and all remaining gates.

Exact review and completion handoff (ordinary artifacts, no controller)

The completion is a new operational job package under the existing cumulative
16 h readiness understanding and human authority. G4 scientific correctness
remains accepted, the implementation 120-minute phase remains closed, and no
budget, correction count, scientific population, protocol or accepted binary
is changed. This source completion consumes no benchmark/scientific submission.
Root must append actual author/reviewer time to the existing ledger, without
resetting prior rows. AUTHENTICATED_INPUTS.json records the eleven named input
identities and the seven workspace baseline hashes verified before editing.
No unprovided implementation source or original metadata result was opened.

Remaining milestones, in dependency order:
1. Fresh non-author exact-source AND job/resource GO: bind inventory, actual
   JSON/schema and immutable TXT, exact CLI/source hashes, Python, scripts and
   helper; explicitly decide the authenticated requested-norm/reported-short
   routing on the shared 6342/x6342,u467 class. This GO precedes all helper
   execution, syntax checks, engineering exercise and the benchmark. Source
   authors cannot provide it. Review may return GO or STOP; no repair chain.
2. Authorized engineering checks after that GO: exact Python executable hash,
   version and executable path, dynamic dependencies and modules datetime,
   decimal, hashlib, json, os, pathlib, re, resource, selectors, signal, stat,
   subprocess, sys and time on the representative class; Bash/Slurm/native hash
   tools, /proc/cgroup scope, filesystem atomic rename/fsync and output/log
   enforcement. Syntax/environment checks are engineering evidence, not G4
   repetition, benchmark qualification or permission to run population probes.
3. G5: one recorded benchmark intent, one original benchmark submission only,
   then the external post-terminal binding receipt below. No benchmark has
   been consumed here. A failed/missing criterion closes the route.
4. G6: independent readiness GO on all receipts, exact production package,
   native resources/routing, ledger totals and unchanged cumulative caps.
5. G7: Root exact-hash scientific authority and separate one-submission intent,
   then the single original scientific job. No scientific submission has been
   consumed here. No retry, resume, requeue or alternate configuration.
6. G8: genuine independent EA of the original job under every §13.2 condition,
   including native job/all-step success, both peak sources, independent DC,
   original agreement, recomputed result hashes and only the allowed analyzer
   validity line. No estimates before EA; no replacement artifacts or reruns.
7. G9: only after EA, scientific interpretation and the focused §20 update,
   including all required estimates/intervals/adverse findings, provenance and
   legacy failure history; then independent publication review. Failure gives
   an uninterpreted feasibility/failure report, never a scientific negative.

External BENCH_RECEIPT.txt completion contract

After the original benchmark is terminal, Root preserves the native accounting
stdout and query/collection provenance as immutable ordinary evidence. Include
job and EVERY native step, not merely the parent allocation or a selected best
row. Bind exact SHA-256 for the original BENCH_MEASUREMENTS.json, IDENTITIES,
B1-B6 step receipts, original bounded logs and native accounting. Query original
IDs only; no new invocation of this helper or any scientific program occurs.
Preserve native State, ExitCode, Elapsed/ElapsedRaw, AllocCPUS, NNodes, NodeList,
Partition, ReqMem/allocated memory, MaxRSS and units for job and all steps. Root
normalizes native memory quantities with explicitly stated unit conversion;
missing MaxRSS remains unavailable, never numeric zero. The maximum observed
required-step MaxRSS is combined with the measured cgroup high water using max,
with both sources and cgroup scope reported separately. Both sources must be
available and cover the original execution for a positive acceptance decision;
reviewer must reject inadequate scope or unresolved scheduler evidence.

The immutable receipt contains: exact job ID and one-submission intent hash;
source inventory/freeze/TXT hashes and every hardware/software identity; requested
and reported partitions; the following definition/value pairs in decimal seconds:
T_prod=B1 whole wall, T_ioS=B2 raw I/O+fsync+SHA whole wall, T_hashS=B3 whole
wall, T_P3=B4 DC whole wall, T_an=B5 independent process wall,
T_an_K=B5 printed K normalized exactly, T_aud=B6 independent process wall,
T_aud_K=B6 printed K exactly; raw source timing stdout hashes, source totals,
status and fixed rounding allowances; interval/overhead evidence;
s=42000/1024=41.015625, C, IO, V, total, twice_total and the exact displayed
formula; both peak sources/max and availability; actual benchmark output with
all partial/temp/log/receipt/marker bytes; production output upper bound;
all B1-B6 success, original parent/all-native-step states and exit codes,
original elapsed/allocation, each individual criterion and final PASS/FAIL.

Recompute the binding projection from immutable measurements without replacing
any term, using the actual printed K. Require 2*total <=14400 s; peak <=32 GiB;
projected production and actual benchmark output each <=1 GiB; analyzer
PASS+exit0 and auditor VALID+exit0; all original logical/native steps successful;
original job within 600 s, one node, 32 CPUs and requested 64 GiB. Hash and seal
the resulting receipt once, as an ordinary post-terminal G5 artifact outside
this helper. If stored inside BENCH, include its final size in the output bound.
The in-job pending G5 string is not a qualification pass. Any unavailable source
or failed criterion produces FAIL and preserves the route's original evidence.
No favorable number is inferred from committed data alone.

For production G8, preserve analogous JOB_MEASUREMENTS/native evidence and
all partial outputs, enforce <=28800 s, one node, <=32 CPUs, both peaks <=64 GiB,
RUN <=2 GiB and logs <=16 MiB. Independently validate each original P1-P6 outcome
including successful P3 chmod, without generating another agreement. An empty
or missing native field is not success. Timeout/OOM/node-loss evidence remains
a failure even if data is DC. Only the independent EA decision permits unseal.

V3 NODE MAPPING SOURCE CORRECTION — UNEXECUTED / NOT SOURCE GO

This addendum describes the current workspace candidate. Earlier V2 reports,
diff, evidence and README text are preserved historical material. The external
16-file V2 STOP snapshot remains untouched and must never be executed.
All 16 starting files matched ROOT_FROZEN_CANDIDATE.json before editing
(manifest SHA-256 4ff2648f22afe5cb45f7805801018c144ab70378742de52490cb4b8235d2fb20).

Only node_class() behavior changes: require SLURMD_NODENAME to match the ASCII
single-token grammar [A-Za-z0-9][A-Za-z0-9_.-]{0,254} (1..255 characters), query
that canonical scheduler name with the existing single scontrol command, and
compare returned NodeName to that name. Empty, whitespace, host-list, leading
option, and other non-token input is rejected before querying; no trimming,
alias, alternate query or OS-name fallback is provided. OS hostname remains
independent provenance, both in IDENTITIES.node and hardware_class.os_hostname;
hardware_class.slurmd_node_name records the canonical scheduler name. Native
NodeHostName/NodeAddr remain available in the unchanged raw scontrol receipt.
The authenticated native example is OS fsitgl-hpc131p.ncifcrf.gov, Slurm cn131,
and NodeHostName/NodeAddr fsitgl-hpc131p. These are distinct identities.

The genuine independent V2 review's nonblocking benchmark, production, DC,
resource and norm/short hardware-class findings are reused proportionately.
All class/resource/path/compiler/binary/config guards remain. Only the helper
package digest in NODE_TOOLCHAIN_FREEZE.json is resealed; schema is unchanged.
Timing, B1-B6, P1-P6, DC, memory, output and scientific logic outside node_class()
are byte-identical, as are both batch scripts and the accepted protocol.

Fresh non-author exact-candidate SOURCE GO/STOP is required before ANY helper
import/execution, syntax check, engineering exercise or runtime. The author
cannot issue that GO. Engineering, G5, G6, G7 and independent G8/EA remain
unperformed; no submission, data, results or unseal is authorized by this edit.
G4 science remains accepted; the 120-minute G4 allowance is closed; the original
cumulative 16-hour readiness budget is unchanged, with no reset or new budget.
See V3_NODE_MAPPING_REPORT.txt and V3_DIFF.patch for exact change evidence.
The V3 inventory includes all payloads and both preserved V2 artifacts plus the
V3 report, excluding itself and V3_DIFF.patch to avoid circular hashes. The V3
diff includes all changed originals and the complete new report, including the
inventory change; it excludes its own bytes. No new infrastructure is added.
