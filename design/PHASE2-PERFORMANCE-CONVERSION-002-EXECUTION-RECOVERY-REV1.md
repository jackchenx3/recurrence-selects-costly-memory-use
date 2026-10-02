# PHASE2-PERFORMANCE-CONVERSION-002 execution-recovery addendum, revision 1

Issued prospectively at 2026-10-02T06:29:59.265954+00:00.

## Preserved failure

Scheduler job 53552434 is the sole attempt under the original execution authority. It built and passed preflight, then `mmem_production` refused at argument validation because the runner omitted the required `--design-go` option. Exit status was 64. The authenticated runner receipt records that `production_output_dir_present` is false. No model update, random draw, path record, estimate or scientific outcome was generated or inspected. Attempt 1 and every log remain immutable.

## Scientific scope

The frozen scientific question, parameters, primary, 41,600 blocks, six cells, 249,600 paths, 19 estimates, namespace `PHASE2-PERFORMANCE-CONVERSION-002|production-r1|<purpose>`, pairing and audit plan are unchanged. Because argument validation occurred before any cohort or scientific output existed, the originally frozen cohort remains prospectively unobserved. This addendum cannot be used if any scientific record from attempt 1 is later found.

## One bounded recovery allocation

One separately authorized recovery job may use `production-stage/performance002_attempt2`. Its runner must authenticate this addendum and the accepted attempt-1 failure receipt, preserve attempt 1, and differ scientifically from the accepted runner only by:

1. using the new exclusive attempt-2 output root;
2. passing `--design-go DESIGN_002_GO.json` to the sole `mmem_production` invocation and recording it in the exact argv; and
3. authenticating the recovery construction authority plus the two recovery records.

The same 32 CPUs, 64 GiB, six-hour limit and 100 GiB ceiling apply. No pilot, preview, parameter change, new law, cohort top-up, analysis, auditor or regeneration is allowed in the production job. A second recovery submission is forbidden. Any failure or uncertainty in the recovery job closes this execution route pending a genuinely new prospective design.

An independent non-implementing reviewer must return `RECOVERY_GO` on the exact recovery authority and runner snapshot before a new execution authority may be issued.
