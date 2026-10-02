**1. Authentication of Records**

*   **Failure Record:** The supplied `PERFORMANCE_002_PRODUCTION_ATTEMPT1_FAILURE.json` confirms that Job 53552434 failed with exit status 64 (argument validation error) due to the omission of `--design-go`. Crucially, `production_output_dir_present` is `false` and `scientific_outcomes_observed` is `false`. This validates the premise that the original execution produced no scientific data and is a pure infrastructure failure prior to RNG/model execution.
*   **Recovery Addendum:** `PHASE2-PERFORMANCE-CONVERSION-002-EXECUTION-RECOVERY-REV1.md` explicitly authorizes exactly one recovery job at a new exclusive root (`performance002_attempt2`). It mandates that the runner authenticate this addendum and the attempt-1 failure, and that the only scientific delta is none (parameters unchanged). It forbids further retries.
*   **Recovery Construction Authority:** `PERFORMANCE_002_RECOVERY_RUNNER_CONSTRUCTION_AUTHORITY.json` defines the allowed delta precisely: (1) new output root, (2) add `--design-go`, (3) authenticate recovery documents. It pins the hash of the Attempt 1 failure record (`289bf3...`) and the Recovery Addendum (`db12c0...`).
*   **Source Receipt:** `PERFORMANCE_002_RECOVERY_RUNNER_SOURCE_RECEIPT.json` provides the SHA-256 of the candidate v2 runner (`52ceff...`) and confirms that no compilation or scientific execution occurred during construction.

**2. Source Delta Analysis (v1 vs. v2)**

I compared the supplied `run_production_stage.sh` (v1, baseline for logic) and the candidate v2 runner (labeled `run_production_stage.sh` in the second supply file).

*   **Delta 1: Output Root**
    *   v1: `OUT="${STAGE_PARENT}/performance002_attempt1"`
    *   v2: `OUT="${STAGE_PARENT}/performance002_attempt2"`
    *   The embedded Python in v2 also correctly updates `OUT = ROOT + "/production-stage/performance002_attempt2"`.
    *   The fixture manifest path remains `fixture-stage/performance002_attempt1/source_manifest.json` in both, which is correct as the source code is unchanged.
    *   *Verdict:* Compliant.

*   **Delta 2: `--design-go` Argument**
    *   v1 Production Command:
        ```bash
        run_stage production "${BUILD_DIR}/mmem_production" --production "${ACK}" \
            --config "${CONFIG}" --spec "${SPEC}" --review "${REVIEW}" \
            --out "${OUT}/production" --threads 32
        ```
    *   v2 Production Command:
        ```bash
        run_stage production "${BUILD_DIR}/mmem_production" --production "${ACK}" \
            --config "${CONFIG}" --spec "${SPEC}" --review "${REVIEW}" \
            --design-go "${DESIGN_GO}" --out "${OUT}/production" --threads 32
        ```
    *   The v2 runner adds `--design-go "${DESIGN_GO}"` to the sole `mmem_production` invocation.
    *   *Verdict:* Compliant. This resolves the exact cause of the attempt 1 failure.

*   **Delta 3: Recovery Authentication**
    *   v1 Authority Constants:
        ```python
        AUTH_REL = "PERFORMANCE_002_PRODUCTION_RUNNER_CONSTRUCTION_AUTHORITY.json"
        AUTH_RECORD = "PHASE2-PERFORMANCE-CONVERSION-002-PRODUCTION-RUNNER-CONSTRUCTION-AUTHORITY-1"
        AUTH_STATUS = "SOURCE_ONLY_RUNNER_CONSTRUCTION_AUTHORIZED"
        ```
    *   v2 Authority Constants:
        ```python
        AUTH_REL = "PERFORMANCE_002_RECOVERY_RUNNER_CONSTRUCTION_AUTHORITY.json"
        AUTH_RECORD = "PHASE2-PERFORMANCE-CONVERSION-002-RECOVERY-RUNNER-CONSTRUCTION-AUTHORITY-1"
        AUTH_STATUS = "SOURCE_ONLY_RECOVERY_RUNNER_CONSTRUCTION_AUTHORIZED"
        ```
    *   v2 Frozen Inputs List:
        The v2 `FROZEN` list includes two new entries not present in v1:
        1.  `("recovery_addendum", "design/PHASE2-PERFORMANCE-CONVERSION-002-EXECUTION-RECOVERY-REV1.md", 2196, "db12c0...")`
        2.  `("attempt1_failure", "PERFORMANCE_002_PRODUCTION_ATTEMPT1_FAILURE.json", 1779, "289bf3...")`
    *   These hashes match the supplied failure record and addendum files.
    *   *Verdict:* Compliant. The runner now validates the existence and integrity of the recovery authority chain.

*   **Fail-Closed & Exclusivity Checks:**
    *   The `refuse` function still checks `[[ ! -e "${OUT}" && ! -L "${OUT}" ]]`. Since `OUT` is now `attempt2`, it will not interfere with the immutable `attempt1` directory.
    *   The `mode_pre` function checks `os.path.lexists(OUT)`, ensuring the new root is empty.
    *   The script still invokes `preflight` exactly once and `production` exactly once.
    *   No scientific parameters (blocks, cells, seeds, etc.) were changed in the embedded Python constants or the script logic.

**3. Methodological Defensibility**

The original specification likely restricted execution to a single allocation to prevent multiple testing or uncontrolled resource usage. However, the authenticated failure record proves that the first allocation failed at argument validation *before* any scientific inference occurred (no RNG draws, no model updates, no output directory). Therefore, the "scientific" unit of work was never consummated. The recovery addendum is prospective and frozen before the v2 runner construction. It explicitly permits this one mechanical fix. Executing v2 does not violate prospective inference because the design is identical and the "failed" attempt generated no evidence. The recovery is a pure infrastructure correction.

**4. Terminal Verification**

The evidence supports the exact delta. The runner is fail-closed, authenticates the new recovery authority and failure records, targets a new exclusive directory, and adds the required `--design-go` flag. There are no blocking defects.

RECOVERY_GO
