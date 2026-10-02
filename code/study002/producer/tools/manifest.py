#!/usr/bin/env python3
"""SHA-256 manifest of the source package (stdlib only).

usage: manifest.py --root PACKAGE_DIR --out NEW_FILE      (write)
       manifest.py --root PACKAGE_DIR --verify MANIFEST    (compare)
The build/ directory and __pycache__ are excluded.
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mmem_common as C  # noqa: E402

EXCLUDE_DIRS = {"build", "__pycache__", ".git"}


def collect(root):
    out = {}
    for d, dirs, files in os.walk(root):
        dirs[:] = sorted(x for x in dirs if x not in EXCLUDE_DIRS)
        for name in sorted(files):
            path = os.path.join(d, name)
            out[os.path.relpath(path, root).replace(os.sep, "/")] = C.sha256_file(path)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--out")
    g.add_argument("--verify")
    a = ap.parse_args()
    files = collect(a.root)
    if a.out:
        with open(a.out, "x", encoding="utf-8") as f:
            json.dump({"manifest": "PCONV-SOURCE-MANIFEST-1", "study_id": C.STUDY_ID, "files": files}, f,
                      indent=2, sort_keys=True)
            f.write("\n")
        return 0
    with open(a.verify, "r", encoding="utf-8") as f:
        want = json.load(f)["files"]
    ok = want == files
    print("MATCH" if ok else "MISMATCH")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
