#!/usr/bin/env python3
"""Independent collision-schema checker (stdlib only; does not read C++ source).

Enumerates every declared draw from config rng.coordinate_schemas for each of the
eight paired cells and classifies counter coincidences:

  INTENTIONAL_PAIRED_CELL_SHARING  one declared draw seen in several cells -> same counter (required)
  WITHIN_PURPOSE_DUPLICATE         distinct declared draws, same purpose, same counter (FAIL)
  PURPOSE_SEPARATED_REUSE          same counter under different purposes/keys (allowed)

Also checks that the nine purpose keys (via hashlib) are pairwise distinct and
disjoint from the fixture namespace. Survival retries are enumerated to the
configured depth; the retry field maps verbatim to the subindex word.

usage: collision_schema_check.py --config config/frozen_config.json [--receipt NEW_FILE]
"""
import argparse
import ast
import itertools
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mmem_common as C  # noqa: E402

BLOCKS = (0, 1, 63, 64, 20800, 41598, 41599)


def compile_expr(text):
    """Allow only integer literals, field names, + and *."""
    tree = ast.parse(text, mode="eval")
    for node in ast.walk(tree):
        ok = isinstance(node, (ast.Expression, ast.BinOp, ast.Add, ast.Mult, ast.Name, ast.Load))
        ok = ok or (hasattr(ast, "Num") and isinstance(node, ast.Num))
        ok = ok or (hasattr(ast, "Constant") and isinstance(node, ast.Constant) and isinstance(node.value, int))
        if not ok:
            raise ValueError("disallowed expression: " + text)
    code = compile(tree, "<schema>", "eval")
    return lambda env: eval(code, {"__builtins__": {}}, dict(env))


def enumerate_purpose(schema, block, retry_depth):
    """Yield (identity, counter) for one purpose in one block (cell-independent schema)."""
    entity = compile_expr(schema["entity"])
    sub = compile_expr(schema["subindex"])
    names = list(schema["fields"].keys())
    ranges = []
    for n in names:
        lo, hi = schema["fields"][n]
        if n == "retry":
            hi = min(hi, retry_depth - 1)
        ranges.append(range(lo, hi + 1))
    u_lo, u_hi = schema["update"]
    for t in range(u_lo, u_hi + 1):
        for values in itertools.product(*ranges):
            env = dict(zip(names, values))
            yield (t,) + tuple(values), (block, t, entity(env), sub(env))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", required=True)
    ap.add_argument("--receipt")
    args = ap.parse_args()
    cfg = C.load_config(args.config)
    errors = C.check_config(cfg)
    rng = cfg["rng"]
    depth = rng["collision_enumeration_retry_depth"]
    schemas = rng["coordinate_schemas"]
    report = {"receipt": "MMEM-COLLISION-SCHEMA-CHECK-PY-1", "config_errors": errors,
              "blocks_enumerated": list(BLOCKS), "survival_retry_depth_enumerated": depth,
              "intentional_paired_cell_sharing": 0, "within_purpose_duplicates_for_distinct_draws": 0,
              "purpose_separated_reuse_allowed": 0, "range_violations": 0, "declared_draws_per_cell_per_block": 0}

    keys = {}
    for ns in (C.PRODUCTION_NAMESPACE, C.FIXTURE_NAMESPACE):
        for p in C.PURPOSES:
            keys[(ns, p)] = C.purpose_key(ns, p)[1:]
    report["keys_pairwise_distinct_and_namespaces_disjoint"] = len(set(keys.values())) == len(keys)
    report["schema_purposes_match"] = list(schemas.keys()) == list(C.PURPOSES)

    for block in BLOCKS:
        per_cell_total = 0
        counter_purposes = {}
        for purpose in C.PURPOSES:
            seen = {}  # counter -> set of identities (cell excluded)
            for cell in range(C.CELLS):
                for identity, ctr in enumerate_purpose(schemas[purpose], block, depth):
                    if cell == 0:
                        per_cell_total += 1
                        if not all(0 <= w <= 0xFFFFFFFF for w in ctr) or block > rng["block_range"][1]:
                            report["range_violations"] += 1
                    ids = seen.setdefault(ctr, {})
                    ids.setdefault(identity, set()).add(cell)
            for ctr, ids in seen.items():
                if len(ids) > 1:
                    report["within_purpose_duplicates_for_distinct_draws"] += len(ids) - 1
                for cells in ids.values():
                    if len(cells) == C.CELLS:
                        report["intentional_paired_cell_sharing"] += C.CELLS - 1
                counter_purposes.setdefault(ctr, set()).add(purpose)
        report["declared_draws_per_cell_per_block"] = per_cell_total
        report["purpose_separated_reuse_allowed"] += sum(len(p) - 1 for p in counter_purposes.values())

    expected_sharing = report["declared_draws_per_cell_per_block"] * (C.CELLS - 1) * len(BLOCKS)
    ok = (not errors and report["keys_pairwise_distinct_and_namespaces_disjoint"] and report["schema_purposes_match"]
          and report["within_purpose_duplicates_for_distinct_draws"] == 0 and report["range_violations"] == 0
          and report["intentional_paired_cell_sharing"] == expected_sharing)
    report["status"] = "PASS" if ok else "FAIL"
    text = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.receipt:
        with open(args.receipt, "x", encoding="utf-8") as f:
            f.write(text)
    sys.stdout.write(text)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
