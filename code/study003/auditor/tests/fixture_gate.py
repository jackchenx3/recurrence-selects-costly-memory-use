#!/usr/bin/env python3
"""Future non-scientific corruption/self-test gate for the independent
PHASE2-TORUS-MEMORY-003 auditor.  Python 3.8, standard library only.

NOT RUN during construction.  Requires separate fixture authority.

What it does, entirely inside a fresh temporary directory:
  1. compact Python hand checks of the inferential arithmetic;
  2. `torus_audit selftest` (SHA-256 vectors, official Threefry KAT, key byte
     order, counter order, lane extraction, purpose separation, range
     refusal, circular loss, rank table, one/two-update hand trace);
  3. `torus_audit emit-fixture` builds a small synthetic record tree in the
     non-production namespace fixture-r1 (never a scientific block); the
     fixture estimate file and a sha256sum manifest are added here;
  4. both verifiers must PASS the untouched baseline;
  5. each deliberate bounded corruption must make every listed verifier return
     INVALID, for a reason other than a stale manifest (except the manifest
     case itself).
It never samples a production block and never invokes producer binaries.

Usage:
  python3 fixture_gate.py --binary /abs/build/torus_audit --source-dir <package> \
      --receipt <new file> [--blocks 2] [--work-parent DIR]
Exit codes: 0 PASS, 1 FAIL (receipt written), 2 usage/refusal, 3 receipt not written.
"""

import argparse
import datetime
import hashlib
import importlib.util
import json
import os
import platform
import shutil
import stat
import struct
import subprocess
import sys
import tempfile

GATE_SCHEMA = "PHASE2-TORUS-MEMORY-003-AUDITOR-FIXTURE-GATE-RECEIPT-v1"
RECORD_FILES = (
    "updates/chunk_00000.t3u", "paths/chunk_00000.t3p", "blocks/chunk_00000.t3b", "estimates.t3s",
    "audit/audit_candidates.t3c", "audit/audit_entries.t3e", "audit/audit_context.t3x",
)
ROW = {"t3u": 24, "t3p": 224, "t3b": 136, "t3s": 256, "t3c": 104, "t3e": 16, "t3x": 4240}
TIMEOUT_SECONDS = 3600
UPD = "updates/chunk_00000.t3u"
PTH = "paths/chunk_00000.t3p"
BLK = "blocks/chunk_00000.t3b"
EST = "estimates.t3s"
CAN = "audit/audit_candidates.t3c"
ENT = "audit/audit_entries.t3e"
CTX = "audit/audit_context.t3x"


def utc_now():
    return datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            b = f.read(1 << 22)
            if not b:
                break
            h.update(b)
    return h.hexdigest()


def load_verifier(source_dir):
    path = os.path.join(source_dir, "python", "torus_record_verifier.py")
    spec = importlib.util.spec_from_file_location("t3a_torus_record_verifier", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod, path


# Row indices in the fixed orders of BINARY_SCHEMAS.md.
def cand_row(b, t, cell, c):
    return ((b * 256 + (t - 1)) * 8 + cell) * 128 + c


def entry_row(b, t, cell, s, e):
    return ((b * 256 + (t - 1)) * 8 + cell) * 128 + 4 * s + e


def ctx_row(b, t, cell):
    return (b * 256 + (t - 1)) * 8 + cell


def upd_row(b, cell, t):
    return (b * 8 + cell) * 256 + (t - 1)


def offset(rel, row, field):
    return 64 + row * ROW[rel.rsplit(".", 1)[1]] + field


def xor_byte(root, rel, off, mask=0x01):
    with open(os.path.join(root, rel), "r+b") as f:
        f.seek(off)
        old = f.read(1)
        if len(old) != 1:
            raise RuntimeError("corruption offset outside file")
        f.seek(off)
        f.write(bytes([old[0] ^ mask]))


def read_row(f, rs, i):
    f.seek(64 + i * rs)
    b = f.read(rs)
    if len(b) != rs:
        raise RuntimeError("row outside file")
    return b


def swap_rows(root, rel, a, b):
    rs = ROW[rel.rsplit(".", 1)[1]]
    with open(os.path.join(root, rel), "r+b") as f:
        ra, rb = read_row(f, rs, a), read_row(f, rs, b)
        f.seek(64 + a * rs)
        f.write(rb)
        f.seek(64 + b * rs)
        f.write(ra)


def duplicate_row(root, rel, src, dst):
    rs = ROW[rel.rsplit(".", 1)[1]]
    with open(os.path.join(root, rel), "r+b") as f:
        r = read_row(f, rs, src)
        f.seek(64 + dst * rs)
        f.write(r)


def drop_last_row(root, rel):
    rs = ROW[rel.rsplit(".", 1)[1]]
    path = os.path.join(root, rel)
    with open(path, "r+b") as f:
        f.seek(24)
        count = struct.unpack("<Q", f.read(8))[0]
        f.seek(24)
        f.write(struct.pack("<Q", count - 1))
        f.truncate(64 + (count - 1) * rs)


def append_zero_bytes(root, rel, n):
    with open(os.path.join(root, rel), "ab") as f:
        f.write(bytes(n))


def write_manifest(root, path):
    lines = ["%s  %s\n" % (sha256_file(os.path.join(root, rel)), rel) for rel in RECORD_FILES]
    with open(path, "x") as f:
        f.write("".join(lines))


def corrupt_manifest(path):
    with open(path, "r") as f:
        lines = f.read().split("\n")
    for i, line in enumerate(lines):
        if line.endswith("  " + BLK):
            c = line[0]
            lines[i] = ("0" if c != "0" else "1") + line[1:]
    with open(path, "w") as f:
        f.write("\n".join(lines))


def cases():
    """(name, verifiers that must return INVALID, corruption function)."""
    both = ("cpp", "py")
    return [
        ("candidate_phenotype", ("cpp",), lambda r: xor_byte(r, CAN, offset(CAN, cand_row(0, 2, 2, 37), 40))),
        ("candidate_loss", ("cpp",), lambda r: xor_byte(r, CAN, offset(CAN, cand_row(0, 3, 1, 70), 16))),
        ("candidate_family", both, lambda r: xor_byte(r, CAN, offset(CAN, cand_row(0, 3, 6, 12), 8))),
        ("entry_entered_index", ("cpp",), lambda r: xor_byte(r, ENT, offset(ENT, entry_row(0, 4, 5, 7, 2), 9))),
        ("entry_winner", both, lambda r: xor_byte(r, ENT, offset(ENT, entry_row(0, 4, 5, 7, 0), 10))),
        ("entry_post_label", both, lambda r: xor_byte(r, ENT, offset(ENT, entry_row(0, 6, 2, 3, 1), 12))),
        ("entry_cache_byte", both, lambda r: xor_byte(r, ENT, offset(ENT, entry_row(0, 6, 2, 3, 2), 14))),
        ("context_target", both, lambda r: xor_byte(r, CTX, offset(CTX, ctx_row(0, 5, 0), 16))),
        ("context_prestate", both, lambda r: xor_byte(r, CTX, offset(CTX, ctx_row(0, 2, 3), 80))),
        ("update_query_count", both, lambda r: xor_byte(r, UPD, offset(UPD, upd_row(0, 4, 9), 10))),
        ("path_digest", both, lambda r: xor_byte(r, PTH, offset(PTH, 3, 96))),
        ("block_n1_n2_flag", both, lambda r: xor_byte(r, BLK, offset(BLK, 0, 4))),
        ("estimate_numerator", ("py",), lambda r: xor_byte(r, EST, offset(EST, 6, 8))),
        ("estimate_classification", ("py",), lambda r: xor_byte(r, EST, offset(EST, 0, 3))),
        ("missing_update_row", both, lambda r: drop_last_row(r, UPD)),
        ("trailing_path_bytes", both, lambda r: append_zero_bytes(r, PTH, 224)),
        ("reordered_update_rows", both, lambda r: swap_rows(r, UPD, 0, 1)),
        ("duplicated_update_row", both, lambda r: duplicate_row(r, UPD, 0, 1)),
        ("reordered_candidate_rows", both, lambda r: swap_rows(r, CAN, 0, 1)),
        ("manifest_hash", both, None),
    ]


def load_receipt(path):
    try:
        with open(path, "r") as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def run_tool(kind, binary, source_dir, verifier_path, root, manifest, receipt, blocks):
    if kind == "cpp":
        cmd = [binary, "replay", "--root", root, "--manifest", manifest, "--source-dir", source_dir,
               "--receipt", receipt, "--namespace", "fixture-r1", "--blocks", str(blocks)]
    else:
        cmd = [sys.executable, verifier_path, "verify", "--root", root, "--manifest", manifest,
               "--receipt", receipt, "--profile", "fixture", "--fixture-blocks", str(blocks)]
    p = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=TIMEOUT_SECONDS)
    rec = load_receipt(receipt)
    verdict = rec.get("verdict") if isinstance(rec, dict) else None
    failure = rec.get("failure") if isinstance(rec, dict) else None
    code = failure.get("code") if isinstance(failure, dict) else None
    return {"tool": kind, "exit": p.returncode, "verdict": verdict, "failure_code": code}


def gate(args, verifier, verifier_path, tmp):
    steps = []
    ok = True

    def record(name, passed, detail):
        nonlocal ok
        ok = ok and passed
        steps.append({"step": name, "pass": bool(passed), "detail": detail})

    try:
        record("python_math_hand_checks", True, verifier.self_check_math())
    except Exception as e:  # bounded: exception type only
        record("python_math_hand_checks", False, type(e).__name__)

    sreceipt = os.path.join(tmp, "cpp_selftest_receipt.json")
    p = subprocess.run([args.binary, "selftest", "--source-dir", args.source_dir, "--receipt", sreceipt],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=TIMEOUT_SECONDS)
    srec = load_receipt(sreceipt) or {}
    record("cpp_selftest_kat_and_hand_trace", p.returncode == 0 and srec.get("verdict") == "PASS",
           {"exit": p.returncode, "selftests_passed": srec.get("selftests_passed")})
    if not ok:
        return steps, False

    base = os.path.join(tmp, "base")
    os.mkdir(base)
    base_root = os.path.join(base, "records")
    p = subprocess.run([args.binary, "emit-fixture", "--out", base_root, "--blocks", str(args.blocks)],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=TIMEOUT_SECONDS)
    if p.returncode != 0:
        record("emit_fixture_records", False, {"exit": p.returncode})
        return steps, False
    prof = verifier.fixture_profile(args.blocks)
    data = verifier.build_fixture_estimates(base_root, prof)
    with open(os.path.join(base_root, EST), "xb") as f:
        f.write(data)
    record("emit_fixture_records", True, {"namespace": "fixture-r1", "blocks": args.blocks})

    base_manifest = os.path.join(base, "manifest.sha256")
    write_manifest(base_root, base_manifest)
    for kind in ("cpp", "py"):
        r = run_tool(kind, args.binary, args.source_dir, verifier_path, base_root, base_manifest,
                     os.path.join(base, "baseline_%s_receipt.json" % kind), args.blocks)
        record("baseline_%s_pass" % kind, r["exit"] == 0 and r["verdict"] == "PASS", r)
    if not ok:
        return steps, False

    for i, (name, tools, corrupt) in enumerate(cases()):
        case_dir = os.path.join(tmp, "case_%02d" % i)
        os.mkdir(case_dir)
        root = os.path.join(case_dir, "records")
        shutil.copytree(base_root, root)
        manifest = os.path.join(case_dir, "manifest.sha256")
        try:
            if corrupt is not None:
                corrupt(root)
            write_manifest(root, manifest)
            if corrupt is None:
                corrupt_manifest(manifest)
            results = []
            case_ok = True
            for kind in tools:
                r = run_tool(kind, args.binary, args.source_dir, verifier_path, root, manifest,
                             os.path.join(case_dir, "%s_receipt.json" % kind), args.blocks)
                code = r["failure_code"] or ""
                stale_manifest = corrupt is not None and code.startswith("MANIFEST")
                case_ok = case_ok and r["exit"] == 1 and r["verdict"] == "INVALID" and not stale_manifest
                results.append(r)
            record("corruption_" + name, case_ok, results)
        except Exception as e:
            record("corruption_" + name, False, type(e).__name__)
        finally:
            shutil.rmtree(case_dir, ignore_errors=True)
    return steps, ok


def main(argv=None):
    ap = argparse.ArgumentParser(description="PHASE2-TORUS-MEMORY-003 auditor fixture gate (non-scientific)")
    ap.add_argument("--binary", required=True)
    ap.add_argument("--source-dir", required=True)
    ap.add_argument("--receipt", required=True)
    ap.add_argument("--blocks", type=int, default=2)
    ap.add_argument("--work-parent", default=None)
    args = ap.parse_args(argv)
    args.binary = os.path.abspath(args.binary)
    args.source_dir = os.path.abspath(args.source_dir)
    receipt = os.path.abspath(args.receipt)
    if args.blocks < 1 or args.blocks > 8:
        sys.stderr.write("refused: --blocks must be 1..8\n")
        return 2
    try:
        st = os.lstat(args.binary)
    except OSError:
        sys.stderr.write("refused: binary missing\n")
        return 2
    if stat.S_ISLNK(st.st_mode) or not stat.S_ISREG(st.st_mode) or not os.access(args.binary, os.X_OK):
        sys.stderr.write("refused: binary must be a regular executable file, not a symlink\n")
        return 2
    if os.path.lexists(receipt):
        sys.stderr.write("refused: receipt path already exists\n")
        return 2
    verifier, verifier_path = load_verifier(args.source_dir)
    started = utc_now()
    with tempfile.TemporaryDirectory(prefix="t3a_fixture_gate_", dir=args.work_parent) as tmp:
        steps, ok = gate(args, verifier, verifier_path, tmp)
    result = {
        "schema": GATE_SCHEMA,
        "verdict": "PASS" if ok else "FAIL",
        "namespace": "fixture-r1",
        "fixture_blocks": args.blocks,
        "scientific_block_sampled": False,
        "producer_code_invoked": False,
        "binary_sha256": sha256_file(args.binary),
        "gate_source_sha256": sha256_file(os.path.abspath(__file__)),
        "verifier_source_sha256": sha256_file(verifier_path),
        "python_version": platform.python_version(),
        "steps": steps,
        "started_utc": started,
        "finished_utc": utc_now(),
    }
    try:
        data = (json.dumps(result, indent=2, sort_keys=True) + "\n").encode("ascii")
        fd = os.open(receipt, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o444)
        with os.fdopen(fd, "wb") as f:
            f.write(data)
    except OSError:
        sys.stderr.write("gate receipt could not be created exclusively\n")
        return 3
    sys.stdout.write("fixture gate %s\n" % result["verdict"])
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
