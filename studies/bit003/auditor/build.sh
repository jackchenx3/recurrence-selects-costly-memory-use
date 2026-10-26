#!/bin/bash
# BUILD ONLY. Invoke only inside FRCE Slurm after exact nonauthor SOURCE GO.
# No built program, Python module, fixture or self-test is executed here.
set -euo pipefail
if [ "$#" -ne 2 ]; then
  echo 'usage: build.sh ABSENT_ABSOLUTE_OUTPUT_DIR FROZEN_SOURCE_PACKAGE_SHA256' >&2
  exit 2
fi
out=$1
source_sha=$2
case "$out" in /*) ;; *) exit 2;; esac
case "$out/" in *'/../'*|*'/./'*|*'//'*) exit 2;; esac
[[ "$source_sha" =~ ^[0-9a-f]{64}$ ]] || exit 2
[ ! -e "$out" ] && [ ! -L "$out" ] || exit 2
src=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
# These paths are intended FRCE dependencies; verify/freeze actual identities
# externally on compute node. No implicit compiler/interpreter fallback.
cxx=/usr/bin/g++
python=/usr/bin/python3
[ -x "$cxx" ] && [ -x "$python" ] || exit 2
mkdir -- "$out"
printf '#pragma once\n#define BRS3_SOURCE_SHA256 "%s"\n' "$source_sha" > "$out/build_metadata.hpp"
flags=(-std=c++17 -O3 -DNDEBUG -pthread -Wall -Wextra -Wpedantic)
"$cxx" "${flags[@]}" -I"$src/src" -I"$out" "$src/src/audit_main.cpp" -lstdc++fs -o "$out/brs3_audit"
"$cxx" "${flags[@]}" -I"$src/src" "$src/src/fixture_stage.cpp" -lstdc++fs -o "$out/brs3_fixture_stage"
cp -- "$src/src/brs3_fixture.py" "$out/brs3_fixture"
cp -- "$src/src/presentation.py" "$out/brs3_presentation"
cp -- "$src/tests/post_g4.py" "$out/brs3_post_g4"
cp -- "$src/tests/selfcheck.py" "$out/brs3_selfcheck"
chmod 0500 "$out/brs3_audit" "$out/brs3_fixture_stage" "$out/brs3_fixture" "$out/brs3_presentation" "$out/brs3_post_g4" "$out/brs3_selfcheck"
# Hashes here are build receipts, never executed scientific outputs.
{
  printf 'BIT003_AUDITOR_BUILD_V1\nsource_sha256=%s\ncompiler=/usr/bin/g++\ninterpreter=/usr/bin/python3\n' "$source_sha"
  printf 'flags=-std=c++17 -O3 -DNDEBUG -pthread -Wall -Wextra -Wpedantic -lstdc++fs\n'
  printf 'public_input_index_sha256=e60b367dbab81dc1388f06f9d7be6da90b927e8302b4d7001be544976abe8889\n'
  printf 'bundle_inventory_sha256=4cc9b10958a04c458aeba5ab3788dd54153b1ce18fc2cb83fe3847e012599bd6\n'
  sha256sum "$cxx" "$python" "$out/brs3_audit" "$out/brs3_fixture_stage" "$out/brs3_fixture" "$out/brs3_presentation" "$out/brs3_post_g4" "$out/brs3_selfcheck"
} > "$out/BUILD_RECEIPT.txt"
chmod 0400 "$out/build_metadata.hpp" "$out/BUILD_RECEIPT.txt"
