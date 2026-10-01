#!/usr/bin/env python3
"""Independently check every registered numerical claim in the manuscript draft.

The checker reads only the saved analyzer, diagnostics, acceptance and audit
records named on the command line, plus the draft Markdown and
provenance/QUOTED_STATISTICS.json under --draft-root. It imports no producer,
analyzer or auditor code and uses only the Python standard library.

It fails (exit status 1) on any discrepancy:
  * SHA-256 of the supplied records versus the acceptance record, and of the acceptance
    record versus its frozen SHA-256 (unless --skip-sha-check);
  * exact rational recomputation of the six inferential estimates from the 16 descriptive means;
  * high-precision recomputation of the six Hoeffding-Bonferroni half-widths and interval endpoints;
  * recomputation of the primary decision and secondary classifications;
  * SHAM identities N1/N2 on the descriptive means and diagnostics;
  * scope counts and audit mismatch counts across all records;
  * every registered quoted literal (value and presence in the named documents);
  * any decimal number with three or more places, or comma-grouped integer, in the
    draft that is not registered.

The JSON report records the SHA-256 of every checked document and of the registry, so
a PASS is bound to the exact manuscript bytes that were checked.
"""
import argparse
import hashlib
import json
import re
import sys
from decimal import Decimal, ROUND_HALF_UP, getcontext
from fractions import Fraction
from pathlib import Path

getcontext().prec = 90
TOL = Decimal("1e-40")

N_BLOCKS = 41600
CELLS_PER_BLOCK = 8
UPDATES = 256
CANDIDATES_PER_UPDATE = 128
AUDIT_BLOCKS = 64
SAVED_ESTIMATES = 22
DELTA = Fraction(1, 32)
ARMS = ("ACTIVE", "SHAM")
LAWS = ("ZERO", "HALF")
STARTS = ("ALL_F", "ALL_M")
CELL_ORDER = ["%s|%s|%s" % (a, l, s) for a in ARMS for l in LAWS for s in STARTS]

PRIMARY = ("C_abs", "C_rec", "D_HALF", "D_ZERO")
SECONDARY = ("P_abs", "P_rec")
RANGE_LENGTH = {"C_abs": 1, "C_rec": 2, "D_HALF": 2, "D_ZERO": 2, "P_abs": 2, "P_rec": 4}
ALPHA_EACH = {name: Decimal("0.05") / 4 for name in PRIMARY}
ALPHA_EACH.update({name: Decimal("0.05") / 2 for name in SECONDARY})
FROZEN_HALF_WIDTH_8DP = {"C_abs": "0.00781023", "C_rec": "0.01562046", "D_HALF": "0.01562046",
                         "D_ZERO": "0.01562046", "P_abs": "0.01451463", "P_rec": "0.02902925"}
NEUTRAL_SPEC = {"r256": 6.678005283720385e-08, "window": 9.57983820600146e-07}

RULE_LABELS = {
    1: "INVALID",
    2: "START-DEPENDENT; SCIENTIFIC QUESTION UNRESOLVED",
    3: "SUPPORTS RECURRENCE-ATTRIBUTABLE SELECTIVE ENRICHMENT",
    4: "SELECTIVE ENRICHMENT NOT ATTRIBUTABLE TO RECURRENCE AT THE FIXED SCALE",
    5: "BOUNDED NEGATIVE AT THE ONE-INDIVIDUAL SCALE",
    6: "UNRESOLVED",
}
# The first two secondary labels never occurred in the supplied records; their exact
# wording is therefore not frozen in any supplied file and they are compared only if reached.
SECONDARY_LABELS = ("MEANINGFUL POSITIVE PERFORMANCE", "MEANINGFUL ADVERSE PERFORMANCE",
                    "BOUNDED BELOW THE POSITIVE ONE-BIT SCALE", "UNRESOLVED")
ACCEPTED_HASH_KEY = {
    "estimates": "estimates_22_json_sha256",
    "decision": "decision_json_sha256",
    "diagnostics": "diagnostics_json_sha256",
    "replay_receipt": "independent_replay_receipt_sha256",
    "records_receipt": "independent_record_receipt_sha256",
}
# SHA-256 of PHASE2_STUDY_001_ACCEPTED.json (PROSPECTIVE_TIMING_BINDINGS accepted_result.sha256).
ACCEPTED_SHA256 = "2eb3aaf5d754f4570a75522aa9e1f38358715c06c674d5a267732a1357926e6f"
DASHES = ["‐", "‑", "‒", "–", "—", "−"]


class Report(object):
    def __init__(self):
        self.failures = []
        self.passed = 0

    def check(self, condition, message):
        if condition:
            self.passed += 1
        else:
            self.failures.append(message)
            print("FAIL: " + message)
        return condition


def sha256_file(path):
    digest = hashlib.sha256()
    with open(str(path), "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def to_decimal(value):
    if isinstance(value, Fraction):
        return Decimal(value.numerator) / Decimal(value.denominator)
    return Decimal(str(value))


def fmt_decimal(value, decimals, plus=False):
    quantum = Decimal(1).scaleb(-decimals)
    q = to_decimal(value).quantize(quantum, rounding=ROUND_HALF_UP)
    if q == 0:
        q = abs(q)
    text = format(q, "f")
    if plus and q > 0:
        text = "+" + text
    return text


def fmt_int(value, commas):
    return "{:,}".format(int(value)) if commas else str(int(value))


def normalize_text(text):
    for mark in DASHES:
        text = text.replace(mark, "-")
    return text.replace(" ", " ")


def get_path(obj, path):
    for key in path:
        obj = obj[key]
    return obj


def half_width(range_length, alpha_each, n):
    return Decimal(range_length) * ((Decimal(2) / alpha_each).ln() / (2 * Decimal(n))).sqrt()


def load_json(path):
    with open(str(path)) as handle:
        return json.load(handle)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--estimates", type=Path, required=True, help="analysis/estimates_22.json")
    parser.add_argument("--decision", type=Path, required=True, help="analysis/decision.json")
    parser.add_argument("--diagnostics", type=Path, required=True, help="diagnostics_descriptive.json")
    parser.add_argument("--accepted", type=Path, required=True, help="PHASE2_STUDY_001_ACCEPTED.json")
    parser.add_argument("--replay-receipt", type=Path, required=True, help="production_replay_receipt.json")
    parser.add_argument("--records-receipt", type=Path, required=True, help="production_records_receipt.json")
    parser.add_argument("--draft-root", type=Path, required=True, help="root of this manuscript source package")
    parser.add_argument("--quoted", type=Path, default=None,
                        help="registry (default: <draft-root>/provenance/QUOTED_STATISTICS.json)")
    parser.add_argument("--skip-sha-check", action="store_true",
                        help="do not require the supplied records to match the authenticated SHA-256 values")
    parser.add_argument("--public-transformations", type=Path, default=None,
                        help="optional public-export map binding sanitized receipt bytes to authenticated source hashes")
    parser.add_argument("--report", type=Path, default=None, help="optional JSON report path")
    args = parser.parse_args()

    R = Report()
    paths = {"estimates": args.estimates, "decision": args.decision, "diagnostics": args.diagnostics,
             "accepted": args.accepted, "replay_receipt": args.replay_receipt,
             "records_receipt": args.records_receipt}
    src = {key: load_json(path) for key, path in paths.items()}
    accepted = src["accepted"]
    estimates = src["estimates"]
    decision = src["decision"]
    diagnostics = src["diagnostics"]
    replay = src["replay_receipt"]
    records = src["records_receipt"]

    # ---- 1. Source authentication -------------------------------------------------
    physical_hashes = {key: sha256_file(path) for key, path in paths.items()}
    file_hashes = dict(physical_hashes)
    public_exports = {}
    if args.public_transformations:
        export_record = load_json(args.public_transformations)
        for item in export_record.get("transformations", []):
            key = item.get("logical_input_key")
            if key:
                public_exports[key] = item
        for key, item in public_exports.items():
            if key in physical_hashes:
                R.check(physical_hashes[key] == item["public_sha256"],
                        "%s public SHA-256 does not match transformation record" % key)
                file_hashes[key] = item["source_sha256"]
    for key, accepted_key in sorted(ACCEPTED_HASH_KEY.items()):
        expected = accepted["authenticated_outputs"][accepted_key]
        if args.skip_sha_check:
            if file_hashes[key] != expected:
                print("WARN: %s SHA-256 %s differs from authenticated %s (check skipped)"
                      % (key, file_hashes[key], expected))
        else:
            R.check(file_hashes[key] == expected,
                    "%s SHA-256 %s != authenticated %s" % (key, file_hashes[key], expected))
    if args.skip_sha_check:
        if file_hashes["accepted"] != ACCEPTED_SHA256:
            print("WARN: accepted SHA-256 %s differs from frozen %s (check skipped)"
                  % (file_hashes["accepted"], ACCEPTED_SHA256))
    else:
        R.check(file_hashes["accepted"] == ACCEPTED_SHA256,
                "accepted SHA-256 %s != frozen %s" % (file_hashes["accepted"], ACCEPTED_SHA256))
    receipt_inputs = {item["label"]: item for item in records["inputs"]}
    R.check(receipt_inputs["analysis/estimates_22.json"]["sha256"]
            == accepted["authenticated_outputs"]["estimates_22_json_sha256"],
            "records receipt estimate hash disagrees with acceptance record")
    R.check(receipt_inputs["analysis/decision.json"]["sha256"]
            == accepted["authenticated_outputs"]["decision_json_sha256"],
            "records receipt decision hash disagrees with acceptance record")

    # ---- 2. Record structure -------------------------------------------------------
    R.check(isinstance(estimates, list) and len(estimates) == SAVED_ESTIMATES, "expected 22 estimate records")
    by_name = {}
    for rec in estimates:
        R.check(rec["name"] not in by_name, "duplicate record name " + rec["name"])
        by_name[rec["name"]] = rec
    R.check(sorted(r["record"] for r in estimates) == list(range(1, 23)), "record numbers are not 1..22")
    families = [r["family"] for r in estimates]
    R.check(families.count("PRIMARY_ALLELE_ENRICHMENT") == 4, "expected 4 primary inferential records")
    R.check(families.count("SECONDARY_POPULATION_PERFORMANCE") == 2, "expected 2 secondary inferential records")
    R.check(families.count("ABSOLUTE_CELL_MEAN_DESCRIPTIVE") == 16, "expected 16 descriptive records")

    exact = {}
    for rec in estimates:
        exact[rec["name"]] = Fraction(rec["estimate_exact"])
        R.check(abs(to_decimal(exact[rec["name"]]) - Decimal(rec["estimate"])) <= TOL,
                "decimal estimate disagrees with exact fraction for " + rec["name"])

    def freq(arm, law, start):
        return exact["M_FREQUENCY_LATE|%s|%s|%s" % (arm, law, start)]

    def acc(arm, law, start):
        return exact["ACCURACY_LATE|%s|%s|%s" % (arm, law, start)]

    for arm in ARMS:
        for law in LAWS:
            for start in STARTS:
                for prefix in ("M_FREQUENCY_LATE", "ACCURACY_LATE"):
                    rec = by_name["%s|%s|%s|%s" % (prefix, arm, law, start)]
                    R.check(rec["lower"] is None and rec["upper"] is None,
                            "descriptive record carries bounds: " + rec["name"])
                    R.check(rec["classification"] == "DESCRIPTIVE; NO INFERENTIAL BOUND FROZEN",
                            "descriptive classification wrong: " + rec["name"])
                    R.check(Fraction(0) <= exact[rec["name"]] <= Fraction(1),
                            "descriptive mean outside [0,1]: " + rec["name"])

    # ---- 3. Exact estimand identities from the 16 descriptive means ---------------
    A_half = (freq("ACTIVE", "HALF", "ALL_F") + freq("ACTIVE", "HALF", "ALL_M")) / 2
    A_zero = (freq("ACTIVE", "ZERO", "ALL_F") + freq("ACTIVE", "ZERO", "ALL_M")) / 2

    def P(arm, law):
        return (acc(arm, law, "ALL_F") + acc(arm, law, "ALL_M")) / 2

    derived = {
        "C_abs": A_half - Fraction(1, 2),
        "C_rec": A_half - A_zero,
        "D_HALF": freq("ACTIVE", "HALF", "ALL_M") - freq("ACTIVE", "HALF", "ALL_F"),
        "D_ZERO": freq("ACTIVE", "ZERO", "ALL_M") - freq("ACTIVE", "ZERO", "ALL_F"),
        "P_abs": P("ACTIVE", "HALF") - P("SHAM", "HALF"),
        "P_rec": (P("ACTIVE", "HALF") - P("SHAM", "HALF")) - (P("ACTIVE", "ZERO") - P("SHAM", "ZERO")),
    }
    for name, value in derived.items():
        R.check(value == exact[name], "exact identity failed for %s: %s != %s" % (name, value, exact[name]))

    # SHAM identities visible in the cell means (N2: frequencies complement; N1: accuracy equal).
    for law in LAWS:
        R.check(freq("SHAM", law, "ALL_F") + freq("SHAM", law, "ALL_M") == 1,
                "N2 complement fails in SHAM %s frequencies" % law)
        R.check(acc("SHAM", law, "ALL_F") == acc("SHAM", law, "ALL_M"),
                "N1 identity fails in SHAM %s accuracy" % law)

    # ---- 4. Hoeffding-Bonferroni half-widths and endpoints -------------------------
    bounds = {}
    for name in PRIMARY + SECONDARY:
        rec = by_name[name]
        R.check(rec["n_blocks"] == N_BLOCKS, name + " n_blocks != 41600")
        R.check(rec["range_length"] == RANGE_LENGTH[name], name + " range length differs from frozen value")
        R.check(Decimal(rec["alpha_each"]) == ALPHA_EACH[name], name + " alpha_each differs from frozen value")
        h = half_width(RANGE_LENGTH[name], ALPHA_EACH[name], N_BLOCKS)
        lo = to_decimal(exact[name]) - h
        hi = to_decimal(exact[name]) + h
        bounds[name] = (lo, hi, h)
        R.check(abs(h - Decimal(rec["half_width"])) <= TOL, name + " half-width recomputation mismatch")
        R.check(abs(lo - Decimal(rec["lower"])) <= TOL, name + " lower endpoint recomputation mismatch")
        R.check(abs(hi - Decimal(rec["upper"])) <= TOL, name + " upper endpoint recomputation mismatch")
        R.check(fmt_decimal(h, 8) == FROZEN_HALF_WIDTH_8DP[name],
                name + " half-width does not round to the frozen specification value")
    R.check(bounds["C_rec"][2] < to_decimal(DELTA / 2), "range-2 primary half-width is not below Delta/2")

    # ---- 5. Decisions ---------------------------------------------------------------
    D = to_decimal(DELTA)
    audits_ok = (replay["status"] == "PASS" and replay["mismatch_count"] == 0
                 and records["status"] == "PASS" and records["mismatch_count"] == 0)

    def inside(name):
        lo, hi, _ = bounds[name]
        return lo > -D and hi < D

    if not audits_ok:
        rule = 1
    elif not (inside("D_HALF") and inside("D_ZERO")):
        rule = 2
    elif bounds["C_abs"][0] > D and bounds["C_rec"][0] > D:
        rule = 3
    elif bounds["C_abs"][0] > D and bounds["C_rec"][1] <= D:
        rule = 4
    elif bounds["C_abs"][1] <= D:
        rule = 5
    else:
        rule = 6
    adverse = (bounds["C_abs"][1] < -D) if rule == 5 else None
    R.check(decision["decision_rule_applied"] == rule, "decision rule applied differs from recomputation")
    R.check(decision["primary_decision"] == RULE_LABELS[rule], "primary decision differs from recomputation")
    R.check(accepted["primary_decision"] == RULE_LABELS[rule], "accepted primary decision differs")
    R.check(decision["adverse_selection_upper_C_abs_below_minus_Delta"] == adverse, "adverse-selection field differs")
    for name in PRIMARY:
        R.check(by_name[name]["family_decision"] == RULE_LABELS[rule], name + " family_decision differs")

    def classify(name):
        lo, hi, _ = bounds[name]
        if lo > D:
            return SECONDARY_LABELS[0]
        if hi < -D:
            return SECONDARY_LABELS[1]
        if hi <= D:
            return SECONDARY_LABELS[2]
        return SECONDARY_LABELS[3]

    for name in SECONDARY:
        label = classify(name)
        R.check(by_name[name]["classification"] == label, name + " classification differs from recomputation")
        R.check(decision["secondary_classifications"][name] == label, name + " decision.json classification differs")
        R.check(accepted["secondary_decisions"][name] == label, name + " accepted classification differs")
        R.check(by_name[name]["cannot_alter_primary_decision"] is True, name + " lacks cannot_alter flag")

    # ---- 6. Neutral SHAM formulas -----------------------------------------------------
    r = Fraction(15, 16)
    r256 = r ** 256
    window = r ** 193 * (1 - r ** 64) / (64 * (1 - r))
    R.check(window == sum(r ** t for t in range(193, 257)) / 64, "closed-form window average is wrong")
    neutral = {"r256": r256, "window": window}
    for key, value in neutral.items():
        R.check(abs(float(value) - NEUTRAL_SPEC[key]) <= 1e-12 * NEUTRAL_SPEC[key],
                "neutral formula %s disagrees with specification" % key)

    # ---- 7. Scope counts ---------------------------------------------------------------
    paths_total = N_BLOCKS * CELLS_PER_BLOCK
    scope = {
        "blocks": N_BLOCKS, "cells": CELLS_PER_BLOCK, "paths": paths_total,
        "path_updates": paths_total * UPDATES,
        "queries": paths_total * UPDATES * CANDIDATES_PER_UPDATE,
        "audit_blocks": AUDIT_BLOCKS, "audit_paths": AUDIT_BLOCKS * CELLS_PER_BLOCK,
        "audit_update_records": AUDIT_BLOCKS * CELLS_PER_BLOCK * UPDATES,
        "audit_rows": AUDIT_BLOCKS * CELLS_PER_BLOCK * UPDATES * CANDIDATES_PER_UPDATE,
        "saved_estimates": SAVED_ESTIMATES,
    }
    R.check(accepted["scope"]["blocks"] == scope["blocks"], "accepted scope blocks")
    R.check(accepted["scope"]["cells_per_block"] == scope["cells"], "accepted scope cells")
    R.check(accepted["scope"]["paths"] == scope["paths"], "accepted scope paths")
    R.check(accepted["scope"]["saved_estimates"] == scope["saved_estimates"], "accepted scope estimates")
    R.check(accepted["scope"]["post_outcome_changes"] == 0, "post-outcome changes recorded")
    R.check(diagnostics["total_objective_queries"] == scope["queries"], "diagnostics query total")
    R.check(diagnostics["cells"] == CELL_ORDER, "diagnostics cell order differs from expected")
    R.check(diagnostics["descriptive_only"] is True, "diagnostics not flagged descriptive-only")
    rc = records["checked"]
    R.check(rc == records["expected"], "records receipt checked != expected")
    R.check(rc["block_records"] == scope["blocks"], "records receipt block count")
    R.check(rc["blocks_with_both_sham_identities"] == scope["blocks"], "N1/N2 not confirmed in every block")
    R.check(rc["path_records"] == scope["paths"], "records receipt path count")
    R.check(rc["update_records"] == scope["path_updates"], "records receipt update count")
    R.check(rc["estimates_compared"] == scope["saved_estimates"], "records receipt estimate count")
    R.check(rc["audit_rows"] == scope["audit_rows"], "records receipt audit rows")
    R.check(rc["audit_paths_hashes_reconstructed"] == scope["audit_paths"], "records receipt audit paths")
    pc = replay["checked"]
    R.check(pc == replay["expected"], "replay receipt checked != expected")
    R.check(pc["blocks"] == scope["audit_blocks"], "replay block count")
    R.check(pc["path_records"] == scope["audit_paths"], "replay path count")
    R.check(pc["update_records"] == scope["audit_update_records"], "replay update count")
    R.check(pc["audit_rows"] == scope["audit_rows"], "replay audit rows")
    R.check(replay["replayed_blocks"] == "0-63", "replayed block range")

    # ---- 8. Audit status and mismatch counts ------------------------------------------------
    R.check(replay["status"] == "PASS", "replay status")
    R.check(replay["mismatch_count"] == 0 and replay["first_discrepancy"] is None, "replay mismatches")
    R.check(replay["mismatch_count_capped"] is False, "replay mismatch count capped")
    R.check(replay["known_answer_tests_pass"] is True and
            all(t["result"] == "PASS" for t in replay["known_answer_tests"]), "replay known-answer tests")
    R.check(replay["mode"] == "production" and replay["key_namespace"] == "production-r1", "replay mode/namespace")
    R.check(records["status"] == "PASS", "records status")
    R.check(records["mismatch_count"] == 0 and records["first_discrepancy"] is None, "records mismatches")
    R.check(records["self_checks_pass"] is True and
            all(t["result"] == "PASS" for t in records["self_checks"]), "records self-checks")
    R.check(records["random_draws_generated"] is False, "record verifier generated random draws")
    audit = accepted["independent_audit_job"]
    R.check(audit["replay_status"] == "PASS" and audit["replay_mismatches"] == 0, "accepted replay status")
    R.check(audit["record_estimate_status"] == "PASS" and audit["record_estimate_mismatches"] == 0,
            "accepted record status")
    for job in ("production_job", "analysis_job", "independent_audit_job"):
        R.check(accepted[job]["state"] == "COMPLETED" and accepted[job]["exit_code"] == "0:0", job + " status")
    f2m, m2f = diagnostics["per_cell_total_f2m"], diagnostics["per_cell_total_m2f"]
    for a, b in ((4, 5), (6, 7)):
        R.check(f2m[a] == m2f[b] and f2m[b] == m2f[a], "SHAM mutation counts not exchanged between starts")
    for i in range(4, 8):
        R.check(diagnostics["per_cell_total_use"][i] == 0 and diagnostics["per_cell_total_surv"][i] == 0,
                "SHAM cell shows cache use")

    # ---- 9. Registered quoted statistics -------------------------------------------------------
    quoted_path = args.quoted or (args.draft_root / "provenance" / "QUOTED_STATISTICS.json")
    registry = load_json(quoted_path)
    documents = {}
    document_hashes = {"registry": sha256_file(quoted_path)}
    for key, rel in registry["documents"].items():
        documents[key] = normalize_text((args.draft_root / rel).read_text(encoding="utf-8"))
        document_hashes[key] = sha256_file(args.draft_root / rel)

    constants = {"Delta": DELTA, "half_Delta": DELTA / 2, "Delta_points": DELTA * 100}

    def expected_literal(d):
        kind = d["type"]
        scale = d.get("scale", 1)
        if kind == "estimate":
            name, field = d["name"], d["field"]
            if field == "estimate_exact":
                return by_name[name]["estimate_exact"]
            if field == "alpha_each":
                return fmt_decimal(Decimal(by_name[name]["alpha_each"]), d["decimals"])
            value = {"estimate": to_decimal(exact[name])}
            if name in bounds:
                value.update({"lower": bounds[name][0], "upper": bounds[name][1], "half_width": bounds[name][2]})
            v = value[field] * scale
            if d.get("absolute"):
                v = abs(v)
            return fmt_decimal(v, d["decimals"], d.get("plus", False))
        if kind == "start_average":
            v = (exact[d["names"][0]] + exact[d["names"][1]]) / 2 * scale
            return fmt_decimal(abs(v) if d.get("absolute") else v, d["decimals"], d.get("plus", False))
        if kind == "average_difference":
            plus_avg = (exact[d["plus_names"][0]] + exact[d["plus_names"][1]]) / 2
            minus_avg = (exact[d["minus_names"][0]] + exact[d["minus_names"][1]]) / 2
            v = (plus_avg - minus_avg) * scale
            return fmt_decimal(abs(v) if d.get("absolute") else v, d["decimals"], d.get("plus", False))
        if kind == "constant":
            return fmt_decimal(constants[d["key"]], d["decimals"])
        if kind == "scope":
            return fmt_int(scope[d["key"]], d.get("commas", False))
        if kind == "source_int":
            return fmt_int(get_path(src[d["source"]], d["path"]), d.get("commas", False))
        raise ValueError("unknown derivation type " + kind)

    registered = {key: set() for key in documents}
    for entry in registry["entries"]:
        literal = normalize_text(entry["literal"])
        d = entry["derivation"]
        label = entry["id"]
        try:
            if d["type"] == "neutral":
                exact_value = float(neutral[d["key"]])
                R.check(abs(float(literal) - exact_value) <= 1e-12 * exact_value,
                        "%s: literal %s disagrees with recomputed %r" % (label, literal, exact_value))
            elif d["type"] == "input_sha256":
                R.check(file_hashes[d["source"]] == literal,
                        "%s: literal %s != SHA-256 of supplied %s" % (label, literal, d["source"]))
            elif d["type"] == "source_value":
                actual = get_path(src[d["source"]], d["path"])
                R.check(actual == d["value"], "%s: source value %r != registered %r" % (label, actual, d["value"]))
            else:
                expected = expected_literal(d)
                R.check(expected == literal, "%s: literal %s != recomputed %s" % (label, literal, expected))
        except (KeyError, IndexError, TypeError, ValueError) as error:
            R.check(False, "%s: cannot evaluate derivation (%s)" % (label, error))
        for doc in entry["documents"]:
            R.check(literal in documents[doc], "%s: literal %r not found in %s" % (label, literal, doc))
            registered[doc].add(literal.lstrip("+"))

    # ---- 10. Unregistered numeric tokens -----------------------------------------------------
    allow = set(registry.get("numeric_allowlist", []))
    decimal_token = re.compile(r"(?<![\w.])[-+]?\d+\.\d{3,}(?:[eE][-+]?\d+)?(?!\d)")
    grouped_token = re.compile(r"(?<![\d,])\d{1,3}(?:,\d{3})+(?!\d)")
    for doc, text in documents.items():
        stripped = re.sub(r"https?://[^\s)]+", " ", text)
        stripped = re.sub(r"\b10\.\d{4,9}/[^\s\])]+", " ", stripped)
        for match in list(decimal_token.finditer(stripped)) + list(grouped_token.finditer(stripped)):
            token = match.group(0).lstrip("+")
            R.check(token in registered[doc] or token in allow,
                    "unregistered numeric token %r in %s" % (match.group(0), doc))

    # ---- Report ------------------------------------------------------------------------------
    summary = {"status": "PASS" if not R.failures else "FAIL", "checks_passed": R.passed,
               "failures": R.failures, "input_sha256": file_hashes, "document_sha256": document_hashes,
               "sha_check_enforced": not args.skip_sha_check,
               "registered_entries": len(registry["entries"])}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
    print("%s: %d checks passed, %d failed" % (summary["status"], R.passed, len(R.failures)))
    return 0 if not R.failures else 1


if __name__ == "__main__":
    sys.exit(main())
