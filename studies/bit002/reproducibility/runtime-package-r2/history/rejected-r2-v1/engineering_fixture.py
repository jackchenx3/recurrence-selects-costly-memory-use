#!/home/chenx3/miniconda3/bin/python3.11
"""AFTER independent SOURCE GO only. Synthetic size-scan engineering checks.

Usage: python3.11 -B engineering_fixture.py NEW_SCRATCH_ROOT HELPER_PATH
No helper entry points other than sizes() are called. No scientific programs,
records, keys, benchmark, scheduler, network, receipts or once markers are used.
The new scratch tree is retained for Root's inspection; never reuse it.
"""
import errno
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import signal
import sys
import threading
import time
from unittest import mock


class FixtureTimeout(Exception):
    pass


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def write_syn(path, body):
    with open(path, 'xb') as stream:
        stream.write(body)
        stream.flush()
        os.fsync(stream.fileno())


def sync_directory(path):
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def expect_error(helper, run, cls, path, operation, attempt, text=None):
    try:
        helper.sizes(run)
    except cls as error:
        context = error.output_monitor_context
        require(context['exception_type'] == cls.__name__, 'lost exception type')
        require(context['message'] == str(error), 'lost original message')
        require(context['path'] == str(path), 'lost path')
        require(context['operation'] == operation, 'lost operation')
        require(context['scan_attempt'] == attempt and
                context['scan_attempt_limit'] == 3, 'wrong scan bound')
        if isinstance(error, OSError):
            require(context['errno'] == error.errno, 'lost errno')
            require(context['filename'] == (str(error.filename) if
                    error.filename is not None else None), 'lost native filename')
        if text is not None:
            require(text in context['message'], 'wrong validation rejection')
        return context
    raise AssertionError('required fatal exception was not raised')


def exercise(helper, scratch, checks):
    def check(name, action):
        try:
            details = action()
            checks.append({'check': name, 'status': 'PASS', 'details': details})
        except FixtureTimeout:
            raise
        except Exception as error:
            checks.append({'check': name, 'status': 'FAIL',
                           'exception_type': type(error).__name__, 'message': str(error)})

    def stable():
        run = scratch / 'stable'
        (run / 'logs').mkdir(parents=True)
        (run / 'empty').mkdir()
        write_syn(run / 'a', b'a' * 17)
        write_syn(run / 'logs' / 'b', b'b' * 23)
        used = helper.sizes(run)
        require(used == {'regular_file_bytes': 40, 'logs_bytes': 23}, 'wrong stable count')
        return used
    check('complete_regular_and_log_counts', stable)

    def vanished(exhaust):
        run = scratch / ('persistent' if exhaust else 'one_rename')
        run.mkdir()
        anchor, temporary, final = run / 'anchor', run / 'temporary', run / 'final'
        write_syn(anchor, b'a' * 37)
        write_syn(temporary, b'z' * 71)
        real_walk = os.walk
        walks = [0]

        def racing_walk(*args, **kwargs):
            walks[0] += 1
            if exhaust and final.exists():
                final.rename(temporary)
            for parent, directories, files in real_walk(*args, **kwargs):
                files.sort()  # anchor is counted before the vanished entry.
                if Path(parent) == run and temporary.name in files:
                    temporary.rename(final)  # real rename AFTER enumeration, BEFORE lstat
                    sync_directory(run)
                yield parent, directories, files

        with mock.patch.object(helper.os, 'walk', racing_walk):
            if exhaust:
                context = expect_error(helper, run, FileNotFoundError, temporary,
                                       'entry_lstat', 3)
                require(walks[0] == 3, 'persistent rename did not exhaust exactly 3 scans')
                return context
            used = helper.sizes(run)
        require(walks[0] == 2, 'single vanished entry did not cause exactly one full rescan')
        require(used == {'regular_file_bytes': 108, 'logs_bytes': 0},
                'rescan skipped final file or retained partial count')
        return {'walks': walks[0], 'usage': used, 'boundary': 'enumeration/entry_lstat'}
    check('deterministic_real_rename_complete_rescan', lambda: vanished(False))
    check('persistent_real_rename_exhaustion_context', lambda: vanished(True))

    def walk_failure(cls, attempts):
        run = scratch / ('walk_' + cls.__name__)
        run.mkdir()
        victim = run / 'unreadable_or_vanished_directory'
        calls = [0]

        def broken_walk(*args, **kwargs):
            calls[0] += 1
            error_number = errno.ENOENT if cls is FileNotFoundError else errno.EACCES
            kwargs['onerror'](cls(error_number, 'synthetic traversal failure', str(victim)))
            return iter(())

        with mock.patch.object(helper.os, 'walk', broken_walk):
            context = expect_error(helper, run, cls, victim, 'walk_scandir', attempts)
        require(calls[0] == attempts, 'wrong traversal retry policy')
        return context
    check('traversal_permission_fatal_no_retry', lambda: walk_failure(PermissionError, 1))
    check('persistent_traversal_disappearance_bounded', lambda: walk_failure(FileNotFoundError, 3))

    def lstat_permission():
        run = scratch / 'lstat_permission'
        run.mkdir()
        victim = run / 'blocked'
        write_syn(victim, b'p')
        real_lstat = Path.lstat
        calls = [0]

        def denied(path, *args, **kwargs):
            if path == victim:
                calls[0] += 1
                raise PermissionError(errno.EACCES, 'synthetic lstat failure', str(path))
            return real_lstat(path, *args, **kwargs)

        with mock.patch.object(Path, 'lstat', denied):
            context = expect_error(helper, run, PermissionError, victim, 'entry_lstat', 1)
        require(calls[0] == 1, 'permission failure was retried')
        return context
    check('entry_permission_fatal_no_retry', lstat_permission)

    def other_os_error():
        run = scratch / 'fatal_io_error'
        run.mkdir()
        victim = run / 'failed_io'
        calls = [0]

        def failed_walk(*args, **kwargs):
            calls[0] += 1
            kwargs['onerror'](OSError(errno.EIO, 'synthetic I/O failure', str(victim)))
            return iter(())

        with mock.patch.object(helper.os, 'walk', failed_walk):
            context = expect_error(helper, run, OSError, victim, 'walk_scandir', 1)
        require(calls[0] == 1, 'non-ENOENT operating-system error was retried')
        return context
    check('other_os_error_fatal_without_retry', other_os_error)

    def native_permission():
        if os.geteuid() == 0:
            return {'native_case': 'NOT_APPLICABLE_PRIVILEGED_UID',
                    'injected_permission_cases': 'required separately'}
        run = scratch / 'native_permission'
        victim = run / 'closed'
        victim.mkdir(parents=True)
        write_syn(victim / 'payload', b'p')
        victim.chmod(0)
        try:
            return expect_error(helper, run, PermissionError, victim, 'walk_scandir', 1)
        finally:
            victim.chmod(0o700)
    check('native_unreadable_directory_fatal_when_applicable', native_permission)

    def rejected_object(kind):
        run = scratch / kind
        run.mkdir()
        victim = run / 'rejected'
        if kind == 'file_symlink':
            write_syn(run / 'target', b'x')
            victim.symlink_to('target')
        elif kind == 'directory_symlink':
            (run / 'target').mkdir()
            victim.symlink_to('target', target_is_directory=True)
        else:
            os.mkfifo(victim, 0o600)
        text = 'run_special_file' if kind == 'fifo' else 'run_symlink'
        return expect_error(helper, run, ValueError, victim, 'entry_lstat', 1, text)
    for kind in ('file_symlink', 'directory_symlink', 'fifo'):
        check(kind + '_fatal', lambda kind=kind: rejected_object(kind))

    def missing_root():
        run = scratch / 'absent'
        return expect_error(helper, run, FileNotFoundError, run, 'root_lstat', 1)
    check('missing_root_fatal_without_rescan', missing_root)

    def bad_root(symlink):
        run = scratch / ('root_link' if symlink else 'root_file')
        if symlink:
            run.symlink_to('stable', target_is_directory=True)
        else:
            write_syn(run, b'x')
        return expect_error(helper, run, ValueError, run, 'root_lstat', 1,
                            'run_symlink' if symlink else 'run_not_directory')
    check('root_symlink_fatal', lambda: bad_root(True))
    check('root_non_directory_fatal', lambda: bad_root(False))

    def concurrent_writer():
        run = scratch / 'concurrent'
        (run / 'logs').mkdir(parents=True)
        payload = b'x' * (128 * 1024)
        ready, go, done, stop, observed = (threading.Event() for _ in range(5))
        failures = []
        progress = [0]
        deadline = time.monotonic() + 35

        def writer():
            try:
                ready.set()
                require(go.wait(2), 'writer start timeout')
                for index in range(64):
                    if stop.is_set() or time.monotonic() >= deadline:
                        raise TimeoutError('writer stopped before completing fixed workload')
                    temporary = run / ('.temporary_%03d' % index)
                    final = run / ('synthetic_%03d' % index)
                    write_syn(temporary, payload)
                    temporary.rename(final)
                    sync_directory(run)
                    progress[0] += 1
                    if index == 0:
                        require(observed.wait(2), 'no live scan observed')
            except Exception as error:
                failures.append({'exception_type': type(error).__name__, 'message': str(error)})
            finally:
                done.set()

        worker = threading.Thread(target=writer, name='synthetic-rename-writer', daemon=True)
        worker.start()
        scans = 0
        try:
            require(ready.wait(2), 'writer not ready')
            go.set()
            while not done.is_set():
                require(time.monotonic() < deadline and scans < 4096, 'live scan bound exceeded')
                used = helper.sizes(run)
                require(used['logs_bytes'] == 0 and
                        0 <= used['regular_file_bytes'] <= 8 * 1024 ** 2,
                        'invalid concurrent byte count')
                scans += 1
                observed.set()
                done.wait(0.001)
            worker.join(timeout=1)
            require(not worker.is_alive() and not failures, 'writer failed: ' + str(failures))
            require(scans > 0 and progress[0] == 64, 'no concurrent observation or incomplete writer')
            used = helper.sizes(run)
            require(used == {'regular_file_bytes': 8 * 1024 ** 2, 'logs_bytes': 0},
                    'final complete count differs')
            require(len(list(run.glob('synthetic_*'))) == 64 and
                    not list(run.glob('.temporary_*')), 'incomplete synthetic commits')
            return {'live_scans': scans, 'committed_files': progress[0],
                    'synthetic_writer_bytes': 8 * 1024 ** 2, 'final_usage': used}
        finally:
            stop.set()
            go.set()
            observed.set()
            worker.join(timeout=1)
    check('real_concurrent_write_fsync_rename_and_final_count', concurrent_writer)


def main():
    checks = []
    report = {'scope': 'synthetic_size_scan_engineering_only', 'status': 'FAIL',
              'checks': checks, 'time_limit_seconds': 55,
              'writer_payload_limit_bytes': 8 * 1024 ** 2,
              'total_synthetic_payload_limit_bytes': 16 * 1024 ** 2}
    started = time.monotonic()

    def expired(signum, frame):
        raise FixtureTimeout('55 second fixture deadline')

    signal.signal(signal.SIGALRM, expired)
    signal.alarm(55)
    try:
        require(len(sys.argv) == 3, 'expected NEW_SCRATCH_ROOT HELPER_PATH')
        require(sys.version_info[:2] == (3, 11), 'existing Python 3.11 required')
        scratch, helper_path = (Path(value) for value in sys.argv[1:])
        require(scratch.is_absolute() and helper_path.is_absolute(), 'absolute paths required')
        require(helper_path.name == 'runtime_helper.py', 'expected reviewed helper path')
        helper_path = helper_path.resolve(strict=True)
        scratch = scratch.parent.resolve(strict=True) / scratch.name
        require(scratch.name not in ('', '.', '..') and
                all(not part.lstrip('.').startswith(('scientific-', 'benchmark-'))
                    for part in scratch.parts), 'scratch overlaps scientific/benchmark namespace')
        require(scratch != helper_path.parent and helper_path.parent not in scratch.parents,
                'scratch must be outside the reviewed package')
        scratch.mkdir(mode=0o700)  # exclusive NEW root, never exist_ok/resume
        sys.dont_write_bytecode = True
        report.update({'scratch_root': str(scratch), 'helper_path': str(helper_path),
                       'helper_sha256': hashlib.sha256(helper_path.read_bytes()).hexdigest(),
                       'python_executable': sys.executable, 'python_version': sys.version})
        spec = importlib.util.spec_from_file_location('reviewed_r2_size_scan', helper_path)
        require(spec is not None and spec.loader is not None, 'helper import spec unavailable')
        helper = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(helper)  # permitted ONLY AFTER nonauthor SOURCE GO
        exercise(helper, scratch, checks)
        report['status'] = 'PASS' if checks and all(c['status'] == 'PASS' for c in checks) else 'FAIL'
    except Exception as error:
        report['error'] = {'exception_type': type(error).__name__, 'message': str(error)}
    finally:
        signal.alarm(0)
        report['elapsed_seconds'] = round(time.monotonic() - started, 6)
    print(json.dumps(report, sort_keys=True))
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())
