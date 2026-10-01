#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Future corruption gate for the independent MMEM auditor (Python 3.6, standard library only).

NOT RUN by the constructing worker. It never modifies the supplied layout directory: every case runs
on a temporary copy inside a new --work-dir.

Steps
  0. Controls: both verifiers must return PASS on an unmodified copy of the fixture layout, and the
     record verifier must return PASS on the synthetic estimate fixture and on the manifest-interface
     fixture. Without passing controls an INVALID result could not be attributed to the deliberate
     corruption, so the gate fails.
  1. audit_candidate_row  - flip bit 0 of the genotype of audit row 1000 (cell 0, update 8, candidate 104).
  2. paired_path_hash     - flip bit 0 of byte 0 of the paired-trajectory hash in path record 5 (SHAM ZERO ALL_M).
  3. reserved_field       - set path record 0 reserved0 (offset 92) to 1.
  4. estimate_record      - add 1 to the numerator of estimates_22.json[0].estimate_exact (C_abs) in the
                            synthetic analysis; no other byte of the analysis differs from the control.
  5. manifest_output_sha256 - change the last hex digit of run_manifest.json outputs[37].sha256 in a copy
                            of the manifest-interface fixture; no other byte of the copy differs.
  Cases 1-3 must make BOTH the C++ replay verifier and the Python record verifier return INVALID.
  Cases 4-5 must make the Python record verifier return INVALID.

Every INVALID receipt must also carry a bounded first discrepancy that names the intended category
(Python) and coordinates/field (both tools) and contains neither the original nor the corrupted value
(case 5: neither the observed nor the manifest-claimed hash nor the file bytes). The synthetic receipts
(control and corruption) must contain no synthetic estimate or decision label; the manifest-interface
receipts must contain no placeholder file bytes and no manifest-claimed corrupted hash. (Like every
verifier receipt, they list each input file's computed SHA-256 as provenance.)

Synthetic estimate fixture (documented, NONSCIENTIFIC): 4 block records with deterministic arithmetic
late sums (no random draw, no simulation) chosen to satisfy the N1/N2 block identities. estimates_22.json
and decision.json are written in the exact producer interface (PRODUCER_OUTPUT_INTERFACE_CONTRACT_R2.json:
22 ordered records with exact key sets; '%d/%d' fractions; precision-60 Decimal strings in the revision-2
order; five exact decision keys; the six exact interpretation labels), with n_blocks = 4. With n=4 the
Hoeffding half-widths exceed every interval threshold, so the primary rule is 2 (START-DEPENDENT), both
secondary classifications are UNRESOLVED and both relations are UNCERTAINTY; the harness asserts this
before writing.

Manifest-interface fixture (documented, NONSCIENTIFIC): 97 tiny placeholder files (one ASCII line each,
no record content) at the exact required relative paths and a run_manifest.json with the exact revision-2
root key set, the exact contract values, placeholder typed fields, and outputs[*] holding each file's
true byte count and SHA-256. The verifier's manifest-interface mode only hashes these files.

Usage
  python3 -B tests/fixture_gate.py --layout-dir LAYOUT --replay-bin BUILD/mmem_audit_replay \
      --record-verifier python/mmem_record_verifier.py --work-dir NEW_DIR
Exit 0 iff every expectation holds; the gate receipt is NEW_DIR/FIXTURE_GATE_RECEIPT.json (create-exclusive).
If NEW_DIR cannot be created the gate exits 2 without a receipt; if an unexpected exception occurs after
NEW_DIR exists, the gate still writes the receipt with status FAIL and exits 1; if the receipt cannot be
written it exits 3. No timing is measured here; an outer runner may wrap each command in /usr/bin/time -v.
"""

import argparse
import copy
import datetime
import decimal
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
from decimal import Decimal
from fractions import Fraction

LAYOUT_FILES = ['layout_sample_README.txt', 'layout_sample_audit.bin', 'layout_sample_block.bin',
                'layout_sample_paths.bin', 'layout_sample_updates.bin']
CELL_LABELS = ['ACTIVE|ZERO|ALL_F', 'ACTIVE|ZERO|ALL_M', 'ACTIVE|HALF|ALL_F', 'ACTIVE|HALF|ALL_M',
               'SHAM|ZERO|ALL_F', 'SHAM|ZERO|ALL_M', 'SHAM|HALF|ALL_F', 'SHAM|HALF|ALL_M']
START_DEPENDENT = 'START-DEPENDENT; SCIENTIFIC QUESTION UNRESOLVED'
UNRESOLVED = 'UNRESOLVED'
UNCERTAINTY = 'UNCERTAINTY'
CELL_CLASSIFICATION = 'DESCRIPTIVE; NO INFERENTIAL BOUND FROZEN'
# PRODUCER_OUTPUT_INTERFACE_CONTRACT_R2.json analysis.interpretation_labels_exact, in order.
INTERPRETATION_LABELS = [
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
]
# PRODUCER_OUTPUT_INTERFACE_CONTRACT_R2.json run_manifest.exact_root_keys.
MANIFEST_ROOT_KEYS = ['manifest', 'study_id', 'status', 'config_path', 'config_sha256', 'specification_sha256',
                      'terminal_review_sha256', 'key_namespace', 'compiler_version', 'cplusplus', 'threads',
                      'shards', 'blocks', 'started_utc', 'finished_utc', 'wall_seconds', 'total_objective_queries',
                      'expected_total_objective_queries', 'n1_failed_blocks', 'n2_failed_blocks',
                      'query_failed_blocks', 'total_output_bytes', 'regeneration_command', 'outputs']
AUDIT_FILE = 'audit_rows_blocks_0000_0063.bin'
MANIFEST_CORRUPT_INDEX = 37
BLK = struct.Struct('<IBBBB6i8I8I')
TIMEOUT_SECONDS = 4 * 3600
DIAGNOSTIC_LIMIT = 483  # 480 characters plus the '...' truncation marker
COORDINATE = re.compile(r'\b(?:block|cell|update|candidate|record|draw|entry)[ =][0-9]+\b')
BARE_NUMBER = re.compile(r'\b[0-9]+\b')


def utc_now():
    return datetime.datetime.utcnow().replace(microsecond=0).isoformat() + 'Z'


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as fh:
        while True:
            data = fh.read(1 << 22)
            if not data:
                break
            h.update(data)
    return h.hexdigest()


def safe_sha256(path):
    try:
        return sha256_file(path)
    except (OSError, IOError):
        return None


def snapshot(layout_dir):
    return dict((name, sha256_file(os.path.join(layout_dir, name))) for name in LAYOUT_FILES)


def read_field(path, offset, width):
    with open(path, 'rb') as fh:
        fh.seek(offset)
        data = fh.read(width)
    if len(data) != width:
        raise RuntimeError('field at offset %d beyond end of %s' % (offset, path))
    return data


def flip_byte(path, offset, xor_value=None, set_value=None):
    with open(path, 'r+b') as fh:
        fh.seek(offset)
        old = fh.read(1)
        if len(old) != 1:
            raise RuntimeError('corruption offset %d beyond end of %s' % (offset, path))
        new = (old[0] ^ xor_value) if xor_value is not None else set_value
        if new == old[0]:
            raise RuntimeError('corruption at %s:%d would not change the byte' % (path, offset))
        fh.seek(offset)
        fh.write(bytes([new]))


def value_tokens(raw):
    """Textual forms in which a corrupted little-endian field could leak into a diagnostic."""
    if len(raw) > 8:
        return [raw.hex()]
    v = int.from_bytes(raw, 'little')
    return [str(v), '%x' % v, '%08x' % v]


def run_verifier(argv, receipt):
    try:
        proc = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True,
                              timeout=TIMEOUT_SECONDS)
        code = proc.returncode
        tail = (proc.stdout[-400:], proc.stderr[-400:])
    except subprocess.TimeoutExpired:
        code, tail = None, ('', 'timeout')
    status = None
    first = None
    text = ''
    if os.path.exists(receipt):
        try:
            with open(receipt, 'r', encoding='utf-8') as fh:
                text = fh.read()
            body = json.loads(text)
            status = body.get('status')
            first = body.get('first_discrepancy')
        except (ValueError, OSError, AttributeError):
            status = 'UNREADABLE'
    return {'argv': argv, 'exit_code': code, 'receipt': receipt, 'receipt_status': status,
            'first_discrepancy': first, 'stdout_tail': tail[0], 'stderr_tail': tail[1]}, text


def expect(result, want_status):
    want_code = 0 if want_status == 'PASS' else 1
    result['expected_status'] = want_status
    result['ok'] = result['exit_code'] == want_code and result['receipt_status'] == want_status
    if want_status == 'PASS':
        result['ok'] = result['ok'] and result['first_discrepancy'] is None
    return result


def check_diagnostic(result, category, required, leaked_values):
    """The INVALID receipt must name the intended category/coordinates/field and no changed value."""
    first = result['first_discrepancy']
    if isinstance(first, dict):
        got_category, detail = first.get('category'), first.get('detail')
    else:
        got_category, detail = None, first
    problems = []
    if not isinstance(detail, str) or not detail:
        problems.append('no first-discrepancy text')
        detail = ''
    if len(detail) > DIAGNOSTIC_LIMIT:
        problems.append('first discrepancy exceeds the bounded length')
    if category is not None and got_category != category:
        problems.append('category is not %s' % category)
    for text in required:
        if text not in detail:
            problems.append('does not name %r' % text)
    remainder = COORDINATE.sub(' ', detail)
    if BARE_NUMBER.search(remainder):
        problems.append('carries a numeric value outside coordinates')
    for token in leaked_values:
        if re.search(r'(?<![0-9A-Za-z])%s(?![0-9A-Za-z])' % re.escape(token), remainder, re.IGNORECASE):
            problems.append('contains a changed value')
            break
    result['diagnostic_expectation'] = {'category': category, 'names': required}
    result['diagnostic_problems'] = problems
    result['ok'] = result['ok'] and not problems
    return result


def check_receipt_free_of(result, receipt_text, forbidden):
    """No synthetic estimate, exact fraction or decision label may appear anywhere in the receipt."""
    leaked = any(s in receipt_text for s in forbidden)
    result['receipt_free_of_estimates_and_decisions'] = not leaked
    result['ok'] = result['ok'] and not leaked
    return result


# ------------------------------------------------------------------------------------------------
# Synthetic estimate fixture (test oracle written from the interface contract, independent of both the
# producer and the verifier code)


def synthetic_blocks(n):
    blocks = []
    for b in range(n):
        L = [(b * 37 + c * 101 + 5) % 2049 for c in range(8)]
        P = [(b * 911 + c * 4099 + 17) % 65537 for c in range(8)]
        L[5] = 2048 - L[4]
        L[7] = 2048 - L[6]
        P[5] = P[4]
        P[7] = P[6]
        p_abs = (P[6] + P[7]) - (P[2] + P[3])
        nums = [L[2] + L[3] - 2048, (L[2] + L[3]) - (L[0] + L[1]), L[3] - L[2], L[1] - L[0],
                p_abs, p_abs - ((P[4] + P[5]) - (P[0] + P[1]))]
        blocks.append((b, 1, 1, 1, 1) + tuple(nums) + tuple(L) + tuple(P))
    return blocks


def exact_text(f):
    return '%d/%d' % (f.numerator, f.denominator)


def write_json(path, obj):
    with open(path, 'w', encoding='utf-8') as fh:
        json.dump(obj, fh, indent=2)
        fh.write('\n')


def write_synthetic(work, n):
    blocks = synthetic_blocks(n)
    blocks_path = os.path.join(work, 'synthetic_blocks', 'blocks.bin')
    os.makedirs(os.path.dirname(blocks_path))
    with open(blocks_path, 'wb') as fh:
        for row in blocks:
            fh.write(BLK.pack(*row))
    sums = [sum(row[5 + i] for row in blocks) for i in range(6)]
    Ls = [sum(row[11 + c] for row in blocks) for c in range(8)]
    Ps = [sum(row[19 + c] for row in blocks) for c in range(8)]
    specs = [('C_abs', 1, 4096, '0.0125'), ('C_rec', 2, 4096, '0.0125'), ('D_HALF', 2, 2048, '0.0125'),
             ('D_ZERO', 2, 2048, '0.0125'), ('P_abs', 2, 131072, '0.025'), ('P_rec', 4, 131072, '0.025')]
    delta = Decimal(1) / Decimal(32)
    records = []
    forbidden = [START_DEPENDENT, UNRESOLVED, UNCERTAINTY]

    def ctx60():
        return decimal.Context(prec=60, rounding=decimal.ROUND_HALF_EVEN)

    def dec(f):
        with decimal.localcontext(ctx60()):
            return Decimal(f.numerator) / Decimal(f.denominator)

    # Revision-2 order: dec(Fraction); then half-width in one context; then lower/upper in another.
    for i, (name, r, den, alpha) in enumerate(specs):
        f = Fraction(sums[i], den * n)
        est = dec(f)
        with decimal.localcontext(ctx60()):
            inner = (Decimal(2) / Decimal(alpha)).ln() / (Decimal(2) * Decimal(n))
            h = Decimal(r) * inner.sqrt()
        with decimal.localcontext(ctx60()):
            lo, up = est - h, est + h
        if name in ('D_HALF', 'D_ZERO'):
            assert not (lo > -delta and up < delta), 'synthetic fixture must trigger rule 2'
        if i >= 4:
            assert not (lo > delta) and not (up < -delta) and not (up <= delta), 'secondary must be UNRESOLVED'
        rec = {'record': i + 1, 'name': name,
               'family': 'PRIMARY_ALLELE_ENRICHMENT' if i < 4 else 'SECONDARY_POPULATION_PERFORMANCE',
               'estimate_exact': exact_text(f), 'estimate': str(est), 'range_length': r, 'alpha_each': alpha,
               'n_blocks': n, 'half_width': str(h), 'lower': str(lo), 'upper': str(up)}
        if i < 4:
            rec['family_decision'] = START_DEPENDENT
        else:
            # UNRESOLVED classification (and rule 2 is neither pos nor neg) gives UNCERTAINTY.
            rec['classification'] = UNRESOLVED
            rec['relation_to_allele_enrichment_descriptive'] = UNCERTAINTY
            rec['cannot_alter_primary_decision'] = True
        records.append(rec)
    cells = ([('M_FREQUENCY_LATE|', Fraction(Ls[c], 2048 * n)) for c in range(8)] +
             [('ACCURACY_LATE|', 1 - Fraction(Ps[c], 65536 * n)) for c in range(8)])
    for k, (prefix, f) in enumerate(cells):
        records.append({'record': 7 + k, 'name': prefix + CELL_LABELS[k % 8],
                        'family': 'ABSOLUTE_CELL_MEAN_DESCRIPTIVE', 'estimate_exact': exact_text(f),
                        'estimate': str(dec(f)), 'lower': None, 'upper': None,
                        'classification': CELL_CLASSIFICATION})
    for rec in records:
        for key in ('estimate_exact', 'estimate', 'half_width', 'lower', 'upper'):
            value = rec.get(key)
            if isinstance(value, str) and len(value) >= 7:
                forbidden.append(value)
    decision = {'primary_decision': START_DEPENDENT, 'decision_rule_applied': 2,
                'adverse_selection_upper_C_abs_below_minus_Delta': None,
                'secondary_classifications': {'P_abs': UNRESOLVED, 'P_rec': UNRESOLVED},
                'interpretation_labels': list(INTERPRETATION_LABELS)}
    analysis = os.path.join(work, 'synthetic_analysis_control')
    os.makedirs(analysis)
    write_json(os.path.join(analysis, 'estimates_22.json'), records)
    write_json(os.path.join(analysis, 'decision.json'), decision)

    corrupt = os.path.join(work, 'synthetic_analysis_corrupt_record1')
    os.makedirs(corrupt)
    bad = copy.deepcopy(records)
    original = bad[0]['estimate_exact']
    num, den = original.split('/')
    bad[0]['estimate_exact'] = '%d/%s' % (int(num) + 1, den)
    forbidden.append(bad[0]['estimate_exact'])
    write_json(os.path.join(corrupt, 'estimates_22.json'), bad)
    write_json(os.path.join(corrupt, 'decision.json'), decision)
    return blocks_path, analysis, corrupt, [original, bad[0]['estimate_exact']], forbidden


# ------------------------------------------------------------------------------------------------
# Manifest-interface fixture (NONSCIENTIFIC placeholder files; written from the interface contract)


def manifest_paths():
    """IFACE-R2 outputs_exact_paths: shards 00..31 x updates/paths/blocks, plus the shard_00 audit file."""
    paths = []
    for s in range(32):
        for name in ('updates.bin', 'paths.bin', 'blocks.bin'):
            paths.append('shards/shard_%02d/%s' % (s, name))
        if s == 0:
            paths.append('shards/shard_00/' + AUDIT_FILE)
    assert len(paths) == 97 and len(set(paths)) == 97
    return paths


def write_manifest_fixture(work):
    control = os.path.join(work, 'manifest_interface_control')
    outputs = []
    contents = []
    for k, rel in enumerate(manifest_paths()):
        data = ('NONSCIENTIFIC MMEM manifest-interface placeholder %03d\n' % k).encode('ascii')
        path = os.path.join(control, *rel.split('/'))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'wb') as fh:
            fh.write(data)
        outputs.append({'path': rel, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
        contents.append(data.decode('ascii').strip())

    def placeholder_sha(text):
        return hashlib.sha256(('NONSCIENTIFIC placeholder ' + text).encode('ascii')).hexdigest()

    manifest = {
        'manifest': 'MMEM-RUN-MANIFEST-1', 'study_id': 'PHASE2-MUTABLE-MEMORY-001', 'status': 'COMPLETE',
        'config_path': 'NONSCIENTIFIC/manifest-interface-fixture/frozen_config.json',
        'config_sha256': placeholder_sha('config'), 'specification_sha256': placeholder_sha('specification'),
        'terminal_review_sha256': placeholder_sha('terminal review'), 'key_namespace': 'production-r1',
        'compiler_version': 'NONSCIENTIFIC placeholder; nothing was compiled', 'cplusplus': 201703,
        'threads': 1, 'shards': 32, 'blocks': 41600, 'started_utc': '1970-01-01T00:00:00Z',
        'finished_utc': '1970-01-01T00:00:00Z', 'wall_seconds': 0,
        'total_objective_queries': 10905190400, 'expected_total_objective_queries': 10905190400,
        'n1_failed_blocks': 0, 'n2_failed_blocks': 0, 'query_failed_blocks': 0,
        'total_output_bytes': sum(e['bytes'] for e in outputs),
        'regeneration_command': 'NONSCIENTIFIC placeholder; no command was run', 'outputs': outputs}
    assert sorted(manifest) == sorted(MANIFEST_ROOT_KEYS)
    write_json(os.path.join(control, 'run_manifest.json'), manifest)

    corrupt = os.path.join(work, 'manifest_interface_corrupt_sha256')
    shutil.copytree(control, corrupt)
    bad = copy.deepcopy(manifest)
    entry = bad['outputs'][MANIFEST_CORRUPT_INDEX]
    observed = entry['sha256']
    entry['sha256'] = observed[:-1] + ('0' if observed[-1] != '0' else '1')
    write_json(os.path.join(corrupt, 'run_manifest.json'), bad)
    return control, corrupt, entry['path'], [observed, entry['sha256']], contents


# ------------------------------------------------------------------------------------------------


def run_gate(args, layout, work, results, state):
    receipts = os.path.join(work, 'receipts')
    os.mkdir(receipts)
    state['before'] = snapshot(layout)

    def replay(case, layout_copy):
        receipt = os.path.join(receipts, '%s.replay.json' % case)
        return run_verifier([args.replay_bin, '--mode', 'fixture', '--layout-dir', layout_copy,
                             '--receipt', receipt], receipt)[0]

    def records(case, layout_copy):
        receipt = os.path.join(receipts, '%s.records.json' % case)
        return run_verifier([args.python, '-B', args.record_verifier, 'fixture', '--layout-dir', layout_copy,
                             '--receipt', receipt], receipt)[0]

    def fresh_copy(case):
        dst = os.path.join(work, 'layout_' + case)
        shutil.copytree(layout, dst)
        return dst

    control = fresh_copy('control')
    results.append(dict(case='control', verifier='replay', **expect(replay('control', control), 'PASS')))
    results.append(dict(case='control', verifier='records', **expect(records('control', control), 'PASS')))

    # (case, file, byte offset, xor, set, (field offset, width), replay names, records category, records names)
    corruptions = [
        ('audit_candidate_row', 'layout_sample_audit.bin', 1000 * 72 + 16, 0x01, None, (1000 * 72 + 16, 4),
         ['cell=0 update=8 candidate=104', 'audit_row.genotype'],
         'MISMATCH', ['cell 0 update 8 candidate 104', 'field mismatch']),
        ('paired_path_hash', 'layout_sample_paths.bin', 5 * 128 + 96, 0x01, None, (5 * 128 + 96, 32),
         ['cell=5', 'path_record.paired_trajectory_sha256'],
         'N1_N2_PAIRED_HASH', ['cell 5', 'paired_trajectory_sha256']),
        ('reserved_field', 'layout_sample_paths.bin', 0 * 128 + 92, None, 1, (0 * 128 + 92, 4),
         ['cell=0', 'path_record.reserved0'],
         'RESERVED', ['cell 0', 'reserved0']),
    ]
    for case, name, offset, xor_value, set_value, field, replay_names, rec_category, rec_names in corruptions:
        copy_dir = fresh_copy(case)
        target = os.path.join(copy_dir, name)
        old = read_field(target, field[0], field[1])
        flip_byte(target, offset, xor_value=xor_value, set_value=set_value)
        new = read_field(target, field[0], field[1])
        leaked = value_tokens(old) + value_tokens(new)
        res = expect(replay(case, copy_dir), 'INVALID')
        results.append(dict(case=case, verifier='replay', corrupted_file=name, offset=offset,
                            **check_diagnostic(res, None, replay_names, leaked)))
        res = expect(records(case, copy_dir), 'INVALID')
        results.append(dict(case=case, verifier='records', corrupted_file=name, offset=offset,
                            **check_diagnostic(res, rec_category, rec_names, leaked)))

    blocks_path, analysis_ok, analysis_bad, changed, forbidden = write_synthetic(work, 4)
    for case, adir, want in (('synthetic_control', analysis_ok, 'PASS'), ('estimate_record', analysis_bad, 'INVALID')):
        receipt = os.path.join(receipts, '%s.records.json' % case)
        res, text = run_verifier([args.python, '-B', args.record_verifier, 'synthetic-estimates', '--blocks-file',
                                  blocks_path, '--n-blocks', '4', '--analysis-dir', adir, '--receipt', receipt],
                                 receipt)
        res = expect(res, want)
        if want == 'INVALID':
            res = check_diagnostic(res, 'ESTIMATE', ['record 1 field estimate_exact'], changed)
        res = check_receipt_free_of(res, text, forbidden)
        results.append(dict(case=case, verifier='records', **res))

    m_control, m_corrupt, m_rel, m_hashes, m_contents = write_manifest_fixture(work)
    for case, rdir, want in (('manifest_interface_control', m_control, 'PASS'),
                             ('manifest_output_sha256', m_corrupt, 'INVALID')):
        receipt = os.path.join(receipts, '%s.records.json' % case)
        res, text = run_verifier([args.python, '-B', args.record_verifier, 'manifest-interface', '--run-dir', rdir,
                                  '--receipt', receipt], receipt)
        res = expect(res, want)
        if want == 'INVALID':
            # May name the entry index, path and field; never the observed or claimed hash or file bytes.
            res = check_diagnostic(res, 'MANIFEST_HASH',
                                   ['outputs entry %d' % MANIFEST_CORRUPT_INDEX, 'field sha256', m_rel],
                                   m_hashes + [m_contents[MANIFEST_CORRUPT_INDEX]])
        # The claimed (corrupted) hash is never computed, and no file bytes belong in any receipt.
        res = check_receipt_free_of(res, text, [m_hashes[1]] + m_contents)
        results.append(dict(case=case, verifier='records', **res))


def main():
    parser = argparse.ArgumentParser(description='MMEM auditor corruption gate (fixture namespace only).')
    parser.add_argument('--layout-dir', required=True)
    parser.add_argument('--replay-bin', required=True)
    parser.add_argument('--record-verifier', required=True)
    parser.add_argument('--work-dir', required=True)
    parser.add_argument('--python', default=sys.executable)
    args = parser.parse_args()

    layout = os.path.abspath(args.layout_dir)
    work = os.path.abspath(args.work_dir)
    try:
        os.mkdir(work)  # fails if it already exists
    except OSError as exc:
        sys.stderr.write('fixture_gate: cannot create work directory %s: %s; no receipt written\n'
                         % (work, exc.strerror or exc))
        return 2
    started = utc_now()
    results = []
    state = {'before': None}
    gate_error = None
    try:
        run_gate(args, layout, work, results, state)
    except Exception as exc:  # fail closed: the receipt below records FAIL
        gate_error = '%s: %s' % (type(exc).__name__, exc)
    try:
        after = snapshot(layout)
    except Exception as exc:
        after = None
        gate_error = gate_error or '%s: %s' % (type(exc).__name__, exc)
    source_unchanged = state['before'] is not None and state['before'] == after
    expected_results = 2 + 3 * 2 + 2 + 2
    gate_pass = (gate_error is None and source_unchanged and len(results) == expected_results and
                 all(r['ok'] for r in results))
    receipt = {
        'receipt_schema': 'MMEM-AUDITOR-FIXTURE-GATE-RECEIPT-3',
        'status': 'PASS' if gate_pass else 'FAIL',
        'started_at_utc': started,
        'finished_at_utc': utc_now(),
        'layout_dir': layout,
        'layout_sha256_before': state['before'],
        'layout_unchanged': source_unchanged,
        'replay_bin': os.path.abspath(args.replay_bin),
        'replay_bin_sha256': safe_sha256(args.replay_bin),
        'record_verifier': os.path.abspath(args.record_verifier),
        'record_verifier_sha256': safe_sha256(args.record_verifier),
        'gate_source_sha256': safe_sha256(os.path.abspath(__file__)),
        'gate_error': gate_error,
        'results_expected': expected_results,
        'results': results,
        'timing_measured': False,
        'is_scientific_result': False,
    }
    try:
        fd = os.open(os.path.join(work, 'FIXTURE_GATE_RECEIPT.json'), os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o444)
        with os.fdopen(fd, 'w', encoding='utf-8') as fh:
            json.dump(receipt, fh, indent=2, sort_keys=True)
            fh.write('\n')
    except OSError as exc:
        sys.stderr.write('fixture_gate: could not create gate receipt: %s\n' % (exc.strerror or exc))
        return 3
    sys.stdout.write('fixture_gate %s\n' % receipt['status'])
    return 0 if gate_pass else 1


if __name__ == '__main__':
    sys.exit(main())
