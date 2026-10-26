#!/usr/bin/python3
# Independent auditor fixture implementation. Python >=3.6 standard library only.
# No case-id dispatch, expected-file reads, scientific CLI overrides or third-party modules.
import os
import sys
import stat
import re
import json
import hashlib
import struct
import ctypes
from fractions import Fraction as F

STUDY = 'PHASE3-BIT-SUSTAINED-RETRIEVAL-003'
CORE = 'e60b367dbab81dc1388f06f9d7be6da90b927e8302b4d7001be544976abe8889'
INVENTORY = '4cc9b10958a04c458aeba5ab3788dd54153b1ce18fc2cb83fe3847e012599bd6'
PROTOCOL = '30754cf9de941f09b8086e8ffb27a2b4af42882c6bfcca484928efa926ae6b98'
CONTRACT = 'ec34610b22fdbd1e0a31523995e9ffaa201b06840fb2da4b17f4fc44386e9913'
KEYHASH = '636c558fdd866cd3d4f60eeee30915d7dce1d2001fbcbd1c5773acb56bcbe522'
PURPOSES = ('INITIAL_GENOTYPE', 'TARGET_INNOVATION', 'TARGET_COPY', 'FRESH_MASK',
            'SCOUT_MASK', 'LOCAL_BIT', 'DONOR_KEY', 'SURVIVAL_UNIFORM', 'POLICY_MUTATION')
TIERS = {'fixture-r1': (0, 16), 'benchmark-r1': (1, 64), 'production-r1': (2, 20000)}
KINDS = set('KEYS KAT ADDRESS STATE LEMIRE OPERATOR CHAIN TARGET TOTALS INFERENCE CLASSIFY CERTIFICATE RECORD CHUNK_HEADER MANIFEST CLI'.split())
MASK32, MASK64 = (1 << 32) - 1, (1 << 64) - 1
RECORD, HASH_OFFSET = 208216, 208184

class Failure(Exception):
    def __init__(self, code, exit_code, where):
        self.code, self.exit_code, self.where = code, exit_code, where
        Exception.__init__(self, code)

def fail(code='FORMAT', exit_code=2, field='payload', **where):
    location = dict(where)
    if field != 'payload' or not location:
        location['field'] = field
    raise Failure(code, exit_code, location)

def check(ok, code='FORMAT', exit_code=2, field='payload', **where):
    if not ok:
        fail(code, exit_code, field, **where)

def canonical(o):
    return (json.dumps(o, sort_keys=True, separators=(',', ':'), ensure_ascii=True) + '\n').encode('ascii')

def strict_json(b, canon=True):
    def pairs(items):
        d = {}
        for k, v in items:
            if k in d:
                raise ValueError('duplicate')
            d[k] = v
        return d
    def integer(s):
        if not re.fullmatch(r'0|-?[1-9][0-9]*', s):
            raise ValueError('integer')
        return int(s)
    def forbidden(s):
        raise ValueError('noninteger')
    def walk(x):
        if x is None or isinstance(x, float):
            raise ValueError('type')
        if isinstance(x, str):
            x.encode('ascii')
        elif isinstance(x, list):
            for v in x:
                walk(v)
        elif isinstance(x, dict):
            for k, v in x.items():
                walk(k)
                walk(v)
    x = json.loads(b.decode('ascii'), object_pairs_hook=pairs, parse_int=integer,
                   parse_float=forbidden, parse_constant=forbidden)
    walk(x)
    if canon and canonical(x) != b:
        raise ValueError('canonical')
    return x

def exact(o, names):
    check(type(o) is dict and set(o) == set(names.split()))

def uint(x, hi, lo=0):
    return type(x) is int and lo <= x <= hi

def hval(x, n):
    check(type(x) is str and re.fullmatch('[0-9a-f]{%d}' % n, x) is not None)
    return int(x, 16)

def bhex(x, nonempty=False):
    check(type(x) is str and len(x) % 2 == 0 and re.fullmatch('[0-9a-f]*', x) is not None and (x or not nonempty))
    return bytes.fromhex(x)

def hh(x, n=8):
    return format(x, '0%dx' % n)

def pop(x):
    return bin(x).count('1')

def digest(b):
    return hashlib.sha256(b).hexdigest()

def lexical(s):
    return (type(s) is str and len(s) > 1 and s.startswith('/') and
            all(32 <= ord(c) <= 126 for c in s) and
            all(x not in ('', '.', '..') for x in s[1:].split('/')))

def pathcheck(s, absent=False):
    check(lexical(s), 'CLI', 2, 'argv')
    pieces = s[1:].split('/')
    q = ''
    for i, p in enumerate(pieces):
        q += '/' + p
        try:
            st = os.lstat(q)
        except FileNotFoundError:
            check(absent and i == len(pieces)-1, 'CLI', 2, 'argv')
            return
        check(not stat.S_ISLNK(st.st_mode), 'CLI', 2, 'argv')
        if i < len(pieces)-1:
            check(stat.S_ISDIR(st.st_mode), 'CLI', 2, 'argv')
        else:
            check(not absent, 'CLI', 2, 'argv')

def cli(argv):
    check(type(argv) is list and len(argv) > 0 and all(type(x) is str and all(32 <= ord(c) <= 126 for c in x) for x in argv), 'CLI', 2, 'argv')
    flags = {
        'brs3_produce': {'--inputs', '--tier', '--output', '--threads'},
        'brs3_analyze': {'--inputs', '--tier', '--data', '--results', '--threads'},
        'brs3_audit': {'--inputs', '--tier', '--data', '--results', '--report', '--threads'},
        'brs3_fixture': {'--inputs', '--case', '--output'}}
    check(argv[0] in flags, 'CLI', 2, 'argv')
    req = flags[argv[0]]
    check(len(argv) == 1+2*len(req), 'CLI', 2, 'argv')
    out = {}
    for i in range(1, len(argv), 2):
        k, v = argv[i:i+2]
        check(k in req and k not in out, 'CLI', 2, 'argv')
        if k == '--tier':
            check(v in TIERS, 'CLI', 2, 'argv')
        elif k == '--threads':
            check(re.fullmatch('[1-9][0-9]*', v) is not None and int(v) <= 32, 'CLI', 2, 'argv')
        else:
            check(lexical(v), 'CLI', 2, 'argv')
        out[k] = v
    return {'command': argv[0], 'options': out}

def regular_read(path, readonly=True):
    st = os.lstat(path)
    if not stat.S_ISREG(st.st_mode) or (readonly and st.st_mode & 0o222):
        raise ValueError('file type/mode')
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_CLOEXEC)
    with os.fdopen(fd, 'rb') as f:
        b = f.read()
    if len(b) != st.st_size:
        raise ValueError('file changed')
    return b

def relpath(s):
    return type(s) is str and s and not s.startswith('/') and all(x not in ('', '.', '..') for x in s.split('/')) and all(32 <= ord(c) <= 126 for c in s)

def authenticate(root):
    # Every read is drawn from the explicit V2 projection. No stat/open of a
    # delegated absent inventory entry, in particular any expectedpath.
    try:
        pathcheck(root)
        core_names = {'PROTOCOL.txt', 'ENGINEERING_CONTRACT.txt', 'KEYS.tsv', 'KAT.txt', 'PUBLIC_INPUT_INDEX.json'}
        files = {}
        for name in sorted(core_names):
            files[name] = regular_read(root+'/'+name)
        if digest(files['PUBLIC_INPUT_INDEX.json']) != CORE:
            raise ValueError('core identity')
        core = strict_json(files['PUBLIC_INPUT_INDEX.json'])
        if set(core) != core_names-{'PUBLIC_INPUT_INDEX.json'}:
            raise ValueError('core keys')
        for name, row in core.items():
            if type(row) is not dict or set(row) != {'bytes', 'sha256'} or type(row['bytes']) is not int:
                raise ValueError('core row')
            if len(files[name]) != row['bytes'] or digest(files[name]) != row['sha256']:
                raise ValueError('core bytes')
        invbytes = regular_read(root+'/BUNDLE_INVENTORY.json')
        if digest(invbytes) != INVENTORY:
            raise ValueError('inventory identity')
        inv = strict_json(invbytes)
        files['BUNDLE_INVENTORY.json'] = invbytes
        def read_auth(name):
            if not relpath(name) or name not in inv:
                raise ValueError('inventory path')
            # Only called on explicit projection members, never arbitrary inv rows.
            pathcheck(root+'/'+name)
            b = regular_read(root+'/'+name)
            row = inv[name]
            if type(row) is not dict or set(row) != {'bytes', 'sha256'} or type(row['bytes']) is not int or len(b) != row['bytes'] or digest(b) != row['sha256']:
                raise ValueError('inventory bytes')
            files[name] = b
        read_auth('FIXIN_BUNDLE/INDEX.json')
        index = strict_json(files['FIXIN_BUNDLE/INDEX.json'])
        if set(index) != {'schema', 'cases'} or type(index['schema']) is not int or index['schema'] != 1 or type(index['cases']) is not list:
            raise ValueError('index schema')
        members = set()
        for entry in index['cases']:
            name = entry['inputpath']
            if name in members or not name.startswith('FIXIN_BUNDLE/in/') or not relpath(name):
                raise ValueError('case membership')
            read_auth(name)
            if digest(files[name]) != entry['input_sha256']:
                raise ValueError('input digest')
            members.add(name)
        for name in ('FIXTURE_SPEC.txt', 'LEGACY_KEYS.tsv', 'INTERVAL_CERTIFICATE.json', 'FIXIN_BUNDLE/literals/SYNTHETIC_RECORD.bin'):
            read_auth(name)
        for name in core_names:
            if len(files[name]) != inv[name]['bytes'] or digest(files[name]) != inv[name]['sha256']:
                raise ValueError('core/inventory inconsistency')
        want_dirs = set()
        for name in files:
            parts = name.split('/')
            for k in range(1, len(parts)):
                want_dirs.add('/'.join(parts[:k]))
        got_files, got_dirs = set(), set()
        for base, dirs, names in os.walk(root, followlinks=False):
            for d in dirs:
                p = os.path.join(base, d)
                if not stat.S_ISDIR(os.lstat(p).st_mode):
                    raise ValueError('directory symlink')
                got_dirs.add(os.path.relpath(p, root))
            for name in names:
                p = os.path.join(base, name)
                if not stat.S_ISREG(os.lstat(p).st_mode):
                    raise ValueError('file symlink')
                got_files.add(os.path.relpath(p, root))
        if got_files != set(files) or got_dirs != want_dirs or len(files) != 107:
            raise ValueError('projected closure')
        return files, members
    except Exception:
        fail('INPUT', 2, 'inputs')

def publish(path, b):
    tmp = os.path.join(os.path.dirname(path), '.partial.'+os.path.basename(path))
    fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW | os.O_CLOEXEC, 0o600)
    with os.fdopen(fd, 'wb') as f:
        f.write(b)
        f.flush()
        os.fchmod(f.fileno(), 0o400)
        os.fsync(f.fileno())
    # FRCE Linux/glibc renameat2 is an explicitly frozen runtime dependency.
    libc = ctypes.CDLL(None, use_errno=True)
    rename = libc.renameat2
    rename.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
    rename.restype = ctypes.c_int
    if rename(-100, os.fsencode(tmp), -100, os.fsencode(path), 1) != 0:
        raise OSError(ctypes.get_errno(), 'exclusive atomic rename')
    d = os.open(os.path.dirname(path), os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC | os.O_NOFOLLOW)
    try:
        os.fsync(d)
    except Exception:
        rename(-100, os.fsencode(path), -100, os.fsencode(tmp), 1)
        raise
    finally:
        os.close(d)

def state(s):
    try:
        exact(s, 'x c policy_mask valid_mask')
        check(type(s['x']) is list and len(s['x']) == 32 and type(s['c']) is list and len(s['c']) == 32)
        x, c = [hval(v, 8) for v in s['x']], [hval(v, 8) for v in s['c']]
        p, v = hval(s['policy_mask'], 8), hval(s['valid_mask'], 8)
    except Failure:
        fail('FORMAT', 2, 'state')
    check(not v & ~p and all(v >> i & 1 or c[i] == 0 for i in range(32)), 'STATE', 2, 'state')
    return x, c, p, v

def state_obj(x, c, p, v):
    return {'x': [hh(a) for a in x], 'c': [hh(a) for a in c], 'policy_mask': hh(p), 'valid_mask': hh(v)}

def state_bytes(s):
    x, c, p, v = state(s)
    return struct.pack('<66I', *(x+c+[p, v]))

def tape(t):
    exact(t, 'fresh scout local donor survival policy_flip')
    out = {}
    for k, n, size in (('fresh', 32, 8), ('scout', 32, 8), ('local', 32, 8), ('donor', 96, 16)):
        check(type(t[k]) is list and len(t[k]) == n)
        out[k] = [hval(v, size) for v in t[k]]
    check(type(t['survival']) is list and len(t['survival']) == 32)
    out['survival'] = []
    for row in t['survival']:
        check(type(row) is list and 1 <= len(row) <= 256)
        out['survival'].append([hval(v, 16) for v in row])
    out['policy_flip'] = hval(t['policy_flip'], 8)
    return out

def lemire(w, xs):
    threshold = (1 << 64) % w
    for retry, x in enumerate(xs):
        m = x*w
        if m & MASK64 >= threshold:
            return threshold, m >> 64, retry
    fail('RETRY_CAP' if len(xs) == 256 else 'TAPE_EXHAUSTED', 4 if len(xs) == 256 else 3, retry=len(xs))

def operator(s, target, t, arm):
    x, cache, policy, valid = state(s)
    switched = policy & valid if arm == 'A' else 0
    g, mismatch, donors = [0]*128, [0]*128, []
    for i in range(32):
        g[i] = x[i]
        g[32+i] = cache[i] if (policy & valid & ~switched) & (1 << i) else x[i] ^ t['fresh'][i]
        g[64+i] = x[i] ^ t['scout'][i]
        for f in range(3):
            mismatch[32*f+i] = pop(g[32*f+i] ^ target)
        best = min(range(3), key=lambda f: (mismatch[32*f+i], t['donor'][3*i+f], f))
        donors.append(best)
        g[96+i] = g[32*best+i] ^ t['local'][i]
        mismatch[96+i] = pop(g[96+i] ^ target)
    weights = [1 << (32-h) for h in mismatch]
    selected, rejects = [], []
    nx, nc, np = [], [], 0
    for d in range(32):
        try:
            _, z, r = lemire(sum(weights), t['survival'][d])
        except Failure as e:
            fail(e.code, e.exit_code, arm=arm, draw=d, retry=e.where['retry'])
        cumulative = 0
        for j, w in enumerate(weights):
            cumulative += w
            if cumulative > z:
                break
        weights[j] = 0
        selected.append(j)
        rejects.append(r)
        parent = j % 32
        p = ((policy >> parent) ^ (t['policy_flip'] >> d)) & 1
        nx.append(g[j])
        nc.append(x[parent] if p else 0)
        np |= p << d
    return {'switched_mask': hh(switched), 'probe': [hh(v) for v in g[32:64]],
            'donor_family': donors, 'local_child': [hh(v) for v in g[96:128]],
            'selected_ids': selected, 'draw_rejections': rejects,
            'state': state_obj(nx, nc, np, np), 'queries': 128, 'M': pop(np),
            'A': sum(32-mismatch[j] for j in selected)}

def paired(states, target, t, order, u=None):
    result, errors = {}, {}
    for arm in order:
        try:
            result[arm] = operator(states[arm], target, t, arm)
        except Failure as e:
            errors[arm] = e
    for arm in ('C', 'A'):
        if arm in errors:
            e = errors[arm]
            if u is not None:
                e.where['u'] = u
            raise e
    return result

def philox(c, k):
    c, k = list(c), list(k)
    for r in range(10):
        a, b = 0xd2511f53*c[0], 0xcd9e8d57*c[2]
        c = [(b >> 32)^c[1]^k[0], b & MASK32, (a >> 32)^c[3]^k[1], a & MASK32]
        if r != 9:
            k = [(k[0]+0x9e3779b9) & MASK32, (k[1]+0xbb67ae85) & MASK32]
    return c

def key_case(files):
    rows, keys = [], set()
    for tier in TIERS:
        for purpose in PURPOSES:
            name = STUDY+'|'+tier+'|'+purpose
            d = hashlib.sha256(name.encode('ascii')).digest()
            a, b = struct.unpack('<II', d[:8])
            key = a+(b << 32)
            check(key not in keys, 'INPUT', 2, 'keys')
            keys.add(key)
            rows.append([name, d.hex(), hh(a), hh(b), hh(key, 16)])
    b = ('key_string\tsha256\tk0\tk1\tkey64\n'+''.join('\t'.join(r)+'\n' for r in sorted(rows))).encode('ascii')
    check(b == files['KEYS.tsv'], 'INPUT', 2, 'keys')
    old = files['LEGACY_KEYS.tsv'].decode('ascii').splitlines()
    check(old[0] == 'purpose_string\tsha256\tk0_le32\tk1_le32\tfirst64_le', 'INPUT', 2, 'keys')
    oldkeys, names = set(), []
    for line in old[1:]:
        vals = line.split('\t')
        check(len(vals) == 5, 'INPUT', 2, 'keys')
        name = vals[0]
        d = hashlib.sha256(name.encode('ascii')).digest()
        a, c = struct.unpack('<II', d[:8])
        check(vals[1:] == [d.hex(), hh(a), hh(c), hh(a+(c << 32), 16)], 'INPUT', 2, 'keys')
        oldkeys.add(a+(c << 32))
        names.append(name)
    check(len(names) == len(set(names)) == len(oldkeys) == 126 and names == sorted(names) and not keys & oldkeys, 'INPUT', 2, 'keys')
    check(files['LEGACY_KEYS.tsv'].endswith(b'\n'), 'INPUT', 2, 'keys')
    return {'keys_sha256': digest(b), 'legacy_sha256': digest(files['LEGACY_KEYS.tsv']),
            'new_count': len(keys), 'legacy_count': len(oldkeys), 'distinct': True, 'disjoint': True}

def rational(s):
    check(type(s) is str and re.fullmatch(r'(0|-?[1-9][0-9]*)/[1-9][0-9]*', s) is not None)
    f = F(s)
    check(str(f.numerator)+'/'+str(f.denominator) == s)
    return f

def rr(x):
    return str(x.numerator)+'/'+str(x.denominator)

def classify(lo, hi):
    if lo > F(1, 32):
        return 'MATERIAL_POSITIVE'
    if hi < F(-1, 32):
        return 'MATERIAL_NEGATIVE'
    if lo >= F(-1, 32) and hi <= F(1, 32):
        return 'BOUNDED_WITHIN_MARGIN'
    if lo > 0:
        return 'POSITIVE_MAGNITUDE_UNRESOLVED'
    if hi < 0:
        return 'NEGATIVE_MAGNITUDE_UNRESOLVED'
    return 'UNRESOLVED'

def outward(x, lower):
    v = x*10**12
    i = v.numerator//v.denominator if lower else -((-v.numerator)//v.denominator)
    return ('-' if i < 0 else '')+str(abs(i)//10**12)+'.'+str(abs(i) % 10**12).zfill(12)

def inference(m, a):
    out = {}
    for label, num, den, h in (('policy', m, 163840000, 19778834661), ('accuracy', a, 5242880000, 23018074131)):
        estimate, hw = F(num, den), F(h, 10**12)
        lo, hi = max(F(-1), estimate-hw), min(F(1), estimate+hw)
        out[label] = {'estimate': rr(estimate), 'lower': rr(lo), 'upper': rr(hi),
                      'lower_decimal': outward(lo, True), 'upper_decimal': outward(hi, False),
                      'classification': classify(lo, hi)}
    return out

def certificate(files):
    b = files['INTERVAL_CERTIFICATE.json']
    c = strict_json(b)
    check(c['schema'] == 1 and c['n_blocks'] == 20000 and c['n_terms'] == 48 and c['range_length'] == 2, 'MISMATCH', 3, 'certificate')
    def series(z):
        lo = 2*sum((z**(2*k+1)/F(2*k+1) for k in range(48)), F(0))
        return lo, lo+2*z**97/(97*(1-z*z))
    l2, u2 = series(F(1, 3))
    lq, uq = series(F(9, 41))
    check(c['series_bounds'] == {'ln2': [rr(l2), rr(u2)], 'ln25_over16': [rr(lq), rr(uq)]}, 'MISMATCH', 3, 'certificate')
    for name, alpha, logarg, factor, grid in (('policy', F(1, 25), 50, 5, 19778834661), ('accuracy', F(1, 100), 200, 7, 23018074131)):
        e = c['endpoints'][name]
        lo, hi, h, prev = factor*l2+lq, factor*u2+uq, F(grid, 10**12), F(grid-1, 10**12)
        want = {'alpha': rr(alpha), 'log_argument': logarg, 'ln_lower': rr(lo), 'ln_upper': rr(hi),
                'h_grid': rr(h), 'previous_grid': rr(prev), 'grid_squared_minus_upper': rr(h*h-hi/10000),
                'lower_minus_previous_squared': rr(lo/10000-prev*prev),
                'certified_chain': 'previous_grid^2 < ln_lower/10000 <= true_h^2 <= ln_upper/10000 < h_grid^2'}
        check(e == want and h*h > hi/10000 and prev*prev < lo/10000, 'MISMATCH', 3, 'certificate')
    check(rational(c['family_alpha']) == F(1,25)+F(1,100) == F(1,20), 'MISMATCH', 3, 'certificate')
    return {'certificate_sha256': digest(b), 'family_alpha': '1/20',
            'policy_grid_ceiling': '19778834661/1000000000000',
            'accuracy_grid_ceiling': '23018074131/1000000000000', 'both_exact_inequalities': True}

def results(tier, m, a):
    n = TIERS[tier][1]
    return ('BIT003_RESULTS_V1\nstatus=VALID\ntier='+tier+'\nn_blocks='+str(n)+'\nn_all_f='+str(n//2)+
            '\nn_all_m='+str(n//2)+'\npolicy_signed_sum='+str(m)+'\npolicy_denominator='+str(n*8192)+
            '\naccuracy_signed_sum='+str(a)+'\naccuracy_denominator='+str(n*262144)+'\ninference=PRODUCTION_ONLY\n')

def manifest(b, tier):
    try:
        j = strict_json(b, False)
    except (ValueError, UnicodeError):
        fail('FORMAT', 2, 'json')
    check(canonical(j) == b, 'FORMAT', 2, 'canonical')
    n = TIERS[tier][1]
    metadata = [('schema', 1), ('study', STUDY), ('tier', tier), ('protocol_sha256', PROTOCOL),
                ('contract_sha256', CONTRACT), ('keys_sha256', KEYHASH), ('block_count', n),
                ('history_horizon', 256), ('followup_horizon', 256), ('record_bytes', RECORD),
                ('chunk_record_limit', 100), ('all_f_blocks', n//2), ('all_m_blocks', n//2)]
    check(type(j) is dict and set(j) == set(k for k, v in metadata) | {'files'}, 'FORMAT', 2, 'keys')
    for k, v in metadata:
        check(type(j[k]) is type(v) and j[k] == v, 'FORMAT', 2, k)
    rows = j['files']
    check(type(rows) is list and len(rows) == (n+99)//100, 'FORMAT', 2, 'files')
    for i, row in enumerate(rows):
        check(type(row) is dict and set(row) == {'name', 'bytes', 'sha256'}, 'FORMAT', 2, 'files')
        check(row['name'] == 'chunk_%03d.bin' % i and type(row['bytes']) is int and row['bytes'] == 64+min(100,n-100*i)*RECORD and type(row['sha256']) is str and re.fullmatch('[0-9a-f]{64}', row['sha256']) is not None, 'FORMAT', 2, 'files')
    return {'block_count': n, 'chunk_count': len(rows), 'manifest_sha256': digest(b)}

def chunk(b, tier, index):
    code, n = TIERS[tier]
    check(uint(index, (n-1)//100), 'FORMAT', 2, 'chunk_index')
    check(len(b) == 64, 'FORMAT', 2, 'length')
    want = [(0,8,b'BRS3CH01','magic'), (8,2,1,'schema'), (10,1,code,'tier'), (11,1,0,'reserved'),
            (12,4,index,'chunk_index'), (16,4,100*index,'first_block_id'), (20,4,min(100,n-100*index),'record_count'),
            (24,4,RECORD,'record_bytes'), (28,2,256,'history_horizon'), (30,2,256,'followup_horizon'),
            (32,32,bytes.fromhex(PROTOCOL),'protocol_sha256')]
    for off, size, val, field in want:
        got = b[off:off+size] if type(val) is bytes else int.from_bytes(b[off:off+size], 'little')
        check(got == val, 'FORMAT', 2, field)
    return {'header_bytes': 64, 'record_count': min(100,n-100*index), 'first_block_id': 100*index,
            'expected_file_bytes': 64+min(100,n-100*index)*RECORD}

def record(r):
    check(len(r) == RECORD, 'FORMAT', 2, 'length')
    def get(off, n=4):
        return int.from_bytes(r[off:off+n], 'little')
    check(r[:8] == b'BRS3R001', 'FORMAT', 2, 'magic')
    check(get(8,2) == 1, 'FORMAT', 2, 'schema')
    code = get(10,1)
    check(code in (0,1,2), 'FORMAT', 2, 'tier')
    start, b = get(11,1), get(12)
    check(start in (0,1) and start == b & 1, 'FORMAT', 2, 'start_code')
    check(b < (16,64,20000)[code], 'FORMAT', 2, 'block_id')
    for off, n, val, field in [(16,2,256,'history_horizon'), (18,2,256,'followup_horizon'),
                               (20,4,RECORD,'record_bytes'), (24,8,0,'reserved')]:
        check(get(off,n) == val, 'FORMAT', 2, field)
    for i in range(768):
        check(get(205096+4*i) <= 8160, 'RANGE', 2, field='retry', index=i)
    check(hashlib.sha256(r[:HASH_OFFSET]).digest() == r[HASH_OFFSET:], 'HASH', 3, 'record_sha256')
    sums = [0,0,0,0]
    for k in range(769):
        vals = struct.unpack_from('<66I', r, 2080+264*k)
        x, c, p, v = vals[:32], vals[32:64], vals[64], vals[65]
        ok = not v & ~p and all(v >> i & 1 or c[i] == 0 for i in range(32))
        ok = ok and ((v == 0 and p == (MASK32 if start else 0)) if k == 0 else v == p)
        check(ok, 'STATE', 3, field='state', index=k)
        if k >= 257:
            arm = 0 if k <= 512 else 1
            u = k if arm == 0 else k-256
            target = get(32+4*(u-1))
            sums[arm] += pop(p)
            sums[arm+2] += sum(32-pop(g ^ target) for g in x)
    for i, field in enumerate(('M_C','M_A','A_C','A_A')):
        check(get(208168+4*i) == sums[i], 'MISMATCH', 3, field)
    return dict(record_bytes=RECORD, block_id=b, tier=('fixture-r1','benchmark-r1','production-r1')[code],
                record_sha256=r[HASH_OFFSET:].hex(), M_C=sums[0], M_A=sums[1], A_C=sums[2], A_A=sums[3])

def compute(kind, p, files):
    if kind in ('KEYS', 'CERTIFICATE'):
        exact(p, '')
        return key_case(files) if kind == 'KEYS' else certificate(files)
    if kind == 'KAT':
        exact(p, 'algorithm rounds')
        check(p['algorithm'] == 'philox4x32' and type(p['rounds']) is int and p['rounds'] == 10)
        vectors = []
        for line in files['KAT.txt'].decode('ascii').splitlines():
            row = line.split()
            if len(row) > 1 and row[:2] == ['philox4x32','10']:
                check(len(row) == 12)
                c, k, expected = [int(x,16) for x in row[2:6]], [int(x,16) for x in row[6:8]], [int(x,16) for x in row[8:12]]
                words = philox(c,k)
                check(words == expected, 'MISMATCH', 3, field='kat', index=len(vectors))
                vectors.append({'counter': [hh(x) for x in c], 'key': [hh(x) for x in k], 'words': [hh(x) for x in words]})
        check(len(vectors) == 3, 'MISMATCH', 3, 'kat')
        return {'vectors': vectors}
    if kind == 'ADDRESS':
        exact(p, 'tier purpose b u entity subindex words')
        for field in ('tier','purpose','b','u','entity','subindex','words'):
            if field == 'tier':
                ok = type(p[field]) is str and p[field] in TIERS
            elif field == 'purpose':
                ok = type(p[field]) is str and p[field] in PURPOSES
            elif field == 'b':
                ok = uint(p[field], TIERS[p['tier']][1]-1)
            elif field == 'u':
                ok = uint(p[field],0) if p['purpose'] == 'INITIAL_GENOTYPE' else uint(p[field],512,1)
            elif field == 'entity':
                limit = 0 if p['purpose'] in ('TARGET_COPY','TARGET_INNOVATION') else 95 if p['purpose'] == 'DONOR_KEY' else 31
                ok = uint(p[field],limit)
            elif field == 'subindex':
                limit = 31 if p['purpose'] == 'LOCAL_BIT' else 255 if p['purpose'] == 'SURVIVAL_UNIFORM' else 0
                ok = uint(p[field],limit)
            else:
                ok = type(p[field]) is list and len(p[field]) == 4 and all(type(v) is str and re.fullmatch('[0-9a-f]{8}',v) for v in p[field])
            check(ok, 'RANGE', 2, field)
        words, purpose = [int(v,16) for v in p['words']], p['purpose']
        if purpose == 'TARGET_COPY':
            ex = words[0] & 1
        elif purpose in ('LOCAL_BIT','POLICY_MUTATION'):
            ex = (words[0] & 31) == 0
        elif purpose in ('DONOR_KEY','SURVIVAL_UNIFORM'):
            ex = hh(words[0]+(words[1] << 32),16)
        else:
            ex = hh(words[0])
        return {'counter': [p[k] for k in ('b','u','entity','subindex')], 'extracted': ex}
    if kind == 'STATE':
        exact(p, 'state')
        b = state_bytes(p['state'])
        vals = struct.unpack('<66I', b)
        return {'state': state_obj(vals[:32], vals[32:64], vals[64], vals[65]), 'bytes_hex': b.hex(), 'bytes': len(b)}
    if kind == 'LEMIRE':
        exact(p, 'w x')
        w = hval(p['w'],16)
        check(1 <= w <= 1 << 39, 'RANGE', 2, 'w')
        check(type(p['x']) is list and 1 <= len(p['x']) <= 256)
        threshold, z, rejects = lemire(w, [hval(v,16) for v in p['x']])
        return {'threshold': hh(threshold,16), 'z': hh(z,16), 'rejections': rejects}
    if kind in ('OPERATOR','CHAIN'):
        exact(p, 'state u target tape arm_order threads' if kind == 'OPERATOR' else 'state first_u steps arm_order threads')
        state(p['state'])
        check(p['arm_order'] in (['C','A'],['A','C']) and uint(p['threads'],32,1))
        first = p['u'] if kind == 'OPERATOR' else p['first_u']
        check(uint(first,512,1))
        steps = [{'target': p['target'], 'tape': p['tape']}] if kind == 'OPERATOR' else p['steps']
        check(type(steps) is list and 1 <= len(steps) <= 4 and first+len(steps)-1 <= 512)
        # Validate all payload structure before performing any step.
        prepared = []
        for step in steps:
            exact(step,'target tape')
            prepared.append((hval(step['target'],8),tape(step['tape'])))
        states, answer = {'C': p['state'], 'A': p['state']}, []
        for i, (target,t) in enumerate(prepared):
            u = first+i
            pair = paired(states,target,t,p['arm_order'],u if kind == 'CHAIN' else None)
            if kind == 'OPERATOR':
                return pair
            answer.append(dict(u=u, **pair))
            states = {arm: pair[arm]['state'] for arm in ('C','A')}
        return {'steps': answer}
    if kind == 'TARGET':
        exact(p, 'first_u previous innovations copy')
        first = p['first_u']
        check(uint(first,512,1) and first != 2, 'RANGE', 2, 'first_u')
        previous = p['previous']
        check(type(previous) is list and len(previous) == (0 if first == 1 else 2) and all(type(x) is str and re.fullmatch('[0-9a-f]{8}',x) for x in previous), 'RANGE', 2, 'previous')
        ins, copy = p['innovations'], p['copy']
        check(type(ins) is list and 1 <= len(ins) <= 512 and first+len(ins)-1 <= 512 and all(type(x) is str and re.fullmatch('[0-9a-f]{8}',x) for x in ins), 'RANGE', 2, 'innovations')
        check(type(copy) is list and len(copy) == len(ins) and all(uint(x,1) for x in copy), 'RANGE', 2, 'copy')
        history = previous[:]
        for i, innovation in enumerate(ins):
            history.append(history[-2] if first+i >= 3 and copy[i] else innovation)
        return {'targets': history[len(previous):]}
    if kind == 'TOTALS':
        exact(p,'tier rows')
        check(type(p['tier']) is str and p['tier'] in TIERS, 'RANGE', 2, 'tier')
        rows = p['rows']
        check(type(rows) is list and len(rows) == TIERS[p['tier']][1], 'RANGE', 2, 'rows')
        sums = [0]*4
        for b, row in enumerate(rows):
            check(type(row) is list and len(row) == 5 and type(row[0]) is int and row[0] == b and all(uint(x,8192 if i < 3 else 262144) for i,x in enumerate(row) if i > 0), 'RANGE', 2, 'rows')
            for i in range(4):
                sums[i] += row[i+1]
        return {'results_text': results(p['tier'],sums[0]-sums[1],sums[2]-sums[3])}
    if kind == 'INFERENCE':
        exact(p,'policy_signed_sum accuracy_signed_sum')
        for key, bound in (('policy_signed_sum',163840000),('accuracy_signed_sum',5242880000)):
            check(type(p[key]) is int and abs(p[key]) <= bound, 'RANGE', 2, key)
        return inference(p['policy_signed_sum'],p['accuracy_signed_sum'])
    if kind == 'CLASSIFY':
        exact(p,'lower upper')
        lo, hi = rational(p['lower']), rational(p['upper'])
        check(-1 <= lo <= hi <= 1, 'RANGE', 2, 'interval')
        return {'classification': classify(lo,hi)}
    if kind == 'RECORD':
        exact(p,'base patches truncate append_hex rehash')
        check(p['base'] == 'FIXIN_BUNDLE/literals/SYNTHETIC_RECORD.bin' and type(p['patches']) is list)
        r = bytearray(files[p['base']])
        touched = set()
        for patch in p['patches']:
            exact(patch,'offset hex')
            x, off = bhex(patch['hex'],True), patch['offset']
            check(uint(off,RECORD) and off+len(x) <= RECORD and not touched.intersection(range(off,off+len(x))))
            touched.update(range(off,off+len(x)))
            r[off:off+len(x)] = x
        check(uint(p['truncate'],RECORD) and type(p['rehash']) is bool)
        r = r[:p['truncate']]+bhex(p['append_hex'])
        if p['rehash'] and len(r) == RECORD:
            r[HASH_OFFSET:] = hashlib.sha256(r[:HASH_OFFSET]).digest()
        return record(bytes(r))
    if kind == 'CHUNK_HEADER':
        exact(p,'bytes_hex tier chunk_index')
        check(type(p['tier']) is str and p['tier'] in TIERS)
        return chunk(bhex(p['bytes_hex']),p['tier'],p['chunk_index'])
    if kind == 'MANIFEST':
        exact(p,'bytes_hex tier')
        check(type(p['tier']) is str and p['tier'] in TIERS)
        return manifest(bhex(p['bytes_hex']),p['tier'])
    if kind == 'CLI':
        exact(p,'argv')
        return cli(p['argv'])
    fail()

def main():
    try:
        parsed = cli(['brs3_fixture']+sys.argv[1:])['options']
        # cli() checks flags and lexical path syntax without filesystem probes.
        # Authenticate the complete expected-free view before case membership.
        files, members = authenticate(parsed['--inputs'])
        root = parsed['--inputs']+'/'
        case = parsed['--case']
        check(case.startswith(root) and case[len(root):] in members, 'INPUT', 2, 'case')
        # Members were already path-checked and read by authenticate(). Never
        # probe an arbitrary --case, including a delegated absent expectedpath.
        pathcheck(parsed['--output'],True)
        try:
            c = strict_json(files[case[len(root):]])
            exact(c,'version id kind payload')
            check(type(c['version']) is int and c['version'] == 1 and type(c['id']) is str and len(c['id']) <= 64 and re.fullmatch('[A-Z][A-Z0-9_]*',c['id']) and type(c['kind']) is str and c['kind'] in KINDS and type(c['payload']) is dict)
        except (Failure,ValueError,UnicodeError):
            fail('FORMAT',2,'case')
        out = dict(version=1,id=c['id'],kind=c['kind'])
        exit_code = 0
        try:
            out.update(status='OK',value=compute(c['kind'],c['payload'],files))
        except Failure as e:
            out.update(status='ERROR',code=e.code,where=e.where)
            exit_code = e.exit_code
            sys.stderr.write('BRS3_ERROR '+e.code+'\n')
        except (ValueError,TypeError,KeyError,IndexError,OverflowError):
            out.update(status='ERROR',code='FORMAT',where={'field':'payload'})
            exit_code = 2
            sys.stderr.write('BRS3_ERROR FORMAT\n')
        publish(parsed['--output'],canonical(out))
        return exit_code
    except Failure as e:
        sys.stderr.write('BRS3_ERROR '+e.code+'\n')
        return e.exit_code
    except MemoryError:
        sys.stderr.write('BRS3_ERROR RESOURCE\n')
        return 5
    except OSError:
        sys.stderr.write('BRS3_ERROR IO\n')
        return 5
    except Exception:
        sys.stderr.write('BRS3_ERROR INTERNAL\n')
        return 5

if __name__ == '__main__':
    sys.exit(main())
