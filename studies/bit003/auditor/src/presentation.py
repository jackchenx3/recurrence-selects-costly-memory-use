#!/usr/bin/python3
# Post-G8 ONLY; never called by production auditor or benchmark workflow.
# Uses exact implementation from the frozen fixture module without execution
# of its command entry point. Output is a separate presentation artifact.
import os
import sys
import importlib.machinery
import importlib.util
sys.dont_write_bytecode = True

def main():
    here = os.path.dirname(os.path.abspath(__file__))
    loader = importlib.machinery.SourceFileLoader('own_brs3_fixture', here+'/brs3_fixture')
    spec = importlib.util.spec_from_loader(loader.name, loader)
    f = importlib.util.module_from_spec(spec)
    loader.exec_module(f)
    try:
        if len(sys.argv) != 5 or sys.argv[1] != '--results' or sys.argv[3] != '--output':
            f.fail('CLI',2,'argv')
        src, out = sys.argv[2],sys.argv[4]
        f.pathcheck(src)
        f.pathcheck(out,True)
        b = f.regular_read(src)
        lines = b.decode('ascii').splitlines()
        f.check(len(lines) == 11 and lines[0] == 'BIT003_RESULTS_V1', 'FORMAT',2,'results')
        pairs = dict(line.split('=',1) for line in lines[1:])
        f.check(pairs['tier'] == 'production-r1', 'FORMAT',2,'tier')
        m, a = int(pairs['policy_signed_sum']),int(pairs['accuracy_signed_sum'])
        f.check(abs(m) <= 163840000 and abs(a) <= 5242880000 and b == f.results('production-r1',m,a).encode('ascii'), 'FORMAT',2,'results')
        f.publish(out, f.canonical(f.inference(m,a)))
        return 0
    except f.Failure as e:
        sys.stderr.write('BRS3_ERROR '+e.code+'\n')
        return e.exit_code
    except Exception:
        sys.stderr.write('BRS3_ERROR IO\n')
        return 5
if __name__ == '__main__':
    sys.exit(main())
