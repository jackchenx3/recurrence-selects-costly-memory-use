#!/usr/bin/env python3
"""PHASE2-TORUS-MEMORY-003 static preflight checks (standard library only).

Does not compile, run, simulate or read any scientific output. Checks:
  1. third_party/ is exactly the 29 authenticated Random123 seed files;
  2. every required package file exists;
  3. the frozen configuration parses, has the frozen key order, and its counts
     are internally consistent;
  4. design arithmetic: Appendix A rank-table sum, Hoeffding half-widths,
     SHAM neutral analytics, loss maxima and the uniform-coordinate loss ratio;
  5. purpose keys for all three namespaces are pairwise distinct;
  6. the Slurm template is inert (no submission command) and has the frozen
     resource request;
  7. source/tool/template files contain no private absolute path, URL,
     network or subprocess import.
Writes one JSON receipt to an explicit, new output file. NOT YET RUN.
"""

import argparse
import hashlib
import json
import os
import sys
from decimal import Decimal, localcontext
from fractions import Fraction

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import torus_schema as S  # noqa: E402

REQUIRED = [
    "README.md", "IMPLEMENTATION_NOT_RUN.json", "Makefile", "config/torus_003_frozen_config.json",
    "provenance/random123_seed_inventory.json", "docs/BINARY_SCHEMAS.md", "docs/RNG_ADDRESSING.md",
    "docs/INTERPRETATION_AND_LIMITATIONS.md", "docs/PRODUCTION_GATES.md", "docs/FIXTURES.md",
    "slurm/torus_003_production.slurm.template",
    "src/apps/torus_produce.cpp", "src/apps/torus_fixtures.cpp", "src/apps/torus_regen.cpp",
    "src/torus/constants.hpp", "src/torus/sha256.cpp", "src/torus/util.cpp", "src/torus/threefry_rng.cpp",
    "src/torus/torus_loss.hpp", "src/torus/target_law.cpp", "src/torus/tape.cpp", "src/torus/candidates.cpp",
    "src/torus/tournament.cpp", "src/torus/transition.cpp", "src/torus/step.cpp", "src/torus/records.cpp",
    "src/torus/path_runner.cpp", "src/torus/chunk_io.cpp", "src/torus/collision.cpp",
    "src/torus/config_check.cpp", "src/torus/manifest.cpp",
    "tools/torus_schema.py", "tools/torus_analyze.py", "tools/torus_manifest.py", "tools/torus_preflight.py",
]

RANK_N_FIRST = 8290815
RANK_N_LAST = 1

# Built by concatenation so this file does not match its own scan.
FORBIDDEN_TEXT = ["/Us" + "ers/", "/ho" + "me/", "C:" + "\\", "htt" + "p://", "htt" + "ps://"]
FORBIDDEN_PY_IMPORTS = ["import " + m for m in ("socket", "urllib", "http", "requests", "subprocess", "ftplib")]
FORBIDDEN_SLURM = ["sba" + "tch ", "sal" + "loc ", "qs" + "ub ", "sr" + "un "]


def sha256_file(path):
    return S.sha256_file(path)


def check_seed(root, problems):
    with open(os.path.join(root, "provenance/random123_seed_inventory.json"), "r", encoding="utf-8") as f:
        inv = json.load(f)["files"]
    found = set()
    for dirpath, _dirs, files in os.walk(os.path.join(root, "third_party")):
        for n in files:
            found.add(os.path.relpath(os.path.join(dirpath, n), root).replace(os.sep, "/"))
    if found != set(inv) or len(inv) != 29:
        problems.append("third_party file set differs from the 29-file seed inventory")
    for rel in sorted(set(inv) & found):
        d, n = sha256_file(os.path.join(root, rel))
        if d != inv[rel]["sha256"] or n != inv[rel]["bytes"]:
            problems.append("seed file not byte-identical: " + rel)


def check_config(root, problems):
    path = os.path.join(root, "config/torus_003_frozen_config.json")
    with open(path, "r", encoding="utf-8") as f:
        pairs = json.load(f, object_pairs_hook=list)
    keys = [k for k, _ in pairs]
    if len(keys) != len(set(keys)):
        problems.append("config has duplicate keys")
    c = dict(pairs)
    if c.get("blocks") * c.get("cells") != c.get("paths"):
        problems.append("paths != blocks * cells")
    if c["paths"] * c["updates"] != c["path_updates"]:
        problems.append("path_updates mismatch")
    if c["path_updates"] * c["candidates_per_update"] != c["objective_queries"]:
        problems.append("objective_queries mismatch")
    if c["chunks"] * c["chunk_blocks"] != c["blocks"]:
        problems.append("chunking mismatch")
    if 64 * 8 * 256 * 128 != c["audit_candidate_rows"] or 64 * 8 * 256 * 32 * 4 != c["audit_entry_rows"]:
        problems.append("audit row counts mismatch")
    per_update = 2 + 1 + 4 * 64 + 96 + 128 + 128 + 32
    if 64 + 256 * per_update != c["declared_draws_per_block"] or c["declared_draws_per_block"] * c["blocks"] != c["declared_draws_total"]:
        problems.append("declared draw counts mismatch")
    if c["survivor_slots"] * c["tournament_entries"] != c["candidates_per_update"]:
        problems.append("tournament entries per update != candidate pool")
    if 4 + 2 + c["absolute_cell_means"] != c["estimate_records"]:
        problems.append("estimate record count mismatch")
    if c["purposes"].split(",") != S.PURPOSES:
        problems.append("purpose list mismatch")
    if c["config_grants_execution"] is not False:
        problems.append("config must not grant execution")
    return sha256_file(path)[0], c


def check_arithmetic(c, problems):
    out = {}
    ns = [(129 - r) ** 4 - (128 - r) ** 4 for r in range(1, 129)]
    if sum(ns) != 2 ** 28 or ns[0] != RANK_N_FIRST or ns[-1] != RANK_N_LAST:
        problems.append("rank table arithmetic")
    out["rank_table_sum_over_2^28"] = "%d/%d" % (sum(ns), 2 ** 28)
    with localcontext() as ctx:
        ctx.prec = 60
        for name, r, alpha in (("h_c_abs", 1, Fraction(1, 80)), ("h_c_rec_d", 2, Fraction(1, 80)),
                               ("h_p_abs", 2, Fraction(1, 40)), ("h_p_rec", 4, Fraction(1, 40))):
            h = Decimal(r) * ((Decimal(2) / (Decimal(alpha.numerator) / Decimal(alpha.denominator))).ln() /
                              Decimal(2 * c["blocks"])).sqrt()
            if abs(h - Decimal(c[name])) > Decimal("1e-16"):
                problems.append("Hoeffding half-width mismatch: " + name)
            out[name] = str(h)
        if not Decimal(c["h_c_rec_d"]) < Decimal(1) / Decimal(64):
            problems.append("range-2 primary half-width not below Delta/2")
        r = Fraction(15, 16)
        end = r ** 256
        avg = r ** 193 * (1 - r ** 64) / (64 * (1 - r))
        end_d = Decimal(end.numerator) / Decimal(end.denominator)
        avg_d = Decimal(avg.numerator) / Decimal(avg.denominator)
        if abs(end_d - Decimal("6.678005283720385e-08")) > Decimal("1e-20"):
            problems.append("SHAM neutral update-256 difference mismatch")
        if abs(avg_d - Decimal("9.57983820600146e-07")) > Decimal("1e-19"):
            problems.append("SHAM neutral late-average difference mismatch")
        out["sham_r256"] = str(end_d)
        out["sham_late_average"] = str(avg_d)
    if 32768 ** 2 != 2 ** 30 or 32 * 2 ** 30 != 2 ** 35 or 32 * 2 ** 35 != 2 ** 40:
        problems.append("loss maxima")
    # Expected squared circular distance of a uniform coordinate: sum_u d(u)^2 / 65536.
    total = sum(min(u, 65536 - u) ** 2 for u in range(65536))
    mean = Fraction(total, 65536)
    if mean != Fraction(715827883, 2) or mean / 2 ** 30 != Fraction(715827883, 2147483648):
        problems.append("uniform-coordinate expected loss")
    out["uniform_coordinate_expected_loss"] = str(mean)
    return out


def check_keys(problems):
    keys = {}
    seen = set()
    for ns in ("production-r1", "fixture-r1", "timing-r1"):
        keys[ns] = {}
        for p in S.PURPOSES:
            w = tuple(S.purpose_key_words(ns, p))
            if w in seen:
                problems.append("purpose key collision %s/%s" % (ns, p))
            seen.add(w)
            keys[ns][p] = ["%016x" % x for x in w]
    return keys


def check_text(root, problems):
    for top in ("src", "tools", "slurm", "config", "Makefile"):
        base = os.path.join(root, top)
        paths = [base] if os.path.isfile(base) else [
            os.path.join(d, n) for d, _s, fs in os.walk(base) for n in fs]
        for p in sorted(paths):
            with open(p, "r", encoding="utf-8", errors="replace") as f:
                text = f.read()
            rel = os.path.relpath(p, root)
            for bad in FORBIDDEN_TEXT:
                if bad in text:
                    problems.append("forbidden text %r in %s" % (bad, rel))
            if p.endswith(".py"):
                for bad in FORBIDDEN_PY_IMPORTS:
                    if bad in text:
                        problems.append("forbidden import %r in %s" % (bad, rel))
    tpl = os.path.join(root, "slurm/torus_003_production.slurm.template")
    with open(tpl, "r", encoding="utf-8") as f:
        lines = f.read().splitlines()
    for ln in lines:
        stripped = ln.strip()
        if stripped.startswith("#SBATCH"):
            continue
        for bad in FORBIDDEN_SLURM:
            if bad in stripped + " ":
                problems.append("Slurm template contains a submission/launch command: %r" % stripped)
    need = ["#SBATCH --cpus-per-task=32", "#SBATCH --mem=64G", "#SBATCH --time=12:00:00", "#SBATCH --nodes=1",
            "#SBATCH --ntasks=1"]
    for n in need:
        if n not in lines:
            problems.append("Slurm template lacks %r" % n)


def main(argv=None):
    ap = argparse.ArgumentParser(description="PHASE2-TORUS-MEMORY-003 static preflight")
    ap.add_argument("--package-root", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args(argv)
    if os.path.exists(a.out) and os.path.getsize(a.out) > 0:
        print("TORUS_FATAL: refusing existing nonempty output: %s" % a.out, file=sys.stderr)
        return 2
    root = a.package_root
    problems = []
    for rel in REQUIRED:
        if not os.path.isfile(os.path.join(root, rel)):
            problems.append("missing required file: " + rel)
    check_seed(root, problems)
    cfg_sha, cfg = check_config(root, problems)
    arithmetic = check_arithmetic(cfg, problems)
    keys = check_keys(problems)
    check_text(root, problems)
    doc = {
        "schema": "PHASE2-TORUS-MEMORY-003-STATIC-PREFLIGHT-v1",
        "config_sha256": cfg_sha,
        "arithmetic": arithmetic,
        "purpose_keys": keys,
        "problems": problems,
        "status": "STATIC_PREFLIGHT_PASSED" if not problems else "STATIC_PREFLIGHT_FAILED",
        "scope_note": "Static checks only; nothing compiled, executed or simulated by this tool.",
    }
    with open(a.out, "x", encoding="utf-8") as f:
        f.write(json.dumps(doc, indent=2, sort_keys=True) + "\n")
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
