#!/usr/bin/env python3
"""Independent fail-closed verifier for PHASE2-TORUS-MEMORY-003 saved records.

Python 3.8, standard library only.  Written from the frozen specification
(PHASE2-TORUS-MEMORY-003-REV1) and the saved-record interface
(BINARY_SCHEMAS.md, version 1).  It never simulates the model and never
imports or invokes producer, analyzer or prior-auditor code.

Usage (future, separately authorized):
  python3 torus_record_verifier.py verify --root DIR --manifest FILE \
      --receipt FILE [--profile production|fixture] [--fixture-blocks K]

Exit codes: 0 PASS, 1 INVALID (receipt written), 2 usage/refusal (no
receipt), 3 receipt could not be written.
"""

import argparse
import datetime
import hashlib
import json
import os
import platform
import re
import stat
import struct
import sys
from decimal import Decimal, localcontext
from fractions import Fraction

TOOL_NAME = "torus_record_verifier"
TOOL_VERSION = "1"
RECEIPT_SCHEMA = "PHASE2-TORUS-MEMORY-003-AUDITOR-PY-RECEIPT-v1"

HEADER_SIZE = 64
SCHEMA_VERSION = 1
ENDIAN_MARKER = 0x01020304
NO_CHUNK = 0xFFFFFFFF
POP = 32
CANDS = 128
UPDATES = 256
LATE_FIRST = 193
MAX_POP_LOSS = 1 << 40
MAX_IND_LOSS = 1 << 35
DISPLAY_UNITS = 10 ** 40
DELTA = Fraction(1, 32)
DELTA_P = Fraction(1, 32)
PRIMARY_ALPHA = Fraction(1, 80)
PERFORMANCE_ALPHA = Fraction(1, 40)
H_PRECISION = 120
H_EPSILON = Fraction(1, 10 ** 100)
FROZEN_H = {
    "C_abs": "0.007810229527949401",
    "C_rec": "0.015620459055898803",
    "D_HALF": "0.015620459055898803",
    "D_ZERO": "0.015620459055898803",
    "P_abs": "0.014514625638859732",
    "P_rec": "0.029029251277719464",
}
CELL_NAMES = ("AZF", "AZM", "AHF", "AHM", "SZF", "SZM", "SHF", "SHM")

RECORD_TYPES = {
    "update": (b"T3UPDATE", 24),
    "path": (b"T3PATH\x00\x00", 224),
    "block": (b"T3BLOCK\x00", 136),
    "estimate": (b"T3ESTIM\x00", 256),
    "candidate": (b"T3CANDID", 104),
    "entry": (b"T3ENTRY\x00", 16),
    "context": (b"T3CONTXT", 4240),
}

HEADER_STRUCT = struct.Struct("<8sIIIIQ16sIIII")
HEADER_FIELDS = (
    (0, 8, "magic"), (8, 4, "schema_version"), (12, 4, "endian_marker"), (16, 4, "row_size"),
    (20, 4, "header_size"), (24, 8, "row_count"), (32, 16, "namespace"), (48, 4, "chunk_index"),
    (52, 4, "first_block"), (56, 4, "block_count"), (60, 4, "reserved"),
)
UPDATE_STRUCT = struct.Struct("<QHHHHBBBBBBBB")
UPDATE_NAMES = (
    "pop_loss", "update", "query_count", "local_flagged", "local_changed", "m_count",
    "valid_cache_pre", "cache_probe_uses", "cache_probe_winner_slots", "f_to_m", "m_to_f",
    "dup_entry_tournaments", "distinct_winners",
)
PATH_STRUCT = struct.Struct("<IBBBBIIQQIIIIIIII32s32s32s32s32s")
PATH_FIELDS = (
    (0, 4, "block"), (4, 1, "cell"), (5, 1, "arm"), (6, 1, "law"), (7, 1, "start"),
    (8, 4, "late_m_sum"), (12, 4, "total_queries"), (16, 8, "late_pop_loss_sum"),
    (24, 8, "all_pop_loss_sum"), (32, 4, "all_m_sum"), (36, 4, "cache_probe_uses"),
    (40, 4, "cache_probe_winner_slots"), (44, 4, "f_to_m"), (48, 4, "m_to_f"),
    (52, 4, "dup_entry_tournaments"), (56, 4, "local_flagged"), (60, 4, "local_changed"),
    (64, 32, "final_state_sha256"), (96, 32, "update_records_sha256"),
    (128, 32, "label_blind_sha256"), (160, 32, "label_sha256"), (192, 32, "complement_label_sha256"),
)
BLOCK_STRUCT = struct.Struct("<II8I8Qiiiiqq")
BLOCK_FIELDS = (
    (0, 4, "block"), (4, 4, "flags"), (8, 32, "late_m_sum"), (40, 64, "late_pop_loss_sum"),
    (104, 4, "c_abs_numerator"), (108, 4, "c_rec_numerator"), (112, 4, "d_half_numerator"),
    (116, 4, "d_zero_numerator"), (120, 8, "p_abs_numerator"), (128, 8, "p_rec_numerator"),
)
CAND_STRUCT = struct.Struct("<IHBBBBBBBBBBQQQ64s")
ENTRY_STRUCT = struct.Struct("<IHBBBBBBBBBB")
CTX_STRUCT = struct.Struct("<IHBBB7s64s2048s32s32s2048s")
ZERO7 = bytes(7)
ZERO64 = bytes(64)

REL_RE = re.compile(r"[A-Za-z0-9._-]+(/[A-Za-z0-9._-]+)*")
MANIFEST_RE = re.compile(r"([0-9a-f]{64}) [ *]([^\r\n]+)")
DISPLAY_RE = re.compile(rb"(-?)([0-9]+)\.([0-9]{40})")


class Invalid(Exception):
    """A bounded, value-redacted failure: a code and a location only."""

    def __init__(self, code, where):
        Exception.__init__(self, code)
        self.code = code
        where = str(where)
        self.where = where if len(where) <= 200 else where[:200] + "..."


def first_diff_field(fields, expected, got):
    n = min(len(expected), len(got))
    i = 0
    while i < n and expected[i] == got[i]:
        i += 1
    for off, size, name in fields:
        if off <= i < off + size:
            return name
    return "unknown"


class Profile(object):
    def __init__(self, name, namespace, chunks, chunk_blocks):
        self.name = name
        self.namespace = namespace
        self.chunks = chunks
        self.chunk_blocks = chunk_blocks
        self.n_blocks = chunks * chunk_blocks
        self.audit_blocks = chunk_blocks  # chunk 0 is exactly the audit set


PRODUCTION = Profile("production", "production-r1", 650, 64)


def fixture_profile(k):
    if not isinstance(k, int) or k < 1 or k > 64:
        raise ValueError("fixture profile needs 1..64 blocks")
    return Profile("fixture", "fixture-r1", 1, k)


def required_files(prof):
    out = []
    for k in range(prof.chunks):
        out.append("updates/chunk_%05d.t3u" % k)
        out.append("paths/chunk_%05d.t3p" % k)
        out.append("blocks/chunk_%05d.t3b" % k)
    out.extend(["estimates.t3s", "audit/audit_candidates.t3c", "audit/audit_entries.t3e",
                "audit/audit_context.t3x"])
    return out


# ---------------------------------------------------------------------------
# Read-only, symlink-refusing input access.

def check_rel(rel, code="INPUT_PATH_FORM"):
    if not REL_RE.fullmatch(rel):
        raise Invalid(code, "path form")
    for part in rel.split("/"):
        if part in (".", ".."):
            raise Invalid(code, "path form")


def check_plain_dir(path, label):
    try:
        st = os.lstat(path)
    except FileNotFoundError:
        raise Invalid("INPUT_ROOT_MISSING", label)
    if stat.S_ISLNK(st.st_mode):
        raise Invalid("INPUT_ROOT_SYMLINK", label)
    if not stat.S_ISDIR(st.st_mode):
        raise Invalid("INPUT_ROOT_NOT_DIRECTORY", label)


def open_input(root, rel):
    check_rel(rel)
    check_plain_dir(root, "input root")
    parts = rel.split("/")
    cur = root
    st = None
    for i, part in enumerate(parts):
        cur = os.path.join(cur, part)
        try:
            st = os.lstat(cur)
        except FileNotFoundError:
            raise Invalid("INPUT_MISSING", rel)
        if stat.S_ISLNK(st.st_mode):
            raise Invalid("INPUT_SYMLINK", rel)
        if i < len(parts) - 1:
            if not stat.S_ISDIR(st.st_mode):
                raise Invalid("INPUT_NOT_DIRECTORY", rel)
        elif not stat.S_ISREG(st.st_mode):
            raise Invalid("INPUT_IRREGULAR", rel)
    flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_CLOEXEC", 0)
    fd = os.open(cur, flags)
    try:
        fst = os.fstat(fd)
        if not stat.S_ISREG(fst.st_mode) or (fst.st_dev, fst.st_ino) != (st.st_dev, st.st_ino):
            raise Invalid("INPUT_CHANGED_DURING_OPEN", rel)
    except BaseException:
        os.close(fd)
        raise
    return os.fdopen(fd, "rb", buffering=1 << 20), fst


def file_identity(root, rel):
    f, st = open_input(root, rel)
    with f:
        h = hashlib.sha256()
        while True:
            chunk = f.read(1 << 22)
            if not chunk:
                break
            h.update(chunk)
        end = os.fstat(f.fileno())
    if end.st_size != st.st_size or end.st_mtime_ns != st.st_mtime_ns:
        raise Invalid("INPUT_CHANGED_DURING_HASH", rel)
    return {"sha256": h.hexdigest(), "bytes": st.st_size, "mtime_ns": st.st_mtime_ns,
            "dev": st.st_dev, "ino": st.st_ino}


def read_plain_file(path, label):
    try:
        st = os.lstat(path)
    except FileNotFoundError:
        raise Invalid(label + "_MISSING", label.lower())
    if stat.S_ISLNK(st.st_mode) or not stat.S_ISREG(st.st_mode):
        raise Invalid(label + "_IRREGULAR", label.lower())
    fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_CLOEXEC", 0))
    with os.fdopen(fd, "rb") as f:
        return f.read()


def parse_manifest(path):
    data = read_plain_file(path, "MANIFEST")
    try:
        text = data.decode("ascii")
    except UnicodeDecodeError:
        raise Invalid("MANIFEST_ENCODING", "manifest")
    if not text.endswith("\n"):
        raise Invalid("MANIFEST_FORMAT", "final newline")
    entries = {}
    for n, line in enumerate(text[:-1].split("\n"), 1):
        m = MANIFEST_RE.fullmatch(line)
        if not m:
            raise Invalid("MANIFEST_FORMAT", "manifest line %d" % n)
        rel = m.group(2)
        if rel.startswith("./") and len(rel) > 2:
            rel = rel[2:]
        check_rel(rel, "MANIFEST_PATH_FORM")
        if rel in entries:
            raise Invalid("MANIFEST_DUPLICATE", "manifest line %d" % n)
        entries[rel] = m.group(1)
    if not entries:
        raise Invalid("MANIFEST_EMPTY", "manifest")
    return entries, hashlib.sha256(data).hexdigest()


def check_record_dirs(root, required):
    req = set(required)
    for d in ("updates", "paths", "blocks", "audit"):
        p = os.path.join(root, d)
        check_plain_dir(p, d)
        for name in sorted(os.listdir(p)):
            if d + "/" + name not in req:
                raise Invalid("UNEXPECTED_RECORD_FILE", d + "/" + name)


class RecordFile(object):
    """Sequential reader with exact header, size, count and EOF checks."""

    def __init__(self, root, rel, kind, rows, prof, chunk_index, first_block, block_count):
        self.rel = rel
        self.rows = rows
        self.read_rows = 0
        magic, self.row_size = RECORD_TYPES[kind]
        self.f, st = open_input(root, rel)
        try:
            if st.st_size != HEADER_SIZE + rows * self.row_size:
                raise Invalid("FILE_SIZE", rel)
            got = self._exact(HEADER_SIZE)
            exp = HEADER_STRUCT.pack(magic, SCHEMA_VERSION, ENDIAN_MARKER, self.row_size, HEADER_SIZE,
                                     rows, prof.namespace.encode("ascii"), chunk_index, first_block,
                                     block_count, 0)
            if got != exp:
                raise Invalid("HEADER_" + first_diff_field(HEADER_FIELDS, exp, got), rel)
        except BaseException:
            self.close()
            raise

    def _exact(self, n):
        b = self.f.read(n)
        if len(b) != n:
            raise Invalid("FILE_SHORT", self.rel)
        return b

    def take(self, n):
        if self.read_rows + n > self.rows:
            raise Invalid("ROW_COUNT_EXCEEDED", self.rel)
        b = self._exact(n * self.row_size)
        self.read_rows += n
        return b

    def finish(self):
        if self.read_rows != self.rows:
            raise Invalid("ROWS_UNCONSUMED", self.rel)
        if self.f.read(1):
            raise Invalid("TRAILING_BYTES", self.rel)
        self.close()

    def close(self):
        if self.f is not None:
            self.f.close()
            self.f = None


# ---------------------------------------------------------------------------
# Chunk files: per-update, path and block records for every block.

def verify_path(raw, path_raw, b, cell, urel, prel, row0, prow):
    arm, law, start = cell >> 2, (cell >> 1) & 1, cell & 1
    rows = list(UPDATE_STRUCT.iter_unpack(raw))
    prev_m = 0
    late_m = late_loss = all_loss = all_m = 0
    cpu_t = cpw_t = f2m_t = m2f_t = dup_t = lf_t = lc_t = 0
    blind, mseq, f2ms, m2fs = [], [], [], []
    for j, r in enumerate(rows):
        (pl, upd, qc, lf, lc, m, vcp, cpu, cpw, f2m, m2f, dup, dw) = r
        t = j + 1
        where = "%s row %d" % (urel, row0 + j)
        if upd != t:
            raise Invalid("UPDATE_ORDER", where)
        if qc != 128:
            raise Invalid("UPDATE_QUERY_COUNT", where)
        if pl > MAX_POP_LOSS:
            raise Invalid("UPDATE_POP_LOSS_DOMAIN", where)
        if lf > 1024:
            raise Invalid("UPDATE_LOCAL_FLAGGED_DOMAIN", where)
        if lc > lf:
            raise Invalid("UPDATE_LOCAL_CHANGED_DOMAIN", where)
        if m > 32:
            raise Invalid("UPDATE_M_COUNT_DOMAIN", where)
        if vcp != prev_m:
            raise Invalid("UPDATE_VALID_CACHE_PRE", where)
        if arm == 1:
            if cpu != 0 or cpw != 0:
                raise Invalid("UPDATE_SHAM_CACHE_USE", where)
        else:
            if cpu != vcp:
                raise Invalid("UPDATE_CACHE_PROBE_USES", where)
            if cpw > 32 or (cpu == 0 and cpw != 0):
                raise Invalid("UPDATE_CACHE_PROBE_WINNERS", where)
        inherited_m = m - f2m + m2f
        if f2m + m2f > 32 or inherited_m < m2f or 32 - inherited_m < f2m:
            raise Invalid("UPDATE_MUTATION_DOMAIN", where)
        if t == 1 and inherited_m != (32 if start == 1 else 0):
            raise Invalid("UPDATE_INITIAL_LABELS", where)
        if dup > 32:
            raise Invalid("UPDATE_DUP_DOMAIN", where)
        if dw < 1 or dw > 32:
            raise Invalid("UPDATE_DISTINCT_DOMAIN", where)
        all_loss += pl
        all_m += m
        if t >= LATE_FIRST:
            late_m += m
            late_loss += pl
        cpu_t += cpu
        cpw_t += cpw
        f2m_t += f2m
        m2f_t += m2f
        dup_t += dup
        lf_t += lf
        lc_t += lc
        blind.append((pl, lf, lc, dup, dw, cpu, cpw))
        mseq.append(m)
        f2ms.append(f2m)
        m2fs.append(m2f)
        prev_m = m
    stored = PATH_STRUCT.unpack(path_raw)
    final_sha, blind_sha, label_sha, comp_sha = stored[17], stored[19], stored[20], stored[21]
    exp = PATH_STRUCT.pack(b, cell, arm, law, start, late_m, UPDATES * 128, late_loss, all_loss, all_m,
                           cpu_t, cpw_t, f2m_t, m2f_t, dup_t, lf_t, lc_t, final_sha,
                           hashlib.sha256(raw).digest(), blind_sha, label_sha, comp_sha)
    pwhere = "%s row %d" % (prel, prow)
    if exp != path_raw:
        raise Invalid("PATH_" + first_diff_field(PATH_FIELDS, exp, path_raw), pwhere)
    if label_sha == comp_sha:
        raise Invalid("PATH_LABEL_DIGEST_PAIR", pwhere)
    return {"rows": rows, "late_m": late_m, "late_loss": late_loss, "blind": blind, "m": mseq,
            "f2m": f2ms, "m2f": m2fs, "blind_sha": blind_sha, "label_sha": label_sha,
            "comp_sha": comp_sha}


def block_numerators(s, L):
    p_abs = (L[6] + L[7]) - (L[2] + L[3])
    return (s[2] + s[3] - 2048, (s[2] + s[3]) - (s[0] + s[1]), 2 * (s[3] - s[2]), 2 * (s[1] - s[0]),
            p_abs, p_abs - ((L[4] + L[5]) - (L[0] + L[1])))


def verify_block(raw, b, cells, where):
    def n1(x, y):
        return cells[x]["blind_sha"] == cells[y]["blind_sha"] and cells[x]["blind"] == cells[y]["blind"]

    def n2(x, y):
        return (cells[x]["label_sha"] == cells[y]["comp_sha"] and cells[y]["label_sha"] == cells[x]["comp_sha"]
                and cells[x]["f2m"] == cells[y]["m2f"] and cells[x]["m2f"] == cells[y]["f2m"])

    sums = (all(a + c == 32 for a, c in zip(cells[4]["m"], cells[5]["m"]))
            and all(a + c == 32 for a, c in zip(cells[6]["m"], cells[7]["m"]))
            and cells[4]["late_m"] + cells[5]["late_m"] == 2048
            and cells[6]["late_m"] + cells[7]["late_m"] == 2048)
    flags = ((1 if n1(4, 5) else 0) | (2 if n1(6, 7) else 0) | (4 if n2(4, 5) else 0)
             | (8 if n2(6, 7) else 0) | (16 if sums else 0) | 32)
    if flags != 0x3F:
        raise Invalid("BLOCK_IDENTITY_N1_N2", "block %d bits %02x" % (b, flags))
    s = [cells[c]["late_m"] for c in range(8)]
    L = [cells[c]["late_loss"] for c in range(8)]
    exp = BLOCK_STRUCT.pack(b, flags, *s, *L, *block_numerators(s, L))
    if exp != raw:
        raise Invalid("BLOCK_" + first_diff_field(BLOCK_FIELDS, exp, raw), where)
    return s, L


def verify_chunks(root, prof, counts):
    blocks = []
    audit_updates = {}
    for k in range(prof.chunks):
        first = k * prof.chunk_blocks
        nb = prof.chunk_blocks
        files = []
        try:
            uf = RecordFile(root, "updates/chunk_%05d.t3u" % k, "update", nb * 8 * UPDATES, prof, k, first, nb)
            files.append(uf)
            pf = RecordFile(root, "paths/chunk_%05d.t3p" % k, "path", nb * 8, prof, k, first, nb)
            files.append(pf)
            bf = RecordFile(root, "blocks/chunk_%05d.t3b" % k, "block", nb, prof, k, first, nb)
            files.append(bf)
            for bi in range(nb):
                b = first + bi
                cells = []
                for cell in range(8):
                    raw = uf.take(UPDATES)
                    praw = pf.take(1)
                    info = verify_path(raw, praw, b, cell, uf.rel, pf.rel, uf.read_rows - UPDATES,
                                       pf.read_rows - 1)
                    cells.append(info)
                    if b < prof.audit_blocks:
                        audit_updates[(b, cell)] = info["rows"]
                    counts["update_rows"] += UPDATES
                    counts["path_rows"] += 1
                s, L = verify_block(bf.take(1), b, cells, "%s row %d" % (bf.rel, bi))
                blocks.append((s, L))
                counts["block_rows"] += 1
                counts["n1_n2_blocks"] += 1
            uf.finish()
            pf.finish()
            bf.finish()
        finally:
            for f in files:
                f.close()
    return blocks, audit_updates


# ---------------------------------------------------------------------------
# Estimates: exact integer reconstruction, Hoeffding bounds, classification.

def estimate_specs():
    specs = [
        (0, 0, "C_abs", "C", 4096, Fraction(1)),
        (1, 0, "C_rec", "C", 4096, Fraction(2)),
        (2, 0, "D_HALF", "D", 4096, Fraction(2)),
        (3, 0, "D_ZERO", "D", 4096, Fraction(2)),
        (4, 1, "P_abs", "P", 1 << 47, Fraction(2)),
        (5, 1, "P_rec", "P", 1 << 47, Fraction(4)),
    ]
    for c in range(8):
        specs.append((6 + c, 2, "M_FREQ_" + CELL_NAMES[c], "M", 2048, Fraction(1)))
    for c in range(8):
        specs.append((14 + c, 2, "ACCURACY_" + CELL_NAMES[c], "A", 1 << 46, Fraction(1)))
    return specs


def estimate_sums(blocks):
    sums = [0] * 22
    for s, L in blocks:
        nums = block_numerators(s, L)
        for i in range(6):
            sums[i] += nums[i]
        for c in range(8):
            sums[6 + c] += s[c]
            sums[14 + c] += (1 << 46) - L[c]
    return sums


def hoeffding_bracket(rng, n, alpha):
    with localcontext() as ctx:
        ctx.prec = H_PRECISION
        two_over_alpha = Decimal(2 * alpha.denominator) / Decimal(alpha.numerator)
        core = (two_over_alpha.ln() / Decimal(2 * n)).sqrt()
        h = core * Decimal(rng.numerator) / Decimal(rng.denominator)
    hf = Fraction(h)
    return hf - H_EPSILON, hf + H_EPSILON


def h_less(hlo, hhi, g, where):
    if hhi < g:
        return True
    if hlo >= g:
        return False
    raise Invalid("ESTIMATE_UNDECIDABLE", where)


def h_le(hlo, hhi, g, where):
    if hhi <= g:
        return True
    if hlo > g:
        return False
    raise Invalid("ESTIMATE_UNDECIDABLE", where)


def floor_units(fr):
    q = fr * DISPLAY_UNITS
    return q.numerator // q.denominator


def ceil_units(fr):
    q = fr * DISPLAY_UNITS
    return -((-q.numerator) // q.denominator)


def half_even_units(fr):
    q = fr * DISPLAY_UNITS
    whole, rem = divmod(q.numerator, q.denominator)
    if 2 * rem > q.denominator or (2 * rem == q.denominator and whole % 2 == 1):
        whole += 1
    return whole


def agree(a, b, where):
    if a != b:
        raise Invalid("ESTIMATE_UNDECIDABLE", where)
    return a


def expected_estimate(spec, total, n):
    idx, family, name, kind, den, rng = spec
    where = "estimate %d" % idx
    est = Fraction(total, n * den)
    out = {"index": idx, "family": family, "name": name, "kind": kind, "total": total, "den": den,
           "range": rng, "n": n, "est": half_even_units(est)}
    if kind in ("M", "A"):
        out.update(code=40, hw=None, lo=None, hi=None)
        return out
    alpha = PRIMARY_ALPHA if kind in ("C", "D") else PERFORMANCE_ALPHA
    hlo, hhi = hoeffding_bracket(rng, n, alpha)
    if kind == "C":
        if h_less(hlo, hhi, est - DELTA, where):
            code = 10
        elif h_less(hlo, hhi, -DELTA - est, where):
            code = 12
        elif h_le(hlo, hhi, DELTA - est, where):
            code = 11
        else:
            code = 13
    elif kind == "D":
        inside = h_less(hlo, hhi, est + DELTA, where) and h_less(hlo, hhi, DELTA - est, where)
        code = 20 if inside else 21
    else:
        if h_less(hlo, hhi, est - DELTA_P, where):
            code = 30
        elif h_less(hlo, hhi, -DELTA_P - est, where):
            code = 31
        elif h_le(hlo, hhi, DELTA_P - est, where):
            code = 32
        else:
            code = 33
    out.update(code=code,
               hw=agree(ceil_units(hlo), ceil_units(hhi), where),
               lo=agree(floor_units(est - hhi), floor_units(est - hlo), where),
               hi=agree(ceil_units(est + hlo), ceil_units(est + hhi), where),
               h_bracket=(hlo, hhi))
    return out


def check_frozen_bounds(n, where):
    for spec in estimate_specs()[:6]:
        alpha = PRIMARY_ALPHA if spec[3] in ("C", "D") else PERFORMANCE_ALPHA
        hlo, hhi = hoeffding_bracket(spec[5], n, alpha)
        frozen = Fraction(Decimal(FROZEN_H[spec[2]]))
        if abs(hlo - frozen) > Fraction(1, 10 ** 16):
            raise Invalid("FROZEN_BOUND_MISMATCH", "%s %s" % (where, spec[2]))


def format_units(v):
    sign = "-" if v < 0 else ""
    a = abs(v)
    return "%s%d.%s" % (sign, a // DISPLAY_UNITS, str(a % DISPLAY_UNITS).zfill(40))


def parse_display(field, where):
    s = field.rstrip(b"\x00")
    if not s or b"\x00" in s or any(c < 0x20 or c > 0x7e for c in s):
        raise Invalid("ESTIMATE_DISPLAY_FORM", where)
    if s == b"NA":
        return None
    m = DISPLAY_RE.fullmatch(s)
    if not m:
        raise Invalid("ESTIMATE_DISPLAY_FORM", where)
    v = int(m.group(2) + m.group(3))
    return -v if m.group(1) else v


def encode_estimate_row(e):
    row = bytearray(256)
    struct.pack_into("<HBBI", row, 0, e["index"], e["family"], e["code"], e["n"])
    row[8:24] = e["total"].to_bytes(16, "little", signed=True)
    row[24:40] = e["den"].to_bytes(16, "little")
    struct.pack_into("<II", row, 40, e["range"].numerator, e["range"].denominator)
    name = e["name"].encode("ascii")
    row[48:48 + len(name)] = name
    for off, key in ((80, "est"), (124, "hw"), (168, "lo"), (212, "hi")):
        text = b"NA" if e[key] is None else format_units(e[key]).encode("ascii")
        row[off:off + len(text)] = text
    return bytes(row)


def verify_estimate_row(raw, exp, where):
    idx, family, code, n = struct.unpack_from("<HBBI", raw, 0)
    total = int.from_bytes(raw[8:24], "little", signed=True)
    den = int.from_bytes(raw[24:40], "little")
    rnum, rden = struct.unpack_from("<II", raw, 40)
    if idx != exp["index"]:
        raise Invalid("ESTIMATE_INDEX", where)
    if family != exp["family"]:
        raise Invalid("ESTIMATE_FAMILY", where)
    if n != exp["n"]:
        raise Invalid("ESTIMATE_N_BLOCKS", where)
    if den == 0 or Fraction(total, den) != Fraction(exp["total"], exp["den"]):
        raise Invalid("ESTIMATE_NUMERATOR", where)
    if rden == 0 or Fraction(rnum, rden) != exp["range"]:
        raise Invalid("ESTIMATE_RANGE", where)
    name = raw[48:80].rstrip(b"\x00")
    if not name or b"\x00" in name or any(c < 0x21 or c > 0x7e for c in name):
        raise Invalid("ESTIMATE_NAME", where)
    if exp["kind"] not in ("M", "A") and name != exp["name"].encode("ascii"):
        raise Invalid("ESTIMATE_NAME", where)
    if code != exp["code"]:
        raise Invalid("ESTIMATE_CLASSIFICATION", where)
    for off, key in ((80, "est"), (124, "hw"), (168, "lo"), (212, "hi")):
        if parse_display(raw[off:off + 44], where) != exp[key]:
            raise Invalid("ESTIMATE_DISPLAY_" + key.upper(), where)


def verify_estimates(root, prof, blocks, counts):
    n = prof.n_blocks
    if len(blocks) != n:
        raise Invalid("COUNT_BLOCKS", "estimates")
    if prof.name == "production":
        check_frozen_bounds(n, "frozen half-width")
    sums = estimate_sums(blocks)
    ef = RecordFile(root, "estimates.t3s", "estimate", 22, prof, NO_CHUNK, 0, n)
    try:
        for spec in estimate_specs():
            exp = expected_estimate(spec, sums[spec[0]], n)
            verify_estimate_row(ef.take(1), exp, "estimates.t3s row %d" % spec[0])
            counts["estimate_rows"] += 1
        ef.finish()
    finally:
        ef.close()


# ---------------------------------------------------------------------------
# Audit files: structural and cross-file transition checks (no simulation).

class AuditChecker(object):
    def __init__(self, prof, audit_updates, counts):
        self.prof = prof
        self.audit_updates = audit_updates
        self.counts = counts

    def run(self, root):
        A = self.prof.audit_blocks
        groups = A * UPDATES * 8
        files = []
        try:
            cf = RecordFile(root, "audit/audit_candidates.t3c", "candidate", groups * CANDS, self.prof,
                            NO_CHUNK, 0, A)
            files.append(cf)
            ef = RecordFile(root, "audit/audit_entries.t3e", "entry", groups * CANDS, self.prof, NO_CHUNK, 0, A)
            files.append(ef)
            xf = RecordFile(root, "audit/audit_context.t3x", "context", groups, self.prof, NO_CHUNK, 0, A)
            files.append(xf)
            g = 0
            for b in range(A):
                self.prev = [None] * 8
                self.half_targets = {}
                self.initial = None
                for t in range(1, UPDATES + 1):
                    self.shared = {"rbit": None, "zero_target": None, "tie": None, "scout": [None] * POP,
                                   "fresh": [None] * POP, "dkey": {}, "entries": [None] * POP,
                                   "flip": [None] * POP}
                    for cell in range(8):
                        self.group(b, t, cell, g, xf.take(1), cf.take(CANDS), ef.take(CANDS))
                        g += 1
            cf.finish()
            ef.finish()
            xf.finish()
        finally:
            for f in files:
                f.close()

    def group(self, b, t, cell, g, ctx_raw, cand_raw, ent_raw):
        arm, law, start = cell >> 2, (cell >> 1) & 1, cell & 1
        sh = self.shared
        xw = "audit_context row %d" % g
        (cb, ct, cc, rbit, copied, zeros, target, pre_blob, labels, cvalid,
         cache_blob) = CTX_STRUCT.unpack(ctx_raw)
        if (cb, ct, cc) != (b, t, cell):
            raise Invalid("CONTEXT_ORDER", xw)
        if zeros != ZERO7:
            raise Invalid("CONTEXT_RESERVED", xw)
        if rbit > 1:
            raise Invalid("CONTEXT_COPY_BIT_DOMAIN", xw)
        if sh["rbit"] is None:
            sh["rbit"] = rbit
        elif sh["rbit"] != rbit:
            raise Invalid("CONTEXT_COPY_BIT_SHARED", xw)
        if copied != (1 if (law == 1 and t >= 3 and rbit == 1) else 0):
            raise Invalid("CONTEXT_LAW_COPIED", xw)
        if law == 0:
            if sh["zero_target"] is None:
                sh["zero_target"] = target
            elif target != sh["zero_target"]:
                raise Invalid("CONTEXT_TARGET_SHARED", xw)
        else:
            expected = self.half_targets[t - 2] if copied else sh["zero_target"]
            if target != expected:
                raise Invalid("CONTEXT_TARGET_LAW", xw)
            prior = self.half_targets.get(t)
            if prior is None:
                self.half_targets[t] = target
            elif prior != target:
                raise Invalid("CONTEXT_TARGET_SHARED", xw)
        if any(x > 1 for x in labels):
            raise Invalid("CONTEXT_LABEL_DOMAIN", xw)
        if any(x > 1 for x in cvalid):
            raise Invalid("CONTEXT_CACHE_FLAG_DOMAIN", xw)
        pre = [pre_blob[64 * i:64 * i + 64] for i in range(POP)]
        cache = [cache_blob[64 * i:64 * i + 64] for i in range(POP)]
        for s in range(POP):
            if not cvalid[s] and cache[s] != ZERO64:
                raise Invalid("CONTEXT_INVALID_CACHE_NONZERO", xw)
        if t == 1:
            if any(x != start for x in labels):
                raise Invalid("CONTEXT_INITIAL_LABELS", xw)
            if any(cvalid):
                raise Invalid("CONTEXT_INITIAL_CACHE", xw)
            if self.initial is None:
                self.initial = pre_blob
            elif pre_blob != self.initial:
                raise Invalid("CONTEXT_INITIAL_PHENOTYPES", xw)
        else:
            if cvalid != labels:
                raise Invalid("CONTEXT_CACHE_RULE", xw)
            p_ph, p_win, p_post, p_pre = self.prev[cell]
            for s in range(POP):
                w = p_win[s]
                if pre[s] != p_ph[w]:
                    raise Invalid("CONTEXT_PRESTATE_PHENOTYPE", xw)
                if labels[s] != p_post[s]:
                    raise Invalid("CONTEXT_PRESTATE_LABEL", xw)
                if cvalid[s] and cache[s] != p_pre[w & 31]:
                    raise Invalid("CONTEXT_PRESTATE_CACHE", xw)

        # Candidate rows.
        base = g * CANDS
        loss = [0] * CANDS
        tie = [0] * CANDS
        ph = [None] * CANDS
        fl = [0] * CANDS
        won = [0] * CANDS
        dkey = [0] * CANDS
        lfl = [0] * POP
        lch = [0] * POP
        dfam = [0] * POP
        donor_flag = [None] * POP
        if sh["tie"] is None:
            sh["tie"] = [None] * CANDS
        for j, r in enumerate(CAND_STRUCT.iter_unpack(cand_raw)):
            (rb, rt, rc, ri, fam, par, lab, flags, df, nfl, nch, wj, lj, dk, tk, phen) = r
            w = "audit_candidates row %d" % (base + j)
            if (rb, rt, rc, ri) != (b, t, cell, j):
                raise Invalid("CANDIDATE_ORDER", w)
            if fam != j >> 5:
                raise Invalid("CANDIDATE_FAMILY", w)
            if par != j & 31:
                raise Invalid("CANDIDATE_PARENT", w)
            if lab != labels[par]:
                raise Invalid("CANDIDATE_LABEL", w)
            if flags & ~7:
                raise Invalid("CANDIDATE_FLAGS_DOMAIN", w)
            if (flags & 1) != cvalid[par]:
                raise Invalid("CANDIDATE_CACHE_VALID_FLAG", w)
            reads = fam == 1 and arm == 0 and lab == 1 and cvalid[par] == 1
            if bool(flags & 2) != reads:
                raise Invalid("CANDIDATE_PROBE_READ_FLAG", w)
            if fam < 3:
                if df != 255 or nfl != 0 or nch != 0:
                    raise Invalid("CANDIDATE_LOCAL_FIELDS", w)
                if flags & 4:
                    if donor_flag[par] is not None:
                        raise Invalid("CANDIDATE_DONOR_FLAG", w)
                    donor_flag[par] = fam
                key = (par, fam)
                prior = sh["dkey"].get(key)
                if prior is None:
                    sh["dkey"][key] = dk
                elif prior != dk:
                    raise Invalid("CANDIDATE_DONOR_KEY_SHARED", w)
            else:
                if flags & 4:
                    raise Invalid("CANDIDATE_DONOR_FLAG", w)
                if df > 2 or nfl > 32 or nch > nfl:
                    raise Invalid("CANDIDATE_LOCAL_FIELDS", w)
                if dk != 0:
                    raise Invalid("CANDIDATE_DONOR_KEY", w)
                lfl[par], lch[par], dfam[par] = nfl, nch, df
            if lj > MAX_IND_LOSS:
                raise Invalid("CANDIDATE_LOSS_DOMAIN", w)
            if fam == 0 and phen != pre[par]:
                raise Invalid("CANDIDATE_PHENOTYPE", w)
            if reads and phen != cache[par]:
                raise Invalid("CANDIDATE_PHENOTYPE", w)
            if fam == 1 and not reads:
                if sh["fresh"][par] is None:
                    sh["fresh"][par] = phen
                elif sh["fresh"][par] != phen:
                    raise Invalid("CANDIDATE_FRESH_SHARED", w)
            if fam == 2:
                if sh["scout"][par] is None:
                    sh["scout"][par] = phen
                elif sh["scout"][par] != phen:
                    raise Invalid("CANDIDATE_SCOUT_SHARED", w)
            if sh["tie"][j] is None:
                sh["tie"][j] = tk
            elif sh["tie"][j] != tk:
                raise Invalid("CANDIDATE_TIE_KEY_SHARED", w)
            loss[j], tie[j], ph[j], fl[j], won[j], dkey[j] = lj, tk, phen, flags, wj, dk
        for par in range(POP):
            w = "audit_candidates row %d" % (base + 96 + par)
            f_star = donor_flag[par]
            if f_star is None:
                raise Invalid("CANDIDATE_DONOR_FLAG", w)
            best = min(range(3), key=lambda f: (loss[32 * f + par], dkey[32 * f + par], f))
            if best != f_star:
                raise Invalid("CANDIDATE_DONOR_CHOICE", w)
            if dfam[par] != f_star:
                raise Invalid("CANDIDATE_DONOR_FAMILY", w)
            child, donor = ph[96 + par], ph[32 * f_star + par]
            differ = sum(1 for k in range(0, 64, 2) if child[k:k + 2] != donor[k:k + 2])
            if differ != lch[par]:
                raise Invalid("CANDIDATE_LOCAL_CHANGED", w)

        # Tournament-entry rows.
        ebase = g * CANDS
        winners = [0] * POP
        inh = [0] * POP
        post = [0] * POP
        dup_count = 0
        erows = list(ENTRY_STRUCT.iter_unpack(ent_raw))
        for s in range(POP):
            ents = erows[4 * s:4 * s + 4]
            w = "audit_entries row %d" % (ebase + 4 * s)
            for e, r in enumerate(ents):
                (eb, et, ec, es, ee, ei, ew, il, pl, ff, cv, ez) = r
                ww = "audit_entries row %d" % (ebase + 4 * s + e)
                if (eb, et, ec, es, ee) != (b, t, cell, s, e):
                    raise Invalid("ENTRY_ORDER", ww)
                if ei > 127 or ew > 127:
                    raise Invalid("ENTRY_INDEX_DOMAIN", ww)
                if ez != 0:
                    raise Invalid("ENTRY_RESERVED", ww)
                if il > 1 or pl > 1:
                    raise Invalid("ENTRY_LABEL_DOMAIN", ww)
                if ff != (il ^ pl):
                    raise Invalid("ENTRY_FLIP", ww)
                if cv != pl:
                    raise Invalid("ENTRY_CACHE_VALID", ww)
            if any((r[6], r[7], r[8], r[9]) != (ents[0][6], ents[0][7], ents[0][8], ents[0][9]) for r in ents):
                raise Invalid("ENTRY_WINNER_CONSISTENCY", w)
            entries = tuple(r[5] for r in ents)
            win = ents[0][6]
            if win != min(entries, key=lambda c: (loss[c], tie[c], c)):
                raise Invalid("ENTRY_WINNER", w)
            if ents[0][7] != labels[win & 31]:
                raise Invalid("ENTRY_INHERITED_LABEL", w)
            if sh["entries"][s] is None:
                sh["entries"][s] = entries
                sh["flip"][s] = ents[0][9]
            elif sh["entries"][s] != entries:
                raise Invalid("ENTRY_SHARED", w)
            elif sh["flip"][s] != ents[0][9]:
                raise Invalid("ENTRY_FLIP_SHARED", w)
            if len(set(entries)) < 4:
                dup_count += 1
            winners[s], inh[s], post[s] = win, ents[0][7], ents[0][8]
        for c in range(CANDS):
            if won[c] != winners.count(c):
                raise Invalid("CANDIDATE_SLOTS_WON", "audit_candidates row %d" % (base + c))

        # Cross-check against the saved per-update row of chunk 0.
        stored = self.audit_updates[(b, cell)][t - 1]
        exp = (sum(loss[x] for x in winners), t, 128, sum(lfl), sum(lch), sum(post), sum(cvalid),
               sum(1 for c in range(CANDS) if fl[c] & 2), sum(1 for x in winners if fl[x] & 2),
               sum(1 for s in range(POP) if inh[s] == 0 and post[s] == 1),
               sum(1 for s in range(POP) if inh[s] == 1 and post[s] == 0), dup_count, len(set(winners)))
        if exp != tuple(stored):
            k = next(i for i in range(len(exp)) if exp[i] != stored[i])
            raise Invalid("AUDIT_UPDATE_" + UPDATE_NAMES[k], "block %d cell %d update %d" % (b, cell, t))
        self.prev[cell] = (ph, winners, post, pre)
        self.counts["context_rows"] += 1
        self.counts["candidate_rows"] += CANDS
        self.counts["entry_rows"] += CANDS


# ---------------------------------------------------------------------------
# Fixture helpers (used only by tests/fixture_gate.py on fixture-r1 inputs).

def build_fixture_estimates(root, prof):
    if prof.namespace == PRODUCTION.namespace:
        raise ValueError("fixture estimates are refused for the production namespace")
    blocks = []
    for k in range(prof.chunks):
        bf = RecordFile(root, "blocks/chunk_%05d.t3b" % k, "block", prof.chunk_blocks, prof, k,
                        k * prof.chunk_blocks, prof.chunk_blocks)
        try:
            for _ in range(prof.chunk_blocks):
                v = BLOCK_STRUCT.unpack(bf.take(1))
                blocks.append((list(v[2:10]), list(v[10:18])))
            bf.finish()
        finally:
            bf.close()
    sums = estimate_sums(blocks)
    header = HEADER_STRUCT.pack(RECORD_TYPES["estimate"][0], SCHEMA_VERSION, ENDIAN_MARKER, 256, HEADER_SIZE, 22,
                                prof.namespace.encode("ascii"), NO_CHUNK, 0, prof.n_blocks, 0)
    rows = [encode_estimate_row(expected_estimate(spec, sums[spec[0]], prof.n_blocks))
            for spec in estimate_specs()]
    return header + b"".join(rows)


def self_check_math():
    """Compact hand checks of the inferential arithmetic; raises Invalid."""
    passed = []
    check_frozen_bounds(PRODUCTION.n_blocks, "selfcheck")
    passed.append("frozen_hoeffding_half_widths")
    if half_even_units(Fraction(5, 10 ** 41)) != 0 or half_even_units(Fraction(15, 10 ** 41)) != 2:
        raise Invalid("SELFCHECK_ROUNDING", "half-even")
    if half_even_units(Fraction(-15, 10 ** 41)) != -2 or floor_units(Fraction(-1, 10 ** 41)) != -1:
        raise Invalid("SELFCHECK_ROUNDING", "negative")
    if ceil_units(Fraction(1, 10 ** 41)) != 1 or ceil_units(Fraction(-1, 10 ** 41)) != 0:
        raise Invalid("SELFCHECK_ROUNDING", "ceiling")
    passed.append("display_rounding")
    specs = estimate_specs()
    n = PRODUCTION.n_blocks
    cases = (
        (specs[0], Fraction(1, 10), 10), (specs[0], Fraction(0), 11), (specs[0], Fraction(-1, 10), 12),
        (specs[0], Fraction(1, 32), 13), (specs[2], Fraction(0), 20), (specs[2], Fraction(1, 32), 21),
        (specs[4], Fraction(1, 10), 30), (specs[4], Fraction(-1, 10), 31), (specs[4], Fraction(0), 32),
        (specs[4], Fraction(1, 32), 33),
    )
    for spec, est, code in cases:
        total = est * n * spec[4]
        if total.denominator != 1 or expected_estimate(spec, total.numerator, n)["code"] != code:
            raise Invalid("SELFCHECK_CLASSIFICATION", "%s code %d" % (spec[2], code))
    passed.append("classification_boundaries")
    sample = expected_estimate(specs[1], 4096 * 41600 // 20, n)
    row = encode_estimate_row(sample)
    verify_estimate_row(row, sample, "selfcheck row")
    if parse_display(format_units(-5).encode("ascii").ljust(44, b"\x00"), "selfcheck") != -5:
        raise Invalid("SELFCHECK_DISPLAY", "parse")
    passed.append("estimate_row_round_trip")
    return passed


# ---------------------------------------------------------------------------
# Driver.

def utc_now():
    return datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def own_source_sha256():
    try:
        with open(os.path.abspath(__file__), "rb") as f:
            return hashlib.sha256(f.read()).hexdigest()
    except OSError:
        return "unavailable"


def run_verification(root, manifest, prof, result):
    counts = result["checked"]
    check_plain_dir(root, "input root")
    entries, manifest_sha = parse_manifest(manifest)
    result["manifest"]["sha256"] = manifest_sha
    required = required_files(prof)
    for rel in required:
        if rel not in entries:
            raise Invalid("MANIFEST_MISSING_ENTRY", rel)
    check_record_dirs(root, required)
    before = {}
    for rel in sorted(entries):
        ident = file_identity(root, rel)
        if ident["sha256"] != entries[rel]:
            raise Invalid("MANIFEST_HASH_MISMATCH", rel)
        before[rel] = ident
    result["inputs"] = [{"path": rel, "sha256": before[rel]["sha256"], "bytes": before[rel]["bytes"]}
                        for rel in sorted(before)]
    blocks, audit_updates = verify_chunks(root, prof, counts)
    verify_estimates(root, prof, blocks, counts)
    AuditChecker(prof, audit_updates, counts).run(root)
    expected = {
        "update_rows": prof.n_blocks * 8 * UPDATES, "path_rows": prof.n_blocks * 8,
        "block_rows": prof.n_blocks, "n1_n2_blocks": prof.n_blocks, "estimate_rows": 22,
        "candidate_rows": prof.audit_blocks * UPDATES * 8 * CANDS,
        "entry_rows": prof.audit_blocks * UPDATES * 8 * CANDS,
        "context_rows": prof.audit_blocks * UPDATES * 8,
    }
    for key, value in expected.items():
        if counts[key] != value:
            raise Invalid("COUNT_TOTAL", key)
    for rel in sorted(before):
        after = file_identity(root, rel)
        if any(after[k] != before[rel][k] for k in ("sha256", "bytes", "mtime_ns", "dev", "ino")):
            raise Invalid("INPUT_CHANGED", rel)
    if parse_manifest(manifest)[1] != manifest_sha:
        raise Invalid("INPUT_CHANGED", "manifest")


def write_receipt_exclusive(path, obj):
    data = (json.dumps(obj, indent=2, sort_keys=True) + "\n").encode("ascii")
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_CLOEXEC", 0), 0o444)
    with os.fdopen(fd, "wb") as f:
        f.write(data)
        f.flush()
        os.fsync(f.fileno())


def path_inside(child, parent):
    child = os.path.realpath(child)
    parent = os.path.realpath(parent)
    try:
        return os.path.commonpath([child, parent]) == parent
    except ValueError:
        return False


def main(argv=None):
    ap = argparse.ArgumentParser(description="PHASE2-TORUS-MEMORY-003 independent record verifier")
    sub = ap.add_subparsers(dest="cmd")
    v = sub.add_parser("verify")
    v.add_argument("--root", required=True)
    v.add_argument("--manifest", required=True)
    v.add_argument("--receipt", required=True)
    v.add_argument("--profile", choices=("production", "fixture"), default="production")
    v.add_argument("--fixture-blocks", type=int, default=None)
    args = ap.parse_args(argv)
    if args.cmd != "verify":
        ap.print_usage(sys.stderr)
        return 2
    if args.profile == "production":
        if args.fixture_blocks is not None:
            sys.stderr.write("refused: --fixture-blocks is only valid with --profile fixture\n")
            return 2
        prof = PRODUCTION
    else:
        try:
            prof = fixture_profile(args.fixture_blocks)
        except ValueError:
            sys.stderr.write("refused: --fixture-blocks must be 1..64\n")
            return 2
    root = os.path.abspath(args.root)
    manifest = os.path.abspath(args.manifest)
    receipt = os.path.abspath(args.receipt)
    if os.path.lexists(receipt):
        sys.stderr.write("refused: receipt path already exists\n")
        return 2
    if path_inside(receipt, root):
        sys.stderr.write("refused: receipt must lie outside the input root\n")
        return 2
    result = {
        "schema": RECEIPT_SCHEMA, "tool": TOOL_NAME, "tool_version": TOOL_VERSION,
        "tool_source_sha256": own_source_sha256(), "python_version": platform.python_version(),
        "profile": prof.name, "namespace": prof.namespace, "blocks": prof.n_blocks, "chunks": prof.chunks,
        "root": root, "manifest": {"path": manifest, "sha256": None}, "verdict": "INVALID",
        "failure": None, "inputs": [], "inputs_modified": False, "producer_code_invoked": False,
        "checked": {"update_rows": 0, "path_rows": 0, "block_rows": 0, "n1_n2_blocks": 0, "estimate_rows": 0,
                    "candidate_rows": 0, "entry_rows": 0, "context_rows": 0},
        "started_utc": utc_now(),
    }
    try:
        run_verification(root, manifest, prof, result)
        result["verdict"] = "PASS"
    except Invalid as e:
        result["failure"] = {"code": e.code, "where": e.where}
    except MemoryError:
        result["failure"] = {"code": "OUT_OF_MEMORY", "where": "allocation"}
    except Exception as e:  # fail closed on any parse or I/O error, value-redacted
        result["failure"] = {"code": "IO_OR_PARSE_ERROR", "where": type(e).__name__}
    result["finished_utc"] = utc_now()
    try:
        write_receipt_exclusive(receipt, result)
    except OSError:
        sys.stderr.write("receipt could not be created exclusively\n")
        return 3
    sys.stdout.write("%s verdict %s\n" % (TOOL_NAME, result["verdict"]))
    if result["failure"]:
        sys.stdout.write("%s (%s)\n" % (result["failure"]["code"], result["failure"]["where"]))
    return 0 if result["verdict"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
