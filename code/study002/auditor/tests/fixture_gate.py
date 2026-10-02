#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Future corruption gate for the independent PCONV auditor (PHASE2-PERFORMANCE-CONVERSION-002).

Python 3.6, standard library only. NOT RUN by the constructing worker. It never modifies the supplied layout
directory: every case runs on a temporary copy inside a new --work-dir.

Steps
  0. Controls: both verifiers must return PASS on an unmodified copy of the fixture layout, and the record
     verifier must return PASS on the synthetic estimate fixture. Without passing controls an INVALID result
     could not be attributed to the deliberate corruption, so the gate fails.
  1. audit_candidate_row  - flip bit 0 of the genotype of audit row 1000 (cell 0, update 8, candidate 104).
  2. permutation_row      - flip bit 0 of perm[0] of permutation record 7 (update 8).
  3. paired_path_hash     - flip bit 0 of byte 0 of the paired-trajectory hash in path record 5 (SHAM|ALL_M).
  4. reserved_field       - set path record 0 reserved_u32 (offset 92) to 1.
  5. estimate_record      - add 1 to the numerator of estimates_19.json[0].estimate_exact (Delta_P) in the
                            synthetic analysis; no other byte of the analysis differs from the control.
  Cases 1-4 must make BOTH the C++ replay verifier and the Python record verifier return INVALID.
  Case 5 must make the Python record verifier return INVALID.

Every INVALID receipt must also carry a bounded first discrepancy that names the intended category (Python)
and coordinates/field (both tools) and contains neither the original nor the corrupted value. The synthetic
receipts (control and corruption) must contain no synthetic estimate, bound or decision label. (Like every
verifier receipt, they list each input file's computed SHA-256 as provenance.)

Synthetic estimate fixture (documented, NONSCIENTIFIC): 4 block records with deterministic arithmetic late
sums (no random draw, no simulation) chosen to satisfy the SHAM block identities. estimates_19.json and
decision.json are written at the verifier's PROVISIONAL key names (docs/INPUT_CONTRACT.md section 6) with
precision-60 Decimal strings, n = 4. With n = 4 every Hoeffding half-width exceeds every interval threshold,
so the primary rule is 2 (START-DEPENDENT), all four secondary classifications are UNRESOLVED and the
interpretive branch is the rule-2/6 statement; the harness asserts this before writing. Because the fixture
and the verifier read the same provisional interface, this case tests the comparison logic, not the
producer interface.

Usage
  python3 -B tests/fixture_gate.py --layout-dir LAYOUT --replay-bin BUILD/pconv_audit_replay \
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

# PROVISIONAL fixture layout names (docs/INPUT_CONTRACT.md section 4), identical to both verifiers.
LAYOUT_FILES = ['layout_sample_README.txt', 'layout_sample_audit.bin', 'layout_sample_permutations.bin',
                'layout_sample_block.bin', 'layout_sample_paths.bin', 'layout_sample_updates.bin']
AUDIT_ROW_BYTES = 88
PATH_BYTES = 176
PERM_BYTES = 416
CELL_LABELS = ['INFO|ALL_F', 'INFO|ALL_M', 'NONINFO|ALL_F', 'NONINFO|ALL_M', 'SHAM|ALL_F', 'SHAM|ALL_M']
START_DEPENDENT = 'START-DEPENDENT; PRIMARY UNRESOLVED'
UNRESOLVED = 'UNRESOLVED'
BRANCH_NONE = ('No interpretive branch applies; the primary question is unresolved and the secondary families are '
               'reported without changing it.')
BLK = struct.Struct('<IBBBB7i6I6IBBHHHI')
TIMEOUT_SECONDS = 4 * 3600
DIAGNOSTIC_LIMIT = 483  # 480 characters plus the '...' truncation marker
COORDINATE = re.compile(r'\b(?:block|cell|update|candidate|record|draw|entry|step)[ =][0-9]+\b')
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
    """No synthetic estimate, exact fraction, bound or decision label may appear anywhere in the receipt."""
    leaked = any(s in receipt_text for s in forbidden)
    result['receipt_free_of_estimates_and_decisions'] = not leaked
    result['ok'] = result['ok'] and not leaked
    return result


# ------------------------------------------------------------------------------------------------
# Synthetic estimate fixture (test oracle written from OUTPUT_FORMATS.md and frozen_config.json, independent of
# the producer; it shares only the provisional key names with the verifier)


def synthetic_blocks(n):
    blocks = []
    for b in range(n):
        L = [(b * 37 + c * 101 + 5) % 2049 for c in range(6)]
        P = [(b * 911 + c * 4099 + 17) % 65537 for c in range(6)]
        L[5] = 2048 - L[4]
        P[5] = P[4]
        nums = [(P[2] + P[3]) - (P[0] + P[1]), L[1] - L[0], L[3] - L[2], (L[0] + L[1]) - (L[2] + L[3]),
                (L[2] + L[3]) - 2048, (P[4] + P[5]) - (P[0] + P[1]), (P[4] + P[5]) - (P[2] + P[3])]
        # c1_ok = 1, reserved 0; decoupling/recurrence/retry fields are arbitrary (unchecked in synthetic mode).
        blocks.append((b, 1, 1, 1, 1) + tuple(nums) + tuple(L) + tuple(P) + (1, 0, 0, 3 + b, 120 + b, 7 * b))
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
    sums = [sum(row[5 + i] for row in blocks) for i in range(7)]
    Ls = [sum(row[12 + c] for row in blocks) for c in range(6)]
    Ps = [sum(row[18 + c] for row in blocks) for c in range(6)]
    # (name, family, R, denominator, 2/alpha_each)
    specs = [('Delta_P', 'PRIMARY', 2, 131072, 120), ('D_INFO', 'PRIMARY', 2, 2048, 120),
             ('D_NONINFO', 'PRIMARY', 2, 2048, 120), ('E_INFO', 'ALLELE', 2, 4096, 80),
             ('E_NONINFO', 'ALLELE', 1, 4096, 80), ('B_INFO', 'PERFORMANCE', 2, 131072, 80),
             ('B_NONINFO', 'PERFORMANCE', 2, 131072, 80)]
    delta = Decimal(1) / Decimal(32)
    records = []
    forbidden = [START_DEPENDENT, UNRESOLVED, BRANCH_NONE]

    def ctx60():
        return decimal.Context(prec=60, rounding=decimal.ROUND_HALF_EVEN)

    def dec(f):
        with decimal.localcontext(ctx60()):
            return Decimal(f.numerator) / Decimal(f.denominator)

    for i, (name, family, r, den, two_over_alpha) in enumerate(specs):
        f = Fraction(sums[i], den * n)
        est = dec(f)
        with decimal.localcontext(ctx60()):
            h = Decimal(r) * (Decimal(two_over_alpha).ln() / (Decimal(2) * Decimal(n))).sqrt()
            lo, up = est - h, est + h
        if name in ('D_INFO', 'D_NONINFO'):
            assert not (lo > -delta and up < delta), 'synthetic fixture must trigger rule 2'
        if family != 'PRIMARY':
            assert not (lo > delta) and not (up < -delta) and not (up <= delta), 'secondary must be UNRESOLVED'
        rec = {'record': i + 1, 'name': name, 'family': family, 'estimate_exact': exact_text(f),
               'estimate': str(est), 'half_width': str(h), 'lower': str(lo), 'upper': str(up)}
        if family == 'PRIMARY':
            rec['family_decision'] = START_DEPENDENT
        else:
            rec['classification'] = UNRESOLVED
        records.append(rec)
    cells = ([('M_FREQUENCY_LATE|', Fraction(Ls[c], 2048 * n)) for c in range(6)] +
             [('ACCURACY_LATE|', 1 - Fraction(Ps[c], 65536 * n)) for c in range(6)])
    for k, (prefix, f) in enumerate(cells):
        records.append({'record': 8 + k, 'name': prefix + CELL_LABELS[k % 6], 'estimate_exact': exact_text(f),
                        'estimate': str(dec(f)), 'lower': None, 'upper': None})
    for rec in records:
        for key in ('estimate_exact', 'estimate', 'half_width', 'lower', 'upper'):
            value = rec.get(key)
            if isinstance(value, str) and len(value) >= 7:
                forbidden.append(value)
    decision = {'primary_decision': START_DEPENDENT, 'decision_rule_applied': 2,
                'bounded_interval_wholly_inside_minus_delta_plus_delta': None,
                'secondary_classifications': {'E_INFO': UNRESOLVED, 'E_NONINFO': UNRESOLVED,
                                              'B_INFO': UNRESOLVED, 'B_NONINFO': UNRESOLVED},
                'interpretive_branch': BRANCH_NONE,
                'interpretation_labels': ['NONSCIENTIFIC synthetic fixture label']}
    analysis = os.path.join(work, 'synthetic_analysis_control')
    os.makedirs(analysis)
    write_json(os.path.join(analysis, 'estimates_19.json'), records)
    write_json(os.path.join(analysis, 'decision.json'), decision)

    corrupt = os.path.join(work, 'synthetic_analysis_corrupt_record1')
    os.makedirs(corrupt)
    bad = copy.deepcopy(records)
    original = bad[0]['estimate_exact']
    num, den = original.split('/')
    bad[0]['estimate_exact'] = '%d/%s' % (int(num) + 1, den)
    forbidden.append(bad[0]['estimate_exact'])
    write_json(os.path.join(corrupt, 'estimates_19.json'), bad)
    write_json(os.path.join(corrupt, 'decision.json'), decision)
    return blocks_path, analysis, corrupt, [original, bad[0]['estimate_exact']], forbidden


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
    row = 1000 * AUDIT_ROW_BYTES
    perm = 7 * PERM_BYTES + 12
    corruptions = [
        ('audit_candidate_row', 'layout_sample_audit.bin', row + 16, 0x01, None, (row + 16, 4),
         ['cell=0 update=8 candidate=104', 'audit_row.genotype'],
         'MISMATCH', ['cell 0 update 8 candidate 104', 'field mismatch']),
        ('permutation_row', 'layout_sample_permutations.bin', perm, 0x01, None, (perm, 32),
         ['update=8', 'audit_permutation.perm'],
         'PERMUTATION', ['update 8', 'field perm']),
        ('paired_path_hash', 'layout_sample_paths.bin', 5 * PATH_BYTES + 96, 0x01, None, (5 * PATH_BYTES + 96, 32),
         ['cell=5', 'path_record.paired_trajectory_sha256'],
         'N1_N2_PAIRED_HASH', ['cell 5', 'paired_trajectory_sha256']),
        ('reserved_field', 'layout_sample_paths.bin', 0 * PATH_BYTES + 92, None, 1, (0 * PATH_BYTES + 92, 4),
         ['cell=0', 'path_record.reserved_u32'],
         'RESERVED', ['cell 0', 'reserved_u32']),
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


def main():
    parser = argparse.ArgumentParser(description='PCONV auditor corruption gate (fixture namespace only).')
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
    expected_results = 2 + 4 * 2 + 2
    gate_pass = (gate_error is None and source_unchanged and len(results) == expected_results and
                 all(r['ok'] for r in results))
    receipt = {
        'receipt_schema': 'PCONV-AUDITOR-FIXTURE-GATE-RECEIPT-1',
        'study_id': 'PHASE2-PERFORMANCE-CONVERSION-002',
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
