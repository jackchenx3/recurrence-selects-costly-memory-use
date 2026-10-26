#!/usr/bin/python3
"""Finite BIT003 packaging only. No scientific calculations or result parsing."""
import sys
import time
START = time.monotonic()                 # Native original-job elapsed also covers imports/startup.
import os
import stat
import json
import hashlib
import re
import struct
import subprocess
import signal
import math

STUDY = 'PHASE3-BIT-SUSTAINED-RETRIEVAL-003'
CAP = 16 * 1024 ** 3
ACTIVE = None
MON = None
HELPER_COUNT = 0


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def unique(pairs):
    out = {}
    for k, v in pairs:
        require(k not in out, 'duplicate JSON key')
        out[k] = v
    return out


def load(path):
    with open(path, 'rb') as f:
        return json.loads(f.read().decode('ascii'), object_pairs_hook=unique)


def encoded(obj):
    return (json.dumps(obj, sort_keys=True, separators=(',', ':'),
                       ensure_ascii=True, allow_nan=False) + '\n').encode('ascii')


def elapsed_us():
    return int(math.ceil((time.monotonic() - START) * 1000000))


def text(path):
    with open(path, 'r') as f:
        return f.read()


def normal(path):
    require(isinstance(path, str) and path.startswith('/'), 'absolute path required')
    require(not any(x in ('', '.', '..') for x in path.split('/')[1:]), 'path components')
    p = '/'
    for x in path.split('/')[1:]:
        p = os.path.join(p, x)
        if os.path.lexists(p):
            require(not os.path.islink(p), 'symlink path: ' + p)
    return path


def fsync_dir(path):
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def write_new(path, obj):
    data = obj if isinstance(obj, bytes) else encoded(obj)
    with open(path, 'xb') as f:
        f.write(data)
        f.flush()
        os.fchmod(f.fileno(), 0o400)
        os.fsync(f.fileno())
    fsync_dir(os.path.dirname(path))


def sha(path, monitored=True):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        while True:
            b = f.read(1024 * 1024)
            if not b:
                break
            h.update(b)                 # RESULTS bytes are only fed to SHA256.
            if monitored and MON:
                MON.pulse()
    return h.hexdigest()


def files(root):
    rows = []
    if not os.path.exists(root):
        return rows
    for parent, dirs, names in os.walk(root, followlinks=False):
        for d in dirs:
            require(stat.S_ISDIR(os.lstat(os.path.join(parent, d)).st_mode), 'non-directory')
        for name in names:
            p = os.path.join(parent, name)
            s = os.lstat(p)
            require(stat.S_ISREG(s.st_mode), 'nonregular output: ' + p)
            rows.append((os.path.relpath(p, root), s))
    return sorted(rows)


def unescape(s):
    return re.sub(r'\\([0-7]{3})', lambda m: chr(int(m.group(1), 8)), s)


def mounts(raw):
    out = []
    for line in raw.splitlines():
        a, b = line.split(' - ', 1)
        left, right = a.split(), b.split()
        out.append({'root': unescape(left[3]), 'point': unescape(left[4]),
                    'type': right[0], 'source': unescape(right[1]), 'options': right[2]})
    return out


def containing(path, entries):
    return max((m for m in entries if path == m['point'] or
                path.startswith(m['point'].rstrip('/') + '/')), key=lambda m: len(m['point']))


def discover(job, rawcg, rawmi):
    found = []
    for line in rawcg.splitlines():
        _, controllers, group = line.split(':', 2)
        for m in mounts(rawmi):
            version = (2 if m['type'] == 'cgroup2' and not controllers else
                       1 if m['type'] == 'cgroup' and 'memory' in controllers.split(',')
                       and 'memory' in m['options'].split(',') else 0)
            if not version:
                continue
            mr = m['root'].rstrip('/')
            if mr and group != mr and not group.startswith(mr + '/'):
                continue
            own = os.path.normpath(m['point'] + '/' + group[len(mr):].lstrip('/'))
            p = own
            while p == m['point'] or p.startswith(m['point'].rstrip('/') + '/'):
                if re.fullmatch(r'job[_-]' + re.escape(job) + r'(?:\.scope|\.slice)?', os.path.basename(p)):
                    found.append((version, p, own))
                    break
                if p == m['point']:
                    break
                p = os.path.dirname(p)
    require(len(found) == 1, 'one legitimate current-job memory ancestor required')
    version, root, own = found[0]
    if version == 1:
        require(text(root + '/memory.use_hierarchy').strip() == '1', 'nonhierarchical v1 memory')
    return version, root, own


class Monitor:
    """Unprivileged non-atomic samples; no RSS substitute or cgroup reset."""
    def __init__(self, cg, roots, logs, ceiling, work):
        self.version, self.root, self.own = cg
        self.roots, self.logs, self.ceiling = roots, logs, ceiling
        self.work = work
        self.last = 0
        self.samples = []
        self.max_peak = 0
        self.failure = None
        self.partial = False
        self.journal = None
        self.baseline = None
        self.pulse(True)                 # Required before any child or helper program.
        self.baseline = self.samples[-1]['raw']

    def raw(self):
        names = (['memory.peak', 'memory.current', 'memory.max', 'memory.high', 'memory.events']
                 if self.version == 2 else
                 ['memory.max_usage_in_bytes', 'memory.usage_in_bytes', 'memory.limit_in_bytes',
                  'memory.soft_limit_in_bytes', 'memory.failcnt', 'memory.use_hierarchy', 'memory.oom_control'])
        return {n: text(self.root + '/' + n).strip() for n in names}

    def total(self):
        n = sum(s.st_size for root in self.roots for _, s in files(root))
        # Scheduler logs are this submission's own paths; never scan old study data.
        for p in self.logs:
            if os.path.exists(p):
                require(stat.S_ISREG(os.lstat(p).st_mode), 'native log not regular')
                n += os.stat(p).st_size
        return n

    def pulse(self, force=False):
        if not force and time.monotonic() - self.last < 0.10:
            return
        self.last = time.monotonic()
        raw = self.raw()
        peak = int(raw['memory.peak' if self.version == 2 else 'memory.max_usage_in_bytes'])
        self.max_peak = max(self.max_peak, peak)
        size = self.total()
        row = {'elapsed_us': elapsed_us(), 'scope': 'job', 'path': self.root,
               'raw': raw, 'observed_peak_bytes': peak, 'whole_output_bytes': size}
        self.samples.append(row)
        why = None
        if self.version == 1:
            require(raw['memory.use_hierarchy'] == '1', 'lost hierarchical accounting')
            if int(raw['memory.failcnt']) or any(int(v) for k, v in
                    (line.split() for line in raw['memory.oom_control'].splitlines())
                    if k in ('under_oom', 'oom_kill')):
                why = 'job memory failcnt/OOM evidence'
        else:
            events = dict(line.split() for line in raw['memory.events'].splitlines())
            if any(int(events.get(k, '0')) for k in ('max', 'oom', 'oom_kill', 'oom_group_kill')):
                why = 'job memory max/OOM evidence'
        if peak > self.ceiling:
            why = 'job-wide recorded peak exceeds mode ceiling'
        if size > CAP:
            why = 'whole-output exceeds 16GiB'
        if why:
            self.failure = self.failure or why
            kill_active()
            # Memory failure still permits one partial-evidence archival attempt.
            # Output excess forbids further copies; all local evidence remains.
            if not self.partial or size > CAP:
                raise RuntimeError(why)
        if self.journal:
            self.flush()

    def flush(self):
        if self.journal and self.samples:
            with open(self.journal, 'ab') as f:
                for row in self.samples:
                    f.write(encoded(row))
                f.flush()
            self.samples = []


def kill_active():
    if ACTIVE is not None:
        try:
            os.killpg(ACTIVE.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass


def interrupted(number, frame):
    kill_active()
    raise RuntimeError('received signal ' + str(number))


def run_child(name, argv, work):
    global ACTIVE
    MON.pulse(True)
    before = elapsed_us()
    write_new(work + '/' + name + '.argv.json', {'argv': argv, 'started_elapsed_us': before})
    out, err = work + '/' + name + '.stdout', work + '/' + name + '.stderr'
    failure = None
    code = None
    try:
        with open(out, 'xb') as so, open(err, 'xb') as se:
            ACTIVE = subprocess.Popen(argv, stdin=subprocess.DEVNULL, stdout=so, stderr=se,
                                      cwd=work, env={'PATH': '/usr/bin:/bin', 'LC_ALL': 'C',
                                                    'OMP_NUM_THREADS': '32'}, start_new_session=True)
            while ACTIVE.poll() is None:
                MON.pulse()
                time.sleep(0.05)
            code = ACTIVE.returncode
            if code != 0:
                kill_active()
            so.flush()
            se.flush()
            os.fsync(so.fileno())
            os.fsync(se.fileno())
    except BaseException as e:
        failure = type(e).__name__ + ': ' + str(e)
        kill_active()
    finally:
        if ACTIVE is not None:
            code = ACTIVE.wait()
        ACTIVE = None
        # Retain native exit/signal and stream identities even after monitored failure.
        evidence = {'argv': argv, 'native_returncode': code, 'failure': failure,
                    'started_elapsed_us': before, 'ended_elapsed_us': elapsed_us(),
                    'streams': {p: {'bytes': os.path.getsize(p), 'sha256': sha(p, False)}
                                for p in [out, err] if os.path.exists(p)}}
        write_new(work + '/' + name + '.exit.json', evidence)
    MON.pulse(True)
    require(failure is None and code == 0, name + ' native child failure')
    require(os.path.getsize(out) == 0, name + ' nonempty scientific stdout')


def cpu_records(raw):
    return [dict((a.strip(), b.strip()) for a, b in
                 (line.split(':', 1) for line in part.splitlines() if ':' in line)
                 if a.strip() not in ('cpu MHz', 'bogomips'))
            for part in raw.strip().split('\n\n')]


def build_id(path):
    # Metadata-only ELF64 PT_NOTE parsing; no ELF is loaded or executed here.
    with open(path, 'rb') as f:
        h = f.read(64)
        require(h[:6] == b'\x7fELF\x02\x01', 'ELF64 little endian required')
        off = struct.unpack_from('<Q', h, 32)[0]
        entsize, count = struct.unpack_from('<HH', h, 54)
        for i in range(count):
            f.seek(off + i * entsize)
            ph = f.read(entsize)
            if struct.unpack_from('<I', ph)[0] != 4:
                continue
            pos, length = struct.unpack_from('<Q', ph, 8)[0], struct.unpack_from('<Q', ph, 32)[0]
            f.seek(pos)
            notes = f.read(length)
            j = 0
            while j + 12 <= len(notes):
                n, d, t = struct.unpack_from('<III', notes, j)
                j += 12
                name = notes[j:j+n]
                j += (n + 3) & ~3
                value = notes[j:j+d]
                j += (d + 3) & ~3
                if name == b'GNU\0' and t == 3:
                    return value.hex()
    raise RuntimeError('missing ELF build ID')


def module_closure(freeze):
    require(freeze.get('prospective_closure_qualified') is True, 'prospective closure pending')
    require(sys.flags.isolated == 1 and sys.flags.dont_write_bytecode == 1 and
            str(sys.flags) == freeze['controller_python_flags'], 'qualified -I -B flags')
    allowed = freeze['controller_module_allowlist']
    for name, module in list(sys.modules.items()):
        p = getattr(module, '__file__', None)
        if name == '__main__':
            continue                    # Workflow is bound by SOURCE inventory.
        if p:
            require(p in allowed, 'unqualified module before benchmark: ' + name + ' ' + p)
            require(sha(p) == allowed[p]['sha256'], 'loaded module hash: ' + p)
            if 'realpath' in allowed[p]:
                require(os.path.realpath(p) == allowed[p]['realpath'], 'module realpath: ' + p)
            if p.endswith('.so'):
                require(p in freeze['controller_extension_paths'], 'unqualified extension: ' + p)
    known = freeze['controller_mapped_elf_allowlist']
    mapped = []
    for line in text('/proc/self/maps').splitlines():
        fields = line.split(None, 5)
        if len(fields) == 6 and fields[5].startswith('/'):
            p = fields[5]
            require(not p.endswith(' (deleted)'), 'deleted mapped file')
            with open(p, 'rb') as f:
                magic = f.read(4)
            if magic == b'\x7fELF':
                require(p in known, 'unqualified mapped ELF: ' + p)
                d = known[p]
                require(os.path.realpath(p) == d['realpath'] and sha(p) == d['sha256'],
                        'mapped ELF identity: ' + p)
                mapped.append(p)
    require(mapped, 'missing mapped ELF evidence')
    return sorted(set(mapped))


def helper(argv):
    # Persist evidence before return-code/path/scheduler validation or later identity errors.
    global ACTIVE, HELPER_COUNT
    require(argv[0] in ('/usr/bin/ldd', '/usr/bin/scontrol'), 'metadata helper allowlist')
    MON.pulse(True)
    HELPER_COUNT += 1
    stem = MON.work + '/metadata-%03d' % HELPER_COUNT
    before = elapsed_us()
    write_new(stem + '.argv.json', {'argv': argv, 'started_elapsed_us': before})
    out, err, code, failure, timed_out = b'', b'', None, None, False
    try:
        ACTIVE = subprocess.Popen(argv, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, start_new_session=True,
                                  env={'PATH': '/usr/bin:/bin', 'LC_ALL': 'C'})
        try:
            out, err = ACTIVE.communicate(timeout=10)
        except subprocess.TimeoutExpired as e:
            timed_out = True
            out, err = e.output or b'', e.stderr or b''
            kill_active()
            # communicate after SIGKILL returns complete available streams and native exit.
            out, err = ACTIVE.communicate()
        code = ACTIVE.returncode
    except BaseException as e:
        failure = type(e).__name__ + ': ' + str(e)
        kill_active()
        if ACTIVE is not None:
            out, err = ACTIVE.communicate()
            code = ACTIVE.returncode
    finally:
        ACTIVE = None
        write_new(stem + '.stdout', out)
        write_new(stem + '.stderr', err)
        write_new(stem + '.exit.json', {'argv': argv, 'native_returncode': code,
                  'failure': failure, 'timed_out': timed_out,
                  'started_elapsed_us': before, 'ended_elapsed_us': elapsed_us()})
    MON.pulse(True)
    require(not timed_out and failure is None and code == 0, 'metadata helper failed: ' + argv[0])
    return {'evidence_prefix': os.path.basename(stem), 'stdout': out.decode('utf-8')}


def identities(f, deployment, authority, work):
    require(sys.version == f['python_version'] and sys.executable == '/usr/bin/python3', 'Python version/path')
    require(f.get('prospective_closure_qualified') is True and
            re.fullmatch('[0-9a-f]{64}', f['closure_proof_sha256']) is not None,
            'authenticated prospective closure required')
    require(f['controller_mapped_elf_allowlist'] and f['controller_extension_paths'], 'empty prospective closure')
    require(set(f['metadata_helper_identities']) == {'/usr/bin/ldd', '/usr/bin/scontrol'}, 'helper identity closure')
    for p, d in f['metadata_helper_identities'].items():
        require(os.path.realpath(p) == d['realpath'] and sha(p) == d['sha256'], 'metadata helper identity: ' + p)
    for p, d in f['controller_mapped_elf_allowlist'].items():
        require(p.startswith('/') and os.path.realpath(p) == d['realpath'] and sha(p) == d['sha256'],
                'qualified mapped ELF file: ' + p)
    require(all(p in f['loader_dependency_paths'] and p in f['dependencies']
                for p in f['controller_extension_paths']), 'extension transitive closure absent')
    module_closure(f)
    require(os.uname().nodename == f['host'] and list(os.uname()) == f['uname'], 'uname/node route')
    require(text('/etc/os-release') == f['os_release'], 'OS release')
    require(cpu_records(text('/proc/cpuinfo')) == f['cpu_stable_records'], 'CPU/ISA/topology/microcode')
    require(len(os.sched_getaffinity(0)) == 32, '32 available affinity CPUs required')
    for p, d in f['dependencies'].items():
        require(os.path.realpath(p) == d['realpath'] and sha(p) == d['sha256'], 'dependency: ' + p)
    for p, d in f['controller_module_allowlist'].items():
        require(sha(p) == d['sha256'], 'qualified stdlib file changed: ' + p)
        if 'realpath' in d:
            require(os.path.realpath(p) == d['realpath'], 'qualified module realpath: ' + p)
    for p, expected in f['ELF_build_ids'].items():
        require(build_id(p) == expected, 'ELF build ID: ' + p)
    loader = {}
    for p, expected in f['loader_dependency_paths'].items():
        row = helper(['/usr/bin/ldd', p])
        require('not found' not in row['stdout'], 'unresolved ELF loader dependency: ' + p)
        require(all(q in f['dependencies'] for q in expected), 'unfrozen ELF dependency path: ' + p)
        actual = sorted(set(re.findall(r'/[^\s()]+', row['stdout'])))
        require(actual == expected, 'ELF loader dependency paths: ' + p)
        loader[p] = {'evidence_prefix': row['evidence_prefix']}
    # Scheduler RPC is external control-plane metadata, not a runtime dependency.
    # Exact submission script/resources are ROOT-authenticated before sbatch;
    # original native job/ALL-step resources and exits remain mandatory postterminal.
    require(os.environ.get('SLURMD_NODENAME') == 'cn038', 'pinned Slurm node')
    require(os.environ.get('SLURM_JOB_PARTITION') in f['accepted_reported_partitions'], 'qualified Slurm partition routing')
    require(os.environ.get('SLURM_CPUS_PER_TASK') == '32', 'Slurm CPU request')
    require(os.environ.get('SLURM_NTASKS') == '1', 'Slurm task request')
    require(not os.environ.get('SLURM_ARRAY_JOB_ID') and not os.environ.get('SLURM_JOB_GPUS'), 'array/GPU disallowed')
    node = {'source': 'Actual host/CPU/ISA/affinity and pinned Slurm node; ROOT postterminal native scheduler evidence required',
            'SLURMD_NODENAME': os.environ.get('SLURMD_NODENAME'), 'host': os.uname().nodename}
    job = {'source': 'Selected Slurm environment only; NOT native scheduler acceptance',
           'selected_slurm_environment': {k: os.environ.get(k) for k in
             ['SLURM_JOB_ID', 'SLURM_JOB_NODELIST', 'SLURM_JOB_PARTITION', 'SLURM_JOB_NUM_NODES',
              'SLURM_NTASKS', 'SLURM_CPUS_PER_TASK', 'SLURM_MEM_PER_NODE', 'SLURM_RESTART_COUNT']},
           'ROOT_native_job_ALL_steps_resources_success_acceptance_required': True}
    entries = mounts(text('/proc/self/mountinfo'))
    for key, p in [('scratch', work), ('archive', authority['runroot'])]:
        require(containing(p, entries) == f['filesystems'][key], 'filesystem identity: ' + key)
    view = normal(authority['science_view'])
    require(set(os.listdir(view)) == set(x['relative_path'] for x in f['science_view']), 'five-file view closure')
    for row in f['science_view']:
        p = normal(view + '/' + row['relative_path'])
        s = os.lstat(p)
        require(stat.S_ISREG(s.st_mode) and s.st_mode & 0o222 == 0, 'readonly regular view')
        require(s.st_size == row['bytes'] and sha(p) == row['sha256'], 'science view identity')
    return {'node_metadata': node, 'job_environment_metadata': job,
            'loader': loader, 'mapped_ELFs': module_closure(f),
            'source_inventory_sha256': f['source_inventory_sha256'], 'resources_in_job_native_accepted': False}


def inventory_check(deployment, expected):
    inv = deployment + '/SOURCE_INVENTORY.json'
    require(sha(inv) == expected, 'authority SOURCE inventory digest')
    rows = load(inv)['files']
    require(set(os.listdir(deployment)) == set(rows) | {'SOURCE_INVENTORY.json'}, 'deployment closure')
    for rel, d in rows.items():
        require('/' not in rel and rel not in ('', '.', '..'), 'inventory name')
        p = normal(deployment + '/' + rel)
        s = os.lstat(p)
        require(stat.S_ISREG(s.st_mode) and s.st_size == d['bytes'] and sha(p) == d['sha256'], 'deployment identity: ' + rel)


def production_gate(authority, freeze):
    gate = authority['benchmark_acceptance']
    p = normal(gate['path'])
    require(sha(p) == gate['sha256'], 'ROOT postterminal benchmark acceptance hash')
    b = load(p)
    for key in ['ROOT_postterminal_authenticated', 'native_original_job_terminal',
                'all_job_and_steps_completed_0_0', 'independent_execution_acceptance_GO',
                'hash_only_results_agree', 'full_audit_valid', 'whole_output_includes_all_copies']:
        require(b.get(key) is True, 'benchmark gate: ' + key)
    require(b['mode'] == 'benchmark' and b['submission_count'] == 1, 'one original benchmark')
    require(b['inventory_sha256'] == authority['inventory_sha256'] and
            b['runtime_freeze_sha256'] == authority['runtime_freeze_sha256'], 'benchmark exact candidate')
    for key in ['native_ElapsedRaw_seconds', 'in_job_full_workflow_us',
                'binding_elapsed_seconds', 'total_output_bytes', 'job_peak_bytes',
                'native_all_step_MaxRSS_bytes']:
        require(type(b[key]) is int and b[key] >= 0, 'nonnegative integer benchmark metadata: ' + key)
    binding = max(b['native_ElapsedRaw_seconds'] + 1,
                  (b['in_job_full_workflow_us'] + 999999) // 1000000)
    require(b['binding_elapsed_seconds'] == binding and binding * 625 <= 14400, 'binding benchmark time gate')
    require(b['total_output_bytes'] * 625 <= CAP, 'binding benchmark output gate')
    require(max(b['job_peak_bytes'], b['native_all_step_MaxRSS_bytes']) <= 350000000000, 'benchmark memory gate')
    require(re.fullmatch('[0-9a-f]{64}', authority['independent_readiness_GO_sha256']) is not None, 'readiness evidence')
    require(re.fullmatch('[0-9a-f]{64}', authority['ROOT_exact_scientific_authority_sha256']) is not None, 'ROOT authority evidence')


def verify_data(data, mode, f):
    count = 64 if mode == 'benchmark' else 20000
    chunks = 1 if mode == 'benchmark' else 200
    names = ['chunk_%03d.bin' % i for i in range(chunks)]
    require(set(os.listdir(data)) == set(names + ['MANIFEST.json', 'MANIFEST.sha256']), 'DATA closure')
    for name in os.listdir(data):
        s = os.lstat(data + '/' + name)
        require(stat.S_ISREG(s.st_mode) and s.st_mode & 0o222 == 0, 'sealed DATA regular files')
    path = data + '/MANIFEST.json'
    m = load(path)
    with open(path, 'rb') as inp:
        require(inp.read() == encoded(m), 'canonical DATA manifest')
    expected = {'schema': 1, 'study': STUDY, 'tier': mode + '-r1', 'protocol_sha256': f['G0_protocol_sha256'],
                'contract_sha256': f['contract_sha256'],
                'keys_sha256': next(x['sha256'] for x in f['science_view'] if x['relative_path'] == 'KEYS.tsv'),
                'block_count': count, 'history_horizon': 256, 'followup_horizon': 256,
                'record_bytes': 208216, 'chunk_record_limit': 100,
                'all_f_blocks': count // 2, 'all_m_blocks': count // 2, 'files': []}
    for name in names:
        p = data + '/' + name
        size = 64 + (64 if mode == 'benchmark' else 100) * 208216
        require(os.path.getsize(p) == size, 'DATA chunk size')
        expected['files'].append({'name': name, 'bytes': size, 'sha256': sha(p)})
    require(m == expected, 'DATA manifest metadata/hash binding')
    digest = sha(path)
    require(text(data + '/MANIFEST.sha256') == digest + '  MANIFEST.json\n', 'manifest hash commit')
    return {'manifest_sha256': digest, 'manifest': m}


def verify_audit(path, mode, f):
    count = 64 if mode == 'benchmark' else 20000
    d = {r['relative_path']: r['sha256'] for r in f['science_view']}
    lines = ['BIT003_AUDIT_V1', 'status=VALID', 'scope=FULL_INDEPENDENT_REPLAY',
             'tier=' + mode + '-r1', 'blocks_checked=' + str(count),
             'states_checked=' + str(769 * count), 'turnovers_checked=' + str(768 * count),
             'protocol_sha256=' + f['G0_protocol_sha256'], 'contract_sha256=' + f['contract_sha256'],
             'public_input_index_sha256=' + d['PUBLIC_INPUT_INDEX.json'],
             'bundle_inventory_sha256=' + f['bundle_inventory_sha256'],
             'source_sha256=' + f['source_inventory_sha256']['auditor'],
             'binary_sha256=' + f['scientific_binaries']['brs3_audit']['sha256'], 'first_mismatch=NONE']
    with open(path, 'rb') as inp:
        require(inp.read() == ('\n'.join(lines) + '\n').encode('ascii'), 'canonical full 14-line audit report')
    return {'sha256': sha(path), 'blocks': count, 'states': 769 * count, 'turnovers': 768 * count}


def copy_new(src, dst):
    s = os.lstat(src)
    require(stat.S_ISREG(s.st_mode), 'copy requires regular file')
    h = hashlib.sha256()
    with open(src, 'rb') as a, open(dst, 'xb') as b:
        while True:
            block = a.read(1024 * 1024)
            if not block:
                break
            h.update(block)
            b.write(block)
            MON.pulse()
        b.flush()
        os.fchmod(b.fileno(), stat.S_IMODE(s.st_mode) & ~0o222)
        os.fsync(b.fileno())
    os.utime(dst, ns=(s.st_atime_ns, s.st_mtime_ns))
    with open(dst, 'rb') as f:
        os.fsync(f.fileno())
    require(sha(dst) == h.hexdigest() and os.path.getsize(dst) == s.st_size, 'archival copy hash/size')
    require(os.stat(src).st_size == s.st_size and os.stat(src).st_mtime_ns == s.st_mtime_ns, 'source changed during archival')
    fsync_dir(os.path.dirname(dst))
    return {'bytes': s.st_size, 'sha256': h.hexdigest(), 'original_mode': stat.S_IMODE(s.st_mode),
            'original_mtime_ns': s.st_mtime_ns, 'original_uid': s.st_uid, 'original_gid': s.st_gid}


def archive(work, runroot, outcome):
    # Exactly one stageout attempt. Each file is exclusive; partial files never retried.
    MON.partial = outcome['failure'] is not None
    MON.pulse(True)
    MON.flush()
    if MON.journal and os.path.exists(MON.journal):
        with open(MON.journal, 'rb') as journal:
            os.fsync(journal.fileno())
    MON.journal = None                    # Freeze journal before copying it.
    write_new(work + '/OUTCOME.json', outcome)
    dest = runroot + '/ARCHIVE'
    os.mkdir(dest, 0o700)
    fsync_dir(runroot)
    listing = {}
    for rel, s in files(work):
        target = dest + '/' + rel
        parent = os.path.dirname(target)
        if not os.path.exists(parent):
            os.makedirs(parent, mode=0o700)
        listing[rel] = copy_new(work + '/' + rel, target)
    MON.pulse(True)
    module_closure(FREEZE)
    # The tail contains samples taken during copy, hashing, and fsync of the main archive.
    tail_count = len(MON.samples)
    tail = {'samples': list(MON.samples), 'non_atomic': True, 'later_recorded_peak_excess_fails': True}
    write_new(work + '/ARCHIVAL_RESOURCES.json', tail)
    listing['ARCHIVAL_RESOURCES.json'] = copy_new(work + '/ARCHIVAL_RESOURCES.json', dest + '/ARCHIVAL_RESOURCES.json')
    for root, dirs, names in os.walk(dest, topdown=False):
        fsync_dir(root)
    fsync_dir(work)
    MON.pulse(True)
    marker = {'schema': 1, 'study': STUDY, 'original_job_id': os.environ['SLURM_JOB_ID'],
              'mode': outcome['mode'], 'archive_copy_complete': True,
              'science_complete': outcome['failure'] is None, 'independent_EA': 'PENDING_ROOT_POSTTERMINAL',
              'inventory_sha256': outcome['inventory_sha256'],
              'runtime_freeze_sha256': outcome['runtime_freeze_sha256'],
              'files': listing, 'final_resource_samples': MON.samples[tail_count:],
              'job_peak_bytes': MON.max_peak, 'memory_failure': MON.failure,
              'elapsed_through_verified_archive_us': elapsed_us(),
              'final_timing_evidence': 'ORIGINAL_NATIVE_STDERR_POSTSEAL', 'total_output_bytes': 0,
              'results_sealed': True, 'benchmark_gate': 'PENDING_ROOT_NATIVE_TERMINAL',
              'native_all_step_acceptance_claimed': False}
    # Local marker and shared marker counted without a recursive hash or size dependency.
    base_bytes = MON.total()
    for _ in range(8):
        n = base_bytes + 2 * len(encoded(marker))
        if marker['total_output_bytes'] == n:
            break
        marker['total_output_bytes'] = n
    require(marker['total_output_bytes'] == base_bytes + 2 * len(encoded(marker)), 'marker size convergence')
    require(marker['total_output_bytes'] <= CAP, 'whole output including archival inventory')
    if outcome['mode'] == 'benchmark':
        # Only an early fail here; ROOT still applies whole original-job timing and native accounting.
        require(marker['total_output_bytes'] * 625 <= CAP, 'projected whole-output gate failed')
    name = 'ARCHIVE_COMMITTED.json' if outcome['failure'] is None else 'PARTIAL_INVENTORY.json'
    write_new(work + '/' + name, marker)
    copy_new(work + '/' + name, dest + '/' + name)
    fsync_dir(dest)
    fsync_dir(runroot)
    fsync_dir(os.path.dirname(runroot))
    MON.pulse(True)
    require(MON.failure is None and outcome['failure'] is None, 'partial/failed job cannot complete')
    final_total = MON.total()
    require(final_total <= CAP, 'final whole-output ceiling')
    if outcome['mode'] == 'benchmark':
        require(final_total * 625 <= CAP, 'final projected whole-output gate')
    # Final observation AFTER verified marker copy/sealing/fsync/resource checks.
    # Its own native write/flush, Python shutdown and Slurm teardown are reconciled
    # by ROOT from original terminal native elapsed/logs; no invented tail budget.
    evidence = {'schema': 1, 'event': 'BIT003_FINAL_NATIVE_EVIDENCE',
                'original_job_id': os.environ['SLURM_JOB_ID'], 'mode': outcome['mode'],
                'inventory_sha256': outcome['inventory_sha256'],
                'runtime_freeze_sha256': outcome['runtime_freeze_sha256'],
                'marker': name, 'job_peak_bytes': MON.max_peak,
                'whole_output_bytes_at_observation': final_total,
                'post_marker_resource_samples': MON.samples[tail_count + len(marker['final_resource_samples']):],
                'in_job_full_workflow_us': elapsed_us(), 'results_sealed': True,
                'native_terminal_reconciliation_required': True}
    sys.stderr.write(encoded(evidence).decode('ascii'))
    sys.stderr.flush()


def main():
    global MON, FREEZE
    require(len(sys.argv) == 4 and sys.argv[1] in ('benchmark', 'production'), 'fixed mode authority digest arguments')
    mode, apath, adigest = sys.argv[1:]
    job = os.environ.get('SLURM_JOB_ID', '')
    require(re.fullmatch('[1-9][0-9]*', job) is not None, 'original Slurm job ID required')
    require(job != '54294741' and not os.environ.get('SLURM_ARRAY_JOB_ID'), 'H200/array prohibited')
    require(os.environ.get('SLURMD_NODENAME') == 'cn038', 'pinned node required')
    deployment = normal(os.path.dirname(os.path.abspath(__file__)))
    FREEZE = load(deployment + '/RUNTIME_FREEZE.json')
    require(FREEZE.get('prospective_closure_qualified') is True, 'closure metadata pending: no execution authority')
    rawcg, rawmi = text('/proc/self/cgroup'), text('/proc/self/mountinfo')
    cg = discover(job, rawcg, rawmi)
    # Do not invoke any external metadata helper or science until job peak is readable.
    ceiling = 350000000000 if mode == 'benchmark' else 700000000000
    MON = Monitor(cg, [], [], ceiling, None)
    normal(apath)
    require(re.fullmatch('[0-9a-f]{64}', adigest) is not None and sha(apath) == adigest, 'ROOT-pinned authority bytes')
    a = load(apath)
    for k in ['authorizes_exact_execution', 'genuine_nonauthor_SOURCE_runtime_GO',
              'ROOT_authenticated_payload_and_execution_before_sbatch', 'G1F_full_bundle_authenticated',
              'original_submission_only', 'no_prior_submission_for_mode',
              'immutable_deployment_retained_for_reproducibility']:
        require(a.get(k) is True, 'external authority: ' + k)
    for k in ['source_runtime_review_sha256', 'ROOT_submission_authentication_sha256', 'G0_acceptance_sha256']:
        require(re.fullmatch('[0-9a-f]{64}', a[k]) is not None, 'external acceptance identity: ' + k)
    require(a['schema'] == 1 and a['study'] == STUDY and a['mode'] == mode and
            a['tier'] == mode + '-r1' and a['submission_ordinal'] == 1, 'authority mode/study')
    require(a['deployment'] == deployment and a['resources'] == FREEZE['resources'], 'authority deployment/resources')
    require(a['closure_proof_sha256'] == FREEZE['closure_proof_sha256'] and
            re.fullmatch('[0-9a-f]{64}', a['closure_proof_sha256']) is not None, 'authority closure proof')
    require(os.stat(deployment).st_mode & 0o222 == 0, 'readonly deployment directory')
    require(all(os.stat(deployment + '/' + n).st_mode & 0o222 == 0
                for n in os.listdir(deployment)), 'readonly deployed files required')
    require(a['g0_protocol_sha256'] == FREEZE['G0_protocol_sha256'] and
            a['g4_acceptance_sha256'] == FREEZE['G4_acceptance_sha256'], 'G0/G4 identity')
    require(sha(deployment + '/RUNTIME_FREEZE.json') == a['runtime_freeze_sha256'], 'runtime freeze identity')
    inventory_check(deployment, a['inventory_sha256'])
    if mode == 'production':
        production_gate(a, FREEZE)
    runroot = normal(a['runroot'])
    base = FREEZE['base']
    require(runroot.startswith(base + '/') and not os.path.lexists(runroot), 'new unique study runroot')
    require(not deployment.startswith(runroot + '/') and not runroot.startswith(deployment + '/'), 'deployment separate')
    require(os.path.isdir(os.path.dirname(runroot)), 'runroot parent must exist')
    submission = normal(a['submission_dir'])
    require(submission != runroot and not submission.startswith(runroot + '/'), 'submission control separate')
    logs = [submission + '/original-' + job + '.out', submission + '/original-' + job + '.err']
    require(all(os.path.isfile(p) and not os.path.islink(p) for p in logs), 'ROOT native log route')
    work = '/tmp/bit003-' + mode + '-' + job
    require(not os.path.lexists(work), 'new absent node-local workroot')
    v = os.statvfs('/tmp')
    require(v.f_bavail * v.f_frsize >= 32 * 1024 ** 3, '32GiB local scratch availability')
    os.umask(0o077)
    os.mkdir(work, 0o700)
    os.mkdir(runroot, 0o700)
    fsync_dir('/tmp')
    fsync_dir(os.path.dirname(runroot))
    MON.roots, MON.logs, MON.work = [work, runroot], logs, work
    MON.journal = work + '/RESOURCE_SAMPLES.jsonl'
    MON.flush()
    for sig in [signal.SIGTERM, signal.SIGINT, signal.SIGHUP]:
        signal.signal(sig, interrupted)
    outcome = {'schema': 1, 'mode': mode, 'job_id': job, 'failure': None,
               'inventory_sha256': a['inventory_sha256'], 'runtime_freeze_sha256': a['runtime_freeze_sha256'],
               'native_acceptance': 'ROOT_POSTTERMINAL_ONLY', 'results_sealed': True}
    try:
        write_new(work + '/RUN_AUTHORITY.json', open(apath, 'rb').read())
        write_new(work + '/CGROUP_DISCOVERY.json', {'proc_self_cgroup': rawcg, 'proc_self_mountinfo': rawmi,
                                                  'version': cg[0], 'job_root': cg[1], 'own_path': cg[2]})
        # Full source/runtime/provenance bytes remain in the authenticated readonly
        # external deployment. Retain exact SHA references instead of two bulk copies.
        write_new(work + '/DEPLOYMENT_REFERENCE.json', {
            'deployment': deployment, 'inventory_sha256': a['inventory_sha256'],
            'runtime_freeze_sha256': a['runtime_freeze_sha256'],
            'closure_proof_sha256': a['closure_proof_sha256'],
            'source_inventory': load(deployment + '/SOURCE_INVENTORY.json'),
            'immutable_deployment_retained_for_reproducibility': True})
        write_new(work + '/IDENTITY.json', identities(FREEZE, deployment, a, work))
        initial_names = set(os.listdir(work))
        binpaths = {n: x['path'] for n, x in FREEZE['scientific_binaries'].items()}
        common = ['--inputs', normal(a['science_view']), '--tier', mode + '-r1']
        data, ra, rb, report = [work + '/' + p for p in ['DATA', 'RESULTS_A.txt', 'RESULTS_B.txt', 'AUDIT_REPORT.txt']]
        run_child('produce', [binpaths['brs3_produce']] + common + ['--output', data, '--threads', '32'], work)
        checked = verify_data(data, mode, FREEZE)
        run_child('analyze', [binpaths['brs3_analyze']] + common + ['--data', data, '--results', ra, '--threads', '32'], work)
        run_child('audit', [binpaths['brs3_audit']] + common + ['--data', data, '--results', rb,
                                                            '--report', report, '--threads', '32'], work)
        audit = verify_audit(report, mode, FREEZE)
        identities_after = {p: sha(p) for p in FREEZE['dependencies']}
        require(all(identities_after[p] == d['sha256'] for p, d in FREEZE['dependencies'].items()), 'dependency changed during science')
        require(verify_data(data, mode, FREEZE) == checked, 'DATA changed during analysis/audit')
        for p in [ra, rb, report]:
            s = os.lstat(p)
            require(stat.S_ISREG(s.st_mode), 'regular result/report')
            os.chmod(p, stat.S_IMODE(s.st_mode) & ~0o222)
            with open(p, 'rb') as sealed:
                os.fsync(sealed.fileno())
        fsync_dir(work)
        hashes = {os.path.basename(p): {'bytes': os.path.getsize(p), 'sha256': sha(p)} for p in [ra, rb]}
        require(hashes['RESULTS_A.txt'] == hashes['RESULTS_B.txt'], 'hash-only RESULTS agreement')
        expected_outputs = {'DATA', 'RESULTS_A.txt', 'RESULTS_B.txt', 'AUDIT_REPORT.txt'}
        for name in ['produce', 'analyze', 'audit']:
            expected_outputs.update(name + suffix for suffix in ['.argv.json', '.exit.json', '.stdout', '.stderr'])
        require(set(os.listdir(work)) == initial_names | expected_outputs, 'unexpected/partial scientific work files')
        write_new(work + '/SEALED_AGREEMENT.json', {'data': checked, 'results': hashes, 'audit': audit,
                                                  'numeric_results_opened_or_parsed': False,
                                                  'protocol_sha256': FREEZE['G0_protocol_sha256'],
                                                  'contract_sha256': FREEZE['contract_sha256'],
                                                  'bundle_inventory_sha256': FREEZE['bundle_inventory_sha256']})
        inventory_check(deployment, a['inventory_sha256'])
        module_closure(FREEZE)
        MON.pulse(True)
    except BaseException as e:
        kill_active()
        outcome['failure'] = {'type': type(e).__name__, 'message': str(e), 'elapsed_us': elapsed_us()}
    MON.partial = outcome['failure'] is not None
    try:
        for i, p in enumerate(logs):
            copy_new(p, work + '/NATIVE_ORIGINAL_' + ('stdout' if i == 0 else 'stderr'))
        archive(work, runroot, outcome)
    except BaseException as e:
        kill_active()
        # Explicit uncommitted suffix. Never repeat archival or any scientific child.
        failure = {'failure': type(e).__name__ + ': ' + str(e), 'elapsed_us': elapsed_us(),
                   'science_outcome': outcome, 'committed_acceptance': False,
                   'native_original_job_must_fail': True, 'samples_after_last_archived_sample': MON.samples}
        for root in [work, runroot]:
            try:
                write_new(root + '/UNCOMMITTED_FAILURE.json', failure)
            except BaseException:
                pass                    # Disk/fsync failure cannot be repaired by a scientific rerun.
        return 1
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except BaseException as error:
        if isinstance(error, SystemExit):
            raise
        kill_active()
        # Before a new workroot exists, the original Slurm stderr is the native evidence.
        sys.stderr.write('BIT003_WORKFLOW_FAILURE ' + type(error).__name__ + ': ' + str(error) + '\n')
        sys.exit(1)
