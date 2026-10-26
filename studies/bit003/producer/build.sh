#!/bin/bash
# SOURCE ONLY: do not invoke until genuine nonauthor exact-source GO.
# Usage: bash /ABS/SOURCE/build.sh /ABS/NEW_BUILD_DIR FROZEN_SOURCE_PACKAGE_SHA256
set -euo pipefail
if [ "$#" -ne 2 ]; then echo 'usage: build.sh ABS_NEW_OUTPUT_DIR FROZEN_SOURCE_SHA256' >&2; exit 2; fi
build_dir=$1
source_sha=$2
case "$build_dir" in /*) ;; *) echo 'absolute output required' >&2; exit 2;; esac
case "$source_sha" in *[!0-9a-f]*|'') echo 'lowercase source SHA256 required' >&2; exit 2;; esac
[ "${#source_sha}" -eq 64 ]
[ ! -e "$build_dir" ]
source_dir=$(cd -- "$(dirname -- "$0")" && pwd -P)
# Identity is SHA256 of canonical inventory bytes; verify the entire frozen tree
# before compiling. This build-time verifier only reads bytes/JSON, never imports
# candidate modules or executes a generated program.
/usr/bin/python3 - "$source_dir" "$source_sha" <<'PY_VERIFY'
import hashlib, json, pathlib, sys
root = pathlib.Path(sys.argv[1])
raw = (root / "SOURCE_INVENTORY.json").read_bytes()
if hashlib.sha256(raw).hexdigest() != sys.argv[2]:
    raise SystemExit("source inventory identity mismatch")
inv = json.loads(raw.decode("ascii"))
canonical = (json.dumps(inv, sort_keys=True, separators=(",", ":"), ensure_ascii=True) + "\n").encode("ascii")
if raw != canonical:
    raise SystemExit("noncanonical source inventory")
listed = set()
for entry in inv["files"]:
    name = entry["path"]
    if name in listed or name == "SOURCE_INVENTORY.json" or not name or any(x in ("", ".", "..") for x in name.split("/")) or name.startswith("/"):
        raise SystemExit("invalid source inventory path")
    listed.add(name)
    data = (root / name).read_bytes()
    if len(data) != entry["bytes"] or hashlib.sha256(data).hexdigest() != entry["sha256"]:
        raise SystemExit("source bytes mismatch: " + name)
actual = set()
for path in root.rglob("*"):
    if path.is_symlink() or not (path.is_file() or path.is_dir()):
        raise SystemExit("nonregular source entry")
    if path.is_file():
        actual.add(path.relative_to(root).as_posix())
if actual != listed | {"SOURCE_INVENTORY.json"}:
    raise SystemExit("source tree closure mismatch")
PY_VERIFY
mkdir -- "$build_dir"
printf '#pragma once\n#define BRS_SOURCE_SHA "%s"\n' "$source_sha" > "$build_dir/build_metadata.hpp"
flags=(-std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -pthread -I"$source_dir/src" -I"$build_dir")
for command in brs3_produce brs3_analyze brs3_fixture; do
 /usr/bin/g++ "${flags[@]}" -DBRS_COMMAND=\""$command"\" "$source_dir/src/main.cpp" -o "$build_dir/$command" -lstdc++fs
done
/usr/bin/g++ "${flags[@]}" "$source_dir/src/support.cpp" -o "$build_dir/brs3_fixture_support" -lstdc++fs
{
 printf 'BIT003_PRODUCER_BUILD_V2\nsource_sha256=%s\n' "$source_sha"
 printf 'public_input_index_sha256=e60b367dbab81dc1388f06f9d7be6da90b927e8302b4d7001be544976abe8889\n'
 printf 'bundle_inventory_sha256=4cc9b10958a04c458aeba5ab3788dd54153b1ce18fc2cb83fe3847e012599bd6\n'
 printf 'compiler=/usr/bin/g++\nflags=-std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -pthread -lstdc++fs\n'
 /usr/bin/python3 --version
 /usr/bin/g++ --version
 sha256sum "$source_dir/SOURCE_INVENTORY.json" /usr/bin/python3 /usr/bin/g++ "$build_dir"/brs3_produce "$build_dir"/brs3_analyze "$build_dir"/brs3_fixture "$build_dir"/brs3_fixture_support
 for command in brs3_produce brs3_analyze brs3_fixture brs3_fixture_support; do ldd "$build_dir/$command"; done
} > "$build_dir/BUILD_RECEIPT.txt"
# Build records identities only. No generated program, fixture or test is run.
