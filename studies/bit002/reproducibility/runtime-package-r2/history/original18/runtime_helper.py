#!/home/chenx3/miniconda3/bin/python3.11
"""BIT002 filesystem, identity, monotonic timing and receipt primitives.

Invoked only by the two batch scripts. No submission, scientific computation,
result parsing, retries, resumption, or post-job entry point is provided.
"""
import datetime
import decimal
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import selectors
import signal
import stat
import subprocess
import sys
import time

STUDY = Path('/mnt/ccrsf-static/Analysis/chenx3/organized-variation-transfer/phase3_bit_retrieval_002_20261024')
PACKAGE = STUDY / 'runtime-package-r1'
PROTO_HASH = '71ccc1d065fa4868c30f96588ec34312bdc435e1047214c60b1f5643846c7daa'
BINARIES = {
    'producer': {'path': str(STUDY / 'build-g2-r1/producer/bin/brc2_produce'),
                 'sha256': 'bc597d652bfab0f05cab7e9ca52e53ea7ddc365e304a7fe4588d7386fc4031f0'},
    'analyzer': {'path': str(STUDY / 'build-g2-r1/producer/bin/brc2_analyze'),
                 'sha256': '0af66a01615414129be30df2d75df9efbfa724660d4c1701b9799982ea367721'},
    'auditor': {'path': str(STUDY / 'auditor-v3-own-r1/auditor/bin/brc2_audit'),
                'sha256': '950ac9ffb49faaca3ec6568c8b55cdd9b8349e1f2782bcfe33c7d07fa3182eca'},
}
PYTHON = {'path': '/home/chenx3/miniconda3/bin/python3.11', 'version': '3.11.4',
          'sha256': '90ebd62efa611953ec39804a827a8df12e743b70b0a7435536bb053e0e1a6ae5'}
CPU_MODEL = 'Intel(R) Xeon(R) Gold 6342 CPU @ 2.80GHz'
HARDWARE_CLASS = {'name': 'norm/short-shared-x6342', 'requested_partition': 'norm',
                  'reported_partitions': ['norm', 'short'], 'constraint': 'x6342',
                  'active_features': ['u467', 'x6342'], 'cpu_model': CPU_MODEL}
COMPILER = {'path': '/usr/bin/g++',
            'sha256': '300a40555cfe820333187a6ed41b732af4905b011fd485bcaf9d9cc7a4facfdb',
            'version': 'g++ (GCC) 8.5.0 20210514 (Red Hat 8.5.0-28.0.1)',
            'flags': {
                'producer_analyzer': '-Isrc -std=c++17 -O3 -Wall -Wextra -Wconversion -Wshadow -fno-fast-math -pthread; link -pthread -lstdc++fs',
                'auditor': '-Isrc -std=c++17 -O2 -Wall -Wextra -Wpedantic -fno-fast-math; link -lstdc++fs'}}
TIMING_SOURCE_HASHES = {
    'producer_analyzer': 'e84d1f195a4c065fc31a393be051291103e01f8e51b58b08dc489426a7ff918a',
    'auditor': 'c25fc2680074a30ab4aa08ca0d1fd5351e6b60522c253d815b1a89040f383d82'}
EVIDENCE_HASHES = {
    'NODE_TOOLCHAIN_FREEZE.txt': 'a5af905abc3c715db7fbdfae07b5ba4f3922b184f774ee1463b2268bea8c4f87',
    'CPU_METADATA_AUTHENTICATION.json': 'e3b1766f2ba2ba89bcaeb46660e1c2a55d0de066614d65d978c6d10785d1352f',
    'PUBLIC_TIMING_CLI_CONTRACT.json': 'bea130dec173f04c1d2739e1c5da7c2cffb6d5db07d60e84d6c14a32bc60ce1e',
    'DEFAULT_PYTHON_IDENTITY_NATIVE.json': '571b075739fe1bb90435d9b27bf07453148b1e80fec4ba2886c64899d010e051'}
ORIGINAL_METADATA_RECEIPT = {
    'path': '/Users/chenx3/Documents/Codex/2026-09-20/review-the-scientific-conclusions-in-mnt/outputs/phase3-bit-retrieval-002-20261024/resource-qualification/cpu-metadata-freeze-r1/remote-evidence/METADATA_TERMINAL.json',
    'sha256': '734abc8fff1fc1c9656fb41c7e4d9a262eaca2173e85d17149b334ab543953de'}
GIB = 1024 ** 3
LOG_STREAM_LIMIT = 1024 ** 2
D = decimal.Decimal
decimal.getcontext().prec = 50


def require(condition, code):
    if not condition:
        raise ValueError(code)


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def digest(path):
    h = hashlib.sha256()
    with open(path, 'rb') as stream:
        for block in iter(lambda: stream.read(1024 ** 2), b''):
            h.update(block)
    return h.hexdigest()


def fsync_dir(path):
    fd = os.open(str(path), os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def commit(path, body, mode=0o400):
    """Exclusive new evidence; write, fsync, rename, directory fsync."""
    path = Path(path)
    require(not path.exists() and not path.is_symlink(), 'existing_evidence')
    tmp = path.parent / ('.tmp.' + path.name)
    fd = os.open(str(tmp), os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, 'wb') as stream:
        stream.write(body)
        stream.flush()
        os.fchmod(stream.fileno(), mode)
        os.fsync(stream.fileno())
    os.rename(str(tmp), str(path))
    fsync_dir(path.parent)


def encoded(value):
    return (json.dumps(value, sort_keys=True, indent=2, ensure_ascii=True) + '\n').encode('ascii')


def put_json(path, value):
    commit(path, encoded(value))


def read_json(path):
    require(Path(path).stat().st_size <= 1024 ** 2, 'oversize_metadata')
    with open(path, encoding='ascii') as stream:
        def unique(pairs):
            obj = {}
            for key, value in pairs:
                require(key not in obj, 'duplicate_metadata_key')
                obj[key] = value
            return obj
        return json.load(stream, object_pairs_hook=unique)


def root(kind):
    require(kind in ('benchmark', 'production'), 'bad_kind')
    return STUDY / ('benchmark-r1' if kind == 'benchmark' else 'scientific-r1')


def allocation():
    env = os.environ
    require(re.fullmatch(r'[1-9][0-9]*', env.get('SLURM_JOB_ID', '')) is not None, 'not_slurm')
    require(env.get('SLURM_JOB_PARTITION') in ('norm', 'short'), 'partition')
    require(env.get('SLURM_JOB_NUM_NODES', env.get('SLURM_NNODES')) == '1', 'nodes')
    require(env.get('SLURM_NTASKS') == '1', 'tasks')
    require(env.get('SLURM_CPUS_PER_TASK') == '32', 'cpus_per_task')
    require(env.get('SLURM_JOB_CPUS_PER_NODE') == '32', 'allocated_cpus')
    require(env.get('SLURM_MEM_PER_NODE') == '65536', 'allocated_memory')
    require(env.get('SLURM_RESTART_COUNT', '0') == '0', 'restarted_job')
    require(not any(k.startswith('SLURM_ARRAY_') for k in env), 'array_job')
    require(env.get('SLURM_JOB_CONSTRAINTS', 'x6342') == 'x6342', 'constraint')
    return {k: env.get(k) for k in (
        'SLURM_JOB_ID', 'SLURM_JOB_NAME', 'SLURM_JOB_PARTITION', 'SLURM_JOB_NODELIST',
        'SLURM_JOB_NUM_NODES', 'SLURM_NNODES', 'SLURM_NTASKS', 'SLURM_CPUS_PER_TASK',
        'SLURM_JOB_CPUS_PER_NODE', 'SLURM_MEM_PER_NODE', 'SLURM_RESTART_COUNT',
        'SLURM_JOB_CONSTRAINTS')}


def unescape_mount(text):
    return re.sub(r'\\([0-7]{3})', lambda m: chr(int(m[1], 8)), text)


def mounts():
    rows = []
    for line in Path('/proc/self/mountinfo').read_text().splitlines():
        left, right = line.split(' - ', 1)
        a, b = left.split(), right.split()
        rows.append({'root': unescape_mount(a[3]), 'point': unescape_mount(a[4]),
                     'type': b[0], 'source': unescape_mount(b[1]), 'options': b[2]})
    return rows


def filesystem():
    resolved = str(STUDY.resolve(strict=True))
    candidates = [m for m in mounts() if resolved == m['point'] or
                  resolved.startswith(m['point'].rstrip('/') + '/')]
    m = max(candidates, key=lambda row: len(row['point']))
    return {'path': str(STUDY), 'realpath': resolved, 'mount_point': m['point'],
            'mount_source': m['source'], 'filesystem_type': m['type']}


def cpu():
    rows = Path('/proc/cpuinfo').read_text().strip().split('\n\n')
    models, flags = set(), set()
    for row in rows:
        fields = dict(line.split(':', 1) for line in row.splitlines() if ':' in line)
        clean = {k.strip(): v.strip() for k, v in fields.items()}
        models.add(clean.get('model name', ''))
        flags.add(tuple(sorted(set(clean.get('flags', '').split()))))
    require(len(models) == 1 and '' not in models, 'heterogeneous_cpu_model')
    return {'model': next(iter(models)), 'observed_isa_flag_sets': [list(x) for x in sorted(flags)]}


def node_class():
    # Native read-only scheduler identity, not an allocation or a measurement.
    node_name = os.environ.get('SLURMD_NODENAME', '')
    require(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,254}', node_name) is not None,
            'invalid_slurmd_node_name')
    result = subprocess.run(['scontrol', '--oneliner', 'show', 'node', node_name],
                            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, check=False)
    require(result.returncode == 0 and len(result.stdout) <= LOG_STREAM_LIMIT and
            len(result.stderr) <= LOG_STREAM_LIMIT, 'native_node_identity_unavailable')
    body = result.stdout.decode('ascii')
    fields = dict(re.findall(r'(?:^|\s)([A-Za-z][A-Za-z0-9_]*)=([^\s]+)', body))
    require(fields.get('NodeName') == node_name, 'native_node_name')
    features = sorted(fields.get('ActiveFeatures', '').split(','))
    require(features == HARDWARE_CLASS['active_features'], 'native_node_feature_class')
    partitions = fields.get('Partitions', '').split(',')
    require(all(p in partitions for p in ('norm', 'short')), 'native_shared_class_routing')
    return {'requested_partition': 'norm', 'reported_partition': os.environ['SLURM_JOB_PARTITION'],
            'class': HARDWARE_CLASS['name'], 'active_features': features,
            'slurmd_node_name': node_name, 'os_hostname': os.uname().nodename,
            'native_scontrol_node': body, 'native_stderr': result.stderr.decode('ascii')}


def peak():
    """Read a legitimate job/batch cgroup high-water mark, never substitute RSS=0."""
    try:
        groups = [line.split(':', 2) for line in Path('/proc/self/cgroup').read_text().splitlines()]
        mount_rows = mounts()
        job = os.environ['SLURM_JOB_ID']
        for version in (2, 1):
            for _, controllers, group in groups:
                if version == 2 and controllers != '':
                    continue
                if version == 1 and 'memory' not in controllers.split(','):
                    continue
                for m in mount_rows:
                    if m['type'] != ('cgroup2' if version == 2 else 'cgroup'):
                        continue
                    if version == 1 and 'memory' not in m['options'].split(','):
                        continue
                    job_pattern = r'job[_-]' + re.escape(job) + r'(?:\.scope|\.slice)?'
                    identity_parts = Path(m['root']).parts + Path(group).parts
                    if not any(re.fullmatch(job_pattern, part) for part in identity_parts):
                        # A namespace/root/user cgroup without an authenticated
                        # Slurm job component is not evidence for this job.
                        continue
                    mr = m['root'].rstrip('/')
                    if mr and group != mr and not group.startswith(mr + '/'):
                        continue
                    relative = group[len(mr):].lstrip('/')
                    own = Path(m['point']) / relative
                    require(own == Path(m['point']) or Path(m['point']) in own.parents, 'cgroup_path')
                    # Prefer the job ancestor when named explicitly by Slurm;
                    # otherwise use only our own batch cgroup, never a shared parent.
                    candidates = [own]
                    for parent in own.parents:
                        if parent == Path(m['point']):
                            break
                        if re.fullmatch(job_pattern, parent.name):
                            candidates.insert(0, parent)
                            break
                    filename = 'memory.peak' if version == 2 else 'memory.max_usage_in_bytes'
                    for directory in candidates:
                        p = directory / filename
                        if p.is_file():
                            value = p.read_text().strip()
                            require(re.fullmatch(r'[0-9]+', value) is not None, 'bad_cgroup_peak')
                            return {'available': True, 'version': version, 'path': str(p),
                                    'scope': 'job' if directory != own else 'own_batch',
                                    'bytes': int(value), 'sample_utc': utc()}
        return {'available': False, 'bytes': None, 'reason': 'no_readable_memory_high_water_file'}
    except (OSError, ValueError, KeyError):
        return {'available': False, 'bytes': None, 'reason': 'cgroup_discovery_or_read_failed'}


def sizes(run):
    total = logs = 0
    for parent, directories, files in os.walk(run, followlinks=False):
        for name in directories + files:
            p = Path(parent) / name
            st = p.lstat()
            require(not stat.S_ISLNK(st.st_mode), 'run_symlink')
            if stat.S_ISREG(st.st_mode):
                total += st.st_size
                if run / 'logs' in p.parents:
                    logs += st.st_size
            else:
                require(stat.S_ISDIR(st.st_mode), 'run_special_file')
    return {'regular_file_bytes': total, 'logs_bytes': logs}


def load_freeze():
    p = PACKAGE / 'NODE_TOOLCHAIN_FREEZE.json'
    require(p.is_file() and not p.is_symlink() and not p.stat().st_mode & 0o222, 'freeze_not_sealed')
    f = read_json(p)
    require(set(f) == {'schema', 'hardware_class', 'filesystem_path', 'compiler', 'binaries',
                      'python', 'protocol', 'package_sha256', 'evidence_sha256',
                      'original_metadata_receipt', 'timing_source_sha256'}, 'freeze_keys')
    require(f['schema'] == 'BIT002-NODE-TOOLCHAIN-FREEZE-V2', 'freeze_schema')
    require(f['hardware_class'] == HARDWARE_CLASS, 'freeze_class')
    require(f['filesystem_path'] == str(STUDY), 'freeze_filesystem_path')
    require(f['binaries'] == BINARIES and f['compiler'] == COMPILER, 'freeze_build_identity')
    require(f['python'] == PYTHON, 'freeze_python')
    require(f['protocol'] == {'path': str(PACKAGE / 'PROTOCOL_V0_4_ACCEPTED.txt'),
                              'sha256': PROTO_HASH}, 'freeze_protocol')
    require(set(f['package_sha256']) == {'benchmark.sh', 'production.sh', 'runtime_helper.py',
                                      'NODE_TOOLCHAIN_FREEZE.schema.json'}, 'source_fields')
    require(f['evidence_sha256'] == EVIDENCE_HASHES, 'freeze_evidence')
    require(f['original_metadata_receipt'] == ORIGINAL_METADATA_RECEIPT, 'freeze_original_receipt')
    require(f['timing_source_sha256'] == TIMING_SOURCE_HASHES, 'freeze_timing_sources')
    return f


def parse_timing(role, body, returncode, measured_ns):
    """Two fixed public interfaces; no templates, aliases or result parsing."""
    if role == 'analyzer':
        match = re.fullmatch(rb'total_ns=(0|[1-9][0-9]*) K_ns=(0|[1-9][0-9]*) (PASS|FAIL)\n', body)
        require(match is not None, 'analyzer_timing_literal')
        wall_ns, k_ns = int(match[1]), int(match[2])
        require(max(wall_ns, k_ns) <= 2 ** 64 - 1, 'analyzer_timing_uint64')
        wall, k = D(wall_ns) / D(10 ** 9), D(k_ns) / D(10 ** 9)
        source_status = match[3].decode('ascii')
        valid = source_status == 'PASS' and returncode == 0
        consistent = (source_status, returncode) in (('PASS', 0), ('FAIL', 1))
        wall_allowance, order_allowance = D(0), D(0)
    else:
        require(role == 'auditor', 'unknown_timing_role')
        match = re.fullmatch(rb'status=(VALID|INVALID) total_seconds=((?:0|[1-9][0-9]*)\.[0-9]{9}) K_seconds=((?:0|[1-9][0-9]*)\.[0-9]{9})\n', body)
        require(match is not None, 'auditor_timing_literal')
        source_status = match[1].decode('ascii')
        wall, k = D(match[2].decode('ascii')), D(match[3].decode('ascii'))
        valid = source_status == 'VALID' and returncode == 0
        consistent = (source_status, returncode) in (('VALID', 0), ('INVALID', 1))
        # Fixed nine-place rendering: <=1 ns per displayed duration (including
        # binary64 conversion below the allocation's 600 s timing limit).
        # Two independently rendered values need <=2 ns for their ordering.
        wall_allowance, order_allowance = D('0.000000001'), D('0.000000002')
    measured = D(measured_ns) / D(10 ** 9)
    require(consistent, 'timing_status_exit_inconsistent')
    require(wall.is_finite() and k.is_finite() and 0 <= k and 0 <= wall and
            wall <= D(600) and k <= wall + order_allowance and
            wall <= measured + wall_allowance and k <= measured, 'timing_duration_order')
    return {'source_status': source_status, 'status': 'VALID' if valid else 'INVALID',
            'wall_seconds': format(wall, 'f'), 'k_seconds': format(k, 'f'),
            'measured_whole_wall_seconds': format(measured, 'f'),
            'source_wall_vs_measured_allowance_seconds': str(wall_allowance),
            'K_vs_source_wall_allowance_seconds': str(order_allowance)}


def prepare(kind):
    prepare_start = time.monotonic_ns()
    allocation_info = allocation()
    require(Path(__file__).resolve() == PACKAGE / 'runtime_helper.py', 'package_location')
    f = load_freeze()
    actual_cpu, actual_fs, actual_class = cpu(), filesystem(), node_class()
    require(actual_cpu['model'] == CPU_MODEL, 'cpu_freeze_mismatch')
    require(actual_fs['path'] == f['filesystem_path'], 'filesystem_freeze_mismatch')
    require(sys.executable == PYTHON['path'] and
            '.'.join(str(x) for x in sys.version_info[:3]) == PYTHON['version'] and
            digest(sys.executable) == PYTHON['sha256'], 'python_identity')
    require(digest(COMPILER['path']) == COMPILER['sha256'], 'compiler_executable_identity')
    for obj in list(BINARIES.values()) + [f['protocol']]:
        require(digest(obj['path']) == obj['sha256'], 'binary_or_protocol_hash')
    for name, expected in f['package_sha256'].items():
        require(re.fullmatch(r'[0-9a-f]{64}', expected) is not None and digest(PACKAGE / name) == expected,
                'package_source_hash')
    for name, expected in EVIDENCE_HASHES.items():
        evidence = PACKAGE / name
        require(evidence.is_file() and not evidence.is_symlink() and
                not evidence.stat().st_mode & 0o222 and digest(evidence) == expected, 'authority_evidence_hash')
    run = root(kind)
    require(not run.exists() and not run.is_symlink(), 'run_exists')
    marker = STUDY / ('.' + run.name + '.once')
    commit(marker, ('job_id=' + os.environ['SLURM_JOB_ID'] + '\ncreated_utc=' + utc() + '\n').encode())
    run.mkdir(mode=0o700)
    fsync_dir(STUDY)
    for name in ('results', 'audit', 'agreement', 'logs'):
        (run / name).mkdir(mode=0o700)
    # data deliberately remains absent: the exact producer creates it new.
    put_json(run / 'logs/IDENTITIES.json', {
        'schema': 'BIT002-RUNTIME-IDENTITIES-R1', 'kind': kind, 'job': allocation_info,
        'node': os.uname().nodename, 'cpu': actual_cpu, 'filesystem': actual_fs,
        'hardware_class': actual_class, 'python': PYTHON,
        'compiler': f['compiler'], 'binaries': BINARIES, 'protocol': f['protocol'],
        'package_sha256': f['package_sha256'], 'freeze_json_sha256': digest(PACKAGE / 'NODE_TOOLCHAIN_FREEZE.json'),
        'evidence_sha256': EVIDENCE_HASHES, 'original_metadata_receipt': ORIGINAL_METADATA_RECEIPT,
        'timing_source_sha256': TIMING_SOURCE_HASHES,
        'cgroup_initial': peak(), 'created_utc': utc(), 'prepare_start_monotonic_ns': prepare_start,
        'native_accounting': 'Root must obtain original job and every step after terminal state'})
    fsync_dir(run)


def step_info(run, name, start, end, success, **extra):
    value = {'step': name, 'start_monotonic_ns': start, 'end_monotonic_ns': end,
             'wall_ns': end - start, 'success': success}
    value.update(extra)
    put_json(run / ('logs/' + name + '.json'), value)


def child_limits():
    # The largest accepted individual data file is exactly this size.
    resource.setrlimit(resource.RLIMIT_FSIZE, (2088064, 2088064))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def timed(step, command):
    kind = 'benchmark' if step in ('B1', 'B5', 'B6') else 'production'
    require(step in ('B1', 'B5', 'B6', 'P2', 'P4', 'P5'), 'unknown_timed_step')
    run = root(kind)
    tier = 'benchmark-r1' if kind == 'benchmark' else 'production-r1'
    role = {'B1': 'producer', 'B5': 'analyzer', 'B6': 'auditor',
            'P2': 'producer', 'P4': 'analyzer', 'P5': 'auditor'}[step]
    expected = [BINARIES[role]['path'], '--tier', tier]
    if role == 'producer':
        expected += ['--out', str(run / 'data'), '--threads', '32']
    else:
        out = run / ('results/RESULTS_ANALYZER.txt' if role == 'analyzer' else 'audit/RESULTS_AUDITOR.txt')
        expected += ['--data', str(run / 'data'), '--out', str(out)]
        if kind == 'benchmark':
            expected += ['--timing-only']
    require(command == expected, 'command_not_frozen')
    # Recheck the binary immediately before each authorized original invocation.
    require(digest(command[0]) == BINARIES[role]['sha256'], 'binary_changed')
    streams = [open(run / ('logs/' + step + '.' + name), 'xb', buffering=0)
               for name in ('stdout', 'stderr')]
    counts = [0, 0]
    violation = None
    initial = peak()
    observed = [initial] if initial['available'] else []
    start_utc = utc()
    t0 = time.monotonic_ns()
    proc = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, start_new_session=True, preexec_fn=child_limits)
    selector = selectors.DefaultSelector()
    selector.register(proc.stdout, selectors.EVENT_READ, 0)
    selector.register(proc.stderr, selectors.EVENT_READ, 1)
    last_poll = 0
    while selector.get_map() or proc.poll() is None:
        for key, _ in selector.select(timeout=0.1):
            block = os.read(key.fileobj.fileno(), 65536)
            index = key.data
            if not block:
                selector.unregister(key.fileobj)
                key.fileobj.close()
                continue
            keep = max(0, LOG_STREAM_LIMIT - counts[index])
            streams[index].write(block[:keep])
            counts[index] += len(block)
            if counts[index] > LOG_STREAM_LIMIT:
                violation = 'stdout_or_stderr_limit_exceeded'
        now = time.monotonic_ns()
        if now - last_poll >= 100000000:
            last_poll = now
            sample = peak()
            if sample['available'] and (not observed or sample['bytes'] > observed[-1]['bytes']):
                observed = [sample]
            try:
                used = sizes(run)
                if used['logs_bytes'] > 16 * 1024 ** 2:
                    violation = 'total_logs_limit_exceeded'
                if used['regular_file_bytes'] > (GIB if kind == 'benchmark' else 2 * GIB):
                    violation = 'total_output_limit_exceeded'
                if sample['available'] and sample['bytes'] > 64 * GIB:
                    violation = 'allocation_memory_limit_exceeded'
            except (OSError, ValueError):
                violation = 'output_monitor_failed'
        if violation and proc.poll() is None:
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
    code = proc.wait()
    t1 = time.monotonic_ns()
    selector.close()
    for stream in streams:
        stream.flush()
        os.fsync(stream.fileno())
        stream.close()
    ending = peak()
    if ending['available']:
        observed.append(ending)
    timing = None
    if kind == 'benchmark' and role != 'producer':
        try:
            body = (run / ('logs/' + step + '.stdout')).read_bytes()
            timing = parse_timing(role, body, code, t1 - t0)
            # Timing-only must not commit any results or audit report.
            require(all(not list((run / d).iterdir()) for d in ('results', 'audit')), 'timing_only_wrote_results')
        except (OSError, ValueError, decimal.InvalidOperation):
            violation = violation or 'timing_stdout_or_discard_contract_failed'
    if kind == 'production' and role != 'producer':
        try:
            seal_outputs(run)
        except (OSError, ValueError):
            violation = violation or 'output_sealing_failed'
    success = code == 0 and violation is None and (timing is None or timing['status'] == 'VALID')
    step_info(run, step, t0, t1, success, command=command, start_utc=start_utc,
              end_utc=utc(), returncode=code, stdout_bytes_seen=counts[0], stderr_bytes_seen=counts[1],
              stdout_bytes_saved=min(counts[0], LOG_STREAM_LIMIT),
              stderr_bytes_saved=min(counts[1], LOG_STREAM_LIMIT), violation=violation,
              cgroup_initial=initial, cgroup_final=ending,
              cgroup_peak_observed=max(observed, key=lambda row: row['bytes']) if observed else
              {'available': False, 'bytes': None, 'reason': 'unavailable_throughout_step'}, timing_stdout=timing)
    return 0 if success else 1


def iovol():
    run = root('benchmark')
    t0 = time.monotonic_ns()
    success = False
    try:
        directory = run / 'iovol'
        directory.mkdir(mode=0o700)
        fsync_dir(run)
        files = [(run / 'data/chunk_00.bin', directory / ('io_%02d.bin' % n), 2088064)
                 for n in range(42)]
        files.append((run / 'data/trace_audit_blocks.bin', directory / 'trace_audit_blocks.bin', 524832))
        inventory = []
        for source, target, length in files:
            require(source.is_file() and not source.is_symlink() and source.stat().st_size == length,
                    'iovol_source_size')
            temporary = directory / ('.tmp.' + target.name)
            with open(source, 'rb') as src, open(temporary, 'xb') as dst:
                copied = 0
                for block in iter(lambda: src.read(1024 ** 2), b''):
                    copied += len(block)
                    require(copied <= length, 'iovol_source_grew')
                    dst.write(block)
                require(copied == length, 'iovol_source_shrank')
                dst.flush()
                os.fsync(dst.fileno())
            os.rename(temporary, target)
            fsync_dir(directory)
            inventory.append({'name': target.name, 'bytes': length, 'sha256': digest(target)})
        success = True
    finally:
        t1 = time.monotonic_ns()
        step_info(run, 'B2', t0, t1, success, files=inventory if success else None)


def checked_sha(command, cwd, input_bytes=None):
    result = subprocess.run(command, cwd=str(cwd), input=input_bytes,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    require(len(result.stdout) <= LOG_STREAM_LIMIT and len(result.stderr) <= LOG_STREAM_LIMIT,
            'sha_diagnostic_limit')
    require(result.returncode == 0, 'sha256sum_failed')
    return result.stdout + result.stderr


def rehash():
    run = root('benchmark')
    t0 = time.monotonic_ns()
    success = False
    output = b''
    try:
        names = ['io_%02d.bin' % n for n in range(42)] + ['trace_audit_blocks.bin']
        require(sorted(p.name for p in (run / 'iovol').iterdir()) == sorted(names), 'iovol_inventory')
        output = checked_sha(['/usr/bin/sha256sum'] + names, run / 'iovol')
        expected = ''.join(row['sha256'] + '  ' + row['name'] + '\n'
                           for row in read_json(run / 'logs/B2.json')['files']).encode('ascii')
        require(output == expected, 'iovol_rehash_mismatch')
        success = True
    finally:
        t1 = time.monotonic_ns()
        commit(run / 'logs/B3.sha256sum', output)
        step_info(run, 'B3', t0, t1, success)


def dc(kind):
    run = root(kind)
    step = 'B4' if kind == 'benchmark' else 'P3'
    t0 = time.monotonic_ns()
    success, output = False, b''
    try:
        producer = read_json(run / ('logs/' + ('B1' if kind == 'benchmark' else 'P2') + '.json'))
        require(producer['success'] and producer['returncode'] == 0, 'producer_not_successful')
        data = run / 'data'
        n, chunks = (1024, 2) if kind == 'benchmark' else (42000, 42)
        tier = 'benchmark-r1' if kind == 'benchmark' else 'production-r1'
        names = ['chunk_%02d.bin' % k for k in range(chunks)] + ['trace_audit_blocks.bin']
        require(data.is_dir() and not data.is_symlink(), 'data_directory')
        require(sorted(p.name for p in data.iterdir()) == sorted(names + ['MANIFEST.txt', 'MANIFEST.sha256']),
                'DC_inventory')
        require(all(stat.S_ISREG(p.lstat().st_mode) for p in data.iterdir()), 'DC_nonregular')
        require((data / 'MANIFEST.txt').stat().st_size < 8192 and
                (data / 'MANIFEST.sha256').stat().st_size == 79, 'DC_manifest_size')
        body = (data / 'MANIFEST.txt').read_bytes()
        lines = body.decode('ascii').splitlines(keepends=True)
        prefix = ['BRC2-MANIFEST-V2', 'study=PHASE3-BIT-RETRIEVAL-CAUSALITY-002',
                  'protocol_sha256=' + PROTO_HASH, 'tier=' + tier,
                  'producer_build_id=' + BINARIES['producer']['sha256'], 'blocks=' + str(n),
                  'chunk_blocks=1000', 'chunks=' + str(chunks), 'record_length=2088']
        require(len(lines) == 9 + len(names) + 2 and lines[:9] == [x + '\n' for x in prefix], 'DC_manifest_prefix')
        checks = []
        for index, name in enumerate(names):
            match = re.fullmatch(r'file ' + re.escape(name) + r' (0|[1-9][0-9]*) ([0-9a-f]{64})\n', lines[9 + index])
            require(match is not None, 'DC_file_line')
            length = int(match[1])
            if name.startswith('chunk_'):
                expected = 64 + 2088 * min(1000, n - index * 1000)
                require(length == expected, 'DC_chunk_size')
            elif kind == 'benchmark':
                require(length == 524832, 'DC_trace_size')
            else:
                require(395808 <= length <= 524832 and (length - 544) % 32 == 0, 'DC_trace_size')
            require((data / name).stat().st_size == length, 'DC_listed_size')
            checks.append(match[2] + '  ' + name + '\n')
        query = re.fullmatch(r'total_queries=(0|[1-9][0-9]*)\n', lines[-2])
        require(query is not None and lines[-1] == 'complete=1\n', 'DC_manifest_suffix')
        if kind == 'benchmark':
            require(int(query[1]) == 34209792, 'DC_benchmark_queries')
        else:
            require(n * 128 * 198 <= int(query[1]) <= n * 128 * 261, 'DC_query_range')
        manifest_hash = hashlib.sha256(body).hexdigest()
        require((data / 'MANIFEST.sha256').read_bytes() ==
                (manifest_hash + '  MANIFEST.txt\n').encode('ascii'), 'DC_manifest_hash_line')
        output += checked_sha(['/usr/bin/sha256sum', '-c', 'MANIFEST.sha256'], data)
        output += checked_sha(['/usr/bin/sha256sum', '-c', '-'], data, ''.join(checks).encode('ascii'))
        success = True
    finally:
        t1 = time.monotonic_ns()
        commit(run / ('logs/' + step + '.sha256sum-c'), output)
        step_info(run, step, t0, t1, success, data_committed=success)


def seal_outputs(run):
    paths = [run / 'results/RESULTS_ANALYZER.txt', run / 'audit/RESULTS_AUDITOR.txt',
             run / 'audit/AUDIT_REPORT.txt']
    for directory in ('results', 'audit'):
        for p in (run / directory).iterdir():
            require(stat.S_ISREG(p.lstat().st_mode), 'nonregular_sealed_output')
            os.chmod(p, 0o400)
            fd = os.open(p, os.O_RDONLY)
            try:
                os.fsync(fd)
            finally:
                os.close(fd)
            fsync_dir(p.parent)
    return paths


def agreement():
    run = root('production')
    paths = seal_outputs(run)
    exits = []
    for name in ('P4', 'P5'):
        p = run / ('logs/' + name + '.json')
        exits.append(read_json(p)['returncode'] if p.is_file() else 255)
    a, u = exits
    committed = read_json(run / 'logs/P3.json')['data_committed']
    present = all(p.is_file() and not p.is_symlink() for p in paths)
    ah = digest(paths[0]) if paths[0].is_file() else None
    uh = digest(paths[1]) if paths[1].is_file() else None
    equal = ah is not None and uh is not None and ah == uh
    status = 'CRASH' if u < 0 or u >= 3 or not all(p.is_file() for p in paths[1:]) else ('PASS' if u == 0 else 'FAIL')
    body = ('BRC2-AGREEMENT-V3\ndata_committed=%d\nanalyzer_exit=%d\nauditor_exit=%d\n'
            'results_sha256_equal=%d\nauditor_status=%s\n') % (int(committed), a, u, int(equal), status)
    commit(run / 'agreement/AGREEMENT.txt', body.encode('ascii'))
    good = committed and a == u == 0 and equal and status == 'PASS' and present
    for name in ('P4', 'P5'):
        p = run / ('logs/' + name + '.json')
        good = good and p.is_file() and read_json(p)['success']
    put_json(run / 'logs/P6.json', {'success': bool(good), 'analyzer_sha256': ah,
                                   'auditor_sha256': uh, 'auditor_status': status,
                                   'agreement_sha256': digest(run / 'agreement/AGREEMENT.txt')})
    return 0 if good else 1


def receipt(kind, shell_status):
    receipt_start = time.monotonic_ns()
    run = root(kind)
    steps = ('B1', 'B2', 'B3', 'B4', 'B5', 'B6') if kind == 'benchmark' else ('P2', 'P3', 'P4', 'P5', 'P6')
    found = {name: read_json(run / ('logs/' + name + '.json'))
             for name in steps if (run / ('logs/' + name + '.json')).is_file()}
    identities = read_json(run / 'logs/IDENTITIES.json')
    samples = [identities['cgroup_initial'], peak()]
    samples += [row[key] for row in found.values() for key in
                ('cgroup_initial', 'cgroup_final', 'cgroup_peak_observed') if key in row]
    available = [p for p in samples if p['available']]
    memory = max(available, key=lambda row: row['bytes']) if available else {
        'available': False, 'bytes': None, 'reason': 'cgroup_peak_unavailable_explicitly'}
    step_success = shell_status == 0 and len(found) == len(steps) and all(row['success'] for row in found.values())
    if kind == 'production':
        seal_outputs(run)
    data = {'schema': 'BIT002-' + kind.upper() + '-MEASUREMENTS-R1',
            'job_id': os.environ['SLURM_JOB_ID'], 'created_utc': utc(),
            'identities': identities, 'shell_exit_before_receipt': shell_status,
            'all_original_steps_successful': step_success, 'steps': found,
            'cgroup_peak': memory, 'cgroup_samples': samples,
            'slurm_MaxRSS_bytes': None, 'native_job_and_all_steps_accounting': 'PENDING_ROOT_AFTER_TERMINAL',
            'job_success': 'NOT_CLAIMED_BEFORE_NATIVE_ACCOUNTING', 'EA': 'NOT_PERFORMED'}
    intervals = [(row['start_monotonic_ns'], row['end_monotonic_ns'])
                 for row in found.values() if 'wall_ns' in row]
    ordered = sorted(intervals)
    require(all(left[1] <= right[0] for left, right in zip(ordered, ordered[1:])), 'overlapping_T_intervals')
    data['timing_partition'] = {
        'measured_intervals_ns': ordered,
        'untimed_ns_through_receipt_start': receipt_start - identities['prepare_start_monotonic_ns'] -
        sum(end - start for start, end in intervals),
        'monitor_within_child_wall': True,
        'other_setup_and_receipt_work': 'Unscaled 120-second allowance; native accounting still required',
        'receipt_start_monotonic_ns': receipt_start}
    data['memory_criterion'] = 'PENDING_NATIVE_MAXRSS' if memory['available'] else 'UNAVAILABLE_CGROUP_PEAK'
    budget = GIB if kind == 'benchmark' else 2 * GIB
    if kind == 'benchmark':
        terms = {}
        for key, name in (('T_prod', 'B1'), ('T_ioS', 'B2'), ('T_hashS', 'B3'),
                          ('T_P3', 'B4'), ('T_an', 'B5'), ('T_aud', 'B6')):
            terms[key] = str(D(found[name]['wall_ns']) / D(10 ** 9)) if name in found else None
        for key, name in (('T_an_K', 'B5'), ('T_aud_K', 'B6')):
            timing = found.get(name, {}).get('timing_stdout')
            terms[key] = timing['k_seconds'] if timing else None
        data['T_seconds'] = terms
        data['scale_exact'] = '42000/1024'
        data['scale_decimal'] = '41.015625'
        data['allowance_seconds'] = '120'
        # Maximum data + manifest bound + 1 MiB result/report/agreement bound
        # + 16 MiB logs + 1 MiB resource/receipt/marker overhead; no scaling of logs.
        projected_output = 87698688 + 524832 + 8192 + 1024 ** 2 + 16 * 1024 ** 2 + 1024 ** 2
        data['projected_production_output_upper_bytes'] = projected_output
        data['projection'] = None
        if all(v is not None for v in terms.values()):
            t = {k: D(v) for k, v in terms.items()}
            scale = D(42000) / D(1024)
            C = scale * t['T_prod']
            IO = t['T_ioS']
            V = (t['T_hashS'] + scale * t['T_P3'] + scale * (t['T_an'] - t['T_an_K']) +
                 scale * (t['T_aud'] - t['T_aud_K']) + t['T_an_K'] + t['T_aud_K'] + D(120))
            total = C + IO + V
            data['projection'] = {k: str(v) for k, v in
                                  (('C', C), ('IO', IO), ('V', V), ('total', total), ('twice_total', 2 * total))}
            data['projection_time_pass'] = 2 * total <= D(14400)
        else:
            data['projection_time_pass'] = False
        data['both_timing_status_VALID'] = all(
            (found.get(n, {}).get('timing_stdout') or {}).get('status') == 'VALID' for n in ('B5', 'B6'))
        step_success = bool(step_success and data['projection_time_pass'] and data['both_timing_status_VALID'] and
                            projected_output <= GIB and (not memory['available'] or memory['bytes'] <= 32 * GIB))
        data['G5'] = 'PENDING_ROOT_BINDING_RECEIPT' if step_success else 'FAILED_NO_RESUBMISSION'
        name = 'BENCH_MEASUREMENTS.json'
    else:
        step_success = bool(step_success and (not memory['available'] or memory['bytes'] <= 64 * GIB))
        summary_bytes = sum(p.stat().st_size for directory in ('results', 'audit', 'agreement')
                            for p in (run / directory).iterdir() if p.is_file())
        data['results_report_agreement_bytes'] = summary_bytes
        step_success = bool(step_success and summary_bytes < 1024 ** 2)
        name = 'JOB_MEASUREMENTS.json'
    data['known_in_job_criteria_pass'] = step_success
    if not step_success:
        commit(run / 'FAILURE.txt', b'Original job or measurement criterion failed. Route closed.\n'
               b'Preserve all evidence; no resubmission, rerun, partial aggregation or interpretation.\n')
    # Include the receipt and its once marker. This is serialization sizing,
    # never a retry of a process, measurement, or scientific operation.
    data['output_limit_pass'] = True
    def settle_size():
        used = sizes(run)
        marker_bytes = (STUDY / ('.' + run.name + '.once')).stat().st_size
        base = used['regular_file_bytes'] + marker_bytes
        data['output'] = dict(used, exclusive_once_marker_bytes=marker_bytes,
                              receipt_bytes=0, total_including_receipt_bytes=base)
        for _ in range(12):
            length = len(encoded(data))
            if data['output']['receipt_bytes'] == length:
                return data['output']['total_including_receipt_bytes'] <= budget and used['logs_bytes'] <= 16 * 1024 ** 2
            data['output']['receipt_bytes'] = length
            data['output']['total_including_receipt_bytes'] = base + length
        raise ValueError('receipt_size_did_not_stabilize')
    within = settle_size()
    if not within:
        data['output_limit_pass'] = False
        data['known_in_job_criteria_pass'] = False
        if kind == 'benchmark':
            data['G5'] = 'FAILED_NO_RESUBMISSION'
        if not (run / 'FAILURE.txt').exists():
            commit(run / 'FAILURE.txt', b'Output criterion failed. Route closed; preserve all evidence.\n')
        settle_size()
    put_json(run / name, data)
    return 0 if step_success and within else 1


def main():
    require(len(sys.argv) >= 2, 'missing_helper_primitive')
    cmd = sys.argv[1]
    if cmd == 'prepare' and len(sys.argv) == 3:
        prepare(sys.argv[2])
        return 0
    if cmd == 'timed' and len(sys.argv) >= 4:
        allocation()
        return timed(sys.argv[2], sys.argv[3:])
    if cmd == 'iovol' and len(sys.argv) == 2:
        allocation()
        iovol()
        return 0
    if cmd == 'rehash' and len(sys.argv) == 2:
        allocation()
        rehash()
        return 0
    if cmd == 'dc' and len(sys.argv) == 3:
        allocation()
        dc(sys.argv[2])
        return 0
    if cmd == 'agreement' and len(sys.argv) == 2:
        allocation()
        return agreement()
    if cmd == 'receipt' and len(sys.argv) == 4:
        allocation()
        return receipt(sys.argv[2], int(sys.argv[3]))
    raise ValueError('unknown_helper_primitive')


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Exception as error:
        # Do not print arbitrary child/data/error text or a results-bearing traceback.
        code = str(error) if isinstance(error, ValueError) and re.fullmatch(r'[A-Za-z0-9_]+', str(error)) else type(error).__name__
        print('BIT002 helper failure category=' + code, file=sys.stderr)
        sys.exit(1)
