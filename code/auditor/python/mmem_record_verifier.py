#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Independent record and estimate verifier for PHASE2-MUTABLE-MEMORY-001 revision 1.

Python 3.6, standard library only. Written from the frozen specification, frozen_config.json,
OUTPUT_FORMATS.md, binary_records.json and PRODUCER_OUTPUT_INTERFACE_CONTRACT_R2.json (revision 2; see
docs/INPUT_CONTRACT.md). It does not implement or call the simulation, generates no random draw, and
imports no producer, analyzer or C++ auditor code. JSON inputs are read only at the exact paths fixed by
the interface contract; no key or string search is performed. Failure diagnostics name only a category,
coordinates, record index or manifest entry index/path, and a field name; they never carry an observed or
recomputed value.

Subcommands
  production           --run-dir DIR --analysis-dir DIR --receipt NEW.json
  fixture              --layout-dir DIR --receipt NEW.json
  synthetic-estimates  --blocks-file FILE --n-blocks N --analysis-dir DIR --receipt NEW.json
                       (NONSCIENTIFIC: exercises the estimate comparison on synthetic block records)
  manifest-interface   --run-dir DIR --receipt NEW.json
                       (NONSCIENTIFIC: manifest schema, entry uniqueness, byte counts and hashes over the 97
                       required relative paths holding tiny placeholder files; no record is parsed)

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

TOOL_NAME = 'mmem_record_verifier'
TOOL_VERSION = 'MMEM-AUDIT-RECORDS-1.2.0'
RECEIPT_SCHEMA = 'MMEM-AUDITOR-RECORDS-RECEIPT-1'

# Frozen design constants (specification sections 2, 3, 9, 11, 12; frozen_config.json).
N_BLOCKS = 41600
N_SHARDS = 32
BLOCKS_PER_SHARD = 1300
CELLS = 8
UPDATES = 256
LATE_FIRST = 193
SLOTS = 32
CANDIDATES = 128
AUDIT_BLOCKS = 64
NA = 255
MASK32 = 0xFFFFFFFF
MASK64 = (1 << 64) - 1
TWO64 = 1 << 64

# Fixed little-endian layouts (binary_records.json).
UPD = struct.Struct('<IHBBHBBBBBBQ')
PTH = struct.Struct('<IBBBBIIIIIIIQHHHH32sIII32s')
BLK = struct.Struct('<IBBBB6i8I8I')
AUD = struct.Struct('<IHBBBBBBIIIBBBBQBBBBQQQII')

UPD_PER_BLOCK = CELLS * UPDATES * UPD.size              # 49,152
PTH_PER_BLOCK = CELLS * PTH.size                        # 1,024
AUD_PER_BLOCK = CELLS * UPDATES * CANDIDATES * AUD.size  # 18,874,368
AUDIT_FILE = 'audit_rows_blocks_0000_0063.bin'
SHARD_SIZES = {
    'updates.bin': BLOCKS_PER_SHARD * UPD_PER_BLOCK,
    'paths.bin': BLOCKS_PER_SHARD * PTH_PER_BLOCK,
    'blocks.bin': BLOCKS_PER_SHARD * BLK.size,
}
FIXTURE_FILES = {
    'layout_sample_updates.bin': UPD_PER_BLOCK,
    'layout_sample_paths.bin': PTH_PER_BLOCK,
    'layout_sample_block.bin': BLK.size,
    'layout_sample_audit.bin': AUD_PER_BLOCK,
    'layout_sample_README.txt': None,
}

CELL_LABELS = ['ACTIVE|ZERO|ALL_F', 'ACTIVE|ZERO|ALL_M', 'ACTIVE|HALF|ALL_F', 'ACTIVE|HALF|ALL_M',
               'SHAM|ZERO|ALL_F', 'SHAM|ZERO|ALL_M', 'SHAM|HALF|ALL_F', 'SHAM|HALF|ALL_M']
PRIMARY_LABELS = {
    1: 'INVALID',
    2: 'START-DEPENDENT; SCIENTIFIC QUESTION UNRESOLVED',
    3: 'SUPPORTS RECURRENCE-ATTRIBUTABLE SELECTIVE ENRICHMENT',
    4: 'SELECTIVE ENRICHMENT NOT ATTRIBUTABLE TO RECURRENCE AT THE FIXED SCALE',
    5: 'BOUNDED NEGATIVE AT THE ONE-INDIVIDUAL SCALE',
    6: 'UNRESOLVED',
}
SECONDARY_LABELS = ['MEANINGFUL POSITIVE PERFORMANCE', 'MEANINGFUL ADVERSE PERFORMANCE',
                    'BOUNDED BELOW THE POSITIVE ONE-BIT SCALE', 'UNRESOLVED']

# (name, range length R, block-value denominator, frozen half-width, block-record numerator index)
PRIMARY = [('C_abs', 1, 4096, '0.00781023', 0), ('C_rec', 2, 4096, '0.01562046', 1),
           ('D_HALF', 2, 2048, '0.01562046', 2), ('D_ZERO', 2, 2048, '0.01562046', 3)]
SECONDARY = [('P_abs', 2, 131072, '0.01451463', 4), ('P_rec', 4, 131072, '0.02902925', 5)]
ALPHA_PRIMARY_TEXT = '0.0125'
ALPHA_SECONDARY_TEXT = '0.025'
ALPHA_PRIMARY = Decimal(ALPHA_PRIMARY_TEXT)
ALPHA_SECONDARY = Decimal(ALPHA_SECONDARY_TEXT)
DELTA = Decimal(1) / Decimal(32)

# Exact producer-output interface (PRODUCER_OUTPUT_INTERFACE_CONTRACT_R2.json, revision 2, frozen before
# production; supersedes revision 1).
PRODUCER_DECIMAL_PREC = 60
FAMILY_PRIMARY = 'PRIMARY_ALLELE_ENRICHMENT'
FAMILY_SECONDARY = 'SECONDARY_POPULATION_PERFORMANCE'
FAMILY_CELL = 'ABSOLUTE_CELL_MEAN_DESCRIPTIVE'
CELL_CLASSIFICATION = 'DESCRIPTIVE; NO INFERENTIAL BOUND FROZEN'
KEYS_PRIMARY = ('record', 'name', 'family', 'estimate_exact', 'estimate', 'range_length', 'alpha_each', 'n_blocks',
                'half_width', 'lower', 'upper', 'family_decision')
KEYS_SECONDARY = ('record', 'name', 'family', 'estimate_exact', 'estimate', 'range_length', 'alpha_each',
                  'n_blocks', 'half_width', 'lower', 'upper', 'classification',
                  'relation_to_allele_enrichment_descriptive', 'cannot_alter_primary_decision')
KEYS_CELL = ('record', 'name', 'family', 'estimate_exact', 'estimate', 'lower', 'upper', 'classification')
DECISION_KEYS = ('primary_decision', 'decision_rule_applied', 'adverse_selection_upper_C_abs_below_minus_Delta',
                 'secondary_classifications', 'interpretation_labels')
# IFACE-R2 interpretation_labels_exact: compared for exact identity and order, never searched.
INTERPRETATION_LABELS = (
    'HALF CACHE ALIGNMENT: the HALF law is a prior outcome-informed choice that exactly matches the '
    'one-generation cache delay to a two-update target return; it favors retrieval by construction. No other '
    'lag, copy probability or target law was tested.',
    'OPPORTUNITY COST: a valid ACTIVE-M carrier replaces exactly one uniform fresh proposal (one of two '
    'effectively uniform random proposals) with its cache; its magnitude is conditional on the frozen '
    'fresh-probe distribution and is not a general cost scale.',
    'START-DEPENDENCE GATE: rule 2 (D_HALF and D_ZERO wholly inside (-1/32, +1/32)) is applied before any '
    'enrichment classification.',
    "ADVERSE OUTCOMES: under BOUNDED NEGATIVE, an upper C_abs bound below -1/32 indicates adverse selection; "
    "otherwise only 'not enriched by one expected individual' is admissible.",
    'SECONDARY POPULATION-PERFORMANCE LIMITS: P_abs and P_rec form a separate family that cannot alter the '
    'primary decision; P_rec half-width 0.029 is close to Delta_P = 1/32 and is often unresolved.',
    'CLAIM LIMITS: no claim of rare invasion, mechanism origin, arbitrary recurrence, general cost scale, '
    'stationarity, equilibrium, evolutionary stability, population benefit, biological extrapolation, global '
    'novelty or replication of studies 056-060.',
)
# IFACE-R2 family_and_relation_rules: primary labels that make a P relation positive / negative.
RELATION_POS = {'P_abs': (PRIMARY_LABELS[3], PRIMARY_LABELS[4]), 'P_rec': (PRIMARY_LABELS[3],)}
RELATION_NEG = {'P_abs': (PRIMARY_LABELS[5],), 'P_rec': (PRIMARY_LABELS[4],)}
MANIFEST_ROOT_KEYS = ('manifest', 'study_id', 'status', 'config_path', 'config_sha256', 'specification_sha256',
                      'terminal_review_sha256', 'key_namespace', 'compiler_version', 'cplusplus', 'threads',
                      'shards', 'blocks', 'started_utc', 'finished_utc', 'wall_seconds', 'total_objective_queries',
                      'expected_total_objective_queries', 'n1_failed_blocks', 'n2_failed_blocks',
                      'query_failed_blocks', 'total_output_bytes', 'regeneration_command', 'outputs')
# manifest-interface mode: placeholder files only; anything larger cannot be a placeholder.
MANIFEST_INTERFACE_MAX_BYTES = 4096
MANIFEST_EXACT = [('manifest', 'MMEM-RUN-MANIFEST-1'), ('study_id', 'PHASE2-MUTABLE-MEMORY-001'),
                  ('status', 'COMPLETE'), ('key_namespace', 'production-r1'), ('shards', 32), ('blocks', 41600),
                  ('total_objective_queries', 10905190400), ('expected_total_objective_queries', 10905190400),
                  ('n1_failed_blocks', 0), ('n2_failed_blocks', 0), ('query_failed_blocks', 0)]
MANIFEST_TYPED = [('config_path', 'string'), ('config_sha256', 'lowercase_sha256'),
                  ('specification_sha256', 'lowercase_sha256'), ('terminal_review_sha256', 'lowercase_sha256'),
                  ('compiler_version', 'string'), ('cplusplus', 'integer'), ('threads', 'integer_1_to_32'),
                  ('started_utc', 'string'), ('finished_utc', 'string'), ('wall_seconds', 'number_nonnegative'),
                  ('total_output_bytes', 'integer_nonnegative'), ('regeneration_command', 'string'),
                  ('outputs', 'array')]
MANIFEST_OUTPUT_KEYS = ('path', 'bytes', 'sha256')

PATH_DERIVED = ['block', 'cell', 'arm', 'law', 'start', 'late_m_sum', 'late_mismatch_sum', 'total_queries',
                'f_to_m', 'm_to_f', 'cache_probe_use', 'cache_probe_survivors', 'survival_retries_u64',
                'fixation_updates', 'extinction_updates', 'late_fixation_updates', 'late_extinction_updates']
BLOCK_FIELDS = (['block', 'n1_ok', 'n2_ok', 'query_ok', 'audit_block', 'c_abs_num_over_4096',
                 'c_rec_num_over_4096', 'd_half_num_over_2048', 'd_zero_num_over_2048',
                 'p_abs_num_over_131072', 'p_rec_num_over_131072'] +
                ['late_m_sum[%d]' % c for c in range(CELLS)] + ['late_mismatch_sum[%d]' % c for c in range(CELLS)])
UPDATE_DERIVED = ['m_count', 'total_mismatch', 'valid_m_cache', 'cache_probe_use', 'cache_probe_survivors',
                  'f_to_m', 'm_to_f', 'query_count', 'survival_retries_u64']

POP16 = bytes(bin(i).count('1') for i in range(1 << 16))
DIAGNOSTIC_MAX = 480
MISMATCH_CAP = 1000000


def pop32(v):
    return POP16[v & 0xFFFF] + POP16[v >> 16]


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
            self.error = 'open failed: %s' % (exc.strerror or exc)

    def read_exact(self, n, rec, what):
        data = self.fh.read(n)
        self.sha.update(data)
        self.nbytes += len(data)
        if len(data) != n:
            rec.fatal('SHORT_RECORD', '%s: %s needs %d bytes at offset %d; found %d'
                      % (self.label, what, n, self.nbytes - len(data), len(data)))
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
# Block, path and update records


def check_block_record(bid, blk, L, P, rec):
    """Block record against values reconstructed from path records (or, in synthetic mode, its own arrays)."""
    where = 'block %d' % bid
    for c in range(CELLS):
        if L[c] > 2048 or P[c] > 65536:
            rec.fail('RANGE', '%s cell %d field late_m_sum/late_mismatch_sum out of range' % (where, c))
    # N2 implies complementary SHAM late M sums; N1 implies equal SHAM mismatch sums.
    if L[4] + L[5] != 2048 or L[6] + L[7] != 2048 or P[4] != P[5] or P[6] != P[7]:
        rec.fail('N1_N2', '%s SHAM fields late_m_sum/late_mismatch_sum violate N1/N2' % where)
    p_abs = (P[6] + P[7]) - (P[2] + P[3])
    nums = (L[2] + L[3] - 2048, (L[2] + L[3]) - (L[0] + L[1]), L[3] - L[2], L[1] - L[0],
            p_abs, p_abs - ((P[4] + P[5]) - (P[0] + P[1])))
    expected = (bid, 1, 1, 1, 1 if bid < AUDIT_BLOCKS else 0) + nums + tuple(L) + tuple(P)
    for i, name in enumerate(BLOCK_FIELDS):
        if blk[i] != expected[i]:
            rec.fail('BLOCK_FIELD', '%s block record field %s mismatch' % (where, name))
            break
    return nums


def verify_block(bid, ub, pb, bb, ab, rec, acc, ctx):
    upd = list(UPD.iter_unpack(ub))
    pth = list(PTH.iter_unpack(pb))
    blk = BLK.unpack(bb)
    where = 'block %d' % bid
    for c in range(CELLS):
        arm, law, start = c >> 2, (c >> 1) & 1, c & 1
        base = c * UPDATES
        late_m = late_mis = queries = f2m = m2f = cpu = cps = retries = 0
        fix = ext = lfix = lext = 0
        prev_m = 0
        last = None
        for k in range(UPDATES):
            r = upd[base + k]
            t = k + 1
            if r[0] != bid or r[1] != t or r[2] != c:
                rec.fatal('ORDER', '%s cell %d update %d update record fields block/update/cell out of order'
                          % (where, c, t))
            m, mis, vmc, cu, cs, fm, mf, q, rt = r[3], r[4], r[5], r[6], r[7], r[8], r[9], r[10], r[11]
            for fname, bad in (('m_count', m > 32), ('total_mismatch', mis > 1024), ('valid_m_cache', vmc > 32),
                               ('f_to_m+m_to_f', fm + mf > 32), ('cache_probe_survivors', cs > cu),
                               ('survival_retries_u64', rt >= (32 << 32))):
                if bad:
                    rec.fail('RANGE', '%s cell %d update %d field %s out of range' % (where, c, t, fname))
                    break
            if q != CANDIDATES:
                rec.fail('QUERY_COUNT', '%s cell %d update %d field query_count mismatch' % (where, c, t))
            if vmc != (0 if t == 1 else prev_m):
                rec.fail('IDENTITY', '%s cell %d update %d field valid_m_cache inconsistent with previous m_count'
                         % (where, c, t))
            if arm == 0:
                if cu != vmc:
                    rec.fail('IDENTITY', '%s cell %d update %d field cache_probe_use inconsistent with valid_m_cache'
                             % (where, c, t))
            elif cu != 0 or cs != 0:
                rec.fail('IDENTITY', '%s cell %d update %d SHAM field cache_probe_use/cache_probe_survivors nonzero'
                         % (where, c, t))
            prev_m = m
            if t >= LATE_FIRST:
                late_m += m
                late_mis += mis
            queries += q
            f2m += fm
            m2f += mf
            cpu += cu
            cps += cs
            retries += rt
            if m == 32:
                fix += 1
                if t >= LATE_FIRST:
                    lfix += 1
            if m == 0:
                ext += 1
                if t >= LATE_FIRST:
                    lext += 1
            last = r
        ctx.bump('update_records', UPDATES)
        p = pth[c]
        derived = (bid, c, arm, law, start, late_m, late_mis, queries, f2m, m2f, cpu, cps, retries,
                   fix, ext, lfix, lext)
        for i, name in enumerate(PATH_DERIVED):
            if p[i] != derived[i]:
                rec.fail('PATH_FIELD', '%s cell %d path record field %s mismatch' % (where, c, name))
                break
        if queries != UPDATES * CANDIDATES:
            rec.fail('QUERY_COUNT', '%s cell %d path field total_queries mismatch' % (where, c))
        if p[18] != last[3] or p[19] != last[4]:
            rec.fail('PATH_FIELD', '%s cell %d path record field final_m_count/final_total_mismatch inconsistent '
                     'with update 256' % (where, c))
        if p[20] != 0:
            rec.fail('RESERVED', '%s cell %d path record field reserved0 nonzero' % (where, c))
        ctx.bump('path_records')

    # N1/N2 on SHAM pairs (4,5) and (6,7): paired-hash identity plus record-level consequences.
    sham_ok = True
    for a, b in ((4, 5), (6, 7)):
        for k in range(UPDATES):
            ra, rb = upd[a * UPDATES + k], upd[b * UPDATES + k]
            if not (ra[4] == rb[4] and ra[3] + rb[3] == 32 and ra[8] == rb[9] and ra[9] == rb[8] and
                    ra[10] == rb[10] and ra[11] == rb[11] and (k == 0 or ra[5] + rb[5] == 32)):
                sham_ok = False
                rec.fail('N1_N2', '%s SHAM cell %d and cell %d update %d update records violate N1/N2'
                         % (where, a, b, k + 1))
                break
        pa, pq = pth[a], pth[b]
        if pa[21] != pq[21]:
            sham_ok = False
            rec.fail('N1_N2_PAIRED_HASH', '%s SHAM cell %d and cell %d field paired_trajectory_sha256 differs'
                     % (where, a, b))
        if not (pa[6] == pq[6] and pa[5] + pq[5] == 2048 and pa[18] + pq[18] == 32 and pa[19] == pq[19] and
                pa[13] == pq[14] and pa[14] == pq[13] and pa[15] == pq[16] and pa[16] == pq[15] and
                pa[8] == pq[9] and pa[9] == pq[8] and pa[12] == pq[12]):
            sham_ok = False
            rec.fail('N1_N2', '%s SHAM cell %d and cell %d path summaries violate N1/N2' % (where, a, b))
    if sham_ok:
        ctx.bump('blocks_with_both_sham_identities')

    L = [pth[c][5] for c in range(CELLS)]
    P = [pth[c][6] for c in range(CELLS)]
    nums = check_block_record(bid, blk, L, P, rec)
    accumulate(acc, nums, L, P)
    ctx.bump('block_records')
    if ab is not None:
        verify_audit_block(bid, ab, upd, pth, rec, ctx)


def accumulate(acc, nums, L, P):
    for i in range(6):
        acc['nums'][i] += nums[i]
    for c in range(CELLS):
        acc['L'][c] += L[c]
        acc['P'][c] += P[c]
    acc['n'] += 1


def new_acc():
    return {'nums': [0] * 6, 'L': [0] * CELLS, 'P': [0] * CELLS, 'n': 0}


# ------------------------------------------------------------------------------------------------
# Audit candidate rows: record-consistency checks only (no draw is generated here)


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


def verify_audit_block(bid, ab, upd, pth, rec, ctx):
    rows = list(AUD.iter_unpack(ab))
    targets = [[0] * (UPDATES + 1) for _ in range(CELLS)]
    rbits = [[0] * (UPDATES + 1) for _ in range(CELLS)]
    applied = [[0] * (UPDATES + 1) for _ in range(CELLS)]
    initial = None
    for c in range(CELLS):
        arm, law, start = c >> 2, (c >> 1) & 1, c & 1
        p_label = [start] * SLOTS
        p_cv = [0] * SLOTS
        p_cache = [0] * SLOTS
        p_geno = None
        paired = hashlib.sha256()
        paired.update(b'MMEM-PAIRED-TRAJECTORY-V1')
        paired.update(struct.pack('<IBB', bid, arm, law))
        for t in range(1, UPDATES + 1):
            base = (c * UPDATES + (t - 1)) * CANDIDATES
            g = rows[base:base + CANDIDATES]
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
            weights = [0] * CANDIDATES
            order = [None] * SLOTS
            ok_sel = True
            s_mis = s_m = vmc = cu = cs = fm = mf = s_retry = 0
            for j in range(CANDIDATES):
                (r_b, r_u, r_c, cand, fam, par, plab, pcv, pcache, geno, tgt, mis, rb, ra, ps, w, rank, post,
                 flip, donor, w_d, x, z, retry, res) = g[j]
                if r_b != bid or r_u != t or r_c != c or cand != j:
                    rec.fatal('ORDER', '%s candidate %d audit row fields block/update/cell/candidate out of order'
                              % (where, j))
                if fam != (j >> 5) or par != (j & 31):
                    rec.fail('AUDIT_FIELD', '%s candidate %d field family/parent mismatch' % (where, j))
                if plab != p_label[par] or pcv != p_cv[par] or pcache != p_cache[par]:
                    rec.fail('CONTINUITY', '%s candidate %d field parent_label_pre/parent_cache_valid/parent_cache '
                             'differs from previous update' % (where, j))
                if tgt != target or rb != rbit or ra != rapp:
                    rec.fail('AUDIT_FIELD', '%s candidate %d field target/recurrence_bit/recurrence_applied not '
                             'constant' % (where, j))
                if mis != pop32(geno ^ tgt):
                    rec.fail('MISMATCH', '%s candidate %d field mismatch != popcount(genotype^target)' % (where, j))
                if mis > 32 or w != (1 << (32 - mis)):
                    rec.fail('WEIGHT', '%s candidate %d field weight != 2^(32-mismatch)' % (where, j))
                else:
                    weights[j] = w
                if fam == 1:
                    use = arm == 0 and plab == 1 and pcv == 1
                    if ps != (1 if use else 0):
                        rec.fail('PROBE', '%s candidate %d field probe_source mismatch' % (where, j))
                    if use:
                        cu += 1
                        if geno != pcache:
                            rec.fail('PROBE', '%s candidate %d field genotype differs from parent_cache' % (where, j))
                elif ps != NA:
                    rec.fail('NOT_APPLICABLE', '%s candidate %d field probe_source not 255' % (where, j))
                if fam == 0 and plab == 1 and pcv == 1:
                    vmc += 1
                if fam == 3:
                    if donor > 2:
                        rec.fail('DONOR', '%s candidate %d field donor_family out of range' % (where, j))
                    elif g[32 * donor + par][11] != min(g[par][11], g[32 + par][11], g[64 + par][11]):
                        rec.fail('DONOR', '%s candidate %d field donor_family is not a lowest-mismatch family'
                                 % (where, j))
                elif donor != NA:
                    rec.fail('NOT_APPLICABLE', '%s candidate %d field donor_family not 255' % (where, j))
                if rank == NA:
                    if post != NA or flip != NA or w_d or x or z or retry:
                        rec.fail('NOT_APPLICABLE', '%s candidate %d unselected but selection fields are set'
                                 % (where, j))
                else:
                    if rank >= SLOTS or order[rank] is not None:
                        ok_sel = False
                        rec.fail('SELECTION', '%s candidate %d field selected_rank invalid or duplicate' % (where, j))
                    else:
                        order[rank] = j
                    if flip > 1 or plab > 1 or post != (plab ^ flip):
                        rec.fail('MUTATION', '%s candidate %d field post_label != parent_label_pre xor policy_flip'
                                 % (where, j))
                    s_mis += mis
                    s_m += 1 if post == 1 else 0
                    fm += 1 if (plab == 0 and post == 1) else 0
                    mf += 1 if (plab == 1 and post == 0) else 0
                    cs += 1 if (fam == 1 and ps == 1) else 0
                    s_retry += retry
                if res != 0:
                    rec.fail('RESERVED', '%s candidate %d field reserved nonzero' % (where, j))
            if rbit > 1 or rapp != (1 if (law == 1 and t >= 3 and rbit == 1) else 0):
                rec.fail('RECURRENCE', '%s field recurrence_bit/recurrence_applied inconsistent' % where)
            targets[c][t], rbits[c][t], applied[c][t] = target, rbit, rapp
            if any(o is None for o in order):
                ok_sel = False
                rec.fail('SELECTION', '%s fewer than 32 distinct survivor ranks' % where)
            if not ok_sel:
                rec.fatal('SELECTION', '%s survivor lineage cannot be reconstructed' % where)
            verify_selection(g, order, weights, where, rec)
            u = upd[c * UPDATES + t - 1]
            derived = (s_m, s_mis, vmc, cu, cs, fm, mf, CANDIDATES, s_retry)
            for i, name in enumerate(UPDATE_DERIVED):
                if u[3 + i] != derived[i]:
                    rec.fail('AUDIT_VS_UPDATE', '%s update record field %s inconsistent with audit rows'
                             % (where, name))
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
        if paired.digest() != pth[c][21]:
            rec.fail('PAIRED_HASH', 'block %d cell %d field paired_trajectory_sha256 differs from audit-row '
                     'reconstruction' % (bid, c))
        final = hashlib.sha256()
        final.update(b'MMEM-FINAL-STATE-V1')
        final.update(struct.pack('<IBI', bid, c, UPDATES))
        for s in range(SLOTS):
            final.update(struct.pack('<IBBI', p_geno[s], p_label[s] & 0xFF, p_cv[s], p_cache[s]))
        final.update(struct.pack('<II', targets[c][UPDATES], targets[c][UPDATES - 1]))
        if final.digest() != pth[c][17]:
            rec.fail('FINAL_HASH', 'block %d cell %d field final_state_sha256 differs from audit-row reconstruction'
                     % (bid, c))
        ctx.bump('audit_paths_hashes_reconstructed')
    # Target-law pairing across cells (specification sections 3 and 4).
    for t in range(1, UPDATES + 1):
        for c in range(CELLS):
            if rbits[c][t] != rbits[0][t]:
                rec.fail('TARGET_LAW', 'block %d update %d recurrence bit differs across cells' % (bid, t))
            reference = targets[0][t] if ((c >> 1) & 1) == 0 else targets[2][t]
            if targets[c][t] != reference:
                rec.fail('TARGET_LAW', 'block %d update %d cell %d target differs within its law' % (bid, t, c))
        expect_half = targets[2][t - 2] if applied[2][t] == 1 else targets[0][t]
        if targets[2][t] != expect_half:
            rec.fail('TARGET_LAW', 'block %d update %d HALF target violates the lag-two law' % (bid, t))


# ------------------------------------------------------------------------------------------------
# Estimates


def half_width(r, alpha, n):
    """High-precision half-width (ambient 110-digit context); used only for the frozen-constant self-check."""
    return Decimal(r) * ((Decimal(2) / alpha).ln() / (Decimal(2) * Decimal(n))).sqrt()


def round8(d):
    return d.quantize(Decimal('0.00000001'), rounding=decimal.ROUND_HALF_EVEN)


def producer_context():
    """The producer's frozen local Decimal context (interface contract numeric_encoding: precision 60)."""
    return decimal.Context(prec=PRODUCER_DECIMAL_PREC, rounding=decimal.ROUND_HALF_EVEN)


def dec(f):
    """IFACE-R2 numeric_encoding.estimate: the exact fraction in its own local precision-60 context."""
    with decimal.localcontext(producer_context()):
        return Decimal(f.numerator) / Decimal(f.denominator)


def producer_half_width(range_length, alpha_text, n):
    """IFACE-R2 numeric_encoding.half_width, in one local precision-60 context, in the stated order."""
    with decimal.localcontext(producer_context()):
        inner = (Decimal(2) / Decimal(alpha_text)).ln() / (Decimal(2) * Decimal(n))
        return Decimal(range_length) * inner.sqrt()


def producer_bounds(estimate_decimal, h):
    """IFACE-R2 numeric_encoding.lower_upper: a separate local precision-60 context."""
    with decimal.localcontext(producer_context()):
        return estimate_decimal - h, estimate_decimal + h


def exact_text(f):
    """IFACE-R2 estimate_exact: '%d/%d' of the reduced fraction, even when the denominator is 1."""
    return '%d/%d' % (f.numerator, f.denominator)


def expected_estimates(acc, n):
    """The 22 records in contract order. Per record: dec(Fraction) first, then the half-width, then
    lower/upper, each in its own precision-60 context (IFACE-R2 numeric_encoding)."""
    out = []
    for name, r, den, frozen, idx in PRIMARY + SECONDARY:
        f = Fraction(acc['nums'][idx], den * n)
        alpha_text = ALPHA_PRIMARY_TEXT if idx < 4 else ALPHA_SECONDARY_TEXT
        est = dec(f)
        h = producer_half_width(r, alpha_text, n)
        lower, upper = producer_bounds(est, h)
        out.append({'name': name, 'kind': 'primary' if idx < 4 else 'secondary',
                    'family': FAMILY_PRIMARY if idx < 4 else FAMILY_SECONDARY,
                    'keys': KEYS_PRIMARY if idx < 4 else KEYS_SECONDARY, 'fraction': f, 'range_length': r,
                    'alpha_each': alpha_text, 'estimate': est, 'half_width': h, 'lower': lower,
                    'upper': upper, 'frozen': frozen})
    cell_fracs = ([('M_FREQUENCY_LATE|', Fraction(acc['L'][c], 2048 * n)) for c in range(CELLS)] +
                  [('ACCURACY_LATE|', 1 - Fraction(acc['P'][c], 65536 * n)) for c in range(CELLS)])
    for i, (prefix, f) in enumerate(cell_fracs):
        out.append({'name': prefix + CELL_LABELS[i % CELLS], 'kind': 'cell', 'family': FAMILY_CELL,
                    'keys': KEYS_CELL, 'fraction': f, 'estimate': dec(f)})
    for e in out:
        e['estimate_exact'] = exact_text(e['fraction'])
    return out


def primary_rule(bounds):
    for name in ('D_HALF', 'D_ZERO'):
        lo, up = bounds[name]
        if not (lo > -DELTA and up < DELTA):
            return 2
    if bounds['C_abs'][0] > DELTA and bounds['C_rec'][0] > DELTA:
        return 3
    if bounds['C_abs'][0] > DELTA and bounds['C_rec'][1] <= DELTA:
        return 4
    if bounds['C_abs'][1] <= DELTA:
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


def relation(name, primary_label, classification):
    """IFACE-R2 family_and_relation_rules.relation_result for P_abs / P_rec."""
    pos = primary_label in RELATION_POS[name]
    neg = primary_label in RELATION_NEG[name]
    if classification == SECONDARY_LABELS[3] or not (pos or neg):
        return 'UNCERTAINTY'
    sec_pos = classification == SECONDARY_LABELS[0]
    return 'AGREEMENT' if sec_pos == pos else 'DISAGREEMENT'


def is_int(v):
    return isinstance(v, int) and not isinstance(v, bool)


def same(v, want):
    """Exact JSON equality including type (so true != 1 and "2" != 2)."""
    return type(v) is type(want) and v == want


FRACTION_TEXT = re.compile(r'-?[0-9]+/[0-9]+')
DECIMAL_TEXT = re.compile(r'-?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][-+]?[0-9]+)?')
SHA256_LOWER = re.compile(r'[0-9a-f]{64}')


def decimal_field(v):
    """A contract Decimal field is a JSON string holding a finite decimal numeral; None otherwise."""
    if isinstance(v, str) and DECIMAL_TEXT.fullmatch(v):
        return Decimal(v)
    return None


def check_estimate_record(i, record, exp, n, derived, rec):
    """Record i of estimates_22.json, read only at the exact contract keys. `derived` holds the recomputed
    primary label and the P_abs/P_rec classifications and relations. Diagnostics name only the record number
    and field; no observed or recomputed value is ever written."""
    tag = 'estimates_22.json record %d' % (i + 1)
    if not isinstance(record, dict):
        rec.fail('ANALYSIS_SCHEMA', '%s is not an object' % tag)
        return
    missing = [k for k in exp['keys'] if k not in record]
    extra = sum(1 for k in record if k not in exp['keys'])
    if missing or extra:
        rec.fail('ANALYSIS_SCHEMA', '%s key set differs from the contract (missing: %s; undocumented keys: %d)'
                 % (tag, ', '.join(missing) or 'none', extra))
        return

    def mismatch(category, field):
        rec.fail(category, '%s field %s mismatch' % (tag, field))

    if not same(record['record'], i + 1):
        mismatch('ANALYSIS_ORDER', 'record')
    if not same(record['name'], exp['name']):
        mismatch('ANALYSIS_ORDER', 'name')
    if not same(record['family'], exp['family']):
        mismatch('ANALYSIS_ORDER', 'family')
    exact = record['estimate_exact']
    if not (isinstance(exact, str) and FRACTION_TEXT.fullmatch(exact)):
        rec.fail('ANALYSIS_SCHEMA', '%s field estimate_exact is not a numerator/denominator string' % tag)
    elif exact != exp['estimate_exact']:
        mismatch('ESTIMATE', 'estimate_exact')
    fields = [('estimate', 'ESTIMATE_DISPLAY')]
    if exp['kind'] != 'cell':
        fields += [('half_width', 'BOUND'), ('lower', 'BOUND'), ('upper', 'BOUND')]
    for field, category in fields:
        value = decimal_field(record[field])
        if value is None:
            rec.fail('ANALYSIS_SCHEMA', '%s field %s is not a Decimal string' % (tag, field))
        elif value != exp[field]:
            mismatch(category, field)
    if exp['kind'] == 'cell':
        for field in ('lower', 'upper'):
            if record[field] is not None:
                mismatch('BOUND', field)
        if not same(record['classification'], CELL_CLASSIFICATION):
            mismatch('ANALYSIS_FIELD', 'classification')
        return
    # IFACE-R2 range_lengths_in_order [1,2,2,2] and [2,4]: exact JSON integers.
    if not same(record['range_length'], exp['range_length']):
        mismatch('ANALYSIS_FIELD', 'range_length')
    if not same(record['alpha_each'], exp['alpha_each']):
        mismatch('ANALYSIS_FIELD', 'alpha_each')
    if not same(record['n_blocks'], n):
        mismatch('ANALYSIS_FIELD', 'n_blocks')
    if exp['kind'] == 'primary':
        if not same(record['family_decision'], derived['primary']):
            mismatch('DECISION', 'family_decision')
        return
    name = exp['name']
    if not same(record['classification'], derived['secondary'][name]):
        mismatch('DECISION', 'classification')
    if not same(record['relation_to_allele_enrichment_descriptive'], derived['relation'][name]):
        mismatch('DECISION', 'relation_to_allele_enrichment_descriptive')
    if not same(record['cannot_alter_primary_decision'], True):
        mismatch('ANALYSIS_FIELD', 'cannot_alter_primary_decision')


def check_decision(decision, rule, adverse, secondary, rec):
    """decision.json, read only at the five exact contract keys. Diagnostics name only the field."""
    if not isinstance(decision, dict):
        rec.fail('ANALYSIS_SCHEMA', 'decision.json root is not an object')
        return
    missing = [k for k in DECISION_KEYS if k not in decision]
    extra = sum(1 for k in decision if k not in DECISION_KEYS)
    if missing or extra:
        rec.fail('ANALYSIS_SCHEMA', 'decision.json key set differs from the contract (missing: %s; undocumented '
                 'keys: %d)' % (', '.join(missing) or 'none', extra))
        return

    def mismatch(field):
        rec.fail('DECISION', 'decision.json field %s mismatch' % field)

    if not same(decision['decision_rule_applied'], rule):
        mismatch('decision_rule_applied')
    if not same(decision['primary_decision'], PRIMARY_LABELS[rule]):
        mismatch('primary_decision')
    flag = decision['adverse_selection_upper_C_abs_below_minus_Delta']
    if not (same(flag, adverse) if rule == 5 else flag is None):
        mismatch('adverse_selection_upper_C_abs_below_minus_Delta')
    sec = decision['secondary_classifications']
    if not (isinstance(sec, dict) and sorted(sec) == ['P_abs', 'P_rec']):
        rec.fail('ANALYSIS_SCHEMA', 'decision.json field secondary_classifications key set differs from P_abs/P_rec')
    else:
        for name in ('P_abs', 'P_rec'):
            if not same(sec[name], secondary[name]):
                mismatch('secondary_classifications.' + name)
    labels = decision['interpretation_labels']
    if not (isinstance(labels, list) and len(labels) == 6 and all(isinstance(x, str) for x in labels)):
        rec.fail('ANALYSIS_SCHEMA', 'decision.json field interpretation_labels is not an array of six strings')
    elif labels != list(INTERPRETATION_LABELS):
        # Exact identity and order only; the labels are never searched.
        mismatch('interpretation_labels')


def load_json_input(path, label, rec, ctx, category):
    reader = ctx.open(path, label, rec)
    data = reader.read_all()
    reader.finish()

    def pairs_hook(pairs):
        out = {}
        for k, v in pairs:
            if k in out:
                raise ValueError('duplicate key %r' % k)
            out[k] = v
        return out

    def reject_constant(name):
        raise ValueError('non-finite constant %s' % name)

    try:
        return json.loads(data.decode('utf-8'), object_pairs_hook=pairs_hook, parse_float=Decimal,
                          parse_constant=reject_constant)
    except (ValueError, UnicodeDecodeError) as exc:
        # The exception type only: a decoder message can quote input text.
        rec.fatal(category, '%s is not strict JSON (%s)' % (label, type(exc).__name__))


def verify_analysis(adir, acc, n, rec, ctx, frozen):
    if not os.path.isdir(adir):
        rec.fatal('MISSING_INPUT', 'analysis directory %s does not exist' % adir)
    closed = os.path.join(adir, 'ROUTE_CLOSED_INVALID.json')
    if os.path.lexists(closed):
        ctx.open(closed, 'analysis/ROUTE_CLOSED_INVALID.json', rec).finish()
        rec.fatal('ANALYSIS_CLOSED_INVALID', 'analysis wrote ROUTE_CLOSED_INVALID.json; no estimate exists to verify')
    est = load_json_input(os.path.join(adir, 'estimates_22.json'), 'analysis/estimates_22.json', rec, ctx,
                          'ANALYSIS_SCHEMA')
    decision = load_json_input(os.path.join(adir, 'decision.json'), 'analysis/decision.json', rec, ctx,
                               'ANALYSIS_SCHEMA')

    exp = expected_estimates(acc, n)
    if frozen:
        for e in exp[:6]:
            if round8(e['half_width']) != Decimal(e['frozen']):
                rec.fatal('CONSTANT', 'half_width for %s does not round to its frozen value' % e['name'])
    # Decisions use the precision-60 bounds the producer is contracted to compute.
    bounds = dict((e['name'], (e['lower'], e['upper'])) for e in exp[:6])
    rule = primary_rule(bounds)
    adverse = bounds['C_abs'][1] < -DELTA
    secondary = {'P_abs': secondary_class(*bounds['P_abs']), 'P_rec': secondary_class(*bounds['P_rec'])}
    derived = {'primary': PRIMARY_LABELS[rule], 'secondary': secondary,
               'relation': dict((name, relation(name, PRIMARY_LABELS[rule], secondary[name]))
                                for name in ('P_abs', 'P_rec'))}

    if not (isinstance(est, list) and len(est) == 22):
        rec.fatal('ANALYSIS_SCHEMA', 'estimates_22.json root is not an array of 22 records')
    for i, record in enumerate(est):
        check_estimate_record(i, record, exp[i], n, derived, rec)
        ctx.bump('estimates_compared')
    check_decision(decision, rule, adverse, secondary, rec)
    ctx.bump('decision_files_compared')


# ------------------------------------------------------------------------------------------------
# Manifest hashes (interface contract run_manifest: exact fields and the exact 97-entry outputs array)


def manifest_type_ok(v, kind):
    if kind == 'string':
        return isinstance(v, str)
    if kind == 'lowercase_sha256':
        return isinstance(v, str) and SHA256_LOWER.fullmatch(v) is not None
    if kind == 'integer':
        return is_int(v)
    if kind == 'integer_1_to_32':
        return is_int(v) and 1 <= v <= 32
    if kind == 'integer_nonnegative':
        return is_int(v) and v >= 0
    if kind == 'number_nonnegative':
        return (is_int(v) or (isinstance(v, Decimal) and v.is_finite())) and v >= 0
    if kind == 'array':
        return isinstance(v, list)
    return False


def verify_manifest(path, expected_files, rec, ctx):
    manifest = load_json_input(path, 'run_manifest.json', rec, ctx, 'MANIFEST_SCHEMA')
    if not isinstance(manifest, dict):
        rec.fatal('MANIFEST_SCHEMA', 'run_manifest.json root is not an object')
    # IFACE-R2 exact_root_keys: missing keys are named below; undocumented keys are not echoed.
    if any(k not in MANIFEST_ROOT_KEYS for k in manifest):
        rec.fail('MANIFEST_SCHEMA', 'run_manifest.json root key set has undocumented keys')
    for key, want in MANIFEST_EXACT:
        if key not in manifest:
            rec.fail('MANIFEST_SCHEMA', 'run_manifest.json field %s missing' % key)
        elif not same(manifest[key], want):
            rec.fail('MANIFEST_FIELD', 'run_manifest.json field %s differs from the contract value' % key)
    for key, kind in MANIFEST_TYPED:
        if key not in manifest:
            rec.fail('MANIFEST_SCHEMA', 'run_manifest.json field %s missing' % key)
        elif not manifest_type_ok(manifest[key], kind):
            rec.fail('MANIFEST_SCHEMA', 'run_manifest.json field %s is not %s' % (key, kind))
    outputs = manifest.get('outputs')
    if not isinstance(outputs, list):
        rec.fatal('MANIFEST_SCHEMA', 'run_manifest.json field outputs is not an array')
    if len(outputs) != len(expected_files):
        rec.fail('MANIFEST_SCHEMA', 'run_manifest.json field outputs entry count differs from the contract')
    seen = set()
    for i, entry in enumerate(outputs):
        # Entry index and relative path are coordinates; a bytes or sha256 value is never written.
        where = 'run_manifest.json outputs entry %d' % i
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
        rec.fail('MANIFEST', 'run_manifest.json outputs has no entry for %s' % rel)


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


def run_layout(run_dir, rec, record_sizes):
    """The exact 97-file run layout plus run_manifest.json. With record_sizes (production) every file must
    have its documented record size; otherwise (manifest-interface) every file must be a placeholder of at
    most MANIFEST_INTERFACE_MAX_BYTES, so that mode can never stand in for a production audit."""
    shards = os.path.join(run_dir, 'shards')
    names = listing(shards, rec, 'shards directory')
    wanted = ['shard_%02d' % s for s in range(N_SHARDS)]
    if names != wanted:
        rec.fatal('LAYOUT', 'shards/ must contain exactly shard_00..shard_31; found %r' % names[:40])
    expected_files = set()
    for s in range(N_SHARDS):
        sdir = os.path.join(shards, wanted[s])
        files = dict(SHARD_SIZES)
        if s == 0:
            files[AUDIT_FILE] = AUDIT_BLOCKS * AUD_PER_BLOCK
        present = listing(sdir, rec, 'shard directory')
        if present != sorted(files):
            rec.fatal('LAYOUT', '%s must contain exactly %r; found %r' % (wanted[s], sorted(files), present))
        for name, size in files.items():
            actual = regular_size(os.path.join(sdir, name), rec, 'shard file')
            if record_sizes and actual != size:
                rec.fatal('SIZE', '%s/%s has %d bytes; documented %d' % (wanted[s], name, actual, size))
            if not record_sizes and actual > MANIFEST_INTERFACE_MAX_BYTES:
                rec.fatal('SIZE', '%s/%s exceeds the manifest-interface placeholder bound' % (wanted[s], name))
            expected_files.add('shards/%s/%s' % (wanted[s], name))
    manifest_path = os.path.join(run_dir, 'run_manifest.json')
    regular_size(manifest_path, rec, 'run manifest')
    return shards, wanted, expected_files, manifest_path


def run_production(args, rec, ctx):
    ctx.expected = {'update_records': N_BLOCKS * CELLS * UPDATES, 'path_records': N_BLOCKS * CELLS,
                    'block_records': N_BLOCKS, 'audit_rows': AUDIT_BLOCKS * CELLS * UPDATES * CANDIDATES,
                    'audit_paths_hashes_reconstructed': AUDIT_BLOCKS * CELLS,
                    'blocks_with_both_sham_identities': N_BLOCKS, 'manifest_files_matched': N_SHARDS * 3 + 1,
                    'estimates_compared': 22, 'decision_files_compared': 1}
    shards, wanted, expected_files, manifest_path = run_layout(args.run_dir, rec, record_sizes=True)

    acc = new_acc()
    for s in range(N_SHARDS):
        sdir = os.path.join(shards, wanted[s])
        prefix = 'shards/%s/' % wanted[s]
        ur = ctx.open(os.path.join(sdir, 'updates.bin'), prefix + 'updates.bin', rec)
        pr = ctx.open(os.path.join(sdir, 'paths.bin'), prefix + 'paths.bin', rec)
        br = ctx.open(os.path.join(sdir, 'blocks.bin'), prefix + 'blocks.bin', rec)
        ar = ctx.open(os.path.join(sdir, AUDIT_FILE), prefix + AUDIT_FILE, rec) if s == 0 else None
        for local in range(BLOCKS_PER_SHARD):
            bid = s * BLOCKS_PER_SHARD + local
            ub = ur.read_exact(UPD_PER_BLOCK, rec, 'update records of block %d' % bid)
            pb = pr.read_exact(PTH_PER_BLOCK, rec, 'path records of block %d' % bid)
            bb = br.read_exact(BLK.size, rec, 'block record %d' % bid)
            ab = ar.read_exact(AUD_PER_BLOCK, rec, 'audit rows of block %d' % bid) if bid < AUDIT_BLOCKS else None
            verify_block(bid, ub, pb, bb, ab, rec, acc, ctx)
            if bid == AUDIT_BLOCKS - 1 and ar is not None:
                ar.expect_eof(rec)
                ar.finish()
        for reader in (ur, pr, br):
            reader.expect_eof(rec)
            reader.finish()
    verify_manifest(manifest_path, expected_files, rec, ctx)
    if rec.count == 0:
        verify_analysis(args.analysis_dir, acc, N_BLOCKS, rec, ctx, frozen=True)


def run_fixture(args, rec, ctx):
    ctx.expected = {'update_records': CELLS * UPDATES, 'path_records': CELLS, 'block_records': 1,
                    'audit_rows': CELLS * UPDATES * CANDIDATES, 'audit_paths_hashes_reconstructed': CELLS,
                    'blocks_with_both_sham_identities': 1}
    d = args.layout_dir
    present = listing(d, rec, 'fixture layout directory')
    if present != sorted(FIXTURE_FILES):
        rec.fatal('LAYOUT', 'fixture layout must contain exactly %r; found %r' % (sorted(FIXTURE_FILES), present))
    for name, size in FIXTURE_FILES.items():
        actual = regular_size(os.path.join(d, name), rec, 'fixture file')
        if size is not None and actual != size:
            rec.fatal('SIZE', '%s has %d bytes; documented %d' % (name, actual, size))
    ctx.open(os.path.join(d, 'layout_sample_README.txt'), 'layout_sample_README.txt', rec).finish()
    br = ctx.open(os.path.join(d, 'layout_sample_block.bin'), 'layout_sample_block.bin', rec)
    bb = br.read_exact(BLK.size, rec, 'block record')
    br.expect_eof(rec)
    bid = struct.unpack_from('<I', bb)[0]
    if bid >= N_BLOCKS:
        rec.fatal('RANGE', 'fixture block id %d outside 0..41599' % bid)
    ur = ctx.open(os.path.join(d, 'layout_sample_updates.bin'), 'layout_sample_updates.bin', rec)
    pr = ctx.open(os.path.join(d, 'layout_sample_paths.bin'), 'layout_sample_paths.bin', rec)
    ar = ctx.open(os.path.join(d, 'layout_sample_audit.bin'), 'layout_sample_audit.bin', rec)
    ub = ur.read_exact(UPD_PER_BLOCK, rec, 'update records')
    pb = pr.read_exact(PTH_PER_BLOCK, rec, 'path records')
    ab = ar.read_exact(AUD_PER_BLOCK, rec, 'audit rows')
    for reader in (ur, pr, ar):
        reader.expect_eof(rec)
    verify_block(bid, ub, pb, bb, ab, rec, new_acc(), ctx)


def run_synthetic(args, rec, ctx):
    n = args.n_blocks
    ctx.expected = {'block_records': n, 'estimates_compared': 22, 'decision_files_compared': 1}
    size = regular_size(args.blocks_file, rec, 'synthetic blocks file')
    if size != n * BLK.size:
        rec.fatal('SIZE', 'synthetic blocks file has %d bytes; expected %d' % (size, n * BLK.size))
    br = ctx.open(args.blocks_file, 'synthetic_blocks.bin', rec)
    acc = new_acc()
    for bid in range(n):
        blk = BLK.unpack(br.read_exact(BLK.size, rec, 'block record %d' % bid))
        L = list(blk[11:19])
        P = list(blk[19:27])
        nums = check_block_record(bid, blk, L, P, rec)
        accumulate(acc, nums, L, P)
        ctx.bump('block_records')
    br.expect_eof(rec)
    if rec.count == 0:
        verify_analysis(args.analysis_dir, acc, n, rec, ctx, frozen=False)


def run_manifest_interface(args, rec, ctx):
    """NONSCIENTIFIC manifest-interface check: manifest schema, entry uniqueness, byte counts and hashes
    over placeholder files at the 97 required relative paths. Each file is only hashed as opaque bytes; no
    record is parsed, nothing is simulated and no random key is derived."""
    ctx.expected = {'manifest_interface_files_hashed': N_SHARDS * 3 + 1,
                    'manifest_files_matched': N_SHARDS * 3 + 1}
    _, _, expected_files, manifest_path = run_layout(args.run_dir, rec, record_sizes=False)
    for rel in sorted(expected_files):
        ctx.open(os.path.join(args.run_dir, *rel.split('/')), rel, rec).finish()
        ctx.bump('manifest_interface_files_hashed')
    verify_manifest(manifest_path, expected_files, rec, ctx)


# ------------------------------------------------------------------------------------------------


def self_checks():
    out = []

    def add(name, ok):
        out.append({'name': name, 'result': 'PASS' if ok else 'FAIL'})

    add('sha256_fips180_abc', hashlib.sha256(b'abc').hexdigest() ==
        'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad')
    add('record_struct_sizes', UPD.size == 24 and PTH.size == 128 and BLK.size == 96 and AUD.size == 72)
    add('total_record_bytes', N_BLOCKS * CELLS * UPDATES * 24 + N_BLOCKS * CELLS * 128 + N_BLOCKS * 96 +
        AUDIT_BLOCKS * CELLS * UPDATES * CANDIDATES * 72 == 3299274752)
    add('popcount', pop32(0) == 0 and pop32(MASK32) == 32 and pop32(0x80000001) == 2)
    add('lemire_threshold', (TWO64 - 3) % 3 == 1 and (TWO64 - (1 << 39)) % (1 << 39) == 0)
    for name, r, _, frozen, idx in PRIMARY + SECONDARY:
        alpha = ALPHA_PRIMARY if idx < 4 else ALPHA_SECONDARY
        add('frozen_half_width_%s' % name, round8(half_width(r, alpha, N_BLOCKS)) == Decimal(frozen))
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
    decimal.getcontext().prec = 110
    parser = argparse.ArgumentParser(description='Independent MMEM record/estimate verifier (fails closed).')
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
    m = sub.add_parser('manifest-interface')
    m.add_argument('--run-dir', required=True)
    m.add_argument('--receipt', required=True)
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
    elif args.command == 'manifest-interface':
        inputs = [args.run_dir]
        namespace = 'none (NONSCIENTIFIC manifest-interface placeholder files)'
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
            rec.fatal('SELF_CHECK', 'self-check failed: %s' % [c['name'] for c in checks if c['result'] != 'PASS'])
        if args.command == 'production':
            run_production(args, rec, ctx)
        elif args.command == 'fixture':
            run_fixture(args, rec, ctx)
        elif args.command == 'manifest-interface':
            run_manifest_interface(args, rec, ctx)
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
        own_sha = hashlib.sha256(open(os.path.abspath(__file__), 'rb').read()).hexdigest()
    except Exception:
        own_sha = None
    receipt = {
        'receipt_schema': RECEIPT_SCHEMA,
        'status': 'PASS' if passed else 'INVALID',
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
        'scope': {'synthetic-estimates': 'NONSCIENTIFIC synthetic estimate-comparison exercise',
                  'manifest-interface': ('NONSCIENTIFIC manifest-interface exercise: manifest schema, entry '
                                         'uniqueness, byte counts and hashes only; no record was parsed')}.get(
                      args.command,
                      'record, hash and estimate agreement only; this receipt reports no estimate or decision'),
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
