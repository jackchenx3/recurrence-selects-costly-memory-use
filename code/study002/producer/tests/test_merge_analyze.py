#!/usr/bin/env python3
"""Pre-production tests of the PHASE2-PERFORMANCE-CONVERSION-002 analyzer with hand-constructed
intervals, hand-built target/copy-bit series and hand-built per-update records (no outcome data).

usage: python3 tests/test_merge_analyze.py [--layout-sample DIR]
  DIR is the directory written by `mmem_fixtures --emit-layout-sample DIR` (fixture namespace,
  non-scientific); it checks that the Python layouts reconstruct the C++ records and that the
  candidate-level replay accepts a correct block.
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
    base = {"D_INFO": ("-0.01", "0.01"), "D_NONINFO": ("-0.01", "0.01"), "Delta_P": ("0", "0.01"),
            "E_INFO": ("0", "0.01"), "B_INFO": ("0", "0.01")}
    base.update(kw)
    return {k: (Decimal(a), Decimal(b)) for k, (a, b) in base.items()}


def pairing_errors(mutate=None, updates=8):
    """Hand-constructed target/copy-bit series for the 6 cells (no Philox, no scientific data),
    replayed through M.paired_target_errors in production order with one map across all cells."""
    innov = [0] + [0x1000 + t for t in range(1, updates + 1)]
    copy = [0, 1, 1, 1, 0, 1, 1, 0, 1][:updates + 1]

    def series(R):
        out, tg = [None], [None]
        for t in range(1, updates + 1):
            rec = int(t >= 3 and R[t] == 1)
            tg.append(tg[t - 2] if rec else innov[t])
            out.append([tg[t], R[t], rec])
        return out

    errs, pairs = [], {}
    for c in range(C.CELLS):
        R = list(copy)
        if mutate is not None and mutate.get("copy_cell") == c:
            R[mutate["copy_t"]] ^= 1  # this cell uses a different R_t, internally consistent
        s = series(R)
        for t in range(1, updates + 1):
            target, cb, rec = s[t]
            if mutate is not None and mutate.get("target_cell") == c and mutate.get("target_t") == t:
                target ^= 1
            errs.extend(M.paired_target_errors(c, t, target, cb, rec, pairs))
            pairs[(c, t)] = (target, cb)
    return errs


def urec(c, t, **kw):
    """One hand-built per-update record tuple (field order of C.UPDATE_REC)."""
    f = {"m": 0, "mm": 500, "valid": 0, "use": 0, "surv": 0, "f2m": 0, "m2f": 0, "q": 128, "ret": 0, "rec": 0,
         "tuse": 0, "duse": 0, "tsurv": 0, "dsurv": 0, "w0": 0, "w32": 0, "dw0": 0, "dw32": 0, "dident": 0}
    f.update(kw)
    return (0, t, c, f["m"], f["mm"], f["valid"], f["use"], f["surv"], f["f2m"], f["m2f"], f["q"], f["ret"], f["rec"],
            f["tuse"], f["duse"], f["tsurv"], f["dsurv"], f["w0"], f["w32"], f["dw0"], f["dw32"], f["dident"], 0, 0,
            0, 0)


def paired_hash_from_audit(block, c, groups):
    """Recompute the documented paired-trajectory SHA-256 of one path from its audit rows."""
    h = hashlib.sha256(b"PCONV-PAIRED-TRAJECTORY-V1" + struct.pack("<IB", block, C.cell_arm(c)))
    for t, rows in enumerate(groups, 1):
        sel = sorted((r for r in rows if r[16] != C.NA), key=lambda r: r[16])
        mask = sum(r[17] << s for s, r in enumerate(sel))
        if C.cell_start(c):
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

    # Cross-cell pairing helper.
    check(pairing_errors() == [], "paired-target helper accepts a correct hand-built block")
    errs = pairing_errors({"target_cell": 1, "target_t": 4})
    check(any("cell 0" in e for e in errs), "paired-target helper detects a changed INFO|ALL_M target (cell 1)")
    errs = pairing_errors({"target_cell": 5, "target_t": 2})
    check(len(errs) > 0, "paired-target helper detects a changed SHAM|ALL_M target (cell 5)")
    errs = pairing_errors({"copy_cell": 3, "copy_t": 5})
    check(any("copy bit" in e for e in errs), "paired-target helper detects a changed NONINFO copy bit (cell 3)")

    # Per-update operator consistency (hand-built records).
    check(M.update_record_errors(2, 5, urec(2, 5, rec=1, valid=4, use=4, duse=4, w0=1, dw0=1, dident=1)) == [],
          "NONINFO recurrent record with full decoy use accepted")
    check(M.update_record_errors(2, 5, urec(2, 5, rec=1, valid=4, use=4, tuse=4)) != [],
          "NONINFO recurrent record using the true cache rejected")
    check(M.update_record_errors(3, 5, urec(3, 5, rec=0, valid=4, use=4, duse=4)) != [],
          "NONINFO decoy at a non-recurrent update rejected")
    check(M.update_record_errors(2, 5, urec(2, 5, rec=1, valid=4, use=4, duse=4, w0=1, dw0=0)) != [],
          "NONINFO weight-0 decoy not counted rejected")
    check(M.update_record_errors(0, 5, urec(0, 5, rec=1, valid=3, use=3, tuse=3)) == [],
          "INFO recurrent record on the true cache accepted")
    check(M.update_record_errors(0, 5, urec(0, 5, rec=1, valid=3, use=3, duse=3)) != [],
          "INFO decoy use rejected")
    check(M.update_record_errors(4, 5, urec(4, 5, valid=3, use=1, tuse=1)) != [], "SHAM cache use rejected")
    check(M.update_record_errors(2, 2, urec(2, 2, rec=1)) != [], "recurrence applied at t < 3 rejected")
    check(M.update_record_errors(1, 9, urec(1, 9, q=127)) != [], "127-query update rejected")

    # Fisher-Yates replay helper.
    perm, ok = C.replay_fisher_yates([(1 << 64) - 1] * 31)
    check(ok and perm == list(range(32)), "Fisher-Yates replay: x = 2^64-1 everywhere gives the identity")
    perm, ok = C.replay_fisher_yates([1] * 31)
    check(ok and perm == [(s + 1) % 32 for s in range(32)], "Fisher-Yates replay: x = 1 everywhere gives s -> s+1")
    xs = [1] * 31
    xs[31 - 2] = 0  # step i = 2, bound 3: low64(0) < 1 must be rejected
    check(not C.replay_fisher_yates(xs)[1], "Fisher-Yates replay rejects a stored x that Lemire would reject")
    check(C.permute_bits(0x80000001, [(s + 1) % 32 for s in range(32)]) == 0x3, "permute_bits maps bit s to perm[s]")
    check(C.lemire_threshold(31) == 16 and C.lemire_threshold(3) == 1 and C.lemire_threshold(32) == 0,
          "Python Lemire thresholds agree with fixture F11")
    check(C.fnv1a32(b"") == 0x811C9DC5 and C.fnv1a32(b"a") == 0xE40C292C, "FNV-1a 32-bit reference values")

    # Inference constants and the frozen decision order.
    check(M.self_checks() == [], "frozen Hoeffding half-widths reproduced (0.0151712845, 0.0145146256, 0.0072573128)")
    check(M.primary_decision(iv(D_INFO=("-0.04", "0")))[0] == M.START_DEPENDENT, "rule 2 D_INFO lower <= -delta")
    check(M.primary_decision(iv(D_NONINFO=("-0.03125", "0")))[0] == M.START_DEPENDENT, "rule 2 open-interval boundary")
    check(M.primary_decision(iv(D_NONINFO=("0", "0.03125")))[0] == M.START_DEPENDENT, "rule 2 upper boundary")
    check(M.primary_decision(iv(Delta_P=("0.04", "0.06")))[:2] == (M.POSITIVE, 3), "rule 3 positive")
    check(M.primary_decision(iv(Delta_P=("0.03125", "0.06")))[:2] == (M.UNRESOLVED, 6), "rule 3 requires lower > delta")
    check(M.primary_decision(iv(Delta_P=("-0.06", "-0.04")))[:2] == (M.ADVERSE, 4), "rule 4 adverse")
    r = M.primary_decision(iv(Delta_P=("-0.01", "0.03125")))
    check(r[0] == M.BOUNDED and r[1] == 5 and r[2] is False, "rule 5 at upper == delta, not wholly inside")
    r = M.primary_decision(iv(Delta_P=("-0.01", "0.01")))
    check(r[0] == M.BOUNDED and r[2] is True, "rule 5 wholly inside (-delta, +delta)")
    r = M.primary_decision(iv(Delta_P=("-0.04", "-0.02")))
    check(r[0] == M.BOUNDED and r[2] is False, "rule 5 bounded but not adverse and not inside")
    check(M.primary_decision(iv(Delta_P=("0.02", "0.04")))[:2] == (M.UNRESOLVED, 6), "rule 6")
    check(M.primary_decision(iv(D_INFO=("-0.04", "0"), Delta_P=("0.05", "0.06")))[0] == M.START_DEPENDENT,
          "rule 2 precedes rule 3")
    d = Decimal
    check(M.secondary_class(d("0.04"), d("0.05")) == M.SEC_POS, "secondary positive")
    check(M.secondary_class(d("-0.06"), d("-0.04")) == M.SEC_ADV, "secondary adverse")
    check(M.secondary_class(d("-0.01"), d("0.03125")) == M.SEC_BOUNDED, "secondary bounded at delta")
    check(M.secondary_class(d("0.0"), d("0.05")) == M.SEC_UNRES, "secondary unresolved")
    check(M.interpretive_branch(3, iv(B_INFO=("0.04", "0.05"))) == M.BRANCH_BOTH, "branch: positive with B_INFO")
    check(M.interpretive_branch(3, iv(B_INFO=("0.03125", "0.05"))) == M.BRANCH_OFFSET, "branch: positive without B_INFO")
    check(M.interpretive_branch(5, iv(E_INFO=("0.04", "0.05"))) == M.BRANCH_SELECTS, "branch: bounded with E_INFO")
    check(M.interpretive_branch(4, iv(E_INFO=("0.0", "0.05"))) == M.BRANCH_CLOSE, "branch: adverse without E_INFO")
    check(M.interpretive_branch(2, iv()) == M.BRANCH_NONE and M.interpretive_branch(6, iv()) == M.BRANCH_NONE,
          "branch: none for rules 2 and 6")
    totals = {"num": [0] * 7, "cell_m": [2048 * C.N_BLOCKS // 2] * C.CELLS, "cell_mm": [0] * C.CELLS}
    recs, dec_ = M.estimates(totals)
    check(len(recs) == 19 and [r["record"] for r in recs] == list(range(1, 20)) and
          [r["name"] for r in recs[:7]] == ["Delta_P", "D_INFO", "D_NONINFO", "E_INFO", "E_NONINFO", "B_INFO",
                                            "B_NONINFO"],
          "exactly 19 records in the frozen order (synthetic zero totals, not data)")
    check(dec_["primary_decision"] == M.BOUNDED and dec_["decision_rule_applied"] == 5 and
          dec_["bounded_interval_wholly_inside_minus_delta_plus_delta"] is True,
          "synthetic all-zero contrasts classify as BOUNDED and wholly inside")

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
        check(F.count == 0, "layout sample: update -> path -> block reconstruction, C1 and N1/N2 (%s)" % F.items[:3])
        check(precs[4][21] == precs[5][21], "layout sample: SHAM paired-trajectory hashes equal for path records 4/5")
        bad = list(precs)
        bad[5] = bad[5][:21] + (bytes(b ^ 0xFF for b in bad[5][21]),) + bad[5][22:]
        Fb = M.Failures()
        M.validate_block(41599, urecs, bad, brec, Fb, {})
        check(any("paired-trajectory" in m for m in Fb.items),
              "layout sample: altered paired-trajectory hash in path record 5 is detected")
        # Permutation records and candidate rows (fixture block 41599, used as if it were audited).
        F2 = M.Failures()
        raw = open(os.path.join(args.layout_sample, "layout_sample_permutations.bin"), "rb").read()
        perms = {}
        for t, r in enumerate(C.AUDIT_PERM.iter_unpack(raw), 1):
            perm, ok = C.replay_fisher_yates(list(r[6:37]))
            if not ok or perm != list(r[5]) or r[1] != t:
                F2.add("permutation replay failure update %d" % t)
            perms[(41599, t)] = (list(r[5]), r[4])
        rows_all = open(os.path.join(args.layout_sample, "layout_sample_audit.bin"), "rb").read()
        group = C.CANDIDATES * C.AUDIT_ROW.size
        audit_updates = {41599: urecs}
        pairs, shared = {}, {}
        recomputed = True
        for c in range(C.CELLS):
            prev, groups = None, []
            for t in range(1, C.UPDATES + 1):
                off = ((c * C.UPDATES) + t - 1) * group
                rows = list(C.AUDIT_ROW.iter_unpack(rows_all[off:off + group]))
                # Fixture-namespace rows carry block 41599; the perm reference is block*256+t-1.
                groups.append(rows)
                prev = M.check_group(41599, c, t, rows, prev, F2, audit_updates, pairs, shared, perms)
            recomputed = recomputed and paired_hash_from_audit(41599, c, groups) == precs[c][21]
        check(F2.count == 0, "layout sample: permutation replay and candidate-level audit replay (%s)" % F2.items[:3])
        check(len(pairs) == C.CELLS * C.UPDATES, "layout sample: pairing map retained across all 6 cells")
        check(recomputed, "layout sample: paired-trajectory SHA-256 recomputed from audit rows for all 6 paths")
    print("failed=%d" % len(FAILED))
    return 1 if FAILED else 0


if __name__ == "__main__":
    sys.exit(main())
