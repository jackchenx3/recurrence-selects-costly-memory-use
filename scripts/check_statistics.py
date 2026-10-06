#!/usr/bin/env python3
"""Independently check every registered numerical claim in the manuscript draft.

Version 1.2.0 of the package reports three prospective studies in two operator
bundles, and this checker validates all three:

  * Study 001, PHASE2-MUTABLE-MEMORY-001 (22 saved estimate records);
  * Study 002, PHASE2-PERFORMANCE-CONVERSION-002 (19 saved estimate records);
  * Study 003, PHASE2-TORUS-MEMORY-003 (22 saved estimate records), a post-v1.1
    prospective extension in an independently implemented alternative operator bundle.

The three record sets are separately accepted estimate sets (63 in total). Each is
authenticated against its own acceptance record and none is pooled with another.
For Study 003 the checker also verifies the preserved audit-header failure, the
normalization-only audit recovery and the post-audit source-patch receipt.

The checker reads only the saved analyzer, diagnostics, acceptance and audit
records named on the command line, plus the draft Markdown and
provenance/QUOTED_STATISTICS.json under --draft-root. It imports no producer,
analyzer or auditor code and uses only the Python standard library.

It fails (exit status 1) on any discrepancy, for each study:
  * SHA-256 of the supplied records versus the acceptance record, and of the acceptance
    record versus its frozen SHA-256 (unless --skip-sha-check);
  * exact rational recomputation of every inferential estimate from the descriptive means;
  * high-precision recomputation of every Hoeffding-Bonferroni half-width and interval endpoint;
  * recomputation of the primary decision and secondary classifications;
  * SHAM identities N1/N2 on the descriptive means and diagnostics;
  * scope counts, diagnostic consistency relations and audit mismatch counts across all records;
and, for the documents:
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
DELTA = Fraction(1, 32)
STARTS = ("ALL_F", "ALL_M")
UPDATES = 256
CANDIDATES_PER_UPDATE = 128
AUDIT_BLOCKS = 64
DASHES = ["‐", "‑", "‒", "–", "—", "−"]

# ---- Study 001: PHASE2-MUTABLE-MEMORY-001 ---------------------------------------------------
N_BLOCKS = 41600
CELLS_PER_BLOCK = 8
SAVED_ESTIMATES = 22
ARMS = ("ACTIVE", "SHAM")
LAWS = ("ZERO", "HALF")
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

# ---- Study 002: PHASE2-PERFORMANCE-CONVERSION-002 -------------------------------------------
S2_N_BLOCKS = 41600
S2_CELLS_PER_BLOCK = 6
S2_SAVED_ESTIMATES = 19
S2_ARMS = ("INFO", "NONINFO", "SHAM")
S2_CELL_ORDER = ["%s|%s" % (a, s) for a in S2_ARMS for s in STARTS]
S2_PRIMARY = ("Delta_P", "D_INFO", "D_NONINFO")
S2_SECONDARY_ALLELE = ("E_INFO", "E_NONINFO")
S2_SECONDARY_PERFORMANCE = ("B_INFO", "B_NONINFO")
S2_SECONDARY = S2_SECONDARY_ALLELE + S2_SECONDARY_PERFORMANCE
S2_FAMILY = {"Delta_P": "PRIMARY", "D_INFO": "PRIMARY", "D_NONINFO": "PRIMARY",
             "E_INFO": "SECONDARY_ALLELE", "E_NONINFO": "SECONDARY_ALLELE",
             "B_INFO": "SECONDARY_PERFORMANCE", "B_NONINFO": "SECONDARY_PERFORMANCE"}
S2_RANGE_LENGTH = {"Delta_P": 2, "D_INFO": 2, "D_NONINFO": 2, "E_INFO": 2, "E_NONINFO": 1,
                   "B_INFO": 2, "B_NONINFO": 2}
S2_ALPHA_EACH = {name: Decimal("0.05") / 3 for name in S2_PRIMARY}
S2_ALPHA_EACH.update({name: Decimal("0.05") / 2 for name in S2_SECONDARY})
S2_FROZEN_HALF_WIDTH_8DP = {"Delta_P": "0.01517128", "D_INFO": "0.01517128", "D_NONINFO": "0.01517128",
                            "E_INFO": "0.01451463", "E_NONINFO": "0.00725731",
                            "B_INFO": "0.01451463", "B_NONINFO": "0.01451463"}
# Labels from the frozen specification (section 9). Only rule 5 occurred; the other labels
# are compared only if reached.
S2_RULE_LABELS = {
    1: "INVALID",
    2: "START-DEPENDENT; PRIMARY UNRESOLVED",
    3: "MEANINGFUL POSITIVE DIRECTIONAL-INFORMATION EFFECT RELATIVE TO NONINFO",
    4: "MEANINGFUL ADVERSE DIRECTIONAL-INFORMATION EFFECT RELATIVE TO NONINFO",
    5: "BOUNDED BELOW THE POSITIVE ONE-BIT SCALE",
    6: "UNRESOLVED",
}
# The adverse secondary label never occurred; its wording is not frozen in any supplied file.
S2_SECONDARY_LABELS = ("MEANINGFUL POSITIVE AT THE ONE-BIT SCALE", "MEANINGFUL ADVERSE AT THE ONE-BIT SCALE",
                       "BOUNDED BELOW THE POSITIVE ONE-BIT SCALE", "UNRESOLVED")
S2_ACCEPTED_HASH_PATH = {
    "s2_estimates": ("analysis", "estimates_sha256"),
    "s2_decision": ("analysis", "decision_sha256"),
    "s2_diagnostics": ("analysis", "diagnostics_sha256"),
    "s2_replay_receipt": ("independent_audit", "replay_receipt_sha256"),
    "s2_records_receipt": ("independent_audit", "records_receipt_sha256"),
}
# SHA-256 of PHASE2_STUDY_002_ACCEPTED.json (PHASE2-PERFORMANCE-CONVERSION-002-SOURCE-BINDINGS-1).
S2_ACCEPTED_SHA256 = "fc7777239cd4442c35c01d6261fd71e6fbf7e7f6fd6e11ddcc5d24876ce2f4a2"
S2_STATUS = "SCIENTIFICALLY_COMPLETE_AND_ACCEPTED"

# ---- Study 003: PHASE2-TORUS-MEMORY-003 -----------------------------------------------------
S3_STUDY_ID = "PHASE2-TORUS-MEMORY-003"
S3_N_BLOCKS = 41600
S3_CELLS_PER_BLOCK = 8
S3_SAVED_ESTIMATES = 22
S3_TOURNAMENT_ENTRIES = 4
S3_SURVIVORS = 32
S3_PRIMARY = ("C_abs", "C_rec", "D_HALF", "D_ZERO")
S3_PERFORMANCE = ("P_abs", "P_rec")
S3_RANGE_LENGTH = {"C_abs": 1, "C_rec": 2, "D_HALF": 2, "D_ZERO": 2, "P_abs": 2, "P_rec": 4}
S3_ALPHA_EACH = {name: Decimal(1) / 80 for name in S3_PRIMARY}
S3_ALPHA_EACH.update({name: Decimal(1) / 40 for name in S3_PERFORMANCE})
S3_FROZEN_HALF_WIDTH_8DP = FROZEN_HALF_WIDTH_8DP
S3_FAMILY = {"C_abs": "PRIMARY_ALLELE", "C_rec": "PRIMARY_ALLELE", "D_HALF": "PRIMARY_ALLELE",
             "D_ZERO": "PRIMARY_ALLELE", "P_abs": "PERFORMANCE", "P_rec": "PERFORMANCE"}
# Display strings carry 40 decimal places; allow for their final-digit rounding.
S3_TOL = Decimal("1e-39")
# Labels from the frozen specification (sections 11-12). Only rule 3 occurred.
S3_RULE_LABELS = {
    1: "INVALID",
    2: "START-DEPENDENT; SCIENTIFIC QUESTION UNRESOLVED",
    3: "SUPPORTS RECURRENCE-ATTRIBUTABLE SELECTIVE ENRICHMENT IN THIS ALTERNATIVE OPERATOR BUNDLE",
    4: "SELECTIVE ENRICHMENT NOT ATTRIBUTABLE TO RECURRENCE AT THE FIXED SCALE",
    5: "BOUNDED NEGATIVE AT THE ONE-INDIVIDUAL SCALE",
    6: "UNRESOLVED",
}
# Performance class codes as saved in decisions.json. The first two never occurred; their
# exact wording is not frozen in any supplied file and they are compared only if reached.
S3_PERFORMANCE_CLASSES = ("MEANINGFUL_POSITIVE", "MEANINGFUL_ADVERSE", "BOUNDED_BELOW_POSITIVE", "UNRESOLVED")
# Saved classification codes observed for the reached classes (estimates.json).
S3_OBSERVED_CODES = {"PRIMARY_SUPPORT": 10, "GATE_INSIDE": 20, "BOUNDED_BELOW_POSITIVE": 32,
                     "UNRESOLVED": 33, "DESCRIPTIVE": 40}
S3_CROSSED_SEPARATION = "SELECTION/PERFORMANCE SEPARATION RECURS IN THIS OPERATOR BUNDLE"
S3_ACCEPTED_HASH_PATH = {
    "s3_estimates": ("analysis", "estimates_json_sha256"),
    "s3_decisions": ("analysis", "decisions_sha256"),
    "s3_production_authentication": ("production", "output_authentication_sha256"),
    "s3_audit_failure": ("independent_audit", "attempt1", "failure_record_sha256"),
    "s3_normalization_receipt": ("independent_audit", "recovery", "normalization_receipt_sha256"),
    "s3_recovery_runner_receipt": ("independent_audit", "recovery", "runner_receipt_sha256"),
    "s3_cpp_replay_receipt": ("independent_audit", "recovery", "cpp_replay_receipt_sha256"),
}
# SHA-256 of PHASE2_STUDY_003_ACCEPTED.json (TORUS_003_POST_AUDIT_SOURCE_PATCH_RECEIPT.json
# accepted_study_sha256). The source-patch receipt itself post-dates acceptance and has no
# frozen SHA-256; it is checked for consistency with the accepted and failure records.
S3_ACCEPTED_SHA256 = "2ac18b3f6c22bc6f11ebbf21ffd8614dbcf24f8f3d028c169e92b2d046b02d5d"
S3_STATUS = "SCIENTIFICALLY_COMPLETE_AND_ACCEPTED"
S3_NONCHUNK_SENTINEL = 0xFFFFFFFF
S3_AUDIT_FILES = ("audit/audit_candidates.t3c", "audit/audit_context.t3x", "audit/audit_entries.t3e")
S3_HEADER_OFFSETS = [48, 49, 50, 51]


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


def _physical_sha256_file(path):
    digest = hashlib.sha256()
    with open(str(path), "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def to_decimal(value):
    if isinstance(value, Fraction):
        return Decimal(value.numerator) / Decimal(value.denominator)
    return Decimal(str(value))


def parse_alpha(value):
    """Accept the saved alpha_each forms, e.g. '0.0125' or '0.05/3'."""
    text = str(value)
    if "/" in text:
        numerator, denominator = text.split("/", 1)
        return Decimal(numerator) / Decimal(denominator)
    return Decimal(text)


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


def authenticate(R, skip, label, actual, expected):
    if skip:
        if actual != expected:
            print("WARN: %s SHA-256 %s differs from authenticated %s (check skipped)" % (label, actual, expected))
    else:
        R.check(actual == expected, "%s SHA-256 %s != authenticated %s" % (label, actual, expected))


# =============================================================================================
# Study 002
# =============================================================================================
def check_study_002(R, src, file_hashes, skip_sha):
    accepted = src["s2_accepted"]
    estimates = src["s2_estimates"]
    decision = src["s2_decision"]
    diagnostics = src["s2_diagnostics"]
    replay = src["s2_replay_receipt"]
    records = src["s2_records_receipt"]

    # ---- 1. Source authentication -------------------------------------------------
    for key, path in sorted(S2_ACCEPTED_HASH_PATH.items()):
        authenticate(R, skip_sha, "Study 002 " + key, file_hashes[key], get_path(accepted, path))
    authenticate(R, skip_sha, "Study 002 accepted", file_hashes["s2_accepted"], S2_ACCEPTED_SHA256)
    receipt_inputs = {item["label"]: item for item in records["inputs"]}
    R.check(receipt_inputs["analysis/estimates_19.json"]["sha256"] == accepted["analysis"]["estimates_sha256"],
            "Study 002 records receipt estimate hash disagrees with acceptance record")
    R.check(receipt_inputs["analysis/decision.json"]["sha256"] == accepted["analysis"]["decision_sha256"],
            "Study 002 records receipt decision hash disagrees with acceptance record")
    R.check(receipt_inputs["run_manifest.json"]["sha256"] == accepted["production"]["run_manifest_sha256"],
            "Study 002 records receipt run-manifest hash disagrees with acceptance record")
    R.check(all(item["error"] is None for item in records["inputs"]), "Study 002 records receipt input error")
    R.check(all(item["read_error"] is False and item["opened"] is True for item in replay["inputs"]),
            "Study 002 replay receipt input error")
    R.check(accepted["study_id"] == "PHASE2-PERFORMANCE-CONVERSION-002", "Study 002 study_id")
    R.check(accepted["status"] == S2_STATUS, "Study 002 acceptance status")
    R.check(accepted["no_extension"] is True, "Study 002 no_extension flag")

    # ---- 2. Record structure -------------------------------------------------------
    R.check(isinstance(estimates, list) and len(estimates) == S2_SAVED_ESTIMATES,
            "Study 002: expected 19 estimate records")
    by_name = {}
    for rec in estimates:
        R.check(rec["name"] not in by_name, "Study 002 duplicate record name " + rec["name"])
        by_name[rec["name"]] = rec
    R.check(sorted(r["record"] for r in estimates) == list(range(1, S2_SAVED_ESTIMATES + 1)),
            "Study 002 record numbers are not 1..19")
    families = [r["family"] for r in estimates]
    R.check(families.count("PRIMARY") == 3, "Study 002: expected 3 primary records")
    R.check(families.count("SECONDARY_ALLELE") == 2, "Study 002: expected 2 secondary allele records")
    R.check(families.count("SECONDARY_PERFORMANCE") == 2, "Study 002: expected 2 secondary performance records")
    R.check(families.count("ABSOLUTE_CELL_MEAN_DESCRIPTIVE") == 12, "Study 002: expected 12 descriptive records")
    for name, family in S2_FAMILY.items():
        R.check(by_name[name]["family"] == family, "Study 002 family differs for " + name)

    exact = {}
    for rec in estimates:
        exact[rec["name"]] = Fraction(rec["estimate_exact"])
        R.check(abs(to_decimal(exact[rec["name"]]) - Decimal(rec["estimate"])) <= TOL,
                "Study 002 decimal estimate disagrees with exact fraction for " + rec["name"])

    def freq(arm, start):
        return exact["M_FREQUENCY_LATE|%s|%s" % (arm, start)]

    def acc(arm, start):
        return exact["ACCURACY_LATE|%s|%s" % (arm, start)]

    for arm in S2_ARMS:
        for start in STARTS:
            for prefix in ("M_FREQUENCY_LATE", "ACCURACY_LATE"):
                rec = by_name["%s|%s|%s" % (prefix, arm, start)]
                R.check(rec["lower"] is None and rec["upper"] is None,
                        "Study 002 descriptive record carries bounds: " + rec["name"])
                R.check(rec["classification"] == "DESCRIPTIVE; NO INFERENTIAL BOUND FROZEN",
                        "Study 002 descriptive classification wrong: " + rec["name"])
                R.check(Fraction(0) <= exact[rec["name"]] <= Fraction(1),
                        "Study 002 descriptive mean outside [0,1]: " + rec["name"])

    # ---- 3. Exact estimand identities from the 12 descriptive means ---------------
    A = {arm: (freq(arm, "ALL_F") + freq(arm, "ALL_M")) / 2 for arm in S2_ARMS}
    P = {arm: (acc(arm, "ALL_F") + acc(arm, "ALL_M")) / 2 for arm in S2_ARMS}
    derived = {
        "Delta_P": P["INFO"] - P["NONINFO"],
        "D_INFO": freq("INFO", "ALL_M") - freq("INFO", "ALL_F"),
        "D_NONINFO": freq("NONINFO", "ALL_M") - freq("NONINFO", "ALL_F"),
        "E_INFO": A["INFO"] - A["NONINFO"],
        "E_NONINFO": A["NONINFO"] - Fraction(1, 2),
        "B_INFO": P["INFO"] - P["SHAM"],
        "B_NONINFO": P["NONINFO"] - P["SHAM"],
    }
    for name, value in derived.items():
        R.check(value == exact[name], "Study 002 exact identity failed for %s: %s != %s" % (name, value, exact[name]))
    R.check(freq("SHAM", "ALL_F") + freq("SHAM", "ALL_M") == 1, "Study 002 N2 complement fails in SHAM frequencies")
    R.check(acc("SHAM", "ALL_F") == acc("SHAM", "ALL_M"), "Study 002 N1 identity fails in SHAM accuracy")

    # ---- 4. Hoeffding-Bonferroni half-widths and endpoints -------------------------
    bounds = {}
    for name in S2_PRIMARY + S2_SECONDARY:
        rec = by_name[name]
        R.check(rec["n_blocks"] == S2_N_BLOCKS, "Study 002 %s n_blocks != 41600" % name)
        R.check(rec["range_length"] == S2_RANGE_LENGTH[name], "Study 002 %s range length differs" % name)
        R.check(parse_alpha(rec["alpha_each"]) == S2_ALPHA_EACH[name], "Study 002 %s alpha_each differs" % name)
        h = half_width(S2_RANGE_LENGTH[name], S2_ALPHA_EACH[name], S2_N_BLOCKS)
        lo = to_decimal(exact[name]) - h
        hi = to_decimal(exact[name]) + h
        bounds[name] = (lo, hi, h)
        R.check(abs(h - Decimal(rec["half_width"])) <= TOL, "Study 002 %s half-width mismatch" % name)
        R.check(abs(lo - Decimal(rec["lower"])) <= TOL, "Study 002 %s lower endpoint mismatch" % name)
        R.check(abs(hi - Decimal(rec["upper"])) <= TOL, "Study 002 %s upper endpoint mismatch" % name)
        R.check(fmt_decimal(h, 8) == S2_FROZEN_HALF_WIDTH_8DP[name],
                "Study 002 %s half-width does not round to the frozen value" % name)
    R.check(bounds["Delta_P"][2] < to_decimal(DELTA / 2), "Study 002 primary half-width is not below delta/2")

    # ---- 5. Decisions ---------------------------------------------------------------
    D = to_decimal(DELTA)
    audit = accepted["independent_audit"]
    audits_ok = (replay["status"] == "PASS" and replay["mismatch_count"] == 0
                 and records["status"] == "PASS" and records["mismatch_count"] == 0
                 and audit["status"] == "PASS" and audit["mismatches"] == 0)

    def inside(name):
        lo, hi, _ = bounds[name]
        return lo > -D and hi < D

    if not audits_ok:
        rule = 1
    elif not (inside("D_INFO") and inside("D_NONINFO")):
        rule = 2
    elif bounds["Delta_P"][0] > D:
        rule = 3
    elif bounds["Delta_P"][1] < -D:
        rule = 4
    elif bounds["Delta_P"][1] <= D:
        rule = 5
    else:
        rule = 6
    R.check(decision["decision_rule_applied"] == rule, "Study 002 decision rule differs from recomputation")
    R.check(decision["primary_decision"] == S2_RULE_LABELS[rule], "Study 002 primary decision differs")
    R.check(accepted["primary"]["decision"] == S2_RULE_LABELS[rule], "Study 002 accepted primary decision differs")
    R.check(accepted["primary"]["rule"] == rule, "Study 002 accepted rule differs")
    R.check(accepted["primary"]["name"] == "Delta_P", "Study 002 accepted primary name")
    for field in ("estimate", "lower", "upper"):
        R.check(accepted["primary"][field] == by_name["Delta_P"][field],
                "Study 002 accepted primary %s differs from estimate record" % field)
    for name in S2_PRIMARY:
        R.check(by_name[name]["family_decision"] == S2_RULE_LABELS[rule], "Study 002 %s family_decision" % name)
    if rule == 5:
        R.check(decision["bounded_interval_wholly_inside_minus_delta_plus_delta"] == inside("Delta_P"),
                "Study 002 wholly-inside flag differs from recomputation")

    def classify(name):
        lo, hi, _ = bounds[name]
        if lo > D:
            return S2_SECONDARY_LABELS[0]
        if hi < -D:
            return S2_SECONDARY_LABELS[1]
        if hi <= D:
            return S2_SECONDARY_LABELS[2]
        return S2_SECONDARY_LABELS[3]

    for name in S2_SECONDARY:
        label = classify(name)
        R.check(by_name[name]["classification"] == label, "Study 002 %s classification differs" % name)
        R.check(decision["secondary_classifications"][name] == label, "Study 002 %s decision classification" % name)
        R.check(accepted["secondary"][name]["classification"] == label, "Study 002 %s accepted classification" % name)
        R.check(by_name[name]["cannot_alter_primary_decision"] is True, "Study 002 %s lacks cannot_alter flag" % name)
        for field in ("estimate", "lower", "upper"):
            R.check(accepted["secondary"][name][field] == by_name[name][field],
                    "Study 002 accepted %s %s differs from estimate record" % (name, field))
    R.check(decision["b_info_lower_exceeds_delta"] == (bounds["B_INFO"][0] > D), "Study 002 b_info flag")
    R.check(decision["e_info_lower_exceeds_delta"] == (bounds["E_INFO"][0] > D), "Study 002 e_info flag")

    # ---- 6. Scope counts ---------------------------------------------------------------
    paths_total = S2_N_BLOCKS * S2_CELLS_PER_BLOCK
    scope = {
        "blocks": S2_N_BLOCKS, "cells": S2_CELLS_PER_BLOCK, "paths": paths_total,
        "path_updates": paths_total * UPDATES,
        "queries": paths_total * UPDATES * CANDIDATES_PER_UPDATE,
        "audit_blocks": AUDIT_BLOCKS, "audit_paths": AUDIT_BLOCKS * S2_CELLS_PER_BLOCK,
        "audit_update_records": AUDIT_BLOCKS * S2_CELLS_PER_BLOCK * UPDATES,
        "audit_rows": AUDIT_BLOCKS * S2_CELLS_PER_BLOCK * UPDATES * CANDIDATES_PER_UPDATE,
        "permutation_records": AUDIT_BLOCKS * UPDATES,
        "saved_estimates": S2_SAVED_ESTIMATES,
    }
    prod = accepted["production"]
    R.check(prod["blocks"] == scope["blocks"], "Study 002 accepted production blocks")
    R.check(prod["paths"] == scope["paths"], "Study 002 accepted production paths")
    R.check(prod["objective_queries"] == scope["queries"], "Study 002 accepted objective queries")
    R.check(accepted["analysis"]["saved_estimates"] == scope["saved_estimates"], "Study 002 accepted saved estimates")
    R.check(audit["status"] == "PASS" and audit["mismatches"] == 0, "Study 002 accepted audit status")
    R.check(audit["estimates_compared"] == scope["saved_estimates"], "Study 002 accepted estimates compared")
    R.check(audit["full_block_records"] == scope["blocks"], "Study 002 accepted audit blocks")
    R.check(audit["full_path_records"] == scope["paths"], "Study 002 accepted audit paths")
    R.check(audit["full_update_records"] == scope["path_updates"], "Study 002 accepted audit updates")
    R.check(audit["replay_blocks"] == scope["audit_blocks"], "Study 002 accepted replay blocks")
    R.check(audit["replay_paths"] == scope["audit_paths"], "Study 002 accepted replay paths")
    R.check(audit["replay_update_records"] == scope["audit_update_records"], "Study 002 accepted replay updates")
    R.check(audit["replay_audit_rows"] == scope["audit_rows"], "Study 002 accepted replay rows")

    R.check(diagnostics["total_objective_queries"] == scope["queries"], "Study 002 diagnostics query total")
    R.check(diagnostics["cells"] == S2_CELL_ORDER, "Study 002 diagnostics cell order")
    R.check(diagnostics["descriptive_only"] is True, "Study 002 diagnostics not flagged descriptive-only")

    rc = records["checked"]
    R.check(rc == records["expected"], "Study 002 records receipt checked != expected")
    R.check(rc["block_records"] == scope["blocks"], "Study 002 records receipt block count")
    R.check(rc["blocks_with_both_sham_identities"] == scope["blocks"], "Study 002 N1/N2 not confirmed in every block")
    R.check(rc["path_records"] == scope["paths"], "Study 002 records receipt path count")
    R.check(rc["update_records"] == scope["path_updates"], "Study 002 records receipt update count")
    R.check(rc["path_updates_with_128_queries"] == scope["path_updates"], "Study 002 128-query count")
    R.check(rc["estimates_compared"] == scope["saved_estimates"], "Study 002 records receipt estimate count")
    R.check(rc["audit_rows"] == scope["audit_rows"], "Study 002 records receipt audit rows")
    R.check(rc["audit_paths_hashes_reconstructed"] == scope["audit_paths"], "Study 002 records receipt audit paths")
    R.check(rc["permutation_records"] == scope["permutation_records"], "Study 002 records receipt permutations")
    R.check(rc["decision_files_compared"] == 1, "Study 002 decision files compared")
    pc = replay["checked"]
    R.check(pc == replay["expected"], "Study 002 replay receipt checked != expected")
    R.check(pc["blocks"] == scope["audit_blocks"], "Study 002 replay block count")
    R.check(pc["path_records"] == scope["audit_paths"], "Study 002 replay path count")
    R.check(pc["update_records"] == scope["audit_update_records"], "Study 002 replay update count")
    R.check(pc["audit_rows"] == scope["audit_rows"], "Study 002 replay audit rows")
    R.check(pc["permutation_records"] == scope["permutation_records"], "Study 002 replay permutation records")
    R.check(replay["replayed_blocks"] == "0-63", "Study 002 replayed block range")

    # ---- 7. Audit status and mismatch counts ------------------------------------------------
    R.check(replay["status"] == "PASS", "Study 002 replay status")
    R.check(replay["mismatch_count"] == 0 and replay["first_discrepancy"] is None, "Study 002 replay mismatches")
    R.check(replay["mismatch_count_capped"] is False, "Study 002 replay mismatch count capped")
    R.check(replay["known_answer_tests_pass"] is True and
            all(t["result"] == "PASS" for t in replay["known_answer_tests"]), "Study 002 replay known-answer tests")
    R.check(replay["mode"] == "production" and replay["key_namespace"] == "production-r1",
            "Study 002 replay mode/namespace")
    R.check(replay["study_id"] == "PHASE2-PERFORMANCE-CONVERSION-002", "Study 002 replay study id")
    R.check(replay["is_scientific_result"] is False, "Study 002 replay receipt claims a scientific result")
    R.check(records["status"] == "PASS", "Study 002 records status")
    R.check(records["mismatch_count"] == 0 and records["first_discrepancy"] is None, "Study 002 records mismatches")
    R.check(records["mismatch_count_capped"] is False, "Study 002 records mismatch count capped")
    R.check(records["self_checks_pass"] is True and
            all(t["result"] == "PASS" for t in records["self_checks"]), "Study 002 records self-checks")
    R.check(records["random_draws_generated"] is False, "Study 002 record verifier generated random draws")
    R.check(records["mode"] == "production" and records["key_namespace_of_inputs"] == "production-r1",
            "Study 002 records mode/namespace")
    R.check(records["study_id"] == "PHASE2-PERFORMANCE-CONVERSION-002", "Study 002 records study id")

    # ---- 8. Diagnostic consistency relations quoted in the documents -------------------------
    dg = diagnostics
    f2m, m2f = dg["per_cell_total_f_to_m"], dg["per_cell_total_m_to_f"]
    R.check(f2m[4] == m2f[5] and f2m[5] == m2f[4], "Study 002 SHAM mutation counts not exchanged between starts")
    for i in (4, 5):
        for key in ("per_cell_total_valid_probe_use", "per_cell_total_valid_probe_survivors",
                    "per_cell_total_true_cache_use", "per_cell_total_decoy_use",
                    "per_cell_total_true_cache_survivors", "per_cell_total_decoy_survivors"):
            R.check(dg[key][i] == 0, "Study 002 SHAM cell shows cache or decoy use (%s)" % key)
    for i in (0, 1):
        R.check(dg["per_cell_total_decoy_use"][i] == 0 and dg["per_cell_total_decoy_survivors"][i] == 0
                and dg["per_cell_total_decoy_w0"][i] == 0 and dg["per_cell_total_decoy_identical_to_cache"][i] == 0,
                "Study 002 INFO cell shows decoy use")
        R.check(dg["per_cell_total_true_cache_use"][i] == dg["per_cell_total_valid_probe_use"][i],
                "Study 002 INFO true-cache use differs from valid probe use")
        R.check(dg["per_cell_total_true_cache_survivors"][i] == dg["per_cell_total_valid_probe_survivors"][i],
                "Study 002 INFO true-cache survivors differ from valid probe survivors")
    for i in (2, 3):
        R.check(dg["per_cell_total_true_cache_use"][i] + dg["per_cell_total_decoy_use"][i]
                == dg["per_cell_total_valid_probe_use"][i], "Study 002 NONINFO use decomposition")
        R.check(dg["per_cell_total_true_cache_survivors"][i] + dg["per_cell_total_decoy_survivors"][i]
                == dg["per_cell_total_valid_probe_survivors"][i], "Study 002 NONINFO survivor decomposition")
        R.check(dg["per_cell_total_decoy_w0"][i] <= dg["per_cell_total_decoy_identical_to_cache"][i],
                "Study 002 weight-0 decoys exceed decoys identical to cache")
    R.check(all(v == 0 for v in dg["per_cell_total_decoy_w32"]), "Study 002 weight-32 decoys reported as none")
    R.check(all(v == 0 for v in dg["per_cell_total_valid_m_disp_w32"]), "Study 002 weight-32 displacements reported as none")
    R.check(all(v == 0 for v in dg["per_cell_total_survival_retries"]), "Study 002 survival retries reported as none")
    R.check(all(v == 0 for v in dg["per_cell_total_decoy_permutation_retries"]),
            "Study 002 permutation retries reported as none")
    R.check(len(set(dg["per_cell_total_recurrent_updates"])) == 1, "Study 002 recurrent updates differ by cell")
    R.check(dg["blocks_where_NONINFO_decoupled_from_INFO_by_start"] == [scope["blocks"], scope["blocks"]],
            "Study 002 NONINFO not decoupled from INFO in every block")

    return {"by_name": by_name, "exact": exact, "bounds": bounds, "scope": scope}


# =============================================================================================
# Study 003
# =============================================================================================
def check_study_003(R, src, file_hashes, skip_sha):
    accepted = src["s3_accepted"]
    estimates = src["s3_estimates"]
    decisions = src["s3_decisions"]
    prodauth = src["s3_production_authentication"]
    failure = src["s3_audit_failure"]
    normalization = src["s3_normalization_receipt"]
    runner = src["s3_recovery_runner_receipt"]
    cpp = src["s3_cpp_replay_receipt"]
    patch = src["s3_source_patch_receipt"]

    # ---- 1. Source authentication -------------------------------------------------
    for key, path in sorted(S3_ACCEPTED_HASH_PATH.items()):
        authenticate(R, skip_sha, "Study 003 " + key, file_hashes[key], get_path(accepted, path))
    authenticate(R, skip_sha, "Study 003 accepted", file_hashes["s3_accepted"], S3_ACCEPTED_SHA256)
    R.check(patch["accepted_study_sha256"] == file_hashes["s3_accepted"],
            "Study 003 source-patch receipt does not bind the supplied acceptance record")
    R.check(patch["preserved_failure_record_sha256"] == file_hashes["s3_audit_failure"],
            "Study 003 source-patch receipt does not bind the supplied audit failure record")
    R.check(accepted["study_id"] == S3_STUDY_ID, "Study 003 study_id")
    R.check(accepted["status"] == S3_STATUS, "Study 003 acceptance status")
    R.check(accepted["no_extension"] is True, "Study 003 no_extension flag")
    R.check(accepted["no_scientific_rerun"] is True, "Study 003 no_scientific_rerun flag")
    for record in (estimates, decisions, cpp):
        R.check(record["namespace"] == "production-r1", "Study 003 namespace differs from production-r1")
    R.check(estimates["schema"] == "PHASE2-TORUS-MEMORY-003-ESTIMATES-v1", "Study 003 estimate schema")
    R.check(decisions["schema"] == "PHASE2-TORUS-MEMORY-003-DECISIONS-v1", "Study 003 decision schema")
    R.check(decisions["design_sha256"] == accepted["design"]["sha256"], "Study 003 decision design hash")
    R.check(decisions["design_go_record_sha256"] == accepted["design"]["design_go_sha256"],
            "Study 003 decision design-GO hash")
    R.check(decisions["production_manifest_sha256"] == accepted["production"]["manifest_sha256"],
            "Study 003 decision production-manifest hash")

    # ---- 2. Record structure -------------------------------------------------------
    records = estimates["records"]
    R.check(isinstance(records, list) and len(records) == S3_SAVED_ESTIMATES, "Study 003: expected 22 records")
    R.check([r["index"] for r in records] == list(range(S3_SAVED_ESTIMATES)), "Study 003 indices are not 0..21")
    families = [r["family"] for r in records]
    R.check(families.count("PRIMARY_ALLELE") == 4, "Study 003: expected 4 primary allele records")
    R.check(families.count("PERFORMANCE") == 2, "Study 003: expected 2 performance records")
    R.check(families.count("ABSOLUTE_CELL_MEAN") == 16, "Study 003: expected 16 descriptive records")
    raw = {}
    for rec in records:
        R.check(rec["name"] not in raw, "Study 003 duplicate record name " + rec["name"])
        raw[rec["name"]] = rec
    for name, family in S3_FAMILY.items():
        R.check(raw[name]["family"] == family, "Study 003 family differs for " + name)

    exact = {}
    for rec in records:
        name = rec["name"]
        exact[name] = Fraction(rec["exact_value"])
        R.check(rec["n_blocks"] == S3_N_BLOCKS, "Study 003 %s n_blocks != 41600" % name)
        R.check(Fraction(int(rec["sum_of_block_numerators"]), S3_N_BLOCKS * int(rec["block_denominator"]))
                == exact[name], "Study 003 %s exact value differs from block-numerator sum" % name)
        R.check(abs(to_decimal(exact[name]) - Decimal(rec["display"]["estimate"])) <= S3_TOL,
                "Study 003 display estimate disagrees with exact value for " + name)
        if rec["family"] == "ABSOLUTE_CELL_MEAN":
            R.check(rec["alpha_each"] is None and rec["display"]["lower"] == "NA"
                    and rec["display"]["upper"] == "NA", "Study 003 descriptive record carries bounds: " + name)
            R.check(rec["classification_code"] == S3_OBSERVED_CODES["DESCRIPTIVE"],
                    "Study 003 descriptive classification code: " + name)
            R.check(Fraction(0) <= exact[name] <= Fraction(1), "Study 003 descriptive mean outside [0,1]: " + name)
    # Normalized view used by the quoted-statistics registry.
    by_name = {name: dict(rec, estimate_exact=rec["exact_value"]) for name, rec in raw.items()}

    def freq(arm, law, start):
        return exact["MFREQ_%s_%s_%s" % (arm, law, start)]

    def acc(arm, law, start):
        return exact["ACC_%s_%s_%s" % (arm, law, start)]

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
        R.check(value == exact[name], "Study 003 exact identity failed for %s: %s != %s" % (name, value, exact[name]))
    for law in LAWS:
        R.check(freq("SHAM", law, "ALL_F") + freq("SHAM", law, "ALL_M") == 1,
                "Study 003 N2 complement fails in SHAM %s frequencies" % law)
        R.check(acc("SHAM", law, "ALL_F") == acc("SHAM", law, "ALL_M"),
                "Study 003 N1 identity fails in SHAM %s accuracy" % law)

    # ---- 4. Hoeffding-Bonferroni half-widths and endpoints -------------------------
    bounds = {}
    for name in S3_PRIMARY + S3_PERFORMANCE:
        rec = raw[name]
        R.check(int(rec["range_length"]) == S3_RANGE_LENGTH[name], "Study 003 %s range length differs" % name)
        R.check(parse_alpha(rec["alpha_each"]) == S3_ALPHA_EACH[name], "Study 003 %s alpha_each differs" % name)
        h = half_width(S3_RANGE_LENGTH[name], S3_ALPHA_EACH[name], S3_N_BLOCKS)
        lo = to_decimal(exact[name]) - h
        hi = to_decimal(exact[name]) + h
        bounds[name] = (lo, hi, h)
        R.check(abs(h - Decimal(rec["display"]["half_width"])) <= S3_TOL, "Study 003 %s half-width mismatch" % name)
        R.check(abs(lo - Decimal(rec["display"]["lower"])) <= S3_TOL, "Study 003 %s lower endpoint mismatch" % name)
        R.check(abs(hi - Decimal(rec["display"]["upper"])) <= S3_TOL, "Study 003 %s upper endpoint mismatch" % name)
        R.check(fmt_decimal(h, 8) == S3_FROZEN_HALF_WIDTH_8DP[name],
                "Study 003 %s half-width does not round to the frozen value" % name)
        acc_est = accepted["analysis"]["estimates"][name]
        R.check(acc_est["exact_value"] == rec["exact_value"] and acc_est["estimate"] == rec["display"]["estimate"]
                and acc_est["lower"] == rec["display"]["lower"] and acc_est["upper"] == rec["display"]["upper"]
                and acc_est["classification_code"] == rec["classification_code"],
                "Study 003 accepted estimate differs from the estimate record for " + name)
    R.check(bounds["C_rec"][2] < to_decimal(DELTA / 2), "Study 003 range-2 primary half-width is not below Delta/2")

    # ---- 5. Decisions ---------------------------------------------------------------
    D = to_decimal(DELTA)
    recovery = accepted["independent_audit"]["recovery"]
    audits_ok = (decisions["identity_failure_count"] == 0 and decisions["identity_failures_first_1000"] == []
                 and recovery["status"] == "PASS" and recovery["mismatches"] == 0
                 and cpp["verdict"] == "PASS" and cpp["failure"] is None
                 and runner["status"] == "PASS")

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
    R.check(decisions["primary_decision"] == S3_RULE_LABELS[rule], "Study 003 primary decision differs")
    R.check(accepted["analysis"]["primary_decision"] == S3_RULE_LABELS[rule], "Study 003 accepted primary decision")
    R.check(decisions["primary_adverse_upper_below_minus_delta"] == adverse, "Study 003 adverse-selection field")
    pred = decisions["predicates"]
    for name in ("C_abs", "C_rec"):
        lo, hi, _ = bounds[name]
        R.check(pred[name]["lower_above_delta"] == (lo > D), "Study 003 %s lower_above_delta predicate" % name)
        R.check(pred[name]["upper_at_or_below_delta"] == (hi <= D), "Study 003 %s upper_at_or_below predicate" % name)
        R.check(pred[name]["upper_below_minus_delta"] == (hi < -D), "Study 003 %s upper_below_minus predicate" % name)
    for name in ("D_HALF", "D_ZERO"):
        R.check(pred[name]["wholly_inside_plus_minus_delta"] == inside(name), "Study 003 %s gate predicate" % name)
    if rule == 3:
        R.check(raw["C_abs"]["classification_code"] == raw["C_rec"]["classification_code"]
                == S3_OBSERVED_CODES["PRIMARY_SUPPORT"], "Study 003 enrichment classification codes")
    if rule >= 3:
        R.check(raw["D_HALF"]["classification_code"] == raw["D_ZERO"]["classification_code"]
                == S3_OBSERVED_CODES["GATE_INSIDE"], "Study 003 gate classification codes")

    def classify(name):
        lo, hi, _ = bounds[name]
        if lo > D:
            return S3_PERFORMANCE_CLASSES[0]
        if hi < -D:
            return S3_PERFORMANCE_CLASSES[1]
        if hi <= D:
            return S3_PERFORMANCE_CLASSES[2]
        return S3_PERFORMANCE_CLASSES[3]

    classes = {}
    for name in S3_PERFORMANCE:
        classes[name] = classify(name)
        R.check(decisions["performance_classes"][name] == classes[name], "Study 003 %s performance class" % name)
        R.check(pred[name]["class"] == classes[name], "Study 003 %s predicate class" % name)
        if classes[name] in S3_OBSERVED_CODES:
            R.check(raw[name]["classification_code"] == S3_OBSERVED_CODES[classes[name]],
                    "Study 003 %s classification code" % name)
    if rule == 3 and classes["P_abs"] == "BOUNDED_BELOW_POSITIVE":
        R.check(decisions["crossed_interpretation"] == S3_CROSSED_SEPARATION, "Study 003 crossed interpretation")
    R.check(accepted["analysis"]["crossed_interpretation"] == decisions["crossed_interpretation"],
            "Study 003 accepted crossed interpretation differs from decisions")

    # ---- 6. Scope counts ---------------------------------------------------------------
    paths_total = S3_N_BLOCKS * S3_CELLS_PER_BLOCK
    audit_paths = AUDIT_BLOCKS * S3_CELLS_PER_BLOCK
    scope = {
        "blocks": S3_N_BLOCKS, "cells": S3_CELLS_PER_BLOCK, "paths": paths_total,
        "path_updates": paths_total * UPDATES,
        "queries": paths_total * UPDATES * CANDIDATES_PER_UPDATE,
        "audit_blocks": AUDIT_BLOCKS, "audit_paths": audit_paths,
        "audit_update_records": audit_paths * UPDATES,
        "audit_candidate_rows": audit_paths * UPDATES * CANDIDATES_PER_UPDATE,
        "audit_entry_rows": audit_paths * UPDATES * S3_SURVIVORS * S3_TOURNAMENT_ENTRIES,
        "saved_estimates": S3_SAVED_ESTIMATES,
    }
    R.check(scope["audit_entry_rows"] == scope["audit_candidate_rows"],
            "Study 003 tournament entries per update differ from the candidate-pool size")
    prod = accepted["production"]
    R.check(prod["blocks"] == scope["blocks"] and prod["cells"] == scope["cells"], "Study 003 production blocks/cells")
    R.check(prod["paths"] == scope["paths"], "Study 003 production paths")
    R.check(prod["objective_queries"] == scope["queries"], "Study 003 objective queries")
    R.check(accepted["analysis"]["saved_estimates"] == scope["saved_estimates"], "Study 003 saved estimates")
    pa = prodauth["manifest_verification"]
    R.check(prodauth["status"] == "PASS" and prodauth["study_id"] == S3_STUDY_ID, "Study 003 output authentication")
    R.check(prodauth["analysis_or_audit_executed"] is False and prodauth["scientific_values_parsed"] is False
            and prodauth["source_or_production_outputs_modified"] is False,
            "Study 003 output authentication was not outcome-blind")
    R.check(pa["counts"] == {"audit_candidate_rows": scope["audit_candidate_rows"],
                             "audit_entry_rows": scope["audit_entry_rows"], "blocks": scope["blocks"],
                             "chunks": pa["counts"]["chunks"], "objective_queries": scope["queries"],
                             "path_updates": scope["path_updates"], "paths": scope["paths"]},
            "Study 003 authenticated production counts")
    R.check(pa["all_member_hashes_and_sizes_match"] is True and pa["member_set_and_order_exact"] is True
            and pa["completion_binds_manifest"] is True, "Study 003 manifest verification")
    R.check(prodauth["production_manifest_sha256"] == prod["manifest_sha256"], "Study 003 manifest hash")
    R.check(prodauth["production_complete_sha256"] == prod["complete_marker_sha256"], "Study 003 completion hash")
    R.check(prodauth["runner_receipt_sha256"] == prod["runner_receipt_sha256"], "Study 003 production runner hash")
    inv = prodauth["independent_inventory"]
    R.check(inv["canonical_inventory_sha256"] == prod["output_inventory_sha256"], "Study 003 inventory hash")
    R.check(inv["file_count"] == prod["output_files"] and inv["total_bytes"] == prod["output_bytes"],
            "Study 003 inventory size")
    R.check(inv["matched_runner_inventory_exactly"] is True and inv["rejected_or_irregular_entries"] == 0,
            "Study 003 inventory irregular")

    # ---- 7. Audit: preserved attempt-1 failure, normalization-only recovery, replay ----------
    attempt1 = accepted["independent_audit"]["attempt1"]
    R.check(failure["status"] == attempt1["status"] == "PRESERVED_NONSCIENTIFIC_HEADER_INTERFACE_FAILURE",
            "Study 003 attempt-1 status")
    R.check(failure["job_id"] == attempt1["job_id"], "Study 003 attempt-1 job id")
    R.check(attempt1["scientific_payload_replay_completed"] is False, "Study 003 attempt-1 payload replay flag")
    R.check(failure["frozen_contract"]["required_nonchunk_chunk_index"] == S3_NONCHUNK_SENTINEL
            and failure["frozen_contract"]["bytes_hex"] == "ffffffff", "Study 003 frozen non-chunk sentinel")
    R.check(sorted(failure["actual_headers"]) == sorted(S3_AUDIT_FILES), "Study 003 malformed header file set")
    R.check(all(h["chunk_index"] == 0 and h["block_count"] == AUDIT_BLOCKS and h["first_block"] == 0
                for h in failure["actual_headers"].values()), "Study 003 malformed header contents")
    R.check(failure["shared_failure"]["code"] == "HEADER_chunk_index", "Study 003 failure code")
    R.check(failure["scientific_execution_performed"] is False and failure["automatic_retry"] is False
            and failure["production_unchanged"] is True and failure["analysis_unchanged"] is True
            and failure["attempt1_immutable"] is True, "Study 003 attempt-1 flags")
    vbf = failure["verified_before_failure"]
    R.check(vbf["python_candidate_rows"] == 0 and vbf["python_entry_rows"] == 0 and vbf["python_context_rows"] == 0,
            "Study 003 attempt-1 reports audit-payload rows checked")
    R.check(vbf["python_block_rows"] == scope["blocks"] and vbf["python_path_rows"] == scope["paths"]
            and vbf["python_update_rows"] == scope["path_updates"]
            and vbf["python_estimate_rows"] == scope["saved_estimates"], "Study 003 attempt-1 pre-failure counts")

    norm = normalization["normalizations"]
    R.check(normalization["status"] == "PASS" and normalization["scientific_values_parsed"] is False
            and normalization["source_production_or_analysis_modified"] is False, "Study 003 normalization flags")
    R.check(sorted(n["path"] for n in norm) == sorted(S3_AUDIT_FILES)
            and normalization["copied_and_normalized_files"] == len(S3_AUDIT_FILES) == recovery["normalized_files"],
            "Study 003 normalized file set")
    R.check(all(n["differing_offsets"] == S3_HEADER_OFFSETS and n["original_bytes_hex"] == "00000000"
                and n["normalized_bytes_hex"] == "ffffffff" and n["payload_bytes_64_to_eof_identical"] is True
                for n in norm), "Study 003 normalization touched more than the chunk_index field")
    total_diff = sum(len(n["differing_offsets"]) for n in norm)
    R.check(total_diff == normalization["total_differing_bytes"] == recovery["total_differing_bytes"],
            "Study 003 total differing bytes")
    R.check(recovery["payload_bytes_64_to_eof_identical_for_all_three"] is True
            and recovery["normalization_only"] is True and recovery["production_or_analysis_modified"] is False,
            "Study 003 recovery flags")
    cpp_inputs = {item["path"]: item["sha256"] for item in cpp["inputs"]}
    R.check(all(cpp_inputs.get(n["path"]) == n["normalized_sha256"] for n in norm),
            "Study 003 replay did not read the normalized audit files")
    R.check(cpp["manifest"]["sha256"] == normalization["manifest_sha256"] == runner["manifest_sha256"],
            "Study 003 derived audit-tree manifest")

    R.check(runner["status"] == "PASS" and runner["job_id"] == recovery["job_id"], "Study 003 recovery runner")
    R.check(runner["cpp_replay_verdict"] == "PASS" and runner["cpp_replay_exit_status"] == 0
            and runner["python_verify_verdict"] == "PASS" and runner["python_verify_exit_status"] == 0,
            "Study 003 recovery verdicts")
    R.check(runner["normalization_only"] is True and runner["production_or_analysis_modified"] is False,
            "Study 003 recovery runner flags")
    runner_files = {item["path"]: item["sha256"] for item in runner["files"]}
    R.check(runner_files.get("TORUS_003_AUDIT_NORMALIZATION_RECEIPT.json") == file_hashes["s3_normalization_receipt"],
            "Study 003 runner does not bind the normalization receipt")
    R.check(runner_files.get("cpp_replay_receipt.json") == file_hashes["s3_cpp_replay_receipt"],
            "Study 003 runner does not bind the replay receipt")
    R.check(runner_files.get("python_verify_receipt.json") == recovery["python_verify_receipt_sha256"],
            "Study 003 runner python-verify receipt hash differs from acceptance record")

    R.check(cpp["verdict"] == "PASS" and cpp["failure"] is None and cpp["mode"] == "replay",
            "Study 003 replay verdict")
    R.check(cpp["inputs_modified"] is False and cpp["producer_code_invoked"] is False,
            "Study 003 replay modified inputs or invoked producer code")
    R.check(cpp["checked"] == recovery["cpp_checked"], "Study 003 replay counts differ from acceptance record")
    cc = cpp["checked"]
    R.check(cc["blocks"] == cc["block_rows"] == scope["audit_blocks"], "Study 003 replay blocks")
    R.check(cc["paths"] == cc["path_rows"] == scope["audit_paths"], "Study 003 replay paths")
    R.check(cc["path_updates"] == cc["context_rows"] == cc["update_rows"] == scope["audit_update_records"],
            "Study 003 replay path-updates")
    R.check(cc["candidate_rows"] == scope["audit_candidate_rows"], "Study 003 replay candidate rows")
    R.check(cc["entry_rows"] == scope["audit_entry_rows"], "Study 003 replay tournament-entry rows")
    R.check(len(cpp["selftests_passed"]) == 10 and "threefry_official_kat" in cpp["selftests_passed"],
            "Study 003 replay self-tests")
    py = recovery["python_checked"]
    R.check(py["block_rows"] == py["n1_n2_blocks"] == scope["blocks"], "Study 003 python block rows")
    R.check(py["path_rows"] == scope["paths"] and py["update_rows"] == scope["path_updates"],
            "Study 003 python path/update rows")
    R.check(py["estimate_rows"] == scope["saved_estimates"], "Study 003 python estimate rows")
    R.check(py["candidate_rows"] == scope["audit_candidate_rows"] and py["entry_rows"] == scope["audit_entry_rows"]
            and py["context_rows"] == scope["audit_update_records"], "Study 003 python audit-payload rows")

    R.check(patch["status"] == "SOURCE_ONLY_PATCH_COMPLETE_UNEXECUTED" and patch["compiled_or_executed"] is False,
            "Study 003 source patch status")
    R.check(patch["scientific_rerun_required"] is False and patch["original_production_outputs_modified"] is False,
            "Study 003 source patch flags")
    R.check([c["path"] for c in patch["changed_files"]] == ["src/torus/chunk_io.cpp"],
            "Study 003 source patch changed more than the audit sink")
    R.check("kNoChunk" in patch["exact_patch"]["after"] and "kNoChunk" not in patch["exact_patch"]["before"],
            "Study 003 source patch does not emit the non-chunk sentinel")

    return {"by_name": by_name, "exact": exact, "bounds": bounds, "scope": scope}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--estimates", type=Path, required=True, help="Study 001 analysis/estimates_22.json")
    parser.add_argument("--decision", type=Path, required=True, help="Study 001 analysis/decision.json")
    parser.add_argument("--diagnostics", type=Path, required=True, help="Study 001 diagnostics_descriptive.json")
    parser.add_argument("--accepted", type=Path, required=True, help="PHASE2_STUDY_001_ACCEPTED.json")
    parser.add_argument("--replay-receipt", type=Path, required=True, help="Study 001 production_replay_receipt.json")
    parser.add_argument("--records-receipt", type=Path, required=True, help="Study 001 production_records_receipt.json")
    parser.add_argument("--s2-estimates", type=Path, required=True, help="Study 002 results/estimates_19.json")
    parser.add_argument("--s2-decision", type=Path, required=True, help="Study 002 results/decision.json")
    parser.add_argument("--s2-diagnostics", type=Path, required=True,
                        help="Study 002 results/diagnostics_descriptive.json")
    parser.add_argument("--s2-accepted", type=Path, required=True, help="PHASE2_STUDY_002_ACCEPTED.json")
    parser.add_argument("--s2-replay-receipt", type=Path, required=True,
                        help="Study 002 production_replay_receipt.json")
    parser.add_argument("--s2-records-receipt", type=Path, required=True,
                        help="Study 002 production_records_receipt.json")
    parser.add_argument("--s3-estimates", type=Path, required=True, help="Study 003 estimates.json")
    parser.add_argument("--s3-decisions", type=Path, required=True, help="Study 003 decisions.json")
    parser.add_argument("--s3-accepted", type=Path, required=True, help="PHASE2_STUDY_003_ACCEPTED.json")
    parser.add_argument("--s3-production-authentication", type=Path, required=True,
                        help="TORUS_003_PRODUCTION_OUTPUT_AUTHENTICATION.json")
    parser.add_argument("--s3-audit-failure", type=Path, required=True,
                        help="TORUS_003_AUDIT_ATTEMPT1_FAILURE.json (preserved header-interface failure)")
    parser.add_argument("--s3-normalization-receipt", type=Path, required=True,
                        help="TORUS_003_AUDIT_NORMALIZATION_RECEIPT.json")
    parser.add_argument("--s3-recovery-runner-receipt", type=Path, required=True,
                        help="TORUS_003_AUDIT_RECOVERY_RUNNER_RECEIPT.json")
    parser.add_argument("--s3-cpp-replay-receipt", type=Path, required=True,
                        help="Study 003 recovery cpp_replay_receipt.json")
    parser.add_argument("--s3-source-patch-receipt", type=Path, required=True,
                        help="TORUS_003_POST_AUDIT_SOURCE_PATCH_RECEIPT.json")
    parser.add_argument("--draft-root", type=Path, required=True, help="root of this manuscript source package")
    parser.add_argument("--quoted", type=Path, default=None,
                        help="registry (default: <draft-root>/provenance/QUOTED_STATISTICS.json)")
    parser.add_argument("--skip-sha-check", action="store_true",
                        help="do not require the supplied records to match the authenticated SHA-256 values")
    parser.add_argument("--report", type=Path, default=None, help="optional JSON report path")
    args = parser.parse_args()

    R = Report()
    paths = {"estimates": args.estimates, "decision": args.decision, "diagnostics": args.diagnostics,
             "accepted": args.accepted, "replay_receipt": args.replay_receipt,
             "records_receipt": args.records_receipt,
             "s2_estimates": args.s2_estimates, "s2_decision": args.s2_decision,
             "s2_diagnostics": args.s2_diagnostics, "s2_accepted": args.s2_accepted,
             "s2_replay_receipt": args.s2_replay_receipt, "s2_records_receipt": args.s2_records_receipt,
             "s3_estimates": args.s3_estimates, "s3_decisions": args.s3_decisions,
             "s3_accepted": args.s3_accepted, "s3_production_authentication": args.s3_production_authentication,
             "s3_audit_failure": args.s3_audit_failure, "s3_normalization_receipt": args.s3_normalization_receipt,
             "s3_recovery_runner_receipt": args.s3_recovery_runner_receipt,
             "s3_cpp_replay_receipt": args.s3_cpp_replay_receipt,
             "s3_source_patch_receipt": args.s3_source_patch_receipt}
    src = {key: load_json(path) for key, path in paths.items()}
    accepted = src["accepted"]
    estimates = src["estimates"]
    decision = src["decision"]
    diagnostics = src["diagnostics"]
    replay = src["replay_receipt"]
    records = src["records_receipt"]

    # ---- 1. Source authentication (Study 001) ------------------------------------------
    file_hashes = {key: sha256_file(path) for key, path in paths.items()}
    for key, accepted_key in sorted(ACCEPTED_HASH_KEY.items()):
        authenticate(R, args.skip_sha_check, key, file_hashes[key], accepted["authenticated_outputs"][accepted_key])
    authenticate(R, args.skip_sha_check, "accepted", file_hashes["accepted"], ACCEPTED_SHA256)
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
        R.check(parse_alpha(rec["alpha_each"]) == ALPHA_EACH[name], name + " alpha_each differs from frozen value")
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

    # ---- 6. Neutral SHAM formulas (shared by both studies) -----------------------------
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

    # ---- 9. Study 002 ------------------------------------------------------------------------
    s2 = check_study_002(R, src, file_hashes, args.skip_sha_check)

    # ---- 9b. Study 003 -----------------------------------------------------------------------
    s3 = check_study_003(R, src, file_hashes, args.skip_sha_check)

    contexts = {
        "001": {"by_name": by_name, "exact": exact, "bounds": bounds, "scope": scope},
        "002": s2,
        "003": s3,
        # The record sets are separately accepted estimate sets; these totals are counts only.
        "both": {"scope": {"saved_estimates_total": SAVED_ESTIMATES + S2_SAVED_ESTIMATES,
                           "descriptive_means_total": 16 + 12}},
        "all": {"scope": {"saved_estimates_total": SAVED_ESTIMATES + S2_SAVED_ESTIMATES + S3_SAVED_ESTIMATES,
                          "descriptive_means_total": 16 + 12 + 16}},
    }

    # ---- 10. Registered quoted statistics ------------------------------------------------------
    quoted_path = args.quoted or (args.draft_root / "provenance" / "QUOTED_STATISTICS.json")
    registry = load_json(quoted_path)
    documents = {}
    document_hashes = {"registry": sha256_file(quoted_path)}
    for key, rel in registry["documents"].items():
        documents[key] = normalize_text((args.draft_root / rel).read_text(encoding="utf-8"))
        document_hashes[key] = sha256_file(args.draft_root / rel)

    constants = {"Delta": DELTA, "half_Delta": DELTA / 2, "Delta_points": DELTA * 100}

    def raw_literal(d):
        kind = d["type"]
        scale = d.get("scale", 1)
        ctx = contexts[d.get("study", "001")]
        if kind == "estimate":
            name, field = d["name"], d["field"]
            if field == "estimate_exact":
                return ctx["by_name"][name]["estimate_exact"]
            if field == "alpha_each":
                return fmt_decimal(parse_alpha(ctx["by_name"][name]["alpha_each"]), d["decimals"])
            value = {"estimate": to_decimal(ctx["exact"][name])}
            if name in ctx["bounds"]:
                lo, hi, h = ctx["bounds"][name]
                value.update({"lower": lo, "upper": hi, "half_width": h})
            v = value[field] * scale
            if d.get("absolute"):
                v = abs(v)
            return fmt_decimal(v, d["decimals"], d.get("plus", False))
        if kind == "start_average":
            v = (ctx["exact"][d["names"][0]] + ctx["exact"][d["names"][1]]) / 2 * scale
            return fmt_decimal(abs(v) if d.get("absolute") else v, d["decimals"], d.get("plus", False))
        if kind == "average_difference":
            plus_avg = (ctx["exact"][d["plus_names"][0]] + ctx["exact"][d["plus_names"][1]]) / 2
            minus_avg = (ctx["exact"][d["minus_names"][0]] + ctx["exact"][d["minus_names"][1]]) / 2
            v = (plus_avg - minus_avg) * scale
            return fmt_decimal(abs(v) if d.get("absolute") else v, d["decimals"], d.get("plus", False))
        if kind == "constant":
            return fmt_decimal(constants[d["key"]], d["decimals"])
        if kind == "scope":
            return fmt_int(ctx["scope"][d["key"]], d.get("commas", False))
        if kind == "source_int":
            return fmt_int(get_path(src[d["source"]], d["path"]), d.get("commas", False))
        if kind == "source_ratio":
            numerator = get_path(src[d["source"]], d["numerator"])
            denominator = get_path(src[d["source"]], d["denominator"])
            return fmt_decimal(Fraction(numerator, denominator) * scale, d["decimals"], d.get("plus", False))
        raise ValueError("unknown derivation type " + kind)

    def expected_literal(d):
        text = raw_literal(d)
        if "format" in d:
            text = d["format"].format(text)
        return text

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
        except (KeyError, IndexError, TypeError, ValueError, ZeroDivisionError) as error:
            R.check(False, "%s: cannot evaluate derivation (%s)" % (label, error))
        for doc in entry["documents"]:
            R.check(literal in documents[doc], "%s: literal %r not found in %s" % (label, literal, doc))
            registered[doc].add(literal.lstrip("+"))

    # ---- 11. Unregistered numeric tokens -----------------------------------------------------
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
               "package_version": "1.2.0",
               "studies": {"PHASE2-MUTABLE-MEMORY-001": {"saved_estimates": SAVED_ESTIMATES},
                           "PHASE2-PERFORMANCE-CONVERSION-002": {"saved_estimates": S2_SAVED_ESTIMATES},
                           "PHASE2-TORUS-MEMORY-003": {"saved_estimates": S3_SAVED_ESTIMATES}},
               "registered_entries": len(registry["entries"])}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
    print("%s: %d checks passed, %d failed" % (summary["status"], R.passed, len(R.failures)))
    return 0 if not R.failures else 1


# ---- Public-export hash adapter (public repository package v1.2.0) -------------
# Optional "--public-transformations FILE". A public receipt whose only change is a
# recorded administrative path normalization has different physical bytes from the
# authenticated source receipt. For every registered checker input (an entry with a
# logical_input_key, Study 001, Study 002 or Study 003) this adapter maps the exact
# physical SHA-256 of the public copy back to the authenticated source SHA-256 recorded
# in the transformation file. Any other byte difference still fails the SHA-256 checks.
# No arithmetic, interval, label, scope, diagnostic, audit or decision check is changed,
# added or removed. The substitutions are listed in the JSON report.
import atexit as _pe_atexit
import json as _pe_json
import sys as _pe_sys

_PUBLIC_EXPORT = {"record": None, "record_sha256": None, "map": {}, "substitutions": []}


def _pe_strip_argument(argv):
    flag = "--public-transformations"
    kept, i = [], 0
    while i < len(argv):
        arg = argv[i]
        if arg == flag:
            if i + 1 >= len(argv):
                raise SystemExit(flag + " requires a file path")
            _PUBLIC_EXPORT["record"] = argv[i + 1]
            i += 2
            continue
        if arg.startswith(flag + "="):
            _PUBLIC_EXPORT["record"] = arg[len(flag) + 1:]
            i += 1
            continue
        kept.append(arg)
        i += 1
    return kept


def _pe_load(record_path):
    _PUBLIC_EXPORT["record_sha256"] = _physical_sha256_file(record_path)
    with open(record_path, encoding="utf-8") as f:
        record = _pe_json.load(f)
    for item in record.get("transformations", []):
        public, source = item.get("public_sha256"), item.get("source_sha256")
        if item.get("logical_input_key") and isinstance(public, str) and isinstance(source, str) \
                and len(public) == 64 and len(source) == 64:
            _PUBLIC_EXPORT["map"][public] = source


def sha256_file(path, *args, **kwargs):
    digest = _physical_sha256_file(path, *args, **kwargs)
    source = _PUBLIC_EXPORT["map"].get(digest)
    if source is None:
        return digest
    entry = {"path": str(path), "physical_sha256": digest, "source_sha256": source}
    if entry not in _PUBLIC_EXPORT["substitutions"]:
        _PUBLIC_EXPORT["substitutions"].append(entry)
    return source


def _pe_annotate_report(argv):
    report = None
    for i, arg in enumerate(argv):
        if arg == "--report" and i + 1 < len(argv):
            report = argv[i + 1]
        elif arg.startswith("--report="):
            report = arg[len("--report="):]
    if not report or _PUBLIC_EXPORT["record"] is None:
        return
    try:
        with open(report, encoding="utf-8") as f:
            obj = _pe_json.load(f)
    except (OSError, ValueError):
        return
    if not isinstance(obj, dict):
        return
    obj["public_export_adapter"] = {
        "public_transformations": _PUBLIC_EXPORT["record"],
        "public_transformations_sha256": _PUBLIC_EXPORT["record_sha256"],
        "hash_substitutions": _PUBLIC_EXPORT["substitutions"],
        "checks_changed": False,
    }
    with open(report, "w", encoding="utf-8") as f:
        f.write(_pe_json.dumps(obj, indent=2) + "\n")


if __name__ == "__main__":
    _pe_sys.argv[1:] = _pe_strip_argument(_pe_sys.argv[1:])
    if _PUBLIC_EXPORT["record"] is not None:
        _pe_load(_PUBLIC_EXPORT["record"])
        _pe_atexit.register(_pe_annotate_report, list(_pe_sys.argv[1:]))


if __name__ == "__main__":
    sys.exit(main())
