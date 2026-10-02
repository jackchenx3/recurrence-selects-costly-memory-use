#!/usr/bin/env python3
"""Deterministic merge and analysis for PHASE2-PERFORMANCE-CONVERSION-002 revision 1 (stdlib only).

usage: merge_analyze.py --config config/frozen_config.json --production-dir DIR --out NEW_ANALYSIS_DIR

Order of work (frozen decision table, spec sections 9-11):
  0. analyzer self-checks (no production data read);
  1. exclusive ANALYSIS_LOCK.json in the production directory (a second analysis is refused);
  2. validity checks: manifest, input hashes (including the design-GO record), preflight receipts,
     purpose keys, output hashes, file sizes, per-update -> per-path -> per-block reconstruction,
     operator-consistency of every per-update record (probe sources, recurrence gate, extreme
     weights, shared permutation evidence), N1/N2 (count level and SHAM paired-trajectory SHA-256
     equality in every block), C1 (INFO/NONINFO coupling before the first effective decoy),
     128-query counts, and the candidate-by-candidate replay of blocks 0-63 (genotype construction
     from parent/cache/permutation/applied masks, cross-cell sharing of exogenous masks, Fisher-Yates
     replay of every shared permutation, Lemire + sequential selection, state continuity);
  3. any failure -> INVALID. Post-production INVALID is terminal: no estimate is computed or
     written, ROUTE_CLOSED_INVALID.json is written and the lock forbids any re-analysis;
  4. otherwise exactly 19 records, the primary decision (rules 2-6 in order), the two secondary
     families and the interpretive branch are computed exactly (integer sums, Fraction means,
     60-digit Decimal Hoeffding half-widths).

The independent audit by a non-implementing reviewer is separate and is not part of this package.
"""
import argparse
import bisect
import itertools
import json
import os
import sys
from decimal import Decimal, localcontext
from fractions import Fraction

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mmem_common as C  # noqa: E402

PREC = 60
N = C.N_BLOCKS
MASK64 = (1 << 64) - 1
MAX_LISTED_FAILURES = 200
DELTA = Fraction(1, 32)

# name, range length R, block-value denominator, frozen rounded half-width
PRIMARY = (("Delta_P", 2, 131072, "0.0151712845"), ("D_INFO", 2, 2048, "0.0151712845"),
           ("D_NONINFO", 2, 2048, "0.0151712845"))
ALLELE = (("E_INFO", 2, 4096, "0.0145146256"), ("E_NONINFO", 1, 4096, "0.0072573128"))
PERFORMANCE = (("B_INFO", 2, 131072, "0.0145146256"), ("B_NONINFO", 2, 131072, "0.0145146256"))
ALPHA_PRIMARY = Fraction(1, 60)      # 0.05 / 3
ALPHA_SECONDARY = Fraction(1, 40)    # 0.025
ALPHA_TEXT = {ALPHA_PRIMARY: "0.05/3", ALPHA_SECONDARY: "0.025"}
# Order of the seven inferential block numerators in the block record (fields 5..11).
BLOCK_VARS = ("Delta_P", "D_INFO", "D_NONINFO", "E_INFO", "E_NONINFO", "B_INFO", "B_NONINFO")
BLOCK_LIMITS = (131072, 2048, 2048, 4096, 2048, 131072, 131072)

INVALID = "INVALID"
START_DEPENDENT = "START-DEPENDENT; PRIMARY UNRESOLVED"
POSITIVE = "MEANINGFUL POSITIVE DIRECTIONAL-INFORMATION EFFECT RELATIVE TO NONINFO"
ADVERSE = "MEANINGFUL ADVERSE DIRECTIONAL-INFORMATION EFFECT RELATIVE TO NONINFO"
BOUNDED = "BOUNDED BELOW THE POSITIVE ONE-BIT SCALE"
UNRESOLVED = "UNRESOLVED"
SEC_POS = "MEANINGFUL POSITIVE AT THE ONE-BIT SCALE"
SEC_ADV = "MEANINGFUL ADVERSE AT THE ONE-BIT SCALE"
SEC_BOUNDED = "BOUNDED BELOW THE POSITIVE ONE-BIT SCALE"
SEC_UNRES = "UNRESOLVED"

BRANCH_BOTH = ("Directional cache information produces a one-bit benefit relative to both the matched decoy and "
               "SHAM in this fixed model.")
BRANCH_OFFSET = ("Directional information offsets some displacement cost relative to NONINFO; no one-bit absolute "
                 "benefit over no-memory use is demonstrated.")
BRANCH_SELECTS = ("Directional information selects for policy use without converting into one-bit population "
                  "performance, sharpening the selection-performance separation.")
BRANCH_CLOSE = "The fixed information intervention does not explain the accepted enrichment; close this conversion route."
BRANCH_NONE = ("No interpretive branch applies; the primary question is unresolved and the secondary families are "
               "reported without changing it.")

INTERPRETATION_LABELS = [
    "TOTAL EFFECT: Delta_P is the total population-accuracy effect of directional cache information beyond the "
    "parent and matched displacement length, mediated through evolved policy use.",
    "NOT PER-USE: Delta_P is not a per-use value-of-information estimate, does not separate retention from "
    "retrieval, and does not demonstrate spontaneous memory origin.",
    "CONTROL GEOMETRY: the fixed shared permutation matches parent distance and one common coordinate relabeling "
    "but does not preserve every cross-genotype relation; displacement weights 0 and 32 cannot be scrambled and "
    "retain direction (counts in diagnostics_descriptive.json).",
    "SCOPE: one supplied cache, one HALF law, one fixed displacement cost and one parent-distance-matched "
    "directional-scramble control; no generalization to other lags, costs, models or biology.",
    "SECONDARY FAMILIES: E_INFO, E_NONINFO, B_INFO and B_NONINFO cannot alter the primary decision. A positive "
    "primary does not establish benefit over no-memory use unless lower(B_INFO) > 1/32.",
    "NO EXTENSION: a null, bounded or adverse outcome is reportable and must not trigger a numerical extension, "
    "parameter change or follow-up cohort.",
]


def dec(x):
    """Exact Fraction -> Decimal at PREC digits."""
    with localcontext() as ctx:
        ctx.prec = PREC
        return Decimal(x.numerator) / Decimal(x.denominator)


def half_width(range_length, alpha_each, n=N):
    """R * sqrt(ln(2/alpha_each) / (2n)) with 2/alpha_each formed exactly."""
    ratio = Fraction(2) / Fraction(alpha_each)
    with localcontext() as ctx:
        ctx.prec = PREC
        inner = (Decimal(ratio.numerator) / Decimal(ratio.denominator)).ln() / (Decimal(2) * Decimal(n))
        return Decimal(range_length) * inner.sqrt()


def primary_decision(iv):
    """iv: name -> (lower, upper) Decimals. Returns (label, rule, interval_inside_or_None)."""
    d = dec(DELTA)
    for name in ("D_INFO", "D_NONINFO"):
        lo, hi = iv[name]
        if not (lo > -d and hi < d):
            return START_DEPENDENT, 2, None
    lo, hi = iv["Delta_P"]
    if lo > d:
        return POSITIVE, 3, None
    if hi < -d:
        return ADVERSE, 4, None
    if hi <= d:
        return BOUNDED, 5, bool(lo > -d and hi < d)
    return UNRESOLVED, 6, None


def secondary_class(lo, hi):
    d = dec(DELTA)
    if lo > d:
        return SEC_POS
    if hi < -d:
        return SEC_ADV
    if hi <= d:
        return SEC_BOUNDED
    return SEC_UNRES


def interpretive_branch(rule, iv):
    d = dec(DELTA)
    if rule == 3:
        return BRANCH_BOTH if iv["B_INFO"][0] > d else BRANCH_OFFSET
    if rule in (4, 5):
        return BRANCH_SELECTS if iv["E_INFO"][0] > d else BRANCH_CLOSE
    return BRANCH_NONE


def self_checks():
    errs = []
    tol = Decimal("0.0000000001")  # one unit in the 10th decimal of the frozen rounded strings
    for name, r, _den, frozen in PRIMARY:
        if abs(half_width(r, ALPHA_PRIMARY) - Decimal(frozen)) > tol:
            errs.append("half-width mismatch " + name)
    for name, r, _den, frozen in ALLELE + PERFORMANCE:
        if abs(half_width(r, ALPHA_SECONDARY) - Decimal(frozen)) > tol:
            errs.append("half-width mismatch " + name)
    if not half_width(2, ALPHA_PRIMARY) < dec(DELTA) / 2:
        errs.append("range-2 primary half-width not below delta/2")
    if len(PRIMARY) + len(ALLELE) + len(PERFORMANCE) + 2 * C.CELLS != C.SAVED_ESTIMATES:
        errs.append("record count differs from 19")
    return errs


class Failures(object):
    def __init__(self):
        self.items = []
        self.count = 0

    def add(self, msg):
        self.count += 1
        if len(self.items) < MAX_LISTED_FAILURES:
            self.items.append(msg)


def expected_outputs():
    expected = set()
    for s in range(C.SHARDS):
        names = ("updates.bin", "paths.bin", "blocks.bin")
        if s == 0:
            names += (C.AUDIT_FILE, C.AUDIT_PERM_FILE)
        for n in names:
            expected.add("shards/shard_%02d/%s" % (s, n))
    return expected


def check_receipts(prod, cfg_path, F):
    def load(rel):
        path = os.path.join(prod, rel)
        if not os.path.exists(path):
            F.add("missing " + rel)
            return None
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)

    man = load("run_manifest.json")
    fx = load("preflight/fixtures.json")
    col = load("preflight/collision_audit.json")
    kat = load("preflight/philox_kat.json")
    keys = load("preflight/purpose_keys.json")
    if os.path.exists(os.path.join(prod, "PREFLIGHT_FAILED.json")):
        F.add("preflight failed marker present")
    if fx is not None and not (fx.get("all_passed") is True and fx.get("failed") == 0):
        F.add("fixture receipt not all-passed")
    if col is not None and col.get("status") != "PASS":
        F.add("collision audit receipt not PASS")
    if kat is not None and kat.get("status") != "PASS":
        F.add("Philox KAT receipt not PASS")
    if keys is not None:
        got = [(k["purpose"], k["text"], int(k["k0"], 16), int(k["k1"], 16)) for k in keys.get("keys", [])]
        want = [(p,) + C.purpose_key(C.PRODUCTION_NAMESPACE, p) for p in C.PURPOSES]
        if keys.get("namespace") != C.PRODUCTION_NAMESPACE or got != want:
            F.add("purpose keys differ from independent hashlib derivation")
    if man is None:
        return None
    if man.get("status") != "COMPLETE":
        F.add("manifest status " + str(man.get("status")))
    if man.get("config_sha256") != C.sha256_file(cfg_path):
        F.add("config SHA-256 differs from manifest")
    if man.get("specification_sha256") != C.SPEC_SHA256 or man.get("terminal_review_sha256") != C.REVIEW_SHA256:
        F.add("input hash mismatch in manifest")
    if not C.is_sha256_hex(C.DESIGN_GO_SHA256) or man.get("design_go_sha256") != C.DESIGN_GO_SHA256:
        F.add("design-GO SHA-256 missing, sentinel, or different from manifest")
    for k in ("n1_failed_blocks", "n2_failed_blocks", "c1_failed_blocks", "query_failed_blocks"):
        if man.get(k) != 0:
            F.add("manifest %s = %r" % (k, man.get(k)))
    if man.get("total_objective_queries") != C.TOTAL_QUERIES:
        F.add("manifest query total differs from 8,178,892,800")
    if man.get("total_output_bytes") != C.TOTAL_RECORD_BYTES:
        F.add("manifest output byte total differs from 4,229,120,000")
    expected = expected_outputs()
    listed = {o["path"]: o for o in man.get("outputs", [])}
    if set(listed) != expected:
        F.add("manifest output list differs from the fixed shard layout")
    for rel in sorted(expected & set(listed)):
        path = os.path.join(prod, rel)
        if not os.path.exists(path) or C.sha256_file(path) != listed[rel]["sha256"]:
            F.add("output hash mismatch " + rel)
    return man


DIAG_KEYS = ("f2m", "m2f", "use", "surv", "ret", "fix", "ext", "lfix", "lext", "tuse", "duse", "tsurv", "dsurv",
             "w0", "w32", "dw0", "dw32", "dident", "rec", "pret")


def new_acc():
    a = {k: 0 for k in DIAG_KEYS}
    a.update({"late_m": 0, "late_mm": 0, "q": 0, "final_m": 0, "final_mm": 0})
    return a


def validate_tables(prod, F, totals, audit_updates):
    sizes = {"updates.bin": C.BLOCKS_PER_SHARD * C.CELLS * C.UPDATES * C.UPDATE_REC.size,
             "paths.bin": C.BLOCKS_PER_SHARD * C.CELLS * C.PATH_REC.size,
             "blocks.bin": C.BLOCKS_PER_SHARD * C.BLOCK_REC.size}
    ublk = C.CELLS * C.UPDATES * C.UPDATE_REC.size
    pblk = C.CELLS * C.PATH_REC.size
    for shard in range(C.SHARDS):
        d = C.shard_dir(prod, shard)
        paths = {n: os.path.join(d, n) for n in sizes}
        if not all(os.path.exists(p) and os.path.getsize(p) == sizes[n] for n, p in paths.items()):
            F.add("shard %02d file missing or wrong size" % shard)
            continue
        with open(paths["updates.bin"], "rb") as fu, open(paths["paths.bin"], "rb") as fp, \
                open(paths["blocks.bin"], "rb") as fb:
            for local in range(C.BLOCKS_PER_SHARD):
                block = shard * C.BLOCKS_PER_SHARD + local
                urecs = list(C.UPDATE_REC.iter_unpack(fu.read(ublk)))
                precs = list(C.PATH_REC.iter_unpack(fp.read(pblk)))
                brec = C.BLOCK_REC.unpack(fb.read(C.BLOCK_REC.size))
                if block < C.AUDIT_BLOCKS:
                    audit_updates[block] = urecs
                validate_block(block, urecs, precs, brec, F, totals)


def update_record_errors(c, t, u):
    """Operator consistency of one per-update record; reads only its arguments."""
    (_b, _u, _cell, m, mm, valid, use, surv, f2m, m2f, q, _ret, rec, tuse, duse, tsurv, dsurv, w0, w32, dw0, dw32,
     dident, res16, _pret, _pfnv, res32) = u
    arm = C.cell_arm(c)
    errs = []
    if q != C.CANDIDATES:
        errs.append("query count %d" % q)
    if m > C.POP or mm > C.POP * 32 or valid > C.POP or f2m + m2f > C.POP or res16 or res32:
        errs.append("range or reserved-field error")
    if rec not in (0, 1):
        errs.append("recurrence flag not 0/1")
    if t < 3 and rec != 0:
        errs.append("recurrence applied at t < 3")
    if use != tuse + duse or surv != tsurv + dsurv or tsurv > tuse or dsurv > duse or use > valid:
        errs.append("probe use/survival totals inconsistent")
    if w0 + w32 > valid or dw0 > w0 or dw32 > w32 or dident > duse or dident < dw0 + dw32:
        errs.append("extreme-weight counts inconsistent")
    if arm == C.SHAM and (use or tuse or duse or dw0 or dw32 or dident):
        errs.append("SHAM probe derived from the cache")
    if arm == C.INFO and (duse or dw0 or dw32 or dident or tuse != valid):
        errs.append("INFO probe source differs from the true cache rule")
    if arm == C.NONINFO:
        if rec == 1 and (duse != valid or tuse != 0 or dw0 != w0 or dw32 != w32):
            errs.append("NONINFO recurrent update not fully decoyed")
        if rec == 0 and (duse or tuse != valid or dw0 or dw32 or dident):
            errs.append("NONINFO non-recurrent update not on the true cache")
    return errs


def validate_block(block, urecs, precs, brec, F, totals):
    accs = []
    for c in range(C.CELLS):
        a = new_acc()
        for t in range(1, C.UPDATES + 1):
            u = urecs[c * C.UPDATES + t - 1]
            if u[0] != block or u[1] != t or u[2] != c:
                F.add("update record order error block %d cell %d update %d" % (block, c, t))
            for e in update_record_errors(c, t, u):
                F.add("%s: block %d cell %d update %d" % (e, block, c, t))
            late = C.LATE_FIRST <= t <= C.LATE_LAST
            if late:
                a["late_m"] += u[3]
                a["late_mm"] += u[4]
            a["q"] += u[10]
            for key, idx in (("f2m", 8), ("m2f", 9), ("use", 6), ("surv", 7), ("ret", 11), ("rec", 12),
                             ("tuse", 13), ("duse", 14), ("tsurv", 15), ("dsurv", 16), ("w0", 17), ("w32", 18),
                             ("dw0", 19), ("dw32", 20), ("dident", 21), ("pret", 23)):
                a[key] += u[idx]
            if u[3] == C.POP:
                a["fix"] += 1
                a["lfix"] += int(late)
            if u[3] == 0:
                a["ext"] += 1
                a["lext"] += int(late)
            a["final_m"] = u[3]
            a["final_mm"] = u[4]
        p = precs[c]
        want = (block, c, C.cell_arm(c), C.cell_start(c), 0, a["late_m"], a["late_mm"], a["q"], a["f2m"], a["m2f"],
                a["use"], a["surv"], a["ret"], a["fix"], a["ext"], a["lfix"], a["lext"])
        want_tail = (a["tuse"], a["duse"], a["tsurv"], a["dsurv"], a["w0"], a["w32"], a["dw0"], a["dw32"],
                     a["dident"], a["rec"], a["pret"])
        if tuple(p[:17]) != want or p[18] != a["final_m"] or p[19] != a["final_mm"] or p[20] \
                or tuple(p[22:33]) != want_tail:
            F.add("path summary does not reconstruct from per-update records: block %d cell %d" % (block, c))
        accs.append(a)
        totals["queries"] = totals.get("queries", 0) + a["q"]
        for key in DIAG_KEYS:
            totals.setdefault("diag_" + key, [0] * C.CELLS)[c] += a[key]
    # Shared exogenous evidence: recurrence flag, permutation retry total and fingerprint per update.
    rec_updates = 0
    pret_block = 0
    for t in range(C.UPDATES):
        ref = urecs[t]
        rec_updates += ref[12]
        pret_block += ref[23]
        for c in range(1, C.CELLS):
            u = urecs[c * C.UPDATES + t]
            if (u[12], u[23], u[24]) != (ref[12], ref[23], ref[24]):
                F.add("shared recurrence/permutation evidence differs across cells: block %d update %d" % (block, t + 1))
    # N1/N2 at count level; SHAM survival retries equal between starts.
    for t in range(C.UPDATES):
        ux, uy = urecs[4 * C.UPDATES + t], urecs[5 * C.UPDATES + t]
        if ux[3] + uy[3] != C.POP or ux[4] != uy[4] or ux[11] != uy[11]:
            F.add("N1/N2 violated in update records: block %d update %d" % (block, t + 1))
    # Paired-trajectory SHA-256 (path field 21) binds targets, survivor indices, genotypes and
    # start-normalized labels after every update: SHAM ALL_F/ALL_M must be identical.
    if precs[4][21] != precs[5][21]:
        F.add("N1/N2 paired-trajectory SHA-256 differs: block %d cells 4/5" % block)
    # C1: INFO and NONINFO of each start agree until the first update with an effective decoy.
    first_dec = []
    for st in (0, 1):
        first = 0
        for t in range(C.UPDATES):
            ui, un = urecs[st * C.UPDATES + t], urecs[(2 + st) * C.UPDATES + t]
            if (ui[5], ui[17], ui[18], ui[6]) != (un[5], un[17], un[18], un[6]):
                F.add("C1 pre-update diagnostics differ: block %d start %d update %d" % (block, st, t + 1))
            if un[14] != un[21]:
                first = t + 1
                if un[12] != 1:
                    F.add("C1 decoupling at a non-recurrent update: block %d start %d" % (block, st))
                break
            same = (ui[3], ui[4], ui[6], ui[7], ui[8], ui[9], ui[11]) == (un[3], un[4], un[6], un[7], un[8], un[9], un[11])
            if not same:
                F.add("C1 coupled INFO/NONINFO records differ: block %d start %d update %d" % (block, st, t + 1))
        first_dec.append(first)
    L = [a["late_m"] for a in accs]
    P = [a["late_mm"] for a in accs]
    nums = ((P[2] + P[3]) - (P[0] + P[1]), L[1] - L[0], L[3] - L[2], (L[0] + L[1]) - (L[2] + L[3]),
            (L[2] + L[3]) - 2048, (P[4] + P[5]) - (P[0] + P[1]), (P[4] + P[5]) - (P[2] + P[3]))
    want_block = ((block, 1, 1, 1, int(block < C.AUDIT_BLOCKS)) + nums + tuple(L) + tuple(P)
                  + (1, 0, first_dec[0], first_dec[1], rec_updates, pret_block))
    if tuple(brec) != want_block:
        F.add("block record does not reconstruct from update/path records (or an N1/N2/C1/query flag is set): "
              "block %d" % block)
    if any(abs(v) > lim for v, lim in zip(nums, BLOCK_LIMITS)):
        F.add("block variable out of range: block %d" % block)
    totals["blocks"] = totals.get("blocks", 0) + 1
    sums = totals.setdefault("num", [0] * len(BLOCK_VARS))
    for i, v in enumerate(nums):
        sums[i] += v
    for c in range(C.CELLS):
        totals.setdefault("cell_m", [0] * C.CELLS)[c] += L[c]
        totals.setdefault("cell_mm", [0] * C.CELLS)[c] += P[c]
    decoupled = totals.setdefault("decoupled_blocks", [0, 0])
    for st in (0, 1):
        if first_dec[st]:
            decoupled[st] += 1


def load_permutations(prod, F):
    """Returns {(block, update): perm list} after replaying Fisher-Yates from the stored values."""
    path = os.path.join(C.shard_dir(prod, 0), C.AUDIT_PERM_FILE)
    if not os.path.exists(path) or os.path.getsize(path) != C.AUDIT_PERM_RECORDS * C.AUDIT_PERM.size:
        F.add("audit permutation file missing or wrong size")
        return None
    perms = {}
    with open(path, "rb") as f:
        for block in range(C.AUDIT_BLOCKS):
            for t in range(1, C.UPDATES + 1):
                r = C.AUDIT_PERM.unpack(f.read(C.AUDIT_PERM.size))
                where = "block %d update %d" % (block, t)
                if r[0] != block or r[1] != t or r[3]:
                    F.add("audit permutation record order error " + where)
                stored = list(r[5])
                xs = list(r[6:6 + C.PERM_STEPS])
                retries = list(r[6 + C.PERM_STEPS:6 + 2 * C.PERM_STEPS])
                perm, ok = C.replay_fisher_yates(xs)
                if not ok or perm != stored or sorted(stored) != list(range(32)):
                    F.add("Fisher-Yates replay failure " + where)
                if r[4] != sum(retries) or r[2] != sum(1 for v in retries if v):
                    F.add("permutation retry evidence inconsistent " + where)
                perms[(block, t)] = (stored, r[4])
    return perms


def validate_audit(prod, F, audit_updates, perms):
    path = os.path.join(C.shard_dir(prod, 0), C.AUDIT_FILE)
    if not os.path.exists(path) or os.path.getsize(path) != C.AUDIT_ROWS * C.AUDIT_ROW.size:
        F.add("audit file missing or wrong size")
        return
    group = C.CANDIDATES * C.AUDIT_ROW.size
    with open(path, "rb") as f:
        for block in range(C.AUDIT_BLOCKS):
            init_geno = None
            pairs = {}   # (cell, update) -> (target, copy_bit) across all cells of the block
            shared = {}  # (update, slot, kind) -> mask; exogenous masks shared by the six cells
            for c in range(C.CELLS):
                prev = None
                for t in range(1, C.UPDATES + 1):
                    rows = list(C.AUDIT_ROW.iter_unpack(f.read(group)))
                    prev = check_group(block, c, t, rows, prev, F, audit_updates, pairs, shared, perms)
                    if t == 1:
                        g = tuple(r[9] for r in rows[:C.POP])
                        if init_geno is None:
                            init_geno = g
                        elif g != init_geno:
                            F.add("initial genotypes differ across cells: block %d" % block)


def paired_target_errors(c, t, target, copy_bit, rec, pairs):
    """Cross-cell target and copy-bit pairing for cell c at update t. Pure: reads only its arguments.

    One HALF law: all six cells share I_t and R_t, hence T_t. Cells are visited 0..5, each over
    updates 1..256, so cell 0 at update t and cell c at t-2 are present whenever needed.
    """
    def target_of(cell, u):
        v = pairs.get((cell, u))
        return None if v is None else v[0]

    errs = []
    if rec != int(t >= 3 and copy_bit == 1):
        errs.append("recurrence flag differs from t >= 3 and R_t = 1")
    if c != 0:
        ref = pairs.get((0, t))
        if ref is None or copy_bit != ref[1] or target != ref[0]:
            errs.append("target or copy bit R_t differs from cell 0")
    if rec == 1 and target_of(c, t - 2) != target:
        errs.append("HALF recurrence does not copy this cell's T_(t-2)")
    return errs


def check_shared(shared, key, value, where, F):
    if key in shared:
        if shared[key] != value:
            F.add("exogenous %s mask differs across cells %s" % (key[2], where))
    else:
        shared[key] = value


def check_group(block, c, t, rows, prev, F, audit_updates, pairs, shared, perms):
    where = "block %d cell %d update %d" % (block, c, t)
    arm, start = C.cell_arm(c), C.cell_start(c)
    target, copy_bit, rec = rows[0][10], rows[0][12], rows[0][13]
    pentry = perms.get((block, t)) if perms is not None else None
    if pentry is None:
        F.add("permutation record unavailable " + where)
        return None
    perm = pentry[0]
    weights = []
    for j, r in enumerate(rows):
        (b, u, cell, cand, fam, parent, plab, pvalid, pcache, geno, tgt, mis, cb, ra, src,
         w, rank, post, flip, donor, W, x, Z, retry, pref, pgeno, disp, pdist, dweight, res16, applied) = r
        if (b, u, cell, cand, fam, parent) != (block, t, c, j, j // C.POP, j % C.POP) or res16:
            F.add("audit row order error " + where)
        if tgt != target or cb != copy_bit or ra != rec or mis != C.popcount(geno ^ tgt) or w != 1 << (32 - mis):
            F.add("audit row mismatch/weight error " + where)
        if pref != block * C.UPDATES + t - 1:
            F.add("permutation reference error " + where)
        if pgeno != rows[parent][9] or pdist != C.popcount(geno ^ pgeno):
            F.add("parent genotype/distance error " + where)
        want_disp = (pcache ^ pgeno) if pvalid else 0
        if disp != want_disp or dweight != (C.popcount(disp) if pvalid else C.NA) or (not pvalid and pcache):
            F.add("true displacement error " + where)
        if fam == 0 and (geno != pgeno or applied != 0):
            F.add("parent candidate error " + where)
        if fam == 1:
            valid_m = plab == 1 and pvalid == 1
            if arm == C.SHAM or not valid_m:
                want_src = C.SRC_FRESH
            elif arm == C.INFO or rec == 0:
                want_src = C.SRC_TRUE_CACHE
            else:
                want_src = C.SRC_DECOY
            if src != want_src:
                F.add("policy probe source error " + where)
            if src == C.SRC_TRUE_CACHE and (geno != pcache or applied != disp):
                F.add("true-cache probe error " + where)
            if src == C.SRC_DECOY and (applied != C.permute_bits(disp, perm) or geno != pgeno ^ applied
                                       or pdist != dweight):
                F.add("decoy probe is not x XOR pi(c XOR x) with the shared permutation " + where)
            if src == C.SRC_FRESH:
                if geno != pgeno ^ applied:
                    F.add("fresh probe error " + where)
                check_shared(shared, (t, parent, "fresh"), applied, where, F)
        elif src != C.NA:
            F.add("probe source set outside family 1 " + where)
        if fam == 2:
            if geno != pgeno ^ applied:
                F.add("scout candidate error " + where)
            check_shared(shared, (t, parent, "scout"), applied, where, F)
        if fam == 3:
            if donor > 2:
                F.add("donor family error " + where)
            else:
                dmis = [rows[k * C.POP + parent][11] for k in range(3)]
                if dmis[donor] != min(dmis) or geno != rows[donor * C.POP + parent][9] ^ applied:
                    F.add("local child / donor error " + where)
            check_shared(shared, (t, parent, "local"), applied, where, F)
        elif donor != C.NA:
            F.add("donor set outside family 3 " + where)
        weights.append(w)
    for e in paired_target_errors(c, t, target, copy_bit, rec, pairs):
        F.add(e + " " + where)
    pairs[(c, t)] = (target, copy_bit)
    sel = sorted((r for r in rows if r[16] != C.NA), key=lambda r: r[16])
    if [r[16] for r in sel] != list(range(C.POP)):
        F.add("selected ranks are not 0..31 " + where)
        return None
    remaining = list(range(C.CANDIDATES))
    for r in sel:
        W = sum(weights[j] for j in remaining)
        m = r[21] * W
        if r[20] != W or (m & MASK64) < ((1 << 64) - W) % W or (m >> 64) != r[22]:
            F.add("Lemire replay failure " + where)
        cums = list(itertools.accumulate(weights[j] for j in remaining))
        k = bisect.bisect_right(cums, r[22])
        if k >= len(remaining) or remaining[k] != r[3]:
            F.add("sequential selection replay failure " + where)
            return None
        remaining.pop(k)
    # Reconstruct the per-update record from the candidate rows.
    u = audit_updates[block][c * C.UPDATES + t - 1]
    fam0 = rows[:C.POP]
    fam1 = rows[C.POP:2 * C.POP]
    valid_rows = [r for r in fam0 if r[6] == 1 and r[7] == 1]
    want = (sum(r[17] for r in sel), sum(r[11] for r in sel), len(valid_rows),
            sum(1 for r in fam1 if r[14] in (1, 2)),
            sum(1 for r in sel if r[4] == 1 and r[14] in (1, 2)),
            sum(1 for r in sel if r[6] == 0 and r[17] == 1),
            sum(1 for r in sel if r[6] == 1 and r[17] == 0), C.CANDIDATES, sum(r[23] for r in sel),
            rec, sum(1 for r in fam1 if r[14] == 1), sum(1 for r in fam1 if r[14] == 2),
            sum(1 for r in sel if r[4] == 1 and r[14] == 1), sum(1 for r in sel if r[4] == 1 and r[14] == 2),
            sum(1 for r in valid_rows if r[28] == 0), sum(1 for r in valid_rows if r[28] == 32),
            sum(1 for r in fam1 if r[14] == 2 and r[28] == 0), sum(1 for r in fam1 if r[14] == 2 and r[28] == 32),
            sum(1 for r in fam1 if r[14] == 2 and r[9] == r[8]),
            pentry[1], C.fnv1a32(bytes(perm)))
    if tuple(u[3:12]) + tuple(u[12:22]) + (u[23], u[24]) != want:
        F.add("audit rows do not reconstruct the per-update record " + where)
    for r in sel:
        if r[17] != r[6] ^ r[18]:
            F.add("policy mutation inconsistent " + where)
    # Continuity with the previous update.
    for s in range(C.POP):
        r = fam0[s]
        if prev is None:
            ok = r[6] == start and r[7] == 0 and r[8] == 0
        else:
            ps = prev["sel"][s]
            want_cache = prev["fam0"][ps[5]][9] if ps[17] == 1 else 0
            ok = r[9] == ps[9] and r[6] == ps[17] and r[7] == int(ps[17] == 1) and r[8] == want_cache
        if not ok:
            F.add("state continuity failure slot %d " % s + where)
    return {"sel": sel, "fam0": fam0}


def interval_record(index, name, family, alpha, r, den, num_sum, h):
    mean = Fraction(num_sum, den * N)
    with localcontext() as ctx:
        ctx.prec = PREC
        lo, hi = dec(mean) - h, dec(mean) + h
    rec = {"record": index, "name": name, "family": family,
           "estimate_exact": "%d/%d" % (mean.numerator, mean.denominator), "estimate": str(dec(mean)),
           "range_length": r, "alpha_each": ALPHA_TEXT[alpha], "n_blocks": N, "half_width": str(h),
           "lower": str(lo), "upper": str(hi)}
    return rec, (lo, hi)


def estimates(totals):
    records, iv = [], {}
    idx = {name: i for i, name in enumerate(BLOCK_VARS)}
    k = 1
    for group, family, alpha in ((PRIMARY, "PRIMARY", ALPHA_PRIMARY),
                                 (ALLELE, "SECONDARY_ALLELE", ALPHA_SECONDARY),
                                 (PERFORMANCE, "SECONDARY_PERFORMANCE", ALPHA_SECONDARY)):
        for name, r, den, _frozen in group:
            rec, iv[name] = interval_record(k, name, family, alpha, r, den, totals["num"][idx[name]],
                                            half_width(r, alpha))
            records.append(rec)
            k += 1
    label, rule, inside = primary_decision(iv)
    for rec in records[:3]:
        rec["family_decision"] = label
    for rec in records[3:7]:
        rec["classification"] = secondary_class(*iv[rec["name"]])
        rec["cannot_alter_primary_decision"] = True
    for c in range(C.CELLS):
        mean = Fraction(totals["cell_m"][c], 2048 * N)
        records.append({"record": k, "name": "M_FREQUENCY_LATE|" + C.CELL_NAMES[c],
                        "family": "ABSOLUTE_CELL_MEAN_DESCRIPTIVE",
                        "estimate_exact": "%d/%d" % (mean.numerator, mean.denominator), "estimate": str(dec(mean)),
                        "lower": None, "upper": None, "classification": "DESCRIPTIVE; NO INFERENTIAL BOUND FROZEN"})
        k += 1
    for c in range(C.CELLS):
        mean = 1 - Fraction(totals["cell_mm"][c], 65536 * N)
        records.append({"record": k, "name": "ACCURACY_LATE|" + C.CELL_NAMES[c],
                        "family": "ABSOLUTE_CELL_MEAN_DESCRIPTIVE",
                        "estimate_exact": "%d/%d" % (mean.numerator, mean.denominator), "estimate": str(dec(mean)),
                        "lower": None, "upper": None, "classification": "DESCRIPTIVE; NO INFERENTIAL BOUND FROZEN"})
        k += 1
    assert len(records) == C.SAVED_ESTIMATES
    decision = {"primary_decision": label, "decision_rule_applied": rule,
                "bounded_interval_wholly_inside_minus_delta_plus_delta": inside,
                "secondary_classifications": {r["name"]: r["classification"] for r in records[3:7]},
                "interpretive_branch": interpretive_branch(rule, iv),
                "b_info_lower_exceeds_delta": bool(iv["B_INFO"][0] > dec(DELTA)),
                "e_info_lower_exceeds_delta": bool(iv["E_INFO"][0] > dec(DELTA)),
                "interpretation_labels": INTERPRETATION_LABELS}
    return records, decision


def write_json(path, obj):
    with open(path, "x", encoding="utf-8") as f:
        json.dump(obj, f, indent=2, sort_keys=False)
        f.write("\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", required=True)
    ap.add_argument("--production-dir", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    errs = self_checks() + C.check_config(C.load_config(args.config))
    if not C.is_sha256_hex(C.DESIGN_GO_SHA256):
        errs.append("design-GO SHA-256 sentinel has not been replaced")
    if errs:
        sys.stderr.write("analyzer self-check or config check failed (no production data read):\n  %s\n"
                         % "\n  ".join(errs))
        return 70
    if os.path.exists(args.out):
        sys.stderr.write("refused: --out must be a new directory\n")
        return 64
    lock = os.path.join(args.production_dir, "ANALYSIS_LOCK.json")
    try:
        fd = os.open(lock, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o444)
    except FileExistsError:
        sys.stderr.write("refused: analysis already attempted for this production directory; "
                         "post-production INVALID is terminal and re-analysis is not permitted\n")
        return 4
    with os.fdopen(fd, "w") as f:
        json.dump({"status": "STARTED", "out": os.path.abspath(args.out)}, f)
    os.makedirs(args.out)

    F = Failures()
    totals, audit_updates = {}, {}
    check_receipts(args.production_dir, args.config, F)
    validate_tables(args.production_dir, F, totals, audit_updates)
    if totals.get("blocks") != N or totals.get("queries") != C.TOTAL_QUERIES:
        F.add("block or query total differs from frozen counts")
    perms = load_permutations(args.production_dir, F)
    if len(audit_updates) == C.AUDIT_BLOCKS and perms is not None:
        validate_audit(args.production_dir, F, audit_updates, perms)
    else:
        F.add("audit-block update records or permutation records unavailable")

    if F.count:
        result = {"primary_decision": INVALID, "terminal": True, "failure_count": F.count, "failures": F.items,
                  "estimates_computed": False,
                  "note": "Post-production INVALID is terminal; no estimate was computed or written."}
        write_json(os.path.join(args.out, "ROUTE_CLOSED_INVALID.json"), result)
        os.chmod(lock, 0o644)
        with open(lock, "w") as f:
            json.dump({"status": "TERMINAL_INVALID", "out": os.path.abspath(args.out)}, f)
        os.chmod(lock, 0o444)
        return 3

    records, decision = estimates(totals)
    diagnostics = {"descriptive_only": True, "cells": list(C.CELL_NAMES),
                   "total_objective_queries": totals["queries"],
                   "blocks_where_NONINFO_decoupled_from_INFO_by_start": totals.get("decoupled_blocks", [0, 0]),
                   "extreme_displacement_note": "decoy_w0/decoy_w32 count NONINFO decoy applications whose "
                   "displacement weight is 0 or 32 and therefore retain direction; valid_m_disp_w0/w32 count "
                   "valid-M parents at those weights in every arm."}
    names = {"f2m": "f_to_m", "m2f": "m_to_f", "use": "valid_probe_use", "surv": "valid_probe_survivors",
             "ret": "survival_retries", "fix": "fixation_updates", "ext": "extinction_updates",
             "lfix": "late_fixation_updates", "lext": "late_extinction_updates", "tuse": "true_cache_use",
             "duse": "decoy_use", "tsurv": "true_cache_survivors", "dsurv": "decoy_survivors",
             "w0": "valid_m_disp_w0", "w32": "valid_m_disp_w32", "dw0": "decoy_w0", "dw32": "decoy_w32",
             "dident": "decoy_identical_to_cache", "rec": "recurrent_updates",
             "pret": "decoy_permutation_retries"}
    for key in DIAG_KEYS:
        diagnostics["per_cell_total_" + names[key]] = totals["diag_" + key]
    write_json(os.path.join(args.out, "estimates_19.json"), records)
    write_json(os.path.join(args.out, "decision.json"), decision)
    write_json(os.path.join(args.out, "diagnostics_descriptive.json"), diagnostics)
    os.chmod(lock, 0o644)
    with open(lock, "w") as f:
        json.dump({"status": "COMPLETE", "decision": decision["primary_decision"],
                   "out": os.path.abspath(args.out)}, f)
    os.chmod(lock, 0o444)
    return 0


if __name__ == "__main__":
    sys.exit(main())
