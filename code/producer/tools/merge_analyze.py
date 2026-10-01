#!/usr/bin/env python3
"""Deterministic merge and analysis for PHASE2-MUTABLE-MEMORY-001 revision 1 (stdlib only).

usage: merge_analyze.py --config config/frozen_config.json --production-dir DIR --out NEW_ANALYSIS_DIR

Order of work (frozen decision table, spec section 10):
  0. analyzer self-checks (no production data read);
  1. exclusive ANALYSIS_LOCK.json in the production directory (a second analysis is refused);
  2. validity checks: manifest, input hashes, preflight receipts, purpose keys, output hashes,
     file sizes, per-update -> per-path -> per-block reconstruction, N1/N2 (count level and
     SHAM paired-trajectory SHA-256 equality in every block), 128-query counts, candidate-level
     audit replay (Lemire + sequential selection + state continuity + cross-cell target pairing);
  3. any failure -> INVALID. Post-production INVALID is terminal: no estimate is computed or
     written, ROUTE_CLOSED_INVALID.json is written and the lock forbids any re-analysis;
  4. otherwise the 22 saved records, the primary decision (rules 2-6 in order) and the
     separate secondary classifications are computed exactly (integer sums, Fraction means,
     60-digit Decimal Hoeffding half-widths).
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

# name, range length R, block-value denominator, frozen rounded half-width
PRIMARY = (("C_abs", 1, 4096, "0.00781023"), ("C_rec", 2, 4096, "0.01562046"),
           ("D_HALF", 2, 2048, "0.01562046"), ("D_ZERO", 2, 2048, "0.01562046"))
SECONDARY = (("P_abs", 2, 131072, "0.01451463"), ("P_rec", 4, 131072, "0.02902925"))
ALPHA_PRIMARY = "0.0125"
ALPHA_SECONDARY = "0.025"

INVALID = "INVALID"
START_DEPENDENT = "START-DEPENDENT; SCIENTIFIC QUESTION UNRESOLVED"
SUPPORTS = "SUPPORTS RECURRENCE-ATTRIBUTABLE SELECTIVE ENRICHMENT"
NOT_ATTRIBUTABLE = "SELECTIVE ENRICHMENT NOT ATTRIBUTABLE TO RECURRENCE AT THE FIXED SCALE"
BOUNDED_NEGATIVE = "BOUNDED NEGATIVE AT THE ONE-INDIVIDUAL SCALE"
UNRESOLVED = "UNRESOLVED"
SEC_POS = "MEANINGFUL POSITIVE PERFORMANCE"
SEC_ADV = "MEANINGFUL ADVERSE PERFORMANCE"
SEC_BOUNDED = "BOUNDED BELOW THE POSITIVE ONE-BIT SCALE"
SEC_UNRES = "UNRESOLVED"

INTERPRETATION_LABELS = [
    "HALF CACHE ALIGNMENT: the HALF law is a prior outcome-informed choice that exactly matches the one-generation "
    "cache delay to a two-update target return; it favors retrieval by construction. No other lag, copy probability "
    "or target law was tested.",
    "OPPORTUNITY COST: a valid ACTIVE-M carrier replaces exactly one uniform fresh proposal (one of two effectively "
    "uniform random proposals) with its cache; its magnitude is conditional on the frozen fresh-probe distribution "
    "and is not a general cost scale.",
    "START-DEPENDENCE GATE: rule 2 (D_HALF and D_ZERO wholly inside (-1/32, +1/32)) is applied before any "
    "enrichment classification.",
    "ADVERSE OUTCOMES: under BOUNDED NEGATIVE, an upper C_abs bound below -1/32 indicates adverse selection; "
    "otherwise only 'not enriched by one expected individual' is admissible.",
    "SECONDARY POPULATION-PERFORMANCE LIMITS: P_abs and P_rec form a separate family that cannot alter the primary "
    "decision; P_rec half-width 0.029 is close to Delta_P = 1/32 and is often unresolved.",
    "CLAIM LIMITS: no claim of rare invasion, mechanism origin, arbitrary recurrence, general cost scale, "
    "stationarity, equilibrium, evolutionary stability, population benefit, biological extrapolation, global "
    "novelty or replication of studies 056-060.",
]


def dec(x):
    """Exact Fraction -> Decimal at PREC digits."""
    with localcontext() as ctx:
        ctx.prec = PREC
        return Decimal(x.numerator) / Decimal(x.denominator)


def half_width(range_length, alpha_each, n=N):
    with localcontext() as ctx:
        ctx.prec = PREC
        inner = (Decimal(2) / Decimal(alpha_each)).ln() / (Decimal(2) * Decimal(n))
        return Decimal(range_length) * inner.sqrt()


def primary_decision(iv):
    """iv: name -> (lower, upper) Decimals. Returns (label, adverse_selection_or_None, rule)."""
    d = Decimal(1) / Decimal(32)
    for name in ("D_HALF", "D_ZERO"):
        lo, hi = iv[name]
        if not (lo > -d and hi < d):
            return START_DEPENDENT, None, 2
    c_lo, c_hi = iv["C_abs"]
    r_lo, r_hi = iv["C_rec"]
    if c_lo > d and r_lo > d:
        return SUPPORTS, None, 3
    if c_lo > d and r_hi <= d:
        return NOT_ATTRIBUTABLE, None, 4
    if c_hi <= d:
        return BOUNDED_NEGATIVE, bool(c_hi < -d), 5
    return UNRESOLVED, None, 6


def secondary_class(lo, hi):
    d = Decimal(1) / Decimal(32)
    if lo > d:
        return SEC_POS
    if hi < -d:
        return SEC_ADV
    if hi <= d:
        return SEC_BOUNDED
    return SEC_UNRES


def relation_to_enrichment(sec_label, primary_label, estimand):
    """Descriptive agreement/disagreement/uncertainty label; cannot alter any decision."""
    if estimand == "P_abs":
        pos = primary_label in (SUPPORTS, NOT_ATTRIBUTABLE)
        neg = primary_label == BOUNDED_NEGATIVE
    else:
        pos = primary_label == SUPPORTS
        neg = primary_label == NOT_ATTRIBUTABLE
    if sec_label == SEC_UNRES or not (pos or neg):
        return "UNCERTAINTY"
    sec_pos = sec_label == SEC_POS
    return "AGREEMENT" if sec_pos == pos else "DISAGREEMENT"


def self_checks():
    errs = []
    for name, r, _den, frozen in PRIMARY:
        if abs(half_width(r, ALPHA_PRIMARY) - Decimal(frozen)) > Decimal("0.000000005"):
            errs.append("half-width mismatch " + name)
    for name, r, _den, frozen in SECONDARY:
        if abs(half_width(r, ALPHA_SECONDARY) - Decimal(frozen)) > Decimal("0.000000005"):
            errs.append("half-width mismatch " + name)
    if not half_width(2, ALPHA_PRIMARY) < Decimal(1) / Decimal(64):
        errs.append("range-2 primary half-width not below Delta/2")
    return errs


class Failures(object):
    def __init__(self):
        self.items = []
        self.count = 0

    def add(self, msg):
        self.count += 1
        if len(self.items) < MAX_LISTED_FAILURES:
            self.items.append(msg)


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
    for k in ("n1_failed_blocks", "n2_failed_blocks", "query_failed_blocks"):
        if man.get(k) != 0:
            F.add("manifest %s = %r" % (k, man.get(k)))
    if man.get("total_objective_queries") != C.TOTAL_QUERIES:
        F.add("manifest query total differs from 10,905,190,400")
    expected = set()
    for s in range(C.SHARDS):
        for n in ("updates.bin", "paths.bin", "blocks.bin") + ((C.AUDIT_FILE,) if s == 0 else ()):
            expected.add("shards/shard_%02d/%s" % (s, n))
    listed = {o["path"]: o for o in man.get("outputs", [])}
    if set(listed) != expected:
        F.add("manifest output list differs from the fixed shard layout")
    for rel in sorted(expected & set(listed)):
        path = os.path.join(prod, rel)
        if not os.path.exists(path) or C.sha256_file(path) != listed[rel]["sha256"]:
            F.add("output hash mismatch " + rel)
    return man


def new_acc():
    return {"late_m": 0, "late_mm": 0, "q": 0, "f2m": 0, "m2f": 0, "use": 0, "surv": 0, "ret": 0,
            "fix": 0, "ext": 0, "lfix": 0, "lext": 0, "final_m": 0, "final_mm": 0}


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


def validate_block(block, urecs, precs, brec, F, totals):
    accs = []
    for c in range(C.CELLS):
        a = new_acc()
        arm = c >> 2
        for t in range(1, C.UPDATES + 1):
            (b, u, cell, m, mm, valid, use, surv, f2m, m2f, q, ret) = urecs[c * C.UPDATES + t - 1]
            if b != block or u != t or cell != c:
                F.add("update record order error block %d cell %d update %d" % (block, c, t))
            if q != C.CANDIDATES:
                F.add("query count %d at block %d cell %d update %d" % (q, block, c, t))
            if m > C.POP or mm > C.POP * 32 or valid > C.POP or use > valid or surv > use or f2m + m2f > C.POP:
                F.add("update record range error block %d cell %d update %d" % (block, c, t))
            if arm == 1 and use != 0:
                F.add("SHAM cache use at block %d cell %d update %d" % (block, c, t))
            late = C.LATE_FIRST <= t <= C.LATE_LAST
            if late:
                a["late_m"] += m
                a["late_mm"] += mm
            a["q"] += q
            a["f2m"] += f2m
            a["m2f"] += m2f
            a["use"] += use
            a["surv"] += surv
            a["ret"] += ret
            if m == C.POP:
                a["fix"] += 1
                a["lfix"] += int(late)
            if m == 0:
                a["ext"] += 1
                a["lext"] += int(late)
            a["final_m"] = m
            a["final_mm"] = mm
        p = precs[c]
        want = (block, c, c >> 2, (c >> 1) & 1, c & 1, a["late_m"], a["late_mm"], a["q"], a["f2m"], a["m2f"],
                a["use"], a["surv"], a["ret"], a["fix"], a["ext"], a["lfix"], a["lext"])
        if tuple(p[:17]) != want or p[18] != a["final_m"] or p[19] != a["final_mm"] or p[20]:
            F.add("path summary does not reconstruct from per-update records: block %d cell %d" % (block, c))
        accs.append(a)
        totals["queries"] = totals.get("queries", 0) + a["q"]
        for key in ("f2m", "m2f", "use", "surv", "ret", "fix", "ext", "lfix", "lext"):
            totals.setdefault("diag_" + key, [0] * C.CELLS)[c] += a[key]
    for t in range(C.UPDATES):
        for x, y in ((4, 5), (6, 7)):
            ux, uy = urecs[x * C.UPDATES + t], urecs[y * C.UPDATES + t]
            if ux[3] + uy[3] != C.POP or ux[4] != uy[4]:
                F.add("N1/N2 violated in update records: block %d update %d" % (block, t + 1))
    # Paired-trajectory SHA-256 (path field 21) binds targets, survivor indices, genotypes and
    # start-normalized labels after every update: SHAM ALL_F/ALL_M pairs must be identical.
    for x, y in ((4, 5), (6, 7)):
        if precs[x][21] != precs[y][21]:
            F.add("N1/N2 paired-trajectory SHA-256 differs: block %d cells %d/%d" % (block, x, y))
    L =[a["late_m"] for a in accs]
    P = [a["late_mm"] for a in accs]
    p_abs = (P[6] + P[7]) - (P[2] + P[3])
    p_rec = p_abs - ((P[4] + P[5]) - (P[0] + P[1]))
    nums = ((L[2] + L[3]) - 2048, (L[2] + L[3]) - (L[0] + L[1]), L[3] - L[2], L[1] - L[0], p_abs, p_rec)
    want_block = (block, 1, 1, 1, int(block < C.AUDIT_BLOCKS)) + nums + tuple(L) + tuple(P)
    if tuple(brec) != want_block:
        F.add("block record does not reconstruct from path summaries (or N1/N2/query flag set): block %d" % block)
    limits = (2048, 4096, 2048, 2048, 131072, 262144)
    if any(abs(v) > lim for v, lim in zip(nums, limits)):
        F.add("block variable out of range: block %d" % block)
    totals["blocks"] = totals.get("blocks", 0) + 1
    sums = totals.setdefault("num", [0] * 6)
    for i, v in enumerate(nums):
        sums[i] += v
    for c in range(C.CELLS):
        totals.setdefault("cell_m", [0] * C.CELLS)[c] += L[c]
        totals.setdefault("cell_mm", [0] * C.CELLS)[c] += P[c]


def popcount(x):
    return bin(x).count("1")


def validate_audit(prod, F, audit_updates):
    path = os.path.join(C.shard_dir(prod, 0), C.AUDIT_FILE)
    if not os.path.exists(path) or os.path.getsize(path) != C.AUDIT_ROWS * C.AUDIT_ROW.size:
        F.add("audit file missing or wrong size")
        return
    group = C.CANDIDATES * C.AUDIT_ROW.size
    with open(path, "rb") as f:
        for block in range(C.AUDIT_BLOCKS):
            init_geno = None
            pairs = {}  # one (cell, update) -> (target, copy_bit) map per block, across all cells
            for c in range(C.CELLS):
                arm, law, start = c >> 2, (c >> 1) & 1, c & 1
                prev = None
                for t in range(1, C.UPDATES + 1):
                    rows = list(C.AUDIT_ROW.iter_unpack(f.read(group)))
                    prev = check_group(block, c, t, arm, law, start, rows, prev, F, audit_updates, pairs)
                    if t == 1:
                        g = tuple(r[9] for r in rows[:C.POP])
                        if init_geno is None:
                            init_geno = g
                        elif g != init_geno:
                            F.add("initial genotypes differ across cells: block %d" % block)


def paired_target_errors(c, t, target, copy_bit, rec, pairs):
    """Cross-cell target and copy-bit pairing for cell c at update t. Pure: reads only its arguments.

    pairs maps (cell, update) -> (target, copy_bit) for every group already checked in this block.
    Cells are visited 0..7, each over updates 1..256 (as validate_audit reads them), so cell 0 at
    update t, cell 2 at update t (for c > 2), cell c-4 at update t and cell c at t-2 are present
    whenever they are needed. A missing reference counts as a failure.
    """
    arm, law = c >> 2, (c >> 1) & 1

    def target_of(cell, u):
        v = pairs.get((cell, u))
        return None if v is None else v[0]

    errs = []
    if c != 0:
        ref = pairs.get((0, t))
        if ref is None or copy_bit != ref[1]:
            errs.append("copy bit R_t differs from cell 0")
    if law == 0 or rec == 0:
        if c != 0 and target_of(0, t) != target:
            errs.append("ZERO or non-recurrent HALF target differs from cell 0 innovation")
    elif target_of(c, t - 2) != target:
        errs.append("HALF recurrence does not copy this cell's T_(t-2)")
    if law == 1 and c != 2 and target_of(2, t) != target:
        errs.append("HALF target differs from cell 2")
    if arm == 1 and target_of(c - 4, t) != target:
        errs.append("ACTIVE/SHAM target differ")
    return errs


def check_group(block, c, t, arm, law, start, rows, prev, F, audit_updates, pairs):
    where = "block %d cell %d update %d" % (block, c, t)
    target, copy_bit, rec = rows[0][10], rows[0][12], rows[0][13]
    weights = []
    for j, r in enumerate(rows):
        (b, u, cell, cand, fam, parent, plab, pvalid, pcache, geno, tgt, mis, cb, ra, src,
         w, rank, post, flip, donor, W, x, Z, retry, res) = r
        if (b, u, cell, cand, fam, parent) != (block, t, c, j, j // C.POP, j % C.POP) or res:
            F.add("audit row order error " + where)
        if tgt != target or cb != copy_bit or ra != rec or mis != popcount(geno ^ tgt) or w != 1 << (32 - mis):
            F.add("audit row mismatch/weight error " + where)
        if fam == 1:
            uses = arm == 0 and plab == 1 and pvalid == 1
            if src != int(uses) or (uses and geno != pcache):
                F.add("policy probe source error " + where)
        weights.append(w)
    if rec != int(law == 1 and t >= 3 and copy_bit == 1):
        F.add("recurrence flag error " + where)
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
    # Reconstruct the per-update record.
    u = audit_updates[block][c * C.UPDATES + t - 1]
    fam0 = rows[:C.POP]
    want = (sum(r[17] for r in sel), sum(r[11] for r in sel),
            sum(1 for r in fam0 if r[6] == 1 and r[7] == 1),
            sum(1 for r in rows if r[4] == 1 and r[14] == 1),
            sum(1 for r in sel if r[4] == 1 and r[14] == 1),
            sum(1 for r in sel if r[6] == 0 and r[17] == 1),
            sum(1 for r in sel if r[6] == 1 and r[17] == 0), C.CANDIDATES, sum(r[23] for r in sel))
    if (u[3], u[4], u[5], u[6], u[7], u[8], u[9], u[10], u[11]) != want:
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


def estimates(totals):
    hp = {r: half_width(r, ALPHA_PRIMARY) for r in (1, 2)}
    hs = {r: half_width(r, ALPHA_SECONDARY) for r in (2, 4)}
    records, iv = [], {}
    specs = [(s, "PRIMARY_ALLELE_ENRICHMENT", ALPHA_PRIMARY, hp[s[1]]) for s in PRIMARY]
    specs += [(s, "SECONDARY_POPULATION_PERFORMANCE", ALPHA_SECONDARY, hs[s[1]]) for s in SECONDARY]
    for i, ((name, r, den, _), family, alpha, h) in enumerate(specs):
        mean = Fraction(totals["num"][i], den * N)
        with localcontext() as ctx:
            ctx.prec = PREC
            lo, hi = dec(mean) - h, dec(mean) + h
        iv[name] = (lo, hi)
        records.append({"record": i + 1, "name": name, "family": family, "estimate_exact": "%d/%d" % (
            mean.numerator, mean.denominator), "estimate": str(dec(mean)), "range_length": r, "alpha_each": alpha,
            "n_blocks": N, "half_width": str(h), "lower": str(lo), "upper": str(hi)})
    label, adverse, rule = primary_decision(iv)
    for rec in records[:4]:
        rec["family_decision"] = label
    for rec in records[4:]:
        rec["classification"] = secondary_class(*iv[rec["name"]])
        rec["relation_to_allele_enrichment_descriptive"] = relation_to_enrichment(rec["classification"], label,
                                                                                  rec["name"])
        rec["cannot_alter_primary_decision"] = True
    k = len(records)
    for c in range(C.CELLS):
        mean = Fraction(totals["cell_m"][c], 2048 * N)
        records.append({"record": k + 1 + c, "name": "M_FREQUENCY_LATE|" + C.CELL_NAMES[c],
                        "family": "ABSOLUTE_CELL_MEAN_DESCRIPTIVE", "estimate_exact": "%d/%d" % (
                            mean.numerator, mean.denominator), "estimate": str(dec(mean)),
                        "lower": None, "upper": None, "classification": "DESCRIPTIVE; NO INFERENTIAL BOUND FROZEN"})
    for c in range(C.CELLS):
        mean = 1 - Fraction(totals["cell_mm"][c], 65536 * N)
        records.append({"record": k + 9 + c, "name": "ACCURACY_LATE|" + C.CELL_NAMES[c],
                        "family": "ABSOLUTE_CELL_MEAN_DESCRIPTIVE", "estimate_exact": "%d/%d" % (
                            mean.numerator, mean.denominator), "estimate": str(dec(mean)),
                        "lower": None, "upper": None, "classification": "DESCRIPTIVE; NO INFERENTIAL BOUND FROZEN"})
    assert len(records) == 22
    decision = {"primary_decision": label, "decision_rule_applied": rule,
                "adverse_selection_upper_C_abs_below_minus_Delta": adverse,
                "secondary_classifications": {r["name"]: r["classification"] for r in records[4:6]},
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
    if len(audit_updates) == C.AUDIT_BLOCKS:
        validate_audit(args.production_dir, F, audit_updates)
    else:
        F.add("audit-block update records unavailable")

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
                   "total_objective_queries": totals["queries"]}
    for key in ("f2m", "m2f", "use", "surv", "ret", "fix", "ext", "lfix", "lext"):
        diagnostics["per_cell_total_" + key] = totals["diag_" + key]
    write_json(os.path.join(args.out, "estimates_22.json"), records)
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
