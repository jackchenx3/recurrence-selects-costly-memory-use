#!/bin/bash
#SBATCH --job-name=bit003-production-r1
#SBATCH --partition=norm
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --mem=96G
#SBATCH --nodelist=cn038
#SBATCH --time=08:00:00
#SBATCH --no-requeue
set -euo pipefail
: "${BIT003_DEPLOYMENT:?ROOT must bind the readonly deployment}"
: "${BIT003_AUTHORITY:?ROOT must provide separate RUN_AUTHORITY.json}"
: "${BIT003_AUTHORITY_SHA256:?ROOT must pin authenticated authority bytes}"
exec /usr/bin/python3 -I -B "${BIT003_DEPLOYMENT}/workflow.py" production "${BIT003_AUTHORITY}" "${BIT003_AUTHORITY_SHA256}"
