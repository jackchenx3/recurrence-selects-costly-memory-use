#!/usr/bin/env python3
"""Pre-production tests of the analyzer with hand-constructed intervals and hand-built
target/copy-bit series (no outcome data).

usage: python3 tests/test_merge_analyze.py [--layout-sample DIR]
  DIR is the directory written by `mmem_fixtures --emit-layout-sample DIR` (fixture namespace,
  non-scientific); it checks that the Python layouts reconstruct the C++ records.
"""
import argparse
import hashlib
import os
import struct
import sys
from decimal import Decimal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "tools"))
import merge_analyze as M  # noqa: E402
import mmem_common as C  # noqa: E402

FAILED = []


def check(ok, name):
    print(("PASS " if ok else "FAIL ") + name)
    if not ok:
        FAILED.append(name)


def iv(**kw):
    base = {"D_HALF": ("-0.01", "0.01"), "D_ZERO": ("-0.01", "0.01"), "C_abs": ("0", "0.01"),
            "C_rec": ("0", "0.01")}
    base.update(kw)
    return {k: (Decimal(a), Decimal(b)) for k, (a, b) in base.items()}


def pairing_errors(mutate=None, updates=8):
    """Hand-constructed target/copy-bit series for the 8 cells (no Philox, no scientific data),
    replayed through M.paired_target_errors in production order with one map across all cells."""
    innov = [0] + [0x1000 + t for t in range(1, updates + 1)]
    copy = [0, 1, 1, 1, 0, 1, 1, 0, 1][:updates + 1]

    def series(law, R):
        out, tg = [None], [None]
        for t in range(1, updates + 1):
            rec = int(law == 1 and t >= 3 and R[t] == 1)
            tg.append(tg[t - 2] if rec else innov[t])
            out.append([tg[t], R[t], rec])
        return out

    errs, pairs = [], {}
    for c in range(C.CELLS):
        R = list(copy)
        if mutate is not None and mutate.get("copy_cell") == c:
            R[mutate["copy_t"]] ^= 1  # this cell uses a different R_t, internally consistent
        s = series((c >> 1) & 1, R)
        for t in range(1, updates + 1):
            target, cb, rec = s[t]
            if mutate is not None and mutate.get("target_cell") == c and mutate.get("target_t") == t:
                target ^= 1
            errs.extend(M.paired_target_errors(c, t, target, cb, rec, pairs))
            pairs[(c, t)] = (target, cb)
    return errs


def paired_hash_from_audit(block, c, groups):
    """Recompute the documented paired-trajectory SHA-256 of one path from its audit rows."""
    h = hashlib.sha256(b"MMEM-PAIRED-TRAJECTORY-V1" + struct.pack("<IBB", block, c >> 2, (c >> 1) & 1))
    for t, rows in enumerate(groups, 1):
        sel = sorted((r for r in rows if r[16] != C.NA), key=lambda r: r[16])
        mask = sum(r[17] << s for s, r in enumerate(sel))
        if c & 1:
            mask ^= 0xFFFFFFFF
        h.update(struct.pack("<HIBBH", t, rows[0][10], rows[0][12], rows[0][13], sum(r[11] for r in sel)))
        h.update(bytes(r[3] for r in sel))
        h.update(struct.pack("<32I", *[r[9] for r in sel]))
        h.update(struct.pack("<I", mask))
    return h.digest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--layout-sample")
    args = ap.parse_args()

    check(pairing_errors() == [], "paired-target helper accepts a correct hand-built block")
    errs = pairing_errors({"target_cell": 1, "target_t": 4})
    check(any("cell 0" in e for e in errs), "paired-target helper detects a changed ALL_M ZERO target (cell 1)")
    errs = pairing_errors({"target_cell": 5, "target_t": 2})
    check(len(errs) > 0, "paired-target helper detects a changed SHAM ALL_M ZERO target (cell 5)")
    errs = pairing_errors({"copy_cell": 3, "copy_t": 5})
    check(any("copy bit" in e for e in errs), "paired-target helper detects a changed HALF-cell copy bit (cell 3)")
    errs = pairing_errors({"copy_cell": 7, "copy_t": 6})
    check(any("copy bit" in e for e in errs), "paired-target helper detects a changed HALF-cell copy bit (cell 7)")

    check(M.self_checks() == [], "frozen Hoeffding half-widths reproduced")
    check(M.primary_decision(iv(D_HALF=("-0.04", "0")))[0] == M.START_DEPENDENT, "rule 2 D_HALF lower <= -Delta")
    check(M.primary_decision(iv(D_ZERO=("-0.03125", "0")))[0] == M.START_DEPENDENT, "rule 2 open-interval boundary")
    check(M.primary_decision(iv(D_ZERO=("0", "0.03125")))[0] == M.START_DEPENDENT, "rule 2 upper boundary")
    check(M.primary_decision(iv(C_abs=("0.05", "0.06"), C_rec=("0.04", "0.05")))[0] == M.SUPPORTS, "rule 3")
    check(M.primary_decision(iv(C_abs=("0.05", "0.06"), C_rec=("0", "0.03125")))[0] == M.NOT_ATTRIBUTABLE,
          "rule 4 with upper(C_rec) == Delta")
    check(M.primary_decision(iv(C_abs=("0.05", "0.06"), C_rec=("0.02", "0.04")))[0] == M.UNRESOLVED,
          "rule 6 enriched but attribution unresolved")
    r = M.primary_decision(iv(C_abs=("-0.01", "0.03125")))
    check(r[0] == M.BOUNDED_NEGATIVE and r[1] is False, "rule 5 boundary, not adverse")
    r = M.primary_decision(iv(C_abs=("-0.06", "-0.04")))
    check(r[0] == M.BOUNDED_NEGATIVE and r[1] is True, "rule 5 adverse selection")
    check(M.primary_decision(iv(C_abs=("0.02", "0.04")))[0] == M.UNRESOLVED, "rule 6")
    check(M.primary_decision(iv(D_HALF=("-0.04", "0"), C_abs=("0.05", "0.06"), C_rec=("0.04", "0.05")))[0]
          == M.START_DEPENDENT, "rule 2 precedes rule 3")
    d = Decimal
    check(M.secondary_class(d("0.04"), d("0.05")) == M.SEC_POS, "secondary positive")
    check(M.secondary_class(d("-0.06"), d("-0.04")) == M.SEC_ADV, "secondary adverse")
    check(M.secondary_class(d("-0.01"), d("0.03125")) == M.SEC_BOUNDED, "secondary bounded at Delta_P")
    check(M.secondary_class(d("0.0"), d("0.05")) == M.SEC_UNRES, "secondary unresolved")

    if args.layout_sample:
        F = M.Failures()
        totals = {}
        with open(os.path.join(args.layout_sample, "layout_sample_updates.bin"), "rb") as f:
            urecs = list(C.UPDATE_REC.iter_unpack(f.read()))
        with open(os.path.join(args.layout_sample, "layout_sample_paths.bin"), "rb") as f:
            precs = list(C.PATH_REC.iter_unpack(f.read()))
        with open(os.path.join(args.layout_sample, "layout_sample_block.bin"), "rb") as f:
            brec = C.BLOCK_REC.unpack(f.read())
        M.validate_block(41599, urecs, precs, brec, F, totals)
        check(F.count == 0, "layout sample: update -> path -> block reconstruction (%s)" % F.items[:3])
        check(precs[4][21] == precs[5][21] and precs[6][21] == precs[7][21],
              "layout sample: SHAM paired-trajectory hashes equal for path records 4/5 and 6/7")
        for x in (5, 7):
            bad = list(precs)
            bad[x] = bad[x][:21] + (bytes(b ^ 0xFF for b in bad[x][21]),)
            Fb = M.Failures()
            M.validate_block(41599, urecs, bad, brec, Fb, {})
            check(any("paired-trajectory" in m for m in Fb.items),
                  "layout sample: altered paired-trajectory hash in path record %d is detected" % x)
        F2 = M.Failures()
        rows_all = open(os.path.join(args.layout_sample, "layout_sample_audit.bin"), "rb").read()
        group = C.CANDIDATES * C.AUDIT_ROW.size
        audit_updates = {41599: urecs}
        pairs = {}  # one target/copy map for the block, across all cells, as in validate_audit
        recomputed = True
        for c in range(C.CELLS):
            prev, groups = None, []
            for t in range(1, C.UPDATES + 1):
                off = ((c * C.UPDATES) + t - 1) * group
                rows = list(C.AUDIT_ROW.iter_unpack(rows_all[off:off + group]))
                groups.append(rows)
                prev = M.check_group(41599, c, t, c >> 2, (c >> 1) & 1, c & 1, rows, prev, F2, audit_updates,
                                     pairs)
            recomputed = recomputed and paired_hash_from_audit(41599, c, groups) == precs[c][21]
        check(F2.count == 0, "layout sample: audit replay including cross-cell target pairing (%s)" % F2.items[:3])
        check(len(pairs) == C.CELLS * C.UPDATES, "layout sample: pairing map retained across all 8 cells")
        check(recomputed, "layout sample: paired-trajectory SHA-256 recomputed from audit rows for all 8 paths")
    print("failed=%d" % len(FAILED))
    return 1 if FAILED else 0


if __name__ == "__main__":
    sys.exit(main())
