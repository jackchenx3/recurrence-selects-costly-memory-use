#!/usr/bin/env python3
"""Preflight checks before any production request (stdlib only; reads no outcome).

usage: preflight.py --config config/frozen_config.json --spec SPEC.md --review REVIEW.md
                    --design-go DESIGN_002_GO.json [--cpp-keys PRODUCTION_DIR/preflight/purpose_keys.json]

PHASE2-PERFORMANCE-CONVERSION-002. Checks the frozen config, the specification, terminal-review
and design-GO SHA-256 values (FAIL while the design-GO sentinel remains), the design-GO record's
identities, and derives the ten purpose keys with hashlib (printed; optionally compared with the
C++ receipt).
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mmem_common as C  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", required=True)
    ap.add_argument("--spec", required=True)
    ap.add_argument("--review", required=True)
    ap.add_argument("--design-go", required=True)
    ap.add_argument("--cpp-keys")
    a = ap.parse_args()
    problems = C.check_config(C.load_config(a.config))
    if C.sha256_file(a.spec) != C.SPEC_SHA256:
        problems.append("specification SHA-256 mismatch")
    if C.sha256_file(a.review) != C.REVIEW_SHA256:
        problems.append("terminal review SHA-256 mismatch")
    if not C.is_sha256_hex(C.DESIGN_GO_SHA256):
        problems.append("design-GO SHA-256 sentinel has not been replaced")
    elif C.sha256_file(a.design_go) != C.DESIGN_GO_SHA256:
        problems.append("design-GO SHA-256 mismatch")
    go = C.load_config(a.design_go)
    want_go = {"record": C.DESIGN_GO_RECORD, "study_id": C.STUDY_ID, "status": "DESIGN_GO",
               "design_sha256": C.SPEC_SHA256, "terminal_review_sha256": C.REVIEW_SHA256,
               "terminal_verdict": "DESIGN_GO"}
    for k, v in want_go.items():
        if go.get(k) != v:
            problems.append("design-GO record mismatch at " + k)
    keys = [{"purpose": p, "text": t, "k0": "%08x" % k0, "k1": "%08x" % k1}
            for p, (t, k0, k1) in ((p, C.purpose_key(C.PRODUCTION_NAMESPACE, p)) for p in C.PURPOSES)]
    if a.cpp_keys:
        with open(a.cpp_keys, "r", encoding="utf-8") as f:
            cpp = json.load(f)
        if cpp.get("namespace") != C.PRODUCTION_NAMESPACE or cpp.get("keys") != keys:
            problems.append("C++ purpose-key receipt differs from hashlib derivation")
    print(json.dumps({"study_id": C.STUDY_ID, "config_sha256": C.sha256_file(a.config),
                      "design_go_sha256_observed": C.sha256_file(a.design_go),
                      "purpose_keys": keys, "problems": problems,
                      "status": "PASS" if not problems else "FAIL"}, indent=2))
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
