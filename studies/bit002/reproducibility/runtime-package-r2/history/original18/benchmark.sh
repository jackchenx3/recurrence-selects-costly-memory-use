#!/bin/bash
#SBATCH --job-name=BIT002-benchmark-r1
#SBATCH --partition=norm
#SBATCH --constraint=x6342
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --mem=64G
#SBATCH --time=00:10:00
#SBATCH --no-requeue
#SBATCH --output=/dev/null
#SBATCH --error=/dev/null
set -euo pipefail
umask 077
readonly STUDY=/mnt/ccrsf-static/Analysis/chenx3/organized-variation-transfer/phase3_bit_retrieval_002_20261024
readonly PACKAGE="$STUDY/runtime-package-r1"
readonly RUN="$STUDY/benchmark-r1"
readonly PY=/home/chenx3/miniconda3/bin/python3.11
readonly HELPER="$PACKAGE/runtime_helper.py"
readonly PRODUCER="$STUDY/build-g2-r1/producer/bin/brc2_produce"
readonly ANALYZER="$STUDY/build-g2-r1/producer/bin/brc2_analyze"
readonly AUDITOR="$STUDY/auditor-v3-own-r1/auditor/bin/brc2_audit"
[[ $# == 0 && ${SLURM_JOB_ID:-} =~ ^[0-9]+$ ]] || exit 2
export LC_ALL=C LANG=C OMP_NUM_THREADS=32 OMP_DYNAMIC=FALSE
export OPENBLAS_NUM_THREADS=1 MKL_NUM_THREADS=1
cd "$PACKAGE"
"$PY" "$HELPER" prepare benchmark
exec > "$RUN/logs/job.log" 2>&1
finish() {
    local status=$? receipt_status
    trap - EXIT
    set +e
    "$PY" "$HELPER" receipt benchmark "$status"
    receipt_status=$?
    if (( status != 0 )); then exit "$status"; fi
    exit "$receipt_status"
}
trap finish EXIT
printf 'B1 producer start\n'
"$PY" "$HELPER" timed B1 "$PRODUCER" --tier benchmark-r1 --out "$RUN/data" --threads 32
printf 'B2 full-volume IO start\n'
"$PY" "$HELPER" iovol
printf 'B3 full-volume rehash start\n'
"$PY" "$HELPER" rehash
printf 'B4 DC start\n'
"$PY" "$HELPER" dc benchmark
printf 'B5 analyzer timing start\n'
if "$PY" "$HELPER" timed B5 "$ANALYZER" --tier benchmark-r1 --data "$RUN/data" --out "$RUN/results/RESULTS_ANALYZER.txt" --timing-only; then
    printf 'B5 exit=0\n'
else
    printf 'B5 failed; original B6 still follows\n'
fi
printf 'B6 auditor timing start\n'
if "$PY" "$HELPER" timed B6 "$AUDITOR" --tier benchmark-r1 --data "$RUN/data" --out "$RUN/audit/RESULTS_AUDITOR.txt" --timing-only; then
    printf 'B6 exit=0\n'
else
    printf 'B6 failed\n'
fi
