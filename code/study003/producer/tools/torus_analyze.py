#!/usr/bin/env python3
"""PHASE2-TORUS-MEMORY-003 merge/analyze utility (standard library only).

Accepts only one complete, authenticated production set (namespace
production-r1, 650 chunks, 41,600 blocks) and emits exactly 22 frozen
estimate records plus the ordered decisions of design sections 10-12.

* Every per-path summary is reconstructed from the per-update rows and every
  per-block variable from the path rows; N1/N2 are re-verified over all blocks
  from saved digests and per-update M counts.
* All accumulation is exact (Python int / Fraction). Hoeffding half-widths are
  irrational; every sign/threshold decision compares an exact rational with a
  rigorously bracketed high-precision Decimal half-width, refining precision
  until the comparison is decided. Display decimals never decide anything.
* Authentication/completeness failures abort with no output. A mechanical
  identity failure inside an authenticated set yields decision INVALID.

NOT YET RUN. No scientific outcome exists.
"""

import argparse
import hashlib
import json
import os
import sys
from decimal import Decimal, localcontext, ROUND_CEILING, ROUND_FLOOR, ROUND_HALF_EVEN
from fractions import Fraction

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import torus_schema as S  # noqa: E402

DESIGN_SHA256 = "0ff662dc31807b0b2cdcae371d884473ed398e2583272d94331bac8c8f9ff0b0"
DESIGN_GO_SHA256 = "e5dc485dd858f18eb9652cfe6b63f142e2d09fb6b069d640438cd8dbe63a786d"
NAMESPACE = "production-r1"

DELTA = Fraction(1, 32)
DELTA_P = Fraction(1, 32)
ALPHA_PRIMARY = Fraction(1, 20) / 4
ALPHA_PERF = Fraction(1, 20) / 2
N = S.BLOCKS
DOCUMENTED_H = {  # design sections 10 and 12 (float displays); checked to 1e-16
    (Fraction(1), ALPHA_PRIMARY): "0.007810229527949401",
    (Fraction(2), ALPHA_PRIMARY): "0.015620459055898803",
    (Fraction(2), ALPHA_PERF): "0.014514625638859732",
    (Fraction(4), ALPHA_PERF): "0.029029251277719464",
}
DISPLAY_PLACES = 40


class Abort(Exception):
    """Authentication or completeness failure: no estimate may be emitted."""


class FailureLog:
    """Counts every identity failure; keeps the first 1000 messages."""

    def __init__(self):
        self.count = 0
        self.items = []

    def append(self, msg):
        self.count += 1
        if len(self.items) < 1000:
            self.items.append(msg)

    def __len__(self):
        return self.count


# --------------------------------------------------------------------------- intervals
def halfwidth_bracket(range_len, alpha, n, digits):
    """Return (lo, hi) Fractions with lo < h < hi, h = R*sqrt(ln(2/alpha)/(2n)).

    Decimal ln/sqrt are correctly rounded at the working precision; with
    `digits + 20` significant digits and h < 10 the absolute error is far below
    10**-digits, so widening by 10**-digits gives a rigorous bracket.
    """
    with localcontext() as ctx:
        ctx.prec = digits + 20
        two_over_alpha = Decimal((2 / alpha).numerator) / Decimal((2 / alpha).denominator)
        h = (Decimal(range_len.numerator) / Decimal(range_len.denominator)) * (two_over_alpha.ln() / Decimal(2 * n)).sqrt()
    eps = Fraction(1, 10 ** digits)
    hf = Fraction(h)
    return hf - eps, hf + eps


def exceeds_halfwidth(x, range_len, alpha, n):
    """Exact decision of x > h for rational x (h is irrational, so never equal)."""
    if x <= 0:
        return False
    digits = 60
    while digits <= 4000:
        lo, hi = halfwidth_bracket(range_len, alpha, n, digits)
        if x >= hi:
            return True
        if x <= lo:
            return False
        digits *= 2
    raise Abort("half-width comparison not resolved at 4000 digits")


def decimal_display(q, rounding):
    with localcontext() as ctx:
        ctx.prec = 200
        d = Decimal(q.numerator) / Decimal(q.denominator)
        s = format(d.quantize(Decimal(1).scaleb(-DISPLAY_PLACES), rounding=rounding), "f")
    if len(s) > 44:
        raise Abort("display field overflow")
    return s


def interval_display(est, range_len, alpha, n):
    lo, hi = halfwidth_bracket(range_len, alpha, n, 120)
    # Outward-rounded displays: lower uses the upper half-width bound, rounded down.
    return {
        "half_width": decimal_display(hi, ROUND_CEILING),
        "lower": decimal_display(est - hi, ROUND_FLOOR),
        "upper": decimal_display(est + hi, ROUND_CEILING),
    }


def check_documented_halfwidths():
    for (r, a), text in DOCUMENTED_H.items():
        lo, hi = halfwidth_bracket(r, a, N, 60)
        doc = Fraction(Decimal(text))
        if abs(doc - lo) > Fraction(1, 10 ** 16):
            raise Abort("computed Hoeffding half-width disagrees with the frozen design for R=%s" % r)


# --------------------------------------------------------------------------- authentication
def load_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def authenticate(prod, config_path, design_spec, design_go):
    if S.sha256_file(design_spec)[0] != DESIGN_SHA256:
        raise Abort("design specification SHA-256 mismatch")
    if S.sha256_file(design_go)[0] != DESIGN_GO_SHA256:
        raise Abort("design-GO record SHA-256 mismatch")
    complete = load_json(os.path.join(prod, "PRODUCTION_COMPLETE.json"))
    if complete.get("status") != "PRODUCTION_FILES_COMPLETE_UNANALYZED":
        raise Abort("production completion record does not carry the exact successful status")
    man_path = os.path.join(prod, "production_manifest.json")
    man_sha = S.sha256_file(man_path)[0]
    if complete.get("production_manifest_sha256") != man_sha:
        raise Abort("production manifest hash does not match the completion record")
    man = load_json(man_path)
    if man.get("namespace") != NAMESPACE:
        raise Abort("not a production-r1 output set")
    if man.get("design_sha256") != DESIGN_SHA256 or man.get("design_go_record_sha256") != DESIGN_GO_SHA256:
        raise Abort("manifest identities do not match the frozen design")
    cfg_sha = S.sha256_file(config_path)[0]
    if man.get("config_sha256") != cfg_sha:
        raise Abort("configuration hash does not match the production manifest")
    for key, want in (("blocks", S.BLOCKS), ("chunks", S.CHUNKS), ("paths", S.PATHS),
                      ("path_updates", S.PATH_UPDATES), ("objective_queries", S.OBJECTIVE_QUERIES)):
        if man.get(key) != want:
            raise Abort("manifest count mismatch: %s" % key)
    listed = {}
    for e in man["files"]:
        listed[e["path"]] = e
    expected = set()
    for c in range(S.CHUNKS):
        expected.add("updates/chunk_%05d.t3u" % c)
        expected.add("paths/chunk_%05d.t3p" % c)
        expected.add("blocks/chunk_%05d.t3b" % c)
    expected |= {"audit/audit_context.t3x", "audit/audit_candidates.t3c", "audit/audit_entries.t3e",
                 "preflight/key_derivation.json", "preflight/collision_receipt.json",
                 "preflight/invariance_receipt.json", "preflight/identity.json"}
    if set(listed) != expected:
        raise Abort("manifest file set is not the complete frozen production set")
    on_disk = set()
    for sub in ("updates", "paths", "blocks", "audit", "preflight"):
        for name in os.listdir(os.path.join(prod, sub)):
            on_disk.add(sub + "/" + name)
    if on_disk != expected:
        raise Abort("files on disk differ from the manifest")
    for rel in sorted(expected):
        digest, size = S.sha256_file(os.path.join(prod, rel))
        if digest != listed[rel]["sha256"] or size != listed[rel]["bytes"]:
            raise Abort("file hash/size mismatch: %s" % rel)
    return man, man_sha, cfg_sha


# --------------------------------------------------------------------------- reconstruction
def read_file(path):
    with open(path, "rb") as f:
        return f.read()


def verify_preflight(prod, failures):
    kd = load_json(os.path.join(prod, "preflight/key_derivation.json"))
    if kd.get("namespace") != NAMESPACE or len(kd.get("purposes", [])) != len(S.PURPOSES):
        failures.append("key derivation receipt malformed")
    else:
        for p, entry in zip(S.PURPOSES, kd["purposes"]):
            words = ["%016x" % w for w in S.purpose_key_words(NAMESPACE, p)]
            if entry.get("purpose") != p or entry.get("key_words_hex") != words:
                failures.append("independent key derivation mismatch for %s" % p)
    if load_json(os.path.join(prod, "preflight/collision_receipt.json")).get("status") != "NO_COLLISION":
        failures.append("collision receipt status is not NO_COLLISION")
    if load_json(os.path.join(prod, "preflight/invariance_receipt.json")).get("status") != "INVARIANT":
        failures.append("invariance receipt status is not INVARIANT")
    for name, kind, rows in (("audit_context.t3x", "context", S.AUDIT_CONTEXT_ROWS),
                             ("audit_candidates.t3c", "candidate", S.AUDIT_CANDIDATE_ROWS),
                             ("audit_entries.t3e", "entry", S.AUDIT_ENTRY_ROWS)):
        path = os.path.join(prod, "audit", name)
        with open(path, "rb") as f:
            head = f.read(S.HEADER_BYTES)
        size = os.path.getsize(path)
        try:
            # Validate the header against the full file size without loading the payload.
            (magic, version, endian, row_size, hsize, count, ns, chunk, first, nblk, res) = S.HEADER.unpack_from(head, 0)
            ok = (magic == S.MAGIC[kind] and version == S.SCHEMA_VERSION and endian == S.ENDIAN_MARKER and
                  row_size == S.ROW_BYTES[kind] and hsize == S.HEADER_BYTES and count == rows and
                  S.fixed_ascii(ns) == NAMESPACE and chunk == 0 and first == 0 and nblk == 64 and res == 0 and
                  size == S.HEADER_BYTES + rows * row_size)
        except Exception:  # noqa: BLE001
            ok = False
        if not ok:
            failures.append("audit file header/size invalid: %s" % name)


def reconstruct(prod, failures):
    """Returns per-block BLOCK tuples (verified) and accumulates identity failures."""
    blocks = []
    late = S.LATE_FIRST - 1
    for chunk in range(S.CHUNKS):
        first = chunk * S.CHUNK_BLOCKS
        try:
            ub = read_file(os.path.join(prod, "updates/chunk_%05d.t3u" % chunk))
            pb = read_file(os.path.join(prod, "paths/chunk_%05d.t3p" % chunk))
            bb = read_file(os.path.join(prod, "blocks/chunk_%05d.t3b" % chunk))
            S.parse_header(ub, "update", NAMESPACE, S.CHUNK_BLOCKS * S.CELLS * S.UPDATES, chunk, first, S.CHUNK_BLOCKS)
            S.parse_header(pb, "path", NAMESPACE, S.CHUNK_BLOCKS * S.CELLS, chunk, first, S.CHUNK_BLOCKS)
            S.parse_header(bb, "block", NAMESPACE, S.CHUNK_BLOCKS, chunk, first, S.CHUNK_BLOCKS)
        except S.SchemaError as e:
            raise Abort("chunk %d schema: %s" % (chunk, e))
        for lb in range(S.CHUNK_BLOCKS):
            block = first + lb
            paths = []
            m_series = []
            for cell in range(S.CELLS):
                pidx = lb * S.CELLS + cell
                off = S.HEADER_BYTES + pidx * S.UPDATES * S.UPDATE.size
                raw = ub[off: off + S.UPDATES * S.UPDATE.size]
                rows = list(S.UPDATE.iter_unpack(raw))
                late_m = late_loss = all_m = all_loss = q = 0
                cpu = cpw = f2m = m2f = dup = flg = chg = 0
                ms = []
                for t, r in enumerate(rows, start=1):
                    (loss, upd, qc, lf, lc, m, vpre, cu, cw, fm, mf, d, dw) = r
                    if upd != t or qc != S.CANDIDATES or loss > S.MAX_POP_LOSS or m > S.POP:
                        failures.append("update-row identity failure block %d cell %d t %d" % (block, cell, t))
                    ms.append(m)
                    if t > late:
                        late_m += m
                        late_loss += loss
                    all_m += m
                    all_loss += loss
                    q += qc
                    cpu += cu
                    cpw += cw
                    f2m += fm
                    m2f += mf
                    dup += d
                    flg += lf
                    chg += lc
                p = S.PATH.unpack_from(pb, S.HEADER_BYTES + pidx * S.PATH.size)
                want = (block, cell, cell >> 2, (cell >> 1) & 1, cell & 1, late_m, q, late_loss, all_loss, all_m,
                        cpu, cpw, f2m, m2f, dup, flg, chg)
                if tuple(p[:17]) != want:
                    failures.append("path summary does not reconstruct: block %d cell %d" % (block, cell))
                if p[18] != hashlib.sha256(raw).digest():
                    failures.append("update-record digest mismatch: block %d cell %d" % (block, cell))
                if q != S.UPDATES * S.CANDIDATES:
                    failures.append("query count failure: block %d cell %d" % (block, cell))
                paths.append(p)
                m_series.append(ms)
            b = S.BLOCK.unpack_from(bb, S.HEADER_BYTES + lb * S.BLOCK.size)
            lm = [paths[c][5] for c in range(S.CELLS)]
            ll = [paths[c][7] for c in range(S.CELLS)]
            c_abs = (lm[2] + lm[3]) - 2048
            c_rec = (lm[2] + lm[3]) - (lm[0] + lm[1])
            d_half = 2 * (lm[3] - lm[2])
            d_zero = 2 * (lm[1] - lm[0])
            half_gap = (ll[6] + ll[7]) - (ll[2] + ll[3])
            zero_gap = (ll[4] + ll[5]) - (ll[0] + ll[1])
            flags = 0
            if paths[4][19] == paths[5][19]:
                flags |= S.FLAG_N1_ZERO
            if paths[6][19] == paths[7][19]:
                flags |= S.FLAG_N1_HALF
            if paths[4][20] == paths[5][21]:
                flags |= S.FLAG_N2_ZERO
            if paths[6][20] == paths[7][21]:
                flags |= S.FLAG_N2_HALF
            if (lm[4] + lm[5] == 2048 and lm[6] + lm[7] == 2048 and
                    all(a + c == S.POP for a, c in zip(m_series[4], m_series[5])) and
                    all(a + c == S.POP for a, c in zip(m_series[6], m_series[7]))):
                flags |= S.FLAG_N2_SUM
            if all(paths[c][6] == S.UPDATES * S.CANDIDATES for c in range(S.CELLS)):
                flags |= S.FLAG_QUERIES
            want_b = (block, flags) + tuple(lm) + tuple(ll) + (c_abs, c_rec, d_half, d_zero, half_gap, half_gap - zero_gap)
            if tuple(b) != want_b:
                failures.append("block record does not reconstruct: block %d" % block)
            if flags != S.FLAG_ALL_VALID:
                failures.append("N1/N2/query identity failure: block %d flags 0x%02x" % (block, flags))
            blocks.append(want_b)
    if len(blocks) != S.BLOCKS or [b[0] for b in blocks] != list(range(S.BLOCKS)):
        raise Abort("block set is not exactly 0..41599")
    return blocks


# --------------------------------------------------------------------------- estimates
def estimate_specs(blocks):
    """(index, name, family, per-block numerators sum, per-block denominator, range length, alpha or None)."""
    def col(i):
        return sum(b[i] for b in blocks)
    specs = [
        (0, "C_abs", 0, col(18), 4096, Fraction(1), ALPHA_PRIMARY),
        (1, "C_rec", 0, col(19), 4096, Fraction(2), ALPHA_PRIMARY),
        (2, "D_HALF", 0, col(20), 4096, Fraction(2), ALPHA_PRIMARY),
        (3, "D_ZERO", 0, col(21), 4096, Fraction(2), ALPHA_PRIMARY),
        (4, "P_abs", 1, col(22), 1 << 47, Fraction(2), ALPHA_PERF),
        (5, "P_rec", 1, col(23), 1 << 47, Fraction(4), ALPHA_PERF),
    ]
    for c in range(S.CELLS):
        specs.append((6 + c, "MFREQ_" + S.CELL_NAMES[c], 2, col(2 + c), 2048, Fraction(1), None))
    for c in range(S.CELLS):
        specs.append((14 + c, "ACC_" + S.CELL_NAMES[c], 2, N * (1 << 46) - col(10 + c), 1 << 46, Fraction(1), None))
    assert len(specs) == 22
    return specs


CODES = {
    "INVALID": 0,
    "LOWER_ABOVE_DELTA": 10, "UPPER_AT_OR_BELOW_DELTA": 11, "UPPER_BELOW_MINUS_DELTA": 12, "NEITHER": 13,
    "INSIDE_PLUS_MINUS_DELTA": 20, "NOT_INSIDE_PLUS_MINUS_DELTA": 21,
    "MEANINGFUL_POSITIVE": 30, "MEANINGFUL_ADVERSE": 31, "BOUNDED_BELOW_POSITIVE": 32, "UNRESOLVED": 33,
    "DESCRIPTIVE_NO_INTERVAL": 40,
}


def classify(estimates, invalid):
    """Applies design sections 11 and 12 in order using exact comparisons only."""
    def gt_h(name, x):
        e = estimates[name]
        return exceeds_halfwidth(x, e["range"], e["alpha"], N)

    def est(name):
        return estimates[name]["value"]

    rel = {}
    # Per-estimate predicates (lower = est - h, upper = est + h).
    for name in ("C_abs", "C_rec"):
        lower_above = gt_h(name, est(name) - DELTA)
        upper_at_or_below = gt_h(name, DELTA - est(name))  # h irrational: >= coincides with >
        upper_below_minus = gt_h(name, -DELTA - est(name))
        rel[name] = {"lower_above_delta": lower_above, "upper_at_or_below_delta": upper_at_or_below,
                     "upper_below_minus_delta": upper_below_minus}
    for name in ("D_HALF", "D_ZERO"):
        inside = gt_h(name, est(name) + DELTA) and gt_h(name, DELTA - est(name))
        rel[name] = {"wholly_inside_plus_minus_delta": inside}
    perf = {}
    for name in ("P_abs", "P_rec"):
        if gt_h(name, est(name) - DELTA_P):
            perf[name] = "MEANINGFUL_POSITIVE"
        elif gt_h(name, -DELTA_P - est(name)):
            perf[name] = "MEANINGFUL_ADVERSE"
        elif gt_h(name, DELTA_P - est(name)):
            perf[name] = "BOUNDED_BELOW_POSITIVE"
        else:
            perf[name] = "UNRESOLVED"
        rel[name] = {"class": perf[name]}

    adverse = None
    if invalid:
        primary = "INVALID"
    elif not (rel["D_HALF"]["wholly_inside_plus_minus_delta"] and rel["D_ZERO"]["wholly_inside_plus_minus_delta"]):
        primary = "START-DEPENDENT; SCIENTIFIC QUESTION UNRESOLVED"
    elif rel["C_abs"]["lower_above_delta"] and rel["C_rec"]["lower_above_delta"]:
        primary = "SUPPORTS RECURRENCE-ATTRIBUTABLE SELECTIVE ENRICHMENT IN THIS ALTERNATIVE OPERATOR BUNDLE"
    elif rel["C_abs"]["lower_above_delta"] and rel["C_rec"]["upper_at_or_below_delta"]:
        primary = "SELECTIVE ENRICHMENT NOT ATTRIBUTABLE TO RECURRENCE AT THE FIXED SCALE"
    elif rel["C_abs"]["upper_at_or_below_delta"]:
        primary = "BOUNDED NEGATIVE AT THE ONE-INDIVIDUAL SCALE"
        adverse = rel["C_abs"]["upper_below_minus_delta"]
    else:
        primary = "UNRESOLVED"

    if invalid:
        crossed = "INVALID"
        perf = {k: "INVALID" for k in perf}
    elif primary.startswith("SUPPORTS") and perf["P_abs"] == "BOUNDED_BELOW_POSITIVE":
        crossed = "SELECTION/PERFORMANCE SEPARATION RECURS IN THIS OPERATOR BUNDLE"
    elif primary.startswith("SUPPORTS") and perf["P_abs"] == "MEANINGFUL_POSITIVE":
        crossed = "SEPARATION IS CLASS-SPECIFIC AT THE DECLARED SCALES"
    else:
        crossed = "REPORTED LITERALLY (NO CROSSED INTERPRETATION APPLIES)"
    return primary, adverse, perf, crossed, rel


def estimate_code(name, rel, invalid):
    if invalid:
        return CODES["INVALID"]
    if name in ("C_abs", "C_rec"):
        r = rel[name]
        if r["lower_above_delta"]:
            return CODES["LOWER_ABOVE_DELTA"]
        if r["upper_below_minus_delta"]:
            return CODES["UPPER_BELOW_MINUS_DELTA"]
        if r["upper_at_or_below_delta"]:
            return CODES["UPPER_AT_OR_BELOW_DELTA"]
        return CODES["NEITHER"]
    if name in ("D_HALF", "D_ZERO"):
        return CODES["INSIDE_PLUS_MINUS_DELTA"] if rel[name]["wholly_inside_plus_minus_delta"] else CODES["NOT_INSIDE_PLUS_MINUS_DELTA"]
    if name in ("P_abs", "P_rec"):
        return CODES[rel[name]["class"]]
    return CODES["DESCRIPTIVE_NO_INTERVAL"]


def ascii_field(s, width):
    b = s.encode("ascii")
    if len(b) > width:
        raise Abort("estimate field overflow")
    return b.ljust(width, b"\x00")


def pack_estimate(idx, name, family, code, total, denom, range_len, disp):
    out = bytearray()
    out += idx.to_bytes(2, "little") + bytes([family, code]) + N.to_bytes(4, "little")
    out += total.to_bytes(16, "little", signed=True) + denom.to_bytes(16, "little", signed=False)
    out += range_len.numerator.to_bytes(4, "little") + range_len.denominator.to_bytes(4, "little")
    out += ascii_field(name, 32)
    for key in ("estimate", "half_width", "lower", "upper"):
        out += ascii_field(disp[key], 44)
    if len(out) != S.ESTIMATE_BYTES:
        raise Abort("estimate record size")
    return bytes(out)


# --------------------------------------------------------------------------- main
def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--production-dir", required=True)
    ap.add_argument("--config", required=True)
    ap.add_argument("--design-spec", required=True)
    ap.add_argument("--design-go-record", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args(argv)

    if os.path.exists(a.out) and (not os.path.isdir(a.out) or os.listdir(a.out)):
        print("TORUS_FATAL: refusing existing nonempty output: %s" % a.out, file=sys.stderr)
        return 2
    try:
        check_documented_halfwidths()
        man, man_sha, cfg_sha = authenticate(a.production_dir, a.config, a.design_spec, a.design_go_record)
        failures = FailureLog()
        verify_preflight(a.production_dir, failures)
        blocks = reconstruct(a.production_dir, failures)
        invalid = len(failures) > 0

        estimates = {}
        for (idx, name, family, total, denom, rng, alpha) in estimate_specs(blocks):
            estimates[name] = {"index": idx, "family": family, "total": total, "denom": denom, "range": rng,
                               "alpha": alpha, "value": Fraction(total, N * denom)}
        primary, adverse, perf, crossed, rel = classify(estimates, invalid)

        records = []
        rows = []
        for name, e in sorted(estimates.items(), key=lambda kv: kv[1]["index"]):
            disp = {"estimate": decimal_display(e["value"], ROUND_HALF_EVEN),
                    "half_width": "NA", "lower": "NA", "upper": "NA"}
            if e["alpha"] is not None:
                disp.update(interval_display(e["value"], e["range"], e["alpha"], N))
            code = estimate_code(name, rel, invalid)
            rows.append(pack_estimate(e["index"], name, e["family"], code, e["total"], e["denom"], e["range"], disp))
            records.append({
                "index": e["index"], "name": name,
                "family": ["PRIMARY_ALLELE", "PERFORMANCE", "ABSOLUTE_CELL_MEAN"][e["family"]],
                "n_blocks": N, "sum_of_block_numerators": str(e["total"]), "block_denominator": str(e["denom"]),
                "exact_value": "%d/%d" % (e["value"].numerator, e["value"].denominator),
                "range_length": str(e["range"]),
                "alpha_each": None if e["alpha"] is None else str(e["alpha"]),
                "display": disp, "classification_code": code,
            })
        if len(rows) != 22:
            raise Abort("estimate count is not 22")
    except Abort as e:
        print("TORUS_FATAL: %s" % e, file=sys.stderr)
        return 2

    os.makedirs(a.out, exist_ok=True)
    header = S.build_header("estimate", 22, NAMESPACE, S.NO_CHUNK, 0, S.BLOCKS)

    def write_new(path, data, mode):
        with open(path, mode) as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())

    write_new(os.path.join(a.out, "estimates.t3s"), header + b"".join(rows), "xb")
    write_new(os.path.join(a.out, "estimates.json"),
              json.dumps({"schema": "PHASE2-TORUS-MEMORY-003-ESTIMATES-v1", "namespace": NAMESPACE,
                          "records": records}, indent=2, sort_keys=True) + "\n", "x")
    decisions = {
        "schema": "PHASE2-TORUS-MEMORY-003-DECISIONS-v1",
        "namespace": NAMESPACE,
        "design_sha256": DESIGN_SHA256,
        "design_go_record_sha256": DESIGN_GO_SHA256,
        "production_manifest_sha256": man_sha,
        "config_sha256": cfg_sha,
        "identity_failures_first_1000": failures.items,
        "identity_failure_count": failures.count,
        "primary_decision": primary,
        "primary_adverse_upper_below_minus_delta": adverse,
        "performance_classes": perf,
        "crossed_interpretation": crossed,
        "predicates": rel if not invalid else None,
        "decision_rule": "design sections 11-12 applied in order; exact rational estimates vs. bracketed Decimal half-widths",
    }
    write_new(os.path.join(a.out, "decisions.json"), json.dumps(decisions, indent=2, sort_keys=True) + "\n", "x")
    return 0


if __name__ == "__main__":
    sys.exit(main())
