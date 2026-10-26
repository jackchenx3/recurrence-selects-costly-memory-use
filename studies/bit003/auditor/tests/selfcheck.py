#!/usr/bin/python3
# Own-output creation and invariance harness. FRCE Slurm after SOURCEGO only.
# No expected tree, producer binary/source/results, runtime timers or benchmark.
import os
import sys
import json
import hashlib
import subprocess
import importlib.machinery
import importlib.util
sys.dont_write_bytecode = True

def module(here):
    loader = importlib.machinery.SourceFileLoader('own_fixture', here+'/brs3_fixture')
    spec = importlib.util.spec_from_loader(loader.name,loader)
    m = importlib.util.module_from_spec(spec)
    loader.exec_module(m)
    return m

def main():
    here = os.path.dirname(os.path.abspath(__file__))
    f = module(here)
    try:
        a = sys.argv[1:]
        f.check(len(a) == 8 and set(a[::2]) == {'--inputs','--fixture-inputs','--output','--mode'},'CLI',2,'argv')
        p = dict(zip(a[::2],a[1::2]))
        f.check(p['--mode'] in ('own','invariance'),'CLI',2,'argv')
        for key in ('--inputs','--fixture-inputs'):
            f.pathcheck(p[key])
        f.pathcheck(p['--output'],True)
        files, members = f.authenticate(p['--fixture-inputs'])
        os.mkdir(p['--output'],0o700)
        root = p['--output']
        env = {'PATH':'/usr/bin:/bin','LANG':'C','LC_ALL':'C','PYTHONDONTWRITEBYTECODE':'1'}
        receipt = []
        def run(name,args,expected=0):
            command = [here+'/'+name]+args
            index = len(receipt)
            with open(root+'/child_%04d.stderr' % index,'xb') as err, open(root+'/child_%04d.stdout' % index,'xb') as out:
                rc = subprocess.call(command,cwd=root,env=env,stdin=subprocess.DEVNULL,stdout=out,stderr=err,close_fds=True)
            receipt.append({'argv':command,'exit':rc})
            f.publish(root+'/progress_%04d.json' % index,f.canonical(receipt[-1]))
            f.check(rc == expected,'MISMATCH',3,'child_exit')
        if p['--mode'] == 'own':
            # Compute every literal case result/exit from payload, never INDEX's
            # expected metadata. The outer comparator remains on coordinator.
            for member in sorted(members):
                c = f.strict_json(files[member])
                try:
                    f.compute(c['kind'],c['payload'],files)
                    expected = 0
                except f.Failure as e:
                    expected = e.exit_code
                path = root+'/FIXOUT_'+c['id']+'.json'
                run('brs3_fixture',['--inputs',p['--fixture-inputs'],'--case',p['--fixture-inputs']+'/'+member,'--output',path],expected)
            variants = [(1,'CA','forward')]
        else:
            variants = [(n,order,mode) for n in range(1,33) for order,mode in (('CA','forward'),('AC','reverse'))]
        baseline_blob = baseline_results = None
        for nt, order, mode in variants:
            stem = root+'/fixture_%02d_%s' % (nt,order)
            data, result, report = stem+'_DATA',stem+'_RESULTS.txt',stem+'_AUDIT.txt'
            run('brs3_fixture_stage',['--inputs',p['--inputs'],'--output',data,'--threads',str(nt),'--order',order,'--mode',mode])
            run('brs3_audit',['--inputs',p['--inputs'],'--tier','fixture-r1','--data',data,'--results',result,'--report',report,'--threads',str(nt)])
            blob = b''.join(f.regular_read(data+'/'+n) for n in ('chunk_000.bin','MANIFEST.json','MANIFEST.sha256'))
            result_bytes = f.regular_read(result)
            # Separate Python structural/endpoint summation from stored bytes.
            chunk = f.regular_read(data+'/chunk_000.bin')
            sm = sa = 0
            for b in range(16):
                r = f.record(chunk[64+b*f.RECORD:64+(b+1)*f.RECORD])
                sm += r['M_C']-r['M_A']
                sa += r['A_C']-r['A_A']
            f.check(result_bytes == f.results('fixture-r1',sm,sa).encode('ascii'),'MISMATCH',3,'independent_endpoint_sum')
            if baseline_blob is None:
                baseline_blob, baseline_results = blob,result_bytes
            f.check(blob == baseline_blob and result_bytes == baseline_results,'MISMATCH',3,'invariance')
        if p['--mode'] == 'invariance':
            # Opaque packing uses authenticated own fixture bytes as supplied
            # blobs. These PACKING_ONLY inventories are never scientific DATA.
            source_chunk = f.regular_read(root+'/fixture_01_CA_DATA/chunk_000.bin')
            source_hash = f.digest(source_chunk)
            packing_manifests = []
            for order in ('forward','reverse'):
                directory = root+'/PACKING_ONLY_'+order
                os.mkdir(directory,0o700)
                index = list(range(200))
                if order == 'reverse':
                    index.reverse()
                rows = {}
                for i in index:
                    f.check(f.digest(source_chunk) == source_hash,'HASH',3,'opaque_blob')
                    name = 'chunk_%03d.bin' % i
                    f.publish(directory+'/'+name,source_chunk)
                    rows[i] = {'name':name,'bytes':len(source_chunk),'sha256':source_hash}
                inventory = f.canonical({'packing_only':True,'files':[rows[i] for i in sorted(rows)]})
                f.publish(directory+'/PACKING_ONLY.json',inventory)
                f.check(sorted(os.listdir(directory)) == ['PACKING_ONLY.json']+['chunk_%03d.bin' % i for i in range(200)],'MISMATCH',3,'packing_names')
                packing_manifests.append(inventory)
            f.check(packing_manifests[0] == packing_manifests[1],'MISMATCH',3,'packing_order')
        f.publish(root+'/OWN_RECEIPT.json',f.canonical({'schema':1,'mode':p['--mode'],'children':receipt,
                 'fixture_data_commitment_sha256':f.digest(baseline_blob),'results_sha256':f.digest(baseline_results)}))
        # This receipt exists only on actual successful runtime; author makes no pass claim.
        return 0
    except f.Failure as e:
        sys.stderr.write('BRS3_ERROR '+e.code+'\n')
        return e.exit_code
    except Exception:
        sys.stderr.write('BRS3_ERROR IO\n')
        return 5
if __name__ == '__main__':
    sys.exit(main())
