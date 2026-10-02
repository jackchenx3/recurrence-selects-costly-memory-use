#!/bin/bash
# INERT TEMPLATE FOR LATER REVIEW (PHASE2-PERFORMANCE-CONVERSION-002).
# Production execution is NOT authorized. This file never submits itself or any
# other job. Before any use, a reviewer with execution authority must
# (1) replace every __PLACEHOLDER__, (2) confirm the design-GO SHA-256 has been
# inserted into the frozen constants, (3) set PCONV_TEMPLATE_INERT=0 below, and
# (4) submit it once through the approved route. A failed or uncertain
# submission is never duplicated.
#
# Resource request: one allocation, 1 node, 1 task, 32 CPUs, 64 GiB, 6 hours.
# Output: a caller-supplied directory outside the source package; projected
# new output is 4,229,120,000 bytes of records (~3.94 GiB) plus small receipts
# (hard ceiling 100 GiB).
#SBATCH --job-name=pconv-r1-production
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --mem=64G
#SBATCH --time=06:00:00
#SBATCH --output=__CALLER_OUTPUT_DIR__/job_%j.stdout
#SBATCH --error=__CALLER_OUTPUT_DIR__/job_%j.stderr

set -euo pipefail

PCONV_TEMPLATE_INERT=1
if [[ "${PCONV_TEMPLATE_INERT}" != "0" ]]; then
  echo "inert template: production is not authorized; nothing was run" >&2
  exit 64
fi

OUTDIR="__CALLER_OUTPUT_DIR__"          # must exist, be outside PKG, and contain no production/ subdirectory
PKG="__REVIEWED_PACKAGE_DIR__"
SPEC="__FROZEN_SPEC_PATH__"             # SHA-256 58ee414d...9afcb895
REVIEW="__TERMINAL_REVIEW_PATH__"       # SHA-256 76b0c617...55be60f
DESIGN_GO="__DESIGN_002_GO_JSON_PATH__" # SHA-256 must equal the inserted kDesignGoSha256
SOURCE_MANIFEST="__REVIEWED_SOURCE_MANIFEST_JSON__"

cd "${PKG}"
{
  echo "job_id=${SLURM_JOB_ID:-none}"
  echo "start_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "host=$(hostname)"
  g++ --version | head -n 1
  python3 --version
} > "${OUTDIR}/job_environment.txt"
scontrol show job "${SLURM_JOB_ID}" > "${OUTDIR}/job_request.txt" || true

python3 tools/manifest.py --root "${PKG}" --verify "${SOURCE_MANIFEST}"
python3 tools/preflight.py --config config/frozen_config.json --spec "${SPEC}" --review "${REVIEW}" \
  --design-go "${DESIGN_GO}" > "${OUTDIR}/python_preflight.json"

set +e
./build/mmem_production \
  --production I_ACKNOWLEDGE_SINGLE_FROZEN_PRODUCTION_RUN_PHASE2-PERFORMANCE-CONVERSION-002-R1 \
  --config config/frozen_config.json --spec "${SPEC}" --review "${REVIEW}" --design-go "${DESIGN_GO}" \
  --out "${OUTDIR}/production" --threads "${SLURM_CPUS_PER_TASK:-32}"
STATUS=$?
set -e
echo "${STATUS}" > "${OUTDIR}/production_exit_status.txt"
echo "end_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "${OUTDIR}/job_environment.txt"
# Accounting is collected after the job by the reviewer (sacct), and the
# delivery manifest by tools/manifest.py over OUTDIR. No analysis runs here.
exit "${STATUS}"
