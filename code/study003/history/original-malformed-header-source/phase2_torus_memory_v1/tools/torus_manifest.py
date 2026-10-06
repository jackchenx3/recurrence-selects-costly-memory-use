#!/usr/bin/env python3
"""PHASE2-TORUS-MEMORY-003 manifest/hash utility (standard library only).

  create --root DIR --out FILE [--exclude-dir NAME ...]
      Deterministic manifest of every regular file under DIR (sorted relative
      paths, byte counts, SHA-256). Symlinks are refused.
  verify --root DIR --manifest FILE --out FILE
      Re-hashes every listed file and requires the on-disk file set to match.
  seed-check --root DIR --out FILE
      Requires third_party/ to be exactly the 29 authenticated Random123 seed
      files, byte-identical (provenance/random123_seed_inventory.json).

Every output path is explicit; an existing nonempty output file is refused.
NOT YET RUN.
"""

import argparse
import hashlib
import json
import os
import sys


def fail(msg):
    print("TORUS_FATAL: %s" % msg, file=sys.stderr)
    sys.exit(2)


def sha256_file(path):
    h = hashlib.sha256()
    n = 0
    with open(path, "rb") as f:
        while True:
            b = f.read(1 << 22)
            if not b:
                break
            h.update(b)
            n += len(b)
    return h.hexdigest(), n


def walk(root, excludes, skip_abs):
    out = []
    for dirpath, dirnames, filenames in os.walk(root, followlinks=False):
        rel_dir = os.path.relpath(dirpath, root)
        dirnames[:] = sorted(d for d in dirnames if not (rel_dir == "." and d in excludes))
        for d in dirnames:
            if os.path.islink(os.path.join(dirpath, d)):
                fail("symlinked directory refused: %s" % os.path.join(dirpath, d))
        for name in sorted(filenames):
            p = os.path.join(dirpath, name)
            if os.path.abspath(p) in skip_abs:
                continue
            if os.path.islink(p) or not os.path.isfile(p):
                fail("non-regular file refused: %s" % p)
            rel = os.path.relpath(p, root).replace(os.sep, "/")
            out.append(rel)
    return sorted(out)


def write_new(path, text):
    if os.path.exists(path) and os.path.getsize(path) > 0:
        fail("refusing existing nonempty output: %s" % path)
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)
        f.flush()
        os.fsync(f.fileno())


def cmd_create(a):
    skip = {os.path.abspath(a.out)}
    files = []
    for rel in walk(a.root, set(a.exclude_dir or []), skip):
        digest, n = sha256_file(os.path.join(a.root, rel))
        files.append({"path": rel, "bytes": n, "sha256": digest})
    doc = {"schema": "PHASE2-TORUS-MEMORY-003-SOURCE-MANIFEST-v1", "excluded_top_level_dirs": sorted(a.exclude_dir or []),
           "file_count": len(files), "files": files}
    write_new(a.out, json.dumps(doc, indent=2, sort_keys=True) + "\n")


def cmd_verify(a):
    with open(a.manifest, "r", encoding="utf-8") as f:
        man = json.load(f)
    listed = {e["path"]: e for e in man["files"]}
    skip = {os.path.abspath(a.out), os.path.abspath(a.manifest)}
    on_disk = set(walk(a.root, set(man.get("excluded_top_level_dirs", [])), skip))
    problems = []
    if on_disk != set(listed):
        problems.append({"missing": sorted(set(listed) - on_disk), "unexpected": sorted(on_disk - set(listed))})
    for rel in sorted(set(listed) & on_disk):
        digest, n = sha256_file(os.path.join(a.root, rel))
        if digest != listed[rel]["sha256"] or n != listed[rel]["bytes"]:
            problems.append({"mismatch": rel})
    doc = {"schema": "PHASE2-TORUS-MEMORY-003-MANIFEST-VERIFY-v1", "manifest_sha256": sha256_file(a.manifest)[0],
           "problems": problems, "status": "MANIFEST_VERIFIED" if not problems else "MANIFEST_MISMATCH"}
    write_new(a.out, json.dumps(doc, indent=2, sort_keys=True) + "\n")
    return 0 if not problems else 1


def cmd_seed_check(a):
    inv_path = os.path.join(a.root, "provenance", "random123_seed_inventory.json")
    with open(inv_path, "r", encoding="utf-8") as f:
        inv = json.load(f)["files"]
    on_disk = set("third_party/" + p for p in walk(os.path.join(a.root, "third_party"), set(), set()))
    problems = []
    if on_disk != set(inv):
        problems.append({"missing": sorted(set(inv) - on_disk), "unexpected": sorted(on_disk - set(inv))})
    for rel in sorted(set(inv) & on_disk):
        digest, n = sha256_file(os.path.join(a.root, rel))
        if digest != inv[rel]["sha256"] or n != inv[rel]["bytes"]:
            problems.append({"mismatch": rel})
    if len(inv) != 29:
        problems.append({"inventory_size": len(inv)})
    doc = {"schema": "PHASE2-TORUS-MEMORY-003-SEED-CHECK-v1", "problems": problems,
           "status": "SEED_BYTE_IDENTICAL" if not problems else "SEED_MISMATCH"}
    write_new(a.out, json.dumps(doc, indent=2, sort_keys=True) + "\n")
    return 0 if not problems else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="PHASE2-TORUS-MEMORY-003 manifest/hash utility")
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("create")
    c.add_argument("--root", required=True)
    c.add_argument("--out", required=True)
    c.add_argument("--exclude-dir", action="append")
    v = sub.add_parser("verify")
    v.add_argument("--root", required=True)
    v.add_argument("--manifest", required=True)
    v.add_argument("--out", required=True)
    s = sub.add_parser("seed-check")
    s.add_argument("--root", required=True)
    s.add_argument("--out", required=True)
    a = ap.parse_args(argv)
    if a.cmd == "create":
        cmd_create(a)
        return 0
    if a.cmd == "verify":
        return cmd_verify(a)
    return cmd_seed_check(a)


if __name__ == "__main__":
    sys.exit(main())
