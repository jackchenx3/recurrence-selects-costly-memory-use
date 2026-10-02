#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Independent record and estimate verifier for PHASE2-PERFORMANCE-CONVERSION-002 revision 1.

Python 3.6, standard library only. Written from the frozen specification, frozen_config.json,
OUTPUT_FORMATS.md and binary_records.json (see docs/INPUT_CONTRACT.md). It does not implement or call the
simulation, generates no random draw, and imports no producer, analyzer or C++ auditor code. The file keeps
its seed name (mmem_record_verifier.py) so the package file set is unchanged; the tool identifier is
pconv_record_verifier.

Failure diagnostics name only a category, coordinates, record index or manifest entry index/path, and a field
name; they never carry an observed or recomputed value.

Subcommands
  production           --run-dir DIR --analysis-dir DIR --receipt NEW.json
  fixture              --layout-dir DIR --receipt NEW.json
  synthetic-estimates  --blocks-file FILE --n-blocks N --analysis-dir DIR --receipt NEW.json
                       (NONSCIENTIFIC: exercises the estimate comparison on synthetic block records)

Exit status: 0 PASS; 1 INVALID (receipt written); 2 usage error or unusable receipt path (no
receipt); 3 INVALID but the receipt could not be written.
"""

import argparse
import datetime
import decimal
import hashlib
import itertools
import json
import os
import re
import struct
import sys
from decimal import Decimal
from fractions import Fraction

TOOL_NAME = 'pconv_record_verifier'
TOOL_VERSION = 'PCONV-AUDIT-RECORDS-1.0.0'
RECEIPT_SCHEMA = 'PCONV-AUDITOR-RECORDS-RECEIPT-1'
STUDY_ID = 'PHASE2-PERFORMANCE-CONVERSION-002'

# Frozen design constants (specification sections 2, 3, 9, 10, 12; frozen_config.json).
N_BLOCKS = 41600
N_SHARDS = 32
BLOCKS_PER_SHARD = 1300
CELLS = 6
UPDATES = 256
LATE_FIRST = 193
SLOTS = 32
CANDIDATES = 128
AUDIT_BLOCKS = 64
PERM_STEPS = 31
NA = 255
MASK32 = 0xFFFFFFFF
MASK64 = (1 << 64) - 1
TWO64 = 1 << 64

# frozen_config.json input_hashes.
SPECIFICATION_SHA256 = '58ee414d753fab042e6cccee1a4e6b95dd7f167633b4f33c1491687d9afcb895'
TERMINAL_REVIEW_SHA256 = '76b0c6172e61f60befecef183e83d9e43ea50c1b6bc1e93299668b76f55be60f'
DESIGN_GO_SHA256 = 'f4a6180109e0d212cbe784a7722a64576e5e7cb0fbfd4b23c757bb5e7ab84d6a'

# Fixed little-endian layouts (binary_records.json).
UPD = struct.Struct('<IHBBHBBBBBBQBBBBBBBBBBHIII')
PTH = struct.Struct('<IBBBBIIIIIIIQHHHH32sIII32sIIIIIIIIIIQ')
BLK = struct.Struct('<IBBBB7i6I6IBBHHHI')
AUD = struct.Struct('<IHBBBBBBIIIBBBBQBBBBQQQIIIIBBHI')
PRM = struct.Struct('<IHBBI32s31Q31I')

UPD_PER_BLOCK = CELLS * UPDATES * UPD.size          # 73,728
PTH_PER_BLOCK = CELLS * PTH.size                    # 1,056
AUD_PER_UPDATE = CANDIDATES * AUD.size              # 11,264
AUD_PER_BLOCK = CELLS * UPDATES * AUD_PER_UPDATE    # 17,301,504
PRM_PER_BLOCK = UPDATES * PRM.size                  # 106,496
AUDIT_ROWS_FILE = 'audit_rows_blocks_0000_0063.bin'
AUDIT_PERM_FILE = 'audit_permutations_blocks_0000_0063.bin'
SHARD_SIZES = {
    'updates.bin': BLOCKS_PER_SHARD * UPD_PER_BLOCK,
    'paths.bin': BLOCKS_PER_SHARD * PTH_PER_BLOCK,
    'blocks.bin': BLOCKS_PER_SHARD * BLK.size,
}
SHARD_00_EXTRA = {AUDIT_ROWS_FILE: AUDIT_BLOCKS * AUD_PER_BLOCK, AUDIT_PERM_FILE: AUDIT_BLOCKS * PRM_PER_BLOCK}
# PROVISIONAL fixture file names (docs/INPUT_CONTRACT.md section 4); a different accepted layout fails closed.
FIXTURE_UPDATES = 'layout_sample_updates.bin'
FIXTURE_PATHS = 'layout_sample_paths.bin'
FIXTURE_BLOCK = 'layout_sample_block.bin'
FIXTURE_AUDIT = 'layout_sample_audit.bin'
FIXTURE_PERM = 'layout_sample_permutations.bin'
FIXTURE_README = 'layout_sample_README.txt'
FIXTURE_FILES = {FIXTURE_UPDATES: UPD_PER_BLOCK, FIXTURE_PATHS: PTH_PER_BLOCK, FIXTURE_BLOCK: BLK.size,
                 FIXTURE_AUDIT: AUD_PER_BLOCK, FIXTURE_PERM: PRM_PER_BLOCK, FIXTURE_README: None}

# frozen_config.json design.cells_in_index_order; cell = 2*arm + start.
CELL_LABELS = ['INFO|ALL_F', 'INFO|ALL_M', 'NONINFO|ALL_F', 'NONINFO|ALL_M', 'SHAM|ALL_F', 'SHAM|ALL_M']

# Update record field indices (binary_records.json records.update.fields).
UPDATE_FIELDS = ['block', 'update', 'cell', 'm_count', 'total_mismatch', 'valid_m_cache', 'cache_probe_use',
                 'cache_probe_survivors', 'f_to_m', 'm_to_f', 'query_count', 'survival_retries_u64',
                 'recurrence_applied', 'true_cache_use', 'decoy_use', 'true_cache_survivors', 'decoy_survivors',
                 'valid_m_disp_w0', 'valid_m_disp_w32', 'decoy_w0', 'decoy_w32', 'decoy_identical_to_cache',
                 'reserved_u16', 'decoy_perm_retry_total', 'decoy_perm_fnv1a', 'reserved_u32']
(U_BLOCK, U_UPDATE, U_CELL, U_M, U_MIS, U_VMC, U_CPU, U_CPS, U_F2M, U_M2F, U_Q, U_RT, U_RAPP, U_TCU, U_DU, U_TCS,
 U_DS, U_VW0, U_VW32, U_DW0, U_DW32, U_IDENT, U_RES16, U_PRT, U_FNV, U_RES32) = range(26)

# Path record field indices (binary_records.json records.path.fields).
PATH_FIELDS = ['block', 'cell', 'arm', 'start', 'reserved_u8', 'late_m_sum', 'late_mismatch_sum', 'total_queries',
               'f_to_m', 'm_to_f', 'cache_probe_use', 'cache_probe_survivors', 'survival_retries_u64',
               'fixation_updates', 'extinction_updates', 'late_fixation_updates', 'late_extinction_updates',
               'final_state_sha256', 'final_m_count', 'final_total_mismatch', 'reserved_u32',
               'paired_trajectory_sha256', 'true_cache_use', 'decoy_use', 'true_cache_survivors', 'decoy_survivors',
               'valid_m_disp_w0', 'valid_m_disp_w32', 'decoy_w0', 'decoy_w32', 'decoy_identical_to_cache',
               'recurrent_updates', 'decoy_perm_retry_total_u64']
(P_BLOCK, P_CELL, P_ARM, P_START, P_RES8, P_LATE_M, P_LATE_MIS, P_QUERIES, P_F2M, P_M2F, P_CPU, P_CPS, P_RT, P_FIX,
 P_EXT, P_LFIX, P_LEXT, P_FINAL_HASH, P_FINAL_M, P_FINAL_MIS, P_RES32, P_PAIRED, P_TCU, P_DU, P_TCS, P_DS, P_VW0,
 P_VW32, P_DW0, P_DW32, P_IDENT, P_RECURRENT, P_PRT) = range(33)

# Block record field names (binary_records.json records.block.fields).
BLOCK_FIELDS = (['block', 'n1_ok', 'n2_ok', 'query_ok', 'audit_block', 'delta_p_num_over_131072',
                 'd_info_num_over_2048', 'd_noninfo_num_over_2048', 'e_info_num_over_4096',
                 'e_noninfo_num_over_4096', 'b_info_num_over_131072', 'b_noninfo_num_over_131072'] +
                ['late_m_sum[%d]' % c for c in range(CELLS)] + ['late_mismatch_sum[%d]' % c for c in range(CELLS)] +
                ['c1_ok', 'reserved_u8', 'first_decoupling_update_ALL_F', 'first_decoupling_update_ALL_M',
                 'recurrent_updates', 'decoy_perm_retry_total'])

# ------------------------------------------------------------------------------------------------
# Frozen inference (specification sections 9-11; frozen_config.json inference and decision rules).

# (record, name, family, range length R, block-value denominator, 2/alpha_each exactly, frozen half-width,
#  block-record numerator index). alpha_each = 0.05/3 gives 2/alpha = 120; alpha_each = 0.025 gives 80.
ESTIMANDS = [
    (1, 'Delta_P', 'PRIMARY', 2, 131072, 120, '0.0151712845', 0),
    (2, 'D_INFO', 'PRIMARY', 2, 2048, 120, '0.0151712845', 1),
    (3, 'D_NONINFO', 'PRIMARY', 2, 2048, 120, '0.0151712845', 2),
    (4, 'E_INFO', 'ALLELE', 2, 4096, 80, '0.0145146256', 3),
    (5, 'E_NONINFO', 'ALLELE', 1, 4096, 80, '0.0072573128', 4),
    (6, 'B_INFO', 'PERFORMANCE', 2, 131072, 80, '0.0145146256', 5),
    (7, 'B_NONINFO', 'PERFORMANCE', 2, 131072, 80, '0.0145146256', 6),
]
SECONDARY_NAMES = ('E_INFO', 'E_NONINFO', 'B_INFO', 'B_NONINFO')
DELTA = Decimal(1) / Decimal(32)
HP_PREC = 110
# Agreement tolerance for analysis decimal strings (no study-002 producer Decimal operation order is frozen in
# the supplied texts; see docs/INPUT_CONTRACT.md section 6). Exact fractions are compared exactly.
DECIMAL_TOLERANCE = Decimal('1e-40')

PRIMARY_LABELS = {
    1: 'INVALID',
    2: 'START-DEPENDENT; PRIMARY UNRESOLVED',
    3: 'MEANINGFUL POSITIVE DIRECTIONAL-INFORMATION EFFECT RELATIVE TO NONINFO',
    4: 'MEANINGFUL ADVERSE DIRECTIONAL-INFORMATION EFFECT RELATIVE TO NONINFO',
    5: 'BOUNDED BELOW THE POSITIVE ONE-BIT SCALE',
    6: 'UNRESOLVED',
}
SECONDARY_LABELS = ['MEANINGFUL POSITIVE AT THE ONE-BIT SCALE', 'MEANINGFUL ADVERSE AT THE ONE-BIT SCALE',
                    'BOUNDED BELOW THE POSITIVE ONE-BIT SCALE', 'UNRESOLVED']
BRANCH_STATEMENTS = [
    'Directional cache information produces a one-bit benefit relative to both the matched decoy and SHAM in '
    'this fixed model.',
    'Directional information offsets some displacement cost relative to NONINFO; no one-bit absolute benefit '
    'over no-memory use is demonstrated.',
    'Directional information selects for policy use without converting into one-bit population performance, '
    'sharpening the selection-performance separation.',
    'The fixed information intervention does not explain the accepted enrichment; close this conversion route.',
    'No interpretive branch applies; the primary question is unresolved and the secondary families are reported '
    'without changing it.',
]

# PROVISIONAL analysis/manifest interface (OUTPUT_FORMATS.md names the files and their content but no study-002
# producer-output interface contract was supplied; see docs/INPUT_CONTRACT.md sections 5-6). Keys are read only
# at these exact names; nothing is searched.
ESTIMATES_FILE = 'estimates_19.json'
DECISION_FILE = 'decision.json'
CLOSED_FILE = 'ROUTE_CLOSED_INVALID.json'
KEYS_INFERENTIAL = ('record', 'name', 'estimate_exact', 'estimate', 'half_width', 'lower', 'upper')
KEY_PRIMARY_DECISION = 'family_decision'
KEY_SECONDARY_CLASS = 'classification'
KEYS_CELL = ('record', 'name', 'estimate_exact', 'estimate')
DECISION_KEYS = ('primary_decision', 'decision_rule_applied', 'bounded_interval_wholly_inside_minus_delta_plus_delta',
                 'secondary_classifications', 'interpretive_branch', 'interpretation_labels')
MANIFEST_FILE = 'run_manifest.json'
MANIFEST_EXACT = [('study_id', STUDY_ID), ('key_namespace', 'production-r1'),
                  ('specification_sha256', SPECIFICATION_SHA256), ('terminal_review_sha256', TERMINAL_REVIEW_SHA256),
                  ('design_go_sha256', DESIGN_GO_SHA256)]
MANIFEST_OUTPUT_KEYS = ('path', 'bytes', 'sha256')

POP16 = bytes(bin(i).count('1') for i in range(1 << 16))
DIAGNOSTIC_MAX = 480
MISMATCH_CAP = 1000000


def pop32(v):
    return POP16[v & 0xFFFF] + POP16[v >> 16]


def fnv1a32(data):
    h = 0x811C9DC5
    for b in bytearray(data):
        h = ((h ^ b) * 0x01000193) & MASK32
    return h


def apply_perm(d, perm):
    """Source bit s of d moves to destination bit perm[s] (specification section 5)."""
    out = 0
    for s in range(32):
        if (d >> s) & 1:
            out |= 1 << perm[s]
    return out


def fisher_yates_replay(xs):
    """Replays the shared permutation from the 31 accepted 64-bit draws (steps i = 31..1). Returns
    (perm, None), or (None, k) if accepted draw k fails the Lemire acceptance test. No draw is generated."""
    perm = list(range(32))
    for k, i in enumerate(range(31, 0, -1)):
        n = i + 1
        product = xs[k] * n
        if (product & MASK64) < (TWO64 - n) % n:
            return None, k
        j = product >> 64
        perm[i], perm[j] = perm[j], perm[i]
    return perm, None


def bound_text(text):
    text = str(text)
    return text if len(text) <= DIAGNOSTIC_MAX else text[:DIAGNOSTIC_MAX] + '...'


def utc_now():
    return datetime.datetime.utcnow().replace(microsecond=0).isoformat() + 'Z'


class Fatal(Exception):
    """Stops the audit; the discrepancy has already been recorded."""


class Recorder(object):
    def __init__(self):
        self.count = 0
        self.capped = False
        self.first = None

    def fail(self, category, detail):
        if self.first is None:
            self.first = {'category': category, 'detail': bound_text(detail)}
        if self.count >= MISMATCH_CAP:
            self.capped = True
            raise Fatal()
        self.count += 1

    def fatal(self, category, detail):
        self.fail(category, detail)
        raise Fatal()


class HashedReader(object):
    """Read-only input that hashes every byte it delivers and, on finish(), the remainder."""

    def __init__(self, path, label):
        self.path = path
        self.label = label
        self.sha = hashlib.sha256()
        self.nbytes = 0
        self.fh = None
        self.done = False
        self.digest = None
        self.error = None
        try:
            self.fh = open(path, 'rb')
        except (OSError, IOError) as exc:
            self.error = 'open failed: %s' % (exc.strerror or type(exc).__name__)

    def read_exact(self, n, rec, what):
        data = self.fh.read(n)
        self.sha.update(data)
        self.nbytes += len(data)
        if len(data) != n:
            rec.fatal('SHORT_RECORD', '%s: %s is short' % (self.label, what))
        return data

    def read_all(self):
        chunks = []
        while True:
            data = self.fh.read(1 << 22)
            if not data:
                break
            self.sha.update(data)
            self.nbytes += len(data)
            chunks.append(data)
        return b''.join(chunks)

    def expect_eof(self, rec):
        data = self.fh.read(1)
        if data:
            self.sha.update(data)
            self.nbytes += len(data)
            rec.fatal('TRAILING_BYTES', '%s has bytes after the last documented record' % self.label)

    def finish(self):
        if self.done:
            return
        self.done = True
        if self.fh is None:
            return
        while True:
            data = self.fh.read(1 << 22)
            if not data:
                break
            self.sha.update(data)
            self.nbytes += len(data)
        self.fh.close()
        self.fh = None
        self.digest = self.sha.hexdigest()

    def info(self):
        return {'label': self.label, 'path': os.path.abspath(self.path),
                'bytes': self.nbytes if self.digest is not None else None,
                'sha256': self.digest, 'error': self.error}


class Context(object):
    def __init__(self):
        self.readers = []
        self.by_label = {}
        self.counts = {}
        self.expected = {}

    def open(self, path, label, rec):
        reader = HashedReader(path, label)
        self.readers.append(reader)
        self.by_label[label] = reader
        if reader.error is not None:
            rec.fatal('MISSING_INPUT', '%s: %s' % (label, reader.error))
        return reader

    def bump(self, key, amount=1):
        self.counts[key] = self.counts.get(key, 0) + amount


# ------------------------------------------------------------------------------------------------
# Update, path and block records (every block)


def verify_cell_records(bid, c, upd, p, rec, ctx):
    """The 256 update records of one path, then the path record reconstructed from them."""
    arm, start = c >> 1, c & 1
    base = c * UPDATES
    where = 'block %d cell %d' % (bid, c)
    late_m = late_mis = queries = f2m = m2f = cpu_sum = cps_sum = retries = 0
    fix = ext = lfix = lext = 0
    tcu_sum = du_sum = tcs_sum = ds_sum = vw0_sum = vw32_sum = dw0_sum = dw32_sum = ident_sum = 0
    recurrent = perm_retries = 0
    prev_m = 0
    last = None
    for k in range(UPDATES):
        r = upd[base + k]
        t = k + 1
        loc = '%s update %d' % (where, t)
        if r[U_BLOCK] != bid or r[U_UPDATE] != t or r[U_CELL] != c:
            rec.fatal('ORDER', '%s update record fields block/update/cell out of order' % loc)
        (m, mis, vmc, cpu, cps, fm, mf, q, rt, rapp, tcu, du, tcs, ds, vw0, vw32, dw0, dw32, ident, res16, prt,
         _fnv, res32) = r[3:]
        for fname, bad in (('m_count', m > 32), ('total_mismatch', mis > 1024), ('valid_m_cache', vmc > 32),
                           ('f_to_m+m_to_f', fm + mf > 32), ('survival_retries_u64', rt >= (32 << 32)),
                           ('recurrence_applied', rapp > 1 or (t < 3 and rapp != 0)),
                           ('cache_probe_survivors', cps > cpu),
                           ('true_cache_use+decoy_use', tcu + du != cpu),
                           ('true_cache_survivors+decoy_survivors', tcs + ds != cps or tcs > tcu or ds > du),
                           ('valid_m_disp_w0+valid_m_disp_w32', vw0 + vw32 > vmc),
                           ('decoy_identical_to_cache', ident > du or dw0 + dw32 > du or ident < dw0 + dw32)):
            if bad:
                rec.fail('RANGE', '%s field %s out of range' % (loc, fname))
                break
        if res16 != 0:
            rec.fail('RESERVED', '%s update record field reserved_u16 nonzero' % loc)
        if res32 != 0:
            rec.fail('RESERVED', '%s update record field reserved_u32 nonzero' % loc)
        if q != CANDIDATES:
            rec.fail('QUERY_COUNT', '%s field query_count mismatch' % loc)
        else:
            ctx.bump('path_updates_with_128_queries')
        if vmc != (0 if t == 1 else prev_m):
            rec.fail('IDENTITY', '%s field valid_m_cache inconsistent with previous m_count' % loc)
        if arm == 0:
            ok = cpu == vmc and tcu == vmc and du == 0 and dw0 == 0 and dw32 == 0 and ident == 0
        elif arm == 1:
            if rapp:
                ok = cpu == vmc and du == vmc and tcu == 0 and dw0 == vw0 and dw32 == vw32
            else:
                ok = cpu == vmc and tcu == vmc and du == 0 and dw0 == 0 and dw32 == 0 and ident == 0
        else:
            ok = (cpu == 0 and cps == 0 and tcu == 0 and du == 0 and tcs == 0 and ds == 0 and dw0 == 0 and
                  dw32 == 0 and ident == 0)
        if not ok:
            rec.fail('IDENTITY', '%s probe-source counters inconsistent with arm, valid_m_cache and '
                     'recurrence_applied' % loc)
        prev_m = m
        if t >= LATE_FIRST:
            late_m += m
            late_mis += mis
        queries += q
        f2m += fm
        m2f += mf
        cpu_sum += cpu
        cps_sum += cps
        retries += rt
        if m == 32:
            fix += 1
            if t >= LATE_FIRST:
                lfix += 1
        if m == 0:
            ext += 1
            if t >= LATE_FIRST:
                lext += 1
        tcu_sum += tcu
        du_sum += du
        tcs_sum += tcs
        ds_sum += ds
        vw0_sum += vw0
        vw32_sum += vw32
        dw0_sum += dw0
        dw32_sum += dw32
        ident_sum += ident
        recurrent += rapp
        perm_retries += prt
        last = r
    ctx.bump('update_records', UPDATES)

    expected = {P_BLOCK: bid, P_CELL: c, P_ARM: arm, P_START: start, P_LATE_M: late_m, P_LATE_MIS: late_mis,
                P_QUERIES: queries, P_F2M: f2m, P_M2F: m2f, P_CPU: cpu_sum, P_CPS: cps_sum, P_RT: retries,
                P_FIX: fix, P_EXT: ext, P_LFIX: lfix, P_LEXT: lext, P_FINAL_M: last[U_M],
                P_FINAL_MIS: last[U_MIS], P_TCU: tcu_sum, P_DU: du_sum, P_TCS: tcs_sum, P_DS: ds_sum,
                P_VW0: vw0_sum, P_VW32: vw32_sum, P_DW0: dw0_sum, P_DW32: dw32_sum, P_IDENT: ident_sum,
                P_RECURRENT: recurrent, P_PRT: perm_retries}
    for idx in sorted(expected):
        if p[idx] != expected[idx]:
            rec.fail('PATH_FIELD', '%s path record field %s mismatch' % (where, PATH_FIELDS[idx]))
            break
    if queries != UPDATES * CANDIDATES:
        rec.fail('QUERY_COUNT', '%s path field total_queries mismatch' % where)
    if p[P_RES8] != 0:
        rec.fail('RESERVED', '%s path record field reserved_u8 nonzero' % where)
    if p[P_RES32] != 0:
        rec.fail('RESERVED', '%s path record field reserved_u32 nonzero' % where)
    ctx.bump('path_records')


def verify_shared_fields(bid, upd, rec):
    """Recurrence indicator and the shared-permutation evidence are identical in all six cells."""
    for k in range(UPDATES):
        r0 = upd[k]
        for c in range(1, CELLS):
            r = upd[c * UPDATES + k]
            if r[U_RAPP] != r0[U_RAPP] or r[U_PRT] != r0[U_PRT] or r[U_FNV] != r0[U_FNV]:
                rec.fail('SHARED', 'block %d cell %d update %d fields recurrence_applied/decoy_perm_retry_total/'
                         'decoy_perm_fnv1a differ from cell 0' % (bid, c, k + 1))
                return


def verify_sham(bid, upd, pth, rec):
    """N1/N2 for the SHAM pair (cells 4 and 5): paired-hash identity plus record-level consequences."""
    a, b = 4, 5
    ok = True
    for k in range(UPDATES):
        ra, rb = upd[a * UPDATES + k], upd[b * UPDATES + k]
        if not (ra[U_MIS] == rb[U_MIS] and ra[U_M] + rb[U_M] == 32 and ra[U_F2M] == rb[U_M2F] and
                ra[U_M2F] == rb[U_F2M] and ra[U_Q] == rb[U_Q] and ra[U_RT] == rb[U_RT] and
                (k == 0 or ra[U_VMC] + rb[U_VMC] == 32)):
            ok = False
            rec.fail('N1_N2', 'block %d SHAM cell %d and cell %d update %d update records violate N1/N2'
                     % (bid, a, b, k + 1))
            break
    pa, pb = pth[a], pth[b]
    if pa[P_PAIRED] != pb[P_PAIRED]:
        ok = False
        rec.fail('N1_N2_PAIRED_HASH', 'block %d SHAM cell %d and cell %d field paired_trajectory_sha256 differs'
                 % (bid, a, b))
    if not (pa[P_LATE_MIS] == pb[P_LATE_MIS] and pa[P_LATE_M] + pb[P_LATE_M] == 2048 and
            pa[P_FINAL_M] + pb[P_FINAL_M] == 32 and pa[P_FINAL_MIS] == pb[P_FINAL_MIS] and
            pa[P_FIX] == pb[P_EXT] and pa[P_EXT] == pb[P_FIX] and pa[P_LFIX] == pb[P_LEXT] and
            pa[P_LEXT] == pb[P_LFIX] and pa[P_F2M] == pb[P_M2F] and pa[P_M2F] == pb[P_F2M] and
            pa[P_RT] == pb[P_RT]):
        ok = False
        rec.fail('N1_N2', 'block %d SHAM cell %d and cell %d path summaries violate N1/N2' % (bid, a, b))
    return ok


def first_decoupling(upd, cell):
    """First update at which some decoy probe of this NONINFO path differs from the true cache (0 = never)."""
    base = cell * UPDATES
    for k in range(UPDATES):
        r = upd[base + k]
        if r[U_DU] > r[U_IDENT]:
            return k + 1
    return 0


C1_EQUAL = (U_M, U_MIS, U_VMC, U_CPU, U_CPS, U_F2M, U_M2F, U_Q, U_RT, U_VW0, U_VW32)


def verify_c1_records(bid, upd, first, rec):
    """Record-level consequences of C1: INFO and NONINFO of each start agree through the update before
    the first decoupling update, which must be recurrent with agreeing pre-update diagnostics."""
    ok = True
    for s in (0, 1):
        a, b, f = s, 2 + s, first[s]
        last = f - 1 if f else UPDATES
        for k in range(last):
            ra, rb = upd[a * UPDATES + k], upd[b * UPDATES + k]
            if (any(ra[i] != rb[i] for i in C1_EQUAL) or ra[U_TCU] != rb[U_TCU] + rb[U_DU] or
                    ra[U_TCS] != rb[U_TCS] + rb[U_DS]):
                ok = False
                rec.fail('C1', 'block %d cell %d and cell %d update %d update records differ before the first '
                         'decoupling update' % (bid, a, b, k + 1))
                break
        if f:
            ra, rb = upd[a * UPDATES + f - 1], upd[b * UPDATES + f - 1]
            if (rb[U_RAPP] != 1 or ra[U_VMC] != rb[U_VMC] or ra[U_VW0] != rb[U_VW0] or
                    ra[U_VW32] != rb[U_VW32]):
                ok = False
                rec.fail('C1', 'block %d cell %d and cell %d first decoupling update is not recurrent or its '
                         'pre-update diagnostics differ' % (bid, a, b))
    return ok


def block_numerators(L, P):
    """The seven block numerators (OUTPUT_FORMATS.md per-block record; accuracy = 1 - mismatch/65536)."""
    return ((P[2] + P[3]) - (P[0] + P[1]),      # Delta_P / 131072
            L[1] - L[0],                        # D_INFO / 2048
            L[3] - L[2],                        # D_NONINFO / 2048
            (L[0] + L[1]) - (L[2] + L[3]),      # E_INFO / 4096
            (L[2] + L[3]) - 2048,               # E_NONINFO / 4096
            (P[4] + P[5]) - (P[0] + P[1]),      # B_INFO / 131072
            (P[4] + P[5]) - (P[2] + P[3]))      # B_NONINFO / 131072


def check_block_record(bid, blk, L, P, extra, rec):
    """Block record against values reconstructed from the path and update records (or, in synthetic mode,
    its own late-sum arrays). extra = (first decoupling ALL_F, ALL_M, recurrent updates, permutation
    retry total) or None in synthetic mode."""
    where = 'block %d' % bid
    for c in range(CELLS):
        if L[c] > 2048 or P[c] > 65536:
            rec.fail('RANGE', '%s cell %d field late_m_sum/late_mismatch_sum out of range' % (where, c))
    # N2 implies complementary SHAM late M sums; N1 implies equal SHAM mismatch sums.
    if L[4] + L[5] != 2048 or P[4] != P[5]:
        rec.fail('N1_N2', '%s SHAM fields late_m_sum/late_mismatch_sum violate N1/N2' % where)
    nums = block_numerators(L, P)
    tail = tuple(extra) if extra is not None else tuple(blk[26:30])
    expected = ((bid, 1, 1, 1, 1 if bid < AUDIT_BLOCKS else 0) + nums + tuple(L) + tuple(P) + (1, 0) + tail)
    for i, name in enumerate(BLOCK_FIELDS):
        if blk[i] != expected[i]:
            rec.fail('BLOCK_FIELD', '%s block record field %s mismatch' % (where, name))
            break
    return nums


def verify_block(bid, ub, pb, bb, ab, mb, rec, acc, ctx):
    upd = list(UPD.iter_unpack(ub))
    pth = list(PTH.iter_unpack(pb))
    blk = BLK.unpack(bb)
    for c in range(CELLS):
        verify_cell_records(bid, c, upd, pth[c], rec, ctx)
    verify_shared_fields(bid, upd, rec)
    if verify_sham(bid, upd, pth, rec):
        ctx.bump('blocks_with_both_sham_identities')
    first = (first_decoupling(upd, 2), first_decoupling(upd, 3))
    if verify_c1_records(bid, upd, first, rec):
        ctx.bump('blocks_with_c1_record_consistency')
    L = [pth[c][P_LATE_M] for c in range(CELLS)]
    P = [pth[c][P_LATE_MIS] for c in range(CELLS)]
    recurrent = sum(upd[k][U_RAPP] for k in range(UPDATES))
    perm_total = sum(upd[k][U_PRT] for k in range(UPDATES))
    nums = check_block_record(bid, blk, L, P, (first[0], first[1], recurrent, perm_total), rec)
    accumulate(acc, nums, L, P)
    ctx.bump('block_records')
    if ab is not None:
        perms = verify_permutations(bid, mb, upd, rec, ctx)
        verify_audit_block(bid, ab, upd, pth, perms, rec, ctx)


def accumulate(acc, nums, L, P):
    for i in range(7):
        acc['nums'][i] += nums[i]
    for c in range(CELLS):
        acc['L'][c] += L[c]
        acc['P'][c] += P[c]
    acc['n'] += 1


def new_acc():
    return {'nums': [0] * 7, 'L': [0] * CELLS, 'P': [0] * CELLS, 'n': 0}


# ------------------------------------------------------------------------------------------------
# Audit permutation records and candidate rows (consistency checks only; no draw is generated here)


def verify_permutations(bid, mb, upd, rec, ctx):
    """The 256 shared permutation records of an audit block. Returns the replayed permutation per update."""
    perms = [None] * (UPDATES + 1)
    for k, r in enumerate(PRM.iter_unpack(mb)):
        t = k + 1
        loc = 'block %d update %d' % (bid, t)
        if r[0] != bid or r[1] != t:
            rec.fatal('ORDER', '%s permutation record fields block/update out of order' % loc)
        steps, reserved, total, pbytes = r[2], r[3], r[4], r[5]
        xs = r[6:6 + PERM_STEPS]
        retries = r[6 + PERM_STEPS:6 + 2 * PERM_STEPS]
        if reserved != 0:
            rec.fail('RESERVED', '%s permutation record field reserved_u8 nonzero' % loc)
        replayed, _ = fisher_yates_replay(xs)
        stored = list(bytearray(pbytes))
        if replayed is None:
            rec.fail('PERMUTATION', '%s permutation record field accepted_x fails the Lemire acceptance test' % loc)
        elif replayed != stored:
            rec.fail('PERMUTATION', '%s permutation record field perm differs from the Fisher-Yates replay of '
                     'accepted_x' % loc)
        if steps != sum(1 for v in retries if v) or total != sum(retries):
            rec.fail('PERMUTATION', '%s permutation record fields retry_steps/retry_total inconsistent with '
                     'accepted_retry' % loc)
        u = upd[k]  # cell 0; all six cells carry identical shared fields (verify_shared_fields)
        if total != u[U_PRT] or fnv1a32(pbytes) != u[U_FNV]:
            rec.fail('PERMUTATION_VS_UPDATE', '%s permutation record fields retry_total/perm disagree with update '
                     'record fields decoy_perm_retry_total/decoy_perm_fnv1a' % loc)
        perms[t] = replayed if replayed is not None else stored
        ctx.bump('permutation_records')
    return perms


def verify_selection(g, order, weights, where, rec):
    prefix = [0] + list(itertools.accumulate(weights))
    remaining = prefix[CANDIDATES]
    taken = []
    for d in range(SLOTS):
        j = order[d]
        row = g[j]
        w_d, x, z = row[20], row[21], row[22]
        if w_d != remaining or w_d == 0:
            rec.fail('SELECTION', '%s draw %d candidate %d field W differs from remaining weight' % (where, d, j))
            return
        threshold = (TWO64 - w_d) % w_d
        product = x * w_d
        if (product & MASK64) < threshold or (product >> 64) != z:
            rec.fail('LEMIRE', '%s draw %d candidate %d fields x/Z inconsistent with W' % (where, d, j))
            return
        before = prefix[j] - sum(weights[s] for s in taken if s < j)
        if not (before <= z < before + weights[j]):
            rec.fail('SELECTION', '%s draw %d candidate %d field Z does not select this candidate' % (where, d, j))
            return
        taken.append(j)
        remaining -= weights[j]


def check_shared_mask(shared, key, mask, loc, name, rec):
    """Fresh, scout and local masks of one (update, parent) are identical in every cell that uses them."""
    prev = shared.get(key)
    if prev is None:
        shared[key] = mask
    elif prev != mask:
        rec.fail('PAIRING', '%s field applied_mask (%s) differs across cells' % (loc, name))


def verify_audit_block(bid, ab, upd, pth, perms, rec, ctx):
    targets = [None] * (UPDATES + 1)
    rbits = [None] * (UPDATES + 1)
    applied = [None] * (UPDATES + 1)
    initial = None
    shared = {}
    for c in range(CELLS):
        arm, start = c >> 1, c & 1
        p_label = [start] * SLOTS
        p_cv = [0] * SLOTS
        p_cache = [0] * SLOTS
        p_geno = None
        paired = hashlib.sha256()
        paired.update(b'PCONV-PAIRED-TRAJECTORY-V1')
        paired.update(struct.pack('<IB', bid, arm))
        for t in range(1, UPDATES + 1):
            off = (c * UPDATES + (t - 1)) * AUD_PER_UPDATE
            g = list(AUD.iter_unpack(ab[off:off + AUD_PER_UPDATE]))
            where = 'block %d cell %d update %d' % (bid, c, t)
            target, rbit, rapp = g[0][10], g[0][12], g[0][13]
            fam0 = [g[i][9] for i in range(SLOTS)]
            if t == 1:
                if initial is None:
                    initial = fam0
                elif fam0 != initial:
                    rec.fail('INITIAL_STATE', '%s initial genotypes differ from cell 0' % where)
            elif fam0 != p_geno:
                rec.fail('CONTINUITY', '%s parent genotypes differ from previous survivors' % where)
            if targets[t] is None:
                targets[t], rbits[t], applied[t] = target, rbit, rapp
            elif (target, rbit, rapp) != (targets[t], rbits[t], applied[t]):
                rec.fail('TARGET_LAW', '%s fields target/recurrence_bit/recurrence_applied differ from cell 0'
                         % where)
            if rbit > 1 or rapp != (1 if (t >= 3 and rbit == 1) else 0):
                rec.fail('RECURRENCE', '%s fields recurrence_bit/recurrence_applied inconsistent' % where)
            perm = perms[t]
            weights = [0] * CANDIDATES
            order = [None] * SLOTS
            ok_sel = True
            s_mis = s_m = vmc = cpu = cps = fm = mf = s_retry = 0
            tcu = du = tcs = ds = vw0 = vw32 = dw0 = dw32 = ident = 0
            for j in range(CANDIDATES):
                (r_b, r_u, r_c, cand, fam, par, plab, pcv, pcache, geno, tgt, mis, rb, ra, ps, w, rank, post,
                 flip, donor, w_d, x, z, retry, pref, pg, tdisp, pdist, dw, res, amask) = g[j]
                loc = '%s candidate %d' % (where, j)
                if r_b != bid or r_u != t or r_c != c or cand != j:
                    rec.fatal('ORDER', '%s audit row fields block/update/cell/candidate out of order' % loc)
                if fam != (j >> 5) or par != (j & 31):
                    rec.fail('AUDIT_FIELD', '%s field family/parent mismatch' % loc)
                if plab != p_label[par] or pcv != p_cv[par] or pcache != p_cache[par]:
                    rec.fail('CONTINUITY', '%s field parent_label_pre/parent_cache_valid/parent_cache differs from '
                             'previous update' % loc)
                if tgt != target or rb != rbit or ra != rapp:
                    rec.fail('AUDIT_FIELD', '%s field target/recurrence_bit/recurrence_applied not constant' % loc)
                if mis != pop32(geno ^ tgt):
                    rec.fail('MISMATCH', '%s field mismatch != popcount(genotype^target)' % loc)
                if mis > 32 or w != (1 << (32 - mis)):
                    rec.fail('WEIGHT', '%s field weight is not the exact power-of-two weight' % loc)
                else:
                    weights[j] = w
                if pg != fam0[par]:
                    rec.fail('AUDIT_FIELD', '%s field parent_genotype differs from the parent candidate' % loc)
                want_disp = (pcache ^ pg) if pcv == 1 else 0
                want_dw = pop32(want_disp) if pcv == 1 else NA
                if pcv > 1 or tdisp != want_disp or dw != want_dw:
                    rec.fail('AUDIT_FIELD', '%s field true_displacement/displacement_weight mismatch' % loc)
                if pdist != pop32(geno ^ pg):
                    rec.fail('AUDIT_FIELD', '%s field parent_distance mismatch' % loc)
                if pref != bid * 256 + t - 1:
                    rec.fail('AUDIT_FIELD', '%s field perm_ref mismatch' % loc)
                if res != 0:
                    rec.fail('RESERVED', '%s field reserved_u16 nonzero' % loc)
                valid_m = plab == 1 and pcv == 1
                if fam == 0:
                    if ps != NA or amask != 0 or geno != pg:
                        rec.fail('OPERATOR', '%s parent candidate fields probe_source/applied_mask/genotype' % loc)
                elif fam == 1:
                    if arm == 2 or not valid_m:
                        want_ps = 0
                    elif arm == 1 and rapp == 1:
                        want_ps = 2
                    else:
                        want_ps = 1
                    if ps != want_ps:
                        rec.fail('PROBE', '%s field probe_source mismatch' % loc)
                    elif want_ps == 0:
                        if geno != pg ^ amask:
                            rec.fail('PROBE', '%s fresh probe field genotype != parent_genotype^applied_mask' % loc)
                        else:
                            check_shared_mask(shared, (t, 'fresh', par), amask, loc, 'fresh', rec)
                    elif want_ps == 1:
                        if geno != pcache or amask != tdisp:
                            rec.fail('PROBE', '%s true-cache probe fields genotype/applied_mask mismatch' % loc)
                    else:
                        if amask != apply_perm(tdisp, perm) or geno != pg ^ amask or pdist != dw:
                            rec.fail('PROBE', '%s decoy probe fields applied_mask/genotype/parent_distance differ from '
                                     'the shared permutation of true_displacement' % loc)
                    if valid_m:
                        vmc += 1
                        vw0 += 1 if dw == 0 else 0
                        vw32 += 1 if dw == 32 else 0
                    if ps in (1, 2):
                        cpu += 1
                    if ps == 1:
                        tcu += 1
                    elif ps == 2:
                        du += 1
                        dw0 += 1 if dw == 0 else 0
                        dw32 += 1 if dw == 32 else 0
                        ident += 1 if geno == pcache else 0
                elif fam == 2:
                    if ps != NA or geno != pg ^ amask:
                        rec.fail('OPERATOR', '%s scout fields probe_source/genotype/applied_mask' % loc)
                    else:
                        check_shared_mask(shared, (t, 'scout', par), amask, loc, 'scout', rec)
                elif ps != NA:
                    rec.fail('NOT_APPLICABLE', '%s field probe_source not 255' % loc)
                if fam == 3:
                    if donor > 2:
                        rec.fail('DONOR', '%s field donor_family out of range' % loc)
                    elif g[32 * donor + par][11] != min(g[par][11], g[32 + par][11], g[64 + par][11]):
                        rec.fail('DONOR', '%s field donor_family is not a lowest-mismatch family' % loc)
                    elif geno != g[32 * donor + par][9] ^ amask:
                        rec.fail('OPERATOR', '%s local child genotype != donor genotype^applied_mask' % loc)
                    else:
                        check_shared_mask(shared, (t, 'local', par), amask, loc, 'local', rec)
                elif donor != NA:
                    rec.fail('NOT_APPLICABLE', '%s field donor_family not 255' % loc)
                if rank == NA:
                    if post != NA or flip != NA or w_d or x or z or retry:
                        rec.fail('NOT_APPLICABLE', '%s unselected but selection fields are set' % loc)
                else:
                    if rank >= SLOTS or order[rank] is not None:
                        ok_sel = False
                        rec.fail('SELECTION', '%s field selected_rank invalid or duplicate' % loc)
                    else:
                        order[rank] = j
                    if flip > 1 or plab > 1 or post != (plab ^ flip):
                        rec.fail('MUTATION', '%s field post_label != parent_label_pre xor policy_flip' % loc)
                    s_mis += mis
                    s_m += 1 if post == 1 else 0
                    fm += 1 if (plab == 0 and post == 1) else 0
                    mf += 1 if (plab == 1 and post == 0) else 0
                    if fam == 1 and ps in (1, 2):
                        cps += 1
                        tcs += 1 if ps == 1 else 0
                        ds += 1 if ps == 2 else 0
                    s_retry += retry
            if any(o is None for o in order):
                ok_sel = False
                rec.fail('SELECTION', '%s fewer than 32 distinct survivor ranks' % where)
            if not ok_sel:
                rec.fatal('SELECTION', '%s survivor order cannot be reconstructed' % where)
            verify_selection(g, order, weights, where, rec)
            u = upd[c * UPDATES + t - 1]
            derived = (s_m, s_mis, vmc, cpu, cps, fm, mf, CANDIDATES, s_retry, rapp, tcu, du, tcs, ds, vw0, vw32,
                       dw0, dw32, ident)
            for i, value in enumerate(derived):
                if u[U_M + i] != value:
                    rec.fail('AUDIT_VS_UPDATE', '%s update record field %s inconsistent with audit rows'
                             % (where, UPDATE_FIELDS[U_M + i]))
                    break
            new_geno = [g[order[s]][9] for s in range(SLOTS)]
            new_label = [g[order[s]][17] for s in range(SLOTS)]
            p_cache = [fam0[order[s] & 31] if new_label[s] == 1 else 0 for s in range(SLOTS)]
            p_cv = [1 if new_label[s] == 1 else 0 for s in range(SLOTS)]
            p_label = new_label
            p_geno = new_geno
            mask = 0
            for s in range(SLOTS):
                if new_label[s] == 1:
                    mask |= 1 << s
            if start == 1:
                mask ^= MASK32
            paired.update(struct.pack('<HIBBH', t, target, rbit, rapp, s_mis))
            paired.update(bytes(order))
            paired.update(struct.pack('<32I', *new_geno))
            paired.update(struct.pack('<I', mask))
            ctx.bump('audit_rows', CANDIDATES)
        if paired.digest() != pth[c][P_PAIRED]:
            rec.fail('PAIRED_HASH', 'block %d cell %d field paired_trajectory_sha256 differs from audit-row '
                     'reconstruction' % (bid, c))
        final = hashlib.sha256()
        final.update(b'PCONV-FINAL-STATE-V1')
        final.update(struct.pack('<IBI', bid, c, UPDATES))
        for s in range(SLOTS):
            final.update(struct.pack('<IBBI', p_geno[s], p_label[s] & 0xFF, p_cv[s], p_cache[s]))
        final.update(struct.pack('<II', targets[UPDATES], targets[UPDATES - 1]))
        if final.digest() != pth[c][P_FINAL_HASH]:
            rec.fail('FINAL_HASH', 'block %d cell %d field final_state_sha256 differs from audit-row reconstruction'
                     % (bid, c))
        ctx.bump('audit_paths_hashes_reconstructed')
    # HALF law (section 4): a recurrent update copies the target of two updates earlier.
    for t in range(3, UPDATES + 1):
        if applied[t] == 1 and targets[t] != targets[t - 2]:
            rec.fail('TARGET_LAW', 'block %d update %d HALF target violates the lag-two law' % (bid, t))


# ------------------------------------------------------------------------------------------------
# Estimates (19 records) and decisions


def hp_context():
    return decimal.Context(prec=HP_PREC, rounding=decimal.ROUND_HALF_EVEN)


def to_decimal(f):
    with decimal.localcontext(hp_context()):
        return Decimal(f.numerator) / Decimal(f.denominator)


def half_width(r, two_over_alpha, n):
    """h = R * sqrt(ln(2/alpha_each) / (2n)) at 110 digits (frozen_config inference.method)."""
    with decimal.localcontext(hp_context()):
        return Decimal(r) * (Decimal(two_over_alpha).ln() / (Decimal(2) * Decimal(n))).sqrt()


def round10(d):
    return d.quantize(Decimal('1E-10'), rounding=decimal.ROUND_HALF_EVEN)


def expected_estimates(acc, n):
    """The 19 records in OUTPUT_FORMATS.md order: 1-3 primary, 4-5 allele, 6-7 performance, 8-13 late M
    frequency per cell, 14-19 late accuracy per cell."""
    out = []
    for record, name, family, r, den, two_over_alpha, frozen, idx in ESTIMANDS:
        f = Fraction(acc['nums'][idx], den * n)
        est = to_decimal(f)
        h = half_width(r, two_over_alpha, n)
        with decimal.localcontext(hp_context()):
            lower, upper = est - h, est + h
        out.append({'record': record, 'name': name, 'family': family, 'kind': 'inferential', 'fraction': f,
                    'estimate': est, 'half_width': h, 'lower': lower, 'upper': upper, 'frozen': frozen})
    for c in range(CELLS):
        f = Fraction(acc['L'][c], 2048 * n)
        out.append({'record': 8 + c, 'name': 'M_FREQUENCY_LATE|' + CELL_LABELS[c], 'kind': 'cell', 'fraction': f,
                    'estimate': to_decimal(f)})
    for c in range(CELLS):
        f = 1 - Fraction(acc['P'][c], 65536 * n)
        out.append({'record': 14 + c, 'name': 'ACCURACY_LATE|' + CELL_LABELS[c], 'kind': 'cell', 'fraction': f,
                    'estimate': to_decimal(f)})
    return out


def primary_rule(bounds):
    """frozen_config decision_rules_in_order, rules 2-6 (rule 1 is the INVALID route)."""
    for name in ('D_INFO', 'D_NONINFO'):
        lo, up = bounds[name]
        if not (lo > -DELTA and up < DELTA):
            return 2
    lo, up = bounds['Delta_P']
    if lo > DELTA:
        return 3
    if up < -DELTA:
        return 4
    if up <= DELTA:
        return 5
    return 6


def secondary_class(lo, up):
    if lo > DELTA:
        return SECONDARY_LABELS[0]
    if up < -DELTA:
        return SECONDARY_LABELS[1]
    if up <= DELTA:
        return SECONDARY_LABELS[2]
    return SECONDARY_LABELS[3]


def interpretive_branch(rule, bounds):
    if rule == 3:
        return BRANCH_STATEMENTS[0] if bounds['B_INFO'][0] > DELTA else BRANCH_STATEMENTS[1]
    if rule in (4, 5):
        return BRANCH_STATEMENTS[2] if bounds['E_INFO'][0] > DELTA else BRANCH_STATEMENTS[3]
    return BRANCH_STATEMENTS[4]


def derive_decisions(exp):
    bounds = dict((e['name'], (e['lower'], e['upper'])) for e in exp if e['kind'] == 'inferential')
    rule = primary_rule(bounds)
    lo, up = bounds['Delta_P']
    return {'rule': rule, 'primary': PRIMARY_LABELS[rule], 'wholly_inside': bool(lo > -DELTA and up < DELTA),
            'secondary': dict((name, secondary_class(*bounds[name])) for name in SECONDARY_NAMES),
            'branch': interpretive_branch(rule, bounds)}


def is_int(v):
    return isinstance(v, int) and not isinstance(v, bool)


def same(v, want):
    """Exact JSON equality including type (so true != 1 and "2" != 2)."""
    return type(v) is type(want) and v == want


FRACTION_TEXT = re.compile(r'(-?[0-9]+)/([0-9]+)')
DECIMAL_TEXT = re.compile(r'-?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?')
SHA256_LOWER = re.compile(r'[0-9a-f]{64}')


def exact_fraction(v):
    """A JSON string 'p/q' (q > 0) as a Fraction; None otherwise. Compared by exact value."""
    if not isinstance(v, str):
        return None
    m = FRACTION_TEXT.fullmatch(v)
    if m is None or int(m.group(2)) == 0:
        return None
    return Fraction(int(m.group(1)), int(m.group(2)))


def numeric_field(v):
    """A finite decimal given as a JSON string numeral or a JSON number; None otherwise."""
    if isinstance(v, str) and DECIMAL_TEXT.fullmatch(v):
        return Decimal(v)
    if isinstance(v, Decimal) and v.is_finite():
        return v
    if is_int(v):
        return Decimal(v)
    return None


def close(a, b):
    with decimal.localcontext(hp_context()):
        return abs(a - b) <= DECIMAL_TOLERANCE


def check_estimate_record(i, record, exp, derived, rec):
    """Record i of estimates_19.json, read only at the exact provisional keys. Diagnostics name only the
    record number and field; no observed or recomputed value is ever written."""
    tag = '%s record %d' % (ESTIMATES_FILE, i + 1)
    if not isinstance(record, dict):
        rec.fail('ANALYSIS_SCHEMA', '%s is not an object' % tag)
        return
    keys = KEYS_CELL if exp['kind'] == 'cell' else KEYS_INFERENTIAL + (
        (KEY_PRIMARY_DECISION,) if exp['family'] == 'PRIMARY' else (KEY_SECONDARY_CLASS,))
    missing = [k for k in keys if k not in record]
    if missing:
        rec.fail('ANALYSIS_SCHEMA', '%s required keys missing: %s' % (tag, ', '.join(missing)))
        return

    def mismatch(category, field):
        rec.fail(category, '%s field %s mismatch' % (tag, field))

    if not same(record['record'], exp['record']):
        mismatch('ANALYSIS_ORDER', 'record')
    if not same(record['name'], exp['name']):
        mismatch('ANALYSIS_ORDER', 'name')
    exact = exact_fraction(record['estimate_exact'])
    if exact is None:
        rec.fail('ANALYSIS_SCHEMA', '%s field estimate_exact is not a numerator/denominator string' % tag)
    elif exact != exp['fraction']:
        mismatch('ESTIMATE', 'estimate_exact')
    fields = [('estimate', 'ESTIMATE_DISPLAY')]
    if exp['kind'] != 'cell':
        fields += [('half_width', 'BOUND'), ('lower', 'BOUND'), ('upper', 'BOUND')]
    for field, category in fields:
        value = numeric_field(record[field])
        if value is None:
            rec.fail('ANALYSIS_SCHEMA', '%s field %s is not a finite decimal' % (tag, field))
        elif not close(value, exp[field]):
            mismatch(category, field)
    if exp['kind'] == 'cell':
        for field in ('lower', 'upper', 'half_width'):
            if field in record and record[field] is not None:
                mismatch('BOUND', field)
        return
    if exp['family'] == 'PRIMARY':
        if not same(record[KEY_PRIMARY_DECISION], derived['primary']):
            mismatch('DECISION', KEY_PRIMARY_DECISION)
    elif not same(record[KEY_SECONDARY_CLASS], derived['secondary'][exp['name']]):
        mismatch('DECISION', KEY_SECONDARY_CLASS)


def check_decision(decision, derived, rec):
    """decision.json, read only at the exact provisional keys. Diagnostics name only the field."""
    if not isinstance(decision, dict):
        rec.fail('ANALYSIS_SCHEMA', '%s root is not an object' % DECISION_FILE)
        return
    missing = [k for k in DECISION_KEYS if k not in decision]
    if missing:
        rec.fail('ANALYSIS_SCHEMA', '%s required keys missing: %s' % (DECISION_FILE, ', '.join(missing)))
        return

    def mismatch(field):
        rec.fail('DECISION', '%s field %s mismatch' % (DECISION_FILE, field))

    rule = derived['rule']
    if not same(decision['decision_rule_applied'], rule):
        mismatch('decision_rule_applied')
    if not same(decision['primary_decision'], PRIMARY_LABELS[rule]):
        mismatch('primary_decision')
    flag = decision['bounded_interval_wholly_inside_minus_delta_plus_delta']
    if rule == 5:
        if not same(flag, derived['wholly_inside']):
            mismatch('bounded_interval_wholly_inside_minus_delta_plus_delta')
    elif not (flag is None or same(flag, derived['wholly_inside'])):
        mismatch('bounded_interval_wholly_inside_minus_delta_plus_delta')
    sec = decision['secondary_classifications']
    if not (isinstance(sec, dict) and sorted(sec) == sorted(SECONDARY_NAMES)):
        rec.fail('ANALYSIS_SCHEMA', '%s field secondary_classifications key set differs from '
                 'E_INFO/E_NONINFO/B_INFO/B_NONINFO' % DECISION_FILE)
    else:
        for name in SECONDARY_NAMES:
            if not same(sec[name], derived['secondary'][name]):
                mismatch('secondary_classifications.' + name)
    if not same(decision['interpretive_branch'], derived['branch']):
        mismatch('interpretive_branch')
    labels = decision['interpretation_labels']
    if not (isinstance(labels, list) and labels and all(isinstance(x, str) and x for x in labels)):
        rec.fail('ANALYSIS_SCHEMA', '%s field interpretation_labels is not a non-empty array of strings'
                 % DECISION_FILE)


def load_json_input(path, label, rec, ctx, category):
    reader = ctx.open(path, label, rec)
    data = reader.read_all()
    reader.finish()

    def pairs_hook(pairs):
        out = {}
        for k, v in pairs:
            if k in out:
                raise ValueError('duplicate key')
            out[k] = v
        return out

    def reject_constant(name):
        raise ValueError('non-finite constant')

    try:
        return json.loads(data.decode('utf-8'), object_pairs_hook=pairs_hook, parse_float=Decimal,
                          parse_constant=reject_constant)
    except (ValueError, UnicodeDecodeError) as exc:
        # The exception type only: a decoder message can quote input text.
        rec.fatal(category, '%s is not strict JSON (%s)' % (label, type(exc).__name__))


def verify_analysis(adir, acc, n, rec, ctx, frozen):
    if not os.path.isdir(adir):
        rec.fatal('MISSING_INPUT', 'analysis directory does not exist')
    closed = os.path.join(adir, CLOSED_FILE)
    if os.path.lexists(closed):
        ctx.open(closed, 'analysis/' + CLOSED_FILE, rec).finish()
        rec.fatal('ANALYSIS_CLOSED_INVALID', 'analysis wrote %s; no estimate exists to verify' % CLOSED_FILE)
    est = load_json_input(os.path.join(adir, ESTIMATES_FILE), 'analysis/' + ESTIMATES_FILE, rec, ctx,
                          'ANALYSIS_SCHEMA')
    decision = load_json_input(os.path.join(adir, DECISION_FILE), 'analysis/' + DECISION_FILE, rec, ctx,
                               'ANALYSIS_SCHEMA')
    exp = expected_estimates(acc, n)
    if frozen:
        for e in exp[:7]:
            if round10(e['half_width']) != Decimal(e['frozen']):
                rec.fatal('CONSTANT', 'half_width for %s does not round to its frozen value' % e['name'])
    derived = derive_decisions(exp)
    if not (isinstance(est, list) and len(est) == 19):
        rec.fatal('ANALYSIS_SCHEMA', '%s root is not an array of 19 records' % ESTIMATES_FILE)
    for i, record in enumerate(est):
        check_estimate_record(i, record, exp[i], derived, rec)
        ctx.bump('estimates_compared')
    check_decision(decision, derived, rec)
    ctx.bump('decision_files_compared')


# ------------------------------------------------------------------------------------------------
# Manifest hashes (provisional key names; the outputs list must cover exactly the 98 record files)


def verify_manifest(path, expected_files, rec, ctx):
    manifest = load_json_input(path, MANIFEST_FILE, rec, ctx, 'MANIFEST_SCHEMA')
    if not isinstance(manifest, dict):
        rec.fatal('MANIFEST_SCHEMA', '%s root is not an object' % MANIFEST_FILE)
    for key, want in MANIFEST_EXACT:
        if key not in manifest:
            rec.fail('MANIFEST_SCHEMA', '%s field %s missing' % (MANIFEST_FILE, key))
        elif not same(manifest[key], want):
            rec.fail('MANIFEST_FIELD', '%s field %s differs from the frozen value' % (MANIFEST_FILE, key))
    outputs = manifest.get('outputs')
    if not isinstance(outputs, list):
        rec.fatal('MANIFEST_SCHEMA', '%s field outputs is not an array' % MANIFEST_FILE)
    if len(outputs) != len(expected_files):
        rec.fail('MANIFEST_SCHEMA', '%s field outputs entry count differs from the record file set' % MANIFEST_FILE)
    seen = set()
    for i, entry in enumerate(outputs):
        # Entry index and relative path are coordinates; a bytes or sha256 value is never written.
        where = '%s outputs entry %d' % (MANIFEST_FILE, i)
        if not (isinstance(entry, dict) and sorted(entry) == sorted(MANIFEST_OUTPUT_KEYS)):
            rec.fail('MANIFEST_SCHEMA', '%s key set differs from path/bytes/sha256' % where)
            continue
        rel, nbytes, digest = entry['path'], entry['bytes'], entry['sha256']
        if not isinstance(rel, str):
            rec.fail('MANIFEST_SCHEMA', '%s field path is not a string' % where)
            continue
        if not (is_int(nbytes) and nbytes >= 0):
            rec.fail('MANIFEST_SCHEMA', '%s field bytes is not a nonnegative integer' % where)
            continue
        if not (isinstance(digest, str) and SHA256_LOWER.fullmatch(digest)):
            rec.fail('MANIFEST_SCHEMA', '%s field sha256 is not a lowercase SHA-256' % where)
            continue
        if rel not in expected_files:
            rec.fail('MANIFEST', '%s field path names an undocumented file' % where)
            continue
        if rel in seen:
            rec.fail('MANIFEST', '%s field path repeats %s' % (where, rel))
            continue
        seen.add(rel)
        reader = ctx.by_label.get(rel)
        if reader is None or reader.digest is None:
            rec.fail('MANIFEST', '%s field path names %s, which was not read' % (where, rel))
            continue
        ok = True
        if nbytes != reader.nbytes:
            ok = False
            rec.fail('MANIFEST_SIZE', '%s field bytes mismatch for %s' % (where, rel))
        if digest != reader.digest:
            ok = False
            rec.fail('MANIFEST_HASH', '%s field sha256 mismatch for %s' % (where, rel))
        if ok:
            ctx.bump('manifest_files_matched')
    for rel in sorted(expected_files - seen):
        rec.fail('MANIFEST', '%s outputs has no entry for %s' % (MANIFEST_FILE, rel))


# ------------------------------------------------------------------------------------------------
# Modes


def listing(path, rec, what):
    try:
        return sorted(os.listdir(path))
    except OSError as exc:
        rec.fatal('MISSING_INPUT', '%s %s: %s' % (what, path, exc.strerror))


def regular_size(path, rec, what):
    try:
        st = os.stat(path)
    except OSError as exc:
        rec.fatal('MISSING_INPUT', '%s %s: %s' % (what, path, exc.strerror))
    if not os.path.isfile(path):
        rec.fatal('LAYOUT', '%s %s is not a regular file' % (what, path))
    return st.st_size


def run_layout(run_dir, rec):
    """The exact 98-file record layout plus run_manifest.json; every record file at its documented size."""
    shards = os.path.join(run_dir, 'shards')
    names = listing(shards, rec, 'shards directory')
    wanted = ['shard_%02d' % s for s in range(N_SHARDS)]
    if names != wanted:
        rec.fatal('LAYOUT', 'shards/ must contain exactly shard_00..shard_31')
    expected_files = set()
    for s in range(N_SHARDS):
        sdir = os.path.join(shards, wanted[s])
        files = dict(SHARD_SIZES)
        if s == 0:
            files.update(SHARD_00_EXTRA)
        present = listing(sdir, rec, 'shard directory')
        if present != sorted(files):
            rec.fatal('LAYOUT', '%s must contain exactly %s' % (wanted[s], ', '.join(sorted(files))))
        for name, size in files.items():
            if regular_size(os.path.join(sdir, name), rec, 'shard file') != size:
                rec.fatal('SIZE', '%s/%s size differs from its documented record size' % (wanted[s], name))
            expected_files.add('shards/%s/%s' % (wanted[s], name))
    manifest_path = os.path.join(run_dir, MANIFEST_FILE)
    regular_size(manifest_path, rec, 'run manifest')
    return shards, wanted, expected_files, manifest_path


def run_production(args, rec, ctx):
    n_updates = N_BLOCKS * CELLS * UPDATES
    ctx.expected = {'update_records': n_updates, 'path_updates_with_128_queries': n_updates,
                    'path_records': N_BLOCKS * CELLS, 'block_records': N_BLOCKS,
                    'blocks_with_both_sham_identities': N_BLOCKS, 'blocks_with_c1_record_consistency': N_BLOCKS,
                    'audit_rows': AUDIT_BLOCKS * CELLS * UPDATES * CANDIDATES,
                    'permutation_records': AUDIT_BLOCKS * UPDATES,
                    'audit_paths_hashes_reconstructed': AUDIT_BLOCKS * CELLS,
                    'manifest_files_matched': N_SHARDS * 3 + 2, 'estimates_compared': 19,
                    'decision_files_compared': 1}
    shards, wanted, expected_files, manifest_path = run_layout(args.run_dir, rec)

    acc = new_acc()
    for s in range(N_SHARDS):
        sdir = os.path.join(shards, wanted[s])
        prefix = 'shards/%s/' % wanted[s]
        ur = ctx.open(os.path.join(sdir, 'updates.bin'), prefix + 'updates.bin', rec)
        pr = ctx.open(os.path.join(sdir, 'paths.bin'), prefix + 'paths.bin', rec)
        br = ctx.open(os.path.join(sdir, 'blocks.bin'), prefix + 'blocks.bin', rec)
        ar = ctx.open(os.path.join(sdir, AUDIT_ROWS_FILE), prefix + AUDIT_ROWS_FILE, rec) if s == 0 else None
        mr = ctx.open(os.path.join(sdir, AUDIT_PERM_FILE), prefix + AUDIT_PERM_FILE, rec) if s == 0 else None
        for local in range(BLOCKS_PER_SHARD):
            bid = s * BLOCKS_PER_SHARD + local
            ub = ur.read_exact(UPD_PER_BLOCK, rec, 'update records of block %d' % bid)
            pb = pr.read_exact(PTH_PER_BLOCK, rec, 'path records of block %d' % bid)
            bb = br.read_exact(BLK.size, rec, 'block record %d' % bid)
            ab = mb = None
            if bid < AUDIT_BLOCKS:
                ab = ar.read_exact(AUD_PER_BLOCK, rec, 'audit rows of block %d' % bid)
                mb = mr.read_exact(PRM_PER_BLOCK, rec, 'permutation records of block %d' % bid)
            verify_block(bid, ub, pb, bb, ab, mb, rec, acc, ctx)
            if bid == AUDIT_BLOCKS - 1 and ar is not None:
                for reader in (ar, mr):
                    reader.expect_eof(rec)
                    reader.finish()
        for reader in (ur, pr, br):
            reader.expect_eof(rec)
            reader.finish()
    verify_manifest(manifest_path, expected_files, rec, ctx)
    if rec.count == 0:
        verify_analysis(args.analysis_dir, acc, N_BLOCKS, rec, ctx, frozen=True)


def run_fixture(args, rec, ctx):
    ctx.expected = {'update_records': CELLS * UPDATES, 'path_updates_with_128_queries': CELLS * UPDATES,
                    'path_records': CELLS, 'block_records': 1, 'blocks_with_both_sham_identities': 1,
                    'blocks_with_c1_record_consistency': 1, 'audit_rows': CELLS * UPDATES * CANDIDATES,
                    'permutation_records': UPDATES, 'audit_paths_hashes_reconstructed': CELLS}
    d = args.layout_dir
    present = listing(d, rec, 'fixture layout directory')
    if present != sorted(FIXTURE_FILES):
        rec.fatal('LAYOUT', 'fixture layout must contain exactly %s' % ', '.join(sorted(FIXTURE_FILES)))
    for name, size in FIXTURE_FILES.items():
        actual = regular_size(os.path.join(d, name), rec, 'fixture file')
        if size is not None and actual != size:
            rec.fatal('SIZE', '%s size differs from its documented record size' % name)
    ctx.open(os.path.join(d, FIXTURE_README), FIXTURE_README, rec).finish()
    br = ctx.open(os.path.join(d, FIXTURE_BLOCK), FIXTURE_BLOCK, rec)
    bb = br.read_exact(BLK.size, rec, 'block record')
    br.expect_eof(rec)
    bid = struct.unpack_from('<I', bb)[0]
    if bid >= N_BLOCKS:
        rec.fatal('RANGE', 'fixture block id outside the declared block range')
    ur = ctx.open(os.path.join(d, FIXTURE_UPDATES), FIXTURE_UPDATES, rec)
    pr = ctx.open(os.path.join(d, FIXTURE_PATHS), FIXTURE_PATHS, rec)
    ar = ctx.open(os.path.join(d, FIXTURE_AUDIT), FIXTURE_AUDIT, rec)
    mr = ctx.open(os.path.join(d, FIXTURE_PERM), FIXTURE_PERM, rec)
    ub = ur.read_exact(UPD_PER_BLOCK, rec, 'update records')
    pb = pr.read_exact(PTH_PER_BLOCK, rec, 'path records')
    ab = ar.read_exact(AUD_PER_BLOCK, rec, 'audit rows')
    mb = mr.read_exact(PRM_PER_BLOCK, rec, 'permutation records')
    for reader in (ur, pr, ar, mr):
        reader.expect_eof(rec)
    verify_block(bid, ub, pb, bb, ab, mb, rec, new_acc(), ctx)


def run_synthetic(args, rec, ctx):
    n = args.n_blocks
    ctx.expected = {'block_records': n, 'estimates_compared': 19, 'decision_files_compared': 1}
    size = regular_size(args.blocks_file, rec, 'synthetic blocks file')
    if size != n * BLK.size:
        rec.fatal('SIZE', 'synthetic blocks file size differs from n block records')
    br = ctx.open(args.blocks_file, 'synthetic_blocks.bin', rec)
    acc = new_acc()
    for bid in range(n):
        blk = BLK.unpack(br.read_exact(BLK.size, rec, 'block record %d' % bid))
        L = list(blk[12:18])
        P = list(blk[18:24])
        nums = check_block_record(bid, blk, L, P, None, rec)
        accumulate(acc, nums, L, P)
        ctx.bump('block_records')
    br.expect_eof(rec)
    if rec.count == 0:
        verify_analysis(args.analysis_dir, acc, n, rec, ctx, frozen=False)


# ------------------------------------------------------------------------------------------------


def self_checks():
    out = []

    def add(name, ok):
        out.append({'name': name, 'result': 'PASS' if ok else 'FAIL'})

    add('sha256_fips180_abc', hashlib.sha256(b'abc').hexdigest() ==
        'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad')
    add('record_struct_sizes', UPD.size == 48 and PTH.size == 176 and BLK.size == 96 and AUD.size == 88 and
        PRM.size == 416)
    add('total_record_bytes', N_BLOCKS * CELLS * UPDATES * UPD.size + N_BLOCKS * CELLS * PTH.size +
        N_BLOCKS * BLK.size + AUDIT_BLOCKS * CELLS * UPDATES * CANDIDATES * AUD.size +
        AUDIT_BLOCKS * UPDATES * PRM.size == 4229120000)
    add('popcount', pop32(0) == 0 and pop32(MASK32) == 32 and pop32(0x80000001) == 2)
    add('lemire_thresholds', (TWO64 - 3) % 3 == 1 and (TWO64 - (1 << 39)) % (1 << 39) == 0 and
        (TWO64 - 31) % 31 == 16 and (TWO64 - 30) % 30 == 16 and (TWO64 - 7) % 7 == 2 and (TWO64 - 32) % 32 == 0)
    add('fnv1a32_published_vectors', fnv1a32(b'') == 0x811C9DC5 and fnv1a32(b'a') == 0xE40C292C and
        fnv1a32(b'foobar') == 0xBF9CF968)
    identity, _ = fisher_yates_replay([MASK64] * PERM_STEPS)
    rotation, _ = fisher_yates_replay([1] * PERM_STEPS)
    add('fisher_yates_replay_identity_rotation_and_orientation',
        identity == list(range(32)) and rotation == [(s + 1) % 32 for s in range(32)] and
        apply_perm(1, rotation) == 2 and apply_perm(1 << 31, rotation) == 1)
    rejected, step = fisher_yates_replay([MASK64] + [0] + [MASK64] * (PERM_STEPS - 2))
    add('fisher_yates_replay_detects_a_rejected_accepted_x', rejected is None and step == 1)
    add('two_over_alpha_exact', Fraction(2) / (Fraction(5, 100) / 3) == 120 and Fraction(2) / Fraction(25, 1000) == 80)
    for record, name, _family, r, _den, two_over_alpha, frozen, _idx in ESTIMANDS:
        add('frozen_half_width_%s' % name, round10(half_width(r, two_over_alpha, N_BLOCKS)) == Decimal(frozen))
    return out


def receipt_problem(receipt, input_paths):
    rp = os.path.abspath(receipt)
    if os.path.lexists(rp):
        return 'receipt already exists: %s' % rp
    parent = os.path.dirname(rp)
    if not os.path.isdir(parent):
        return 'receipt parent directory does not exist'
    rparent = os.path.realpath(parent)
    for p in input_paths:
        if not p:
            continue
        rd = os.path.realpath(p)
        if rparent == rd or rparent.startswith(rd.rstrip(os.sep) + os.sep):
            return 'receipt may not be written inside an input directory'
    return None


def write_exclusive(path, text):
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o444)
    with os.fdopen(fd, 'w', encoding='utf-8') as fh:
        fh.write(text)
        fh.flush()
        os.fsync(fh.fileno())


def main(argv=None):
    parser = argparse.ArgumentParser(description='Independent PCONV record/estimate verifier (fails closed).')
    sub = parser.add_subparsers(dest='command')
    p = sub.add_parser('production')
    p.add_argument('--run-dir', required=True)
    p.add_argument('--analysis-dir', required=True)
    p.add_argument('--receipt', required=True)
    f = sub.add_parser('fixture')
    f.add_argument('--layout-dir', required=True)
    f.add_argument('--receipt', required=True)
    s = sub.add_parser('synthetic-estimates')
    s.add_argument('--blocks-file', required=True)
    s.add_argument('--n-blocks', required=True, type=int)
    s.add_argument('--analysis-dir', required=True)
    s.add_argument('--receipt', required=True)
    args = parser.parse_args(argv)
    if not args.command:
        parser.print_usage(sys.stderr)
        return 2
    if args.command == 'synthetic-estimates' and not (1 <= args.n_blocks <= AUDIT_BLOCKS):
        sys.stderr.write('%s: synthetic --n-blocks must be 1..64 (nonscientific scale); no receipt written\n'
                         % TOOL_NAME)
        return 2

    if args.command == 'production':
        inputs = [args.run_dir, args.analysis_dir]
        namespace = 'production-r1'
    elif args.command == 'fixture':
        inputs = [args.layout_dir]
        namespace = 'fixture-r1-nonscientific'
    else:
        inputs = [args.analysis_dir, os.path.dirname(os.path.abspath(args.blocks_file))]
        namespace = 'none (NONSCIENTIFIC synthetic block records)'
    problem = receipt_problem(args.receipt, inputs)
    if problem:
        sys.stderr.write('%s: %s; no receipt written\n' % (TOOL_NAME, problem))
        return 2

    started = utc_now()
    rec = Recorder()
    ctx = Context()
    checks = self_checks()
    checks_ok = all(c['result'] == 'PASS' for c in checks)
    try:
        if not checks_ok:
            rec.fatal('SELF_CHECK', 'self-check failed: %s' % ', '.join(c['name'] for c in checks
                                                                         if c['result'] != 'PASS'))
        if args.command == 'production':
            run_production(args, rec, ctx)
        elif args.command == 'fixture':
            run_fixture(args, rec, ctx)
        else:
            run_synthetic(args, rec, ctx)
    except Fatal:
        pass
    except Exception as exc:  # fail closed on anything unexpected
        # Exception type only: an exception message could carry a value from the inputs.
        try:
            rec.fail('INTERNAL_ERROR', 'unexpected %s' % type(exc).__name__)
        except Fatal:
            pass
    for reader in ctx.readers:
        try:
            reader.finish()
        except Exception as exc:
            try:
                rec.fail('READ_ERROR', '%s: %s' % (reader.label, type(exc).__name__))
            except Fatal:
                pass

    counts_complete = all(ctx.counts.get(k, 0) == v for k, v in ctx.expected.items()) and bool(ctx.expected)
    passed = checks_ok and rec.count == 0 and not rec.capped and counts_complete
    if not passed and rec.count == 0:
        rec.first = {'category': 'INCOMPLETE', 'detail': 'checked counts differ from expected counts'}
        rec.count = 1
    try:
        with open(os.path.abspath(__file__), 'rb') as fh:
            own_sha = hashlib.sha256(fh.read()).hexdigest()
    except Exception:
        own_sha = None
    receipt = {
        'receipt_schema': RECEIPT_SCHEMA,
        'status': 'PASS' if passed else 'INVALID',
        'study_id': STUDY_ID,
        'tool': TOOL_NAME,
        'tool_version': TOOL_VERSION,
        'tool_source_sha256': own_sha,
        'python_version': sys.version.split()[0],
        'mode': args.command,
        'key_namespace_of_inputs': namespace,
        'random_draws_generated': False,
        'started_at_utc': started,
        'finished_at_utc': utc_now(),
        'inputs': [r.info() for r in ctx.readers],
        'self_checks': checks,
        'self_checks_pass': checks_ok,
        'checked': ctx.counts,
        'expected': ctx.expected,
        'first_discrepancy': rec.first,
        'mismatch_count': rec.count,
        'mismatch_count_cap': MISMATCH_CAP,
        'mismatch_count_capped': rec.capped,
        'is_scientific_result': False,
        'scope': ('NONSCIENTIFIC synthetic estimate-comparison exercise' if args.command == 'synthetic-estimates'
                  else 'record, hash and estimate agreement only; this receipt reports no estimate or decision'),
    }
    try:
        write_exclusive(args.receipt, json.dumps(receipt, indent=2, sort_keys=True) + '\n')
    except OSError as exc:
        sys.stderr.write('%s: could not create receipt %s: %s\n' % (TOOL_NAME, args.receipt, exc))
        return 3
    sys.stdout.write('%s %s mismatches=%d receipt=%s\n' % (TOOL_NAME, receipt['status'], rec.count, args.receipt))
    return 0 if passed else 1


if __name__ == '__main__':
    sys.exit(main())
