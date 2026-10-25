#!/bin/bash
#SBATCH --job-name=BIT002-scientific-r1
#SBATCH --partition=norm
#SBATCH --constraint=x6342
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --mem=64G
#SBATCH --time=08:00:00
#SBATCH --no-requeue
#SBATCH --output=/dev/null
#SBATCH --error=/dev/null
set -euo pipefail
umask 077
readonly STUDY=/mnt/ccrsf-static/Analysis/chenx3/organized-variation-transfer/phase3_bit_retrieval_002_20261024
readonly PACKAGE="$STUDY/runtime-package-r1"
readonly RUN="$STUDY/scientific-r1"
readonly PY=/home/chenx3/miniconda3/bin/python3.11
readonly HELPER="$PACKAGE/runtime_helper.py"
readonly PRODUCER="$STUDY/build-g2-r1/producer/bin/brc2_produce"
readonly ANALYZER="$STUDY/build-g2-r1/producer/bin/brc2_analyze"
readonly AUDITOR="$STUDY/auditor-v3-own-r1/auditor/bin/brc2_audit"
[[ $# == 0 && ${SLURM_JOB_ID:-} =~ ^[0-9]+$ ]] || exit 2
export LC_ALL=C LANG=C OMP_NUM_THREADS=32 OMP_DYNAMIC=FALSE
export OPENBLAS_NUM_THREADS=1 MKL_NUM_THREADS=1
cd "$PACKAGE"
# P1: exact identities/resources, exclusive marker and new run parent.
"$PY" "$HELPER" prepare production
exec > "$RUN/logs/job.log" 2>&1
finish() {
    local status=$? receipt_status
    trap - EXIT
    set +e
    "$PY" "$HELPER" receipt production "$status"
    receipt_status=$?
    if (( status != 0 )); then exit "$status"; fi
    exit "$receipt_status"
}
trap finish EXIT
printf 'P2 producer start\n'
"$PY" "$HELPER" timed P2 "$PRODUCER" --tier production-r1 --out "$RUN/data" --threads 32
printf 'P3 DC start\n'
"$PY" "$HELPER" dc production
chmod -R a-w -- "$RUN/data"
printf 'P4 analyzer start\n'
if "$PY" "$HELPER" timed P4 "$ANALYZER" --tier production-r1 --data "$RUN/data" --out "$RUN/results/RESULTS_ANALYZER.txt"; then
    printf 'P4 exit=0\n'
else
    printf 'P4 failed; original P5 still follows\n'
fi
printf 'P5 auditor start\n'
if "$PY" "$HELPER" timed P5 "$AUDITOR" --tier production-r1 --data "$RUN/data" --out "$RUN/audit/RESULTS_AUDITOR.txt"; then
    printf 'P5 exit=0\n'
else
    printf 'P5 failed\n'
fi
printf 'P6 agreement start\n'
"$PY" "$HELPER" agreement
