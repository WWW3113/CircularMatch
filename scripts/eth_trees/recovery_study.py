#!/usr/bin/env python3
"""Freeze on existing seeds11-20, then compare fixed methods on seeds21-30."""
import argparse
import concurrent.futures as futures
import csv
from datetime import datetime, timezone
import hashlib
import itertools
import json
import math
from pathlib import Path
import statistics as stats
import subprocess
import threading
import time

from tune_registration import ROOT, TOOL, read_csv, write_csv

OUT=ROOT/'results/eth_trees/failure_followup_20261025'
RUNS=OUT/'runs'
RAW=ROOT/'data/eth_trees/raw/trees'
SCANS=[f's{i}' for i in range(1,7)]
PAIRS=list(itertools.combinations(SCANS,2))
SEEDS=list(range(21,31))
LOCK=threading.Lock()
INFERENCE_FILES=['src/cn_recovery.cpp','include/cm/cn_recovery.hpp','src/cn_matching.cpp',
                 'include/cm/cn_matching.hpp','src/cn_descriptor.cpp','src/registration.cpp',
                 'apps/cm_register.cpp','config/default.ini','build/cm_register','build/cm_extract_batch']


def hashes():
    return {p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in INFERENCE_FILES}


def discover():
    """Generate all old-seed candidate runs; do not read reserved-seed results."""
    plan=json.loads((OUT/'plan.json').read_text())
    old_trees=ROOT/'results/eth_trees/tuning_20261025/runs/extraction/control'
    profiles=[dict(name='baseline',mode='off',trigger=3)]+plan['candidate_recovery']
    failed=[]
    for profile in profiles:
        name=profile['name'];dest=RUNS/name;dest.mkdir(parents=True,exist_ok=True)
        jobs=[]
        for seed,(a,b) in itertools.product(range(11,21),PAIRS):
            source=old_trees/a/f'step_seed{seed}_trees.csv';target=old_trees/b/f'step_seed{seed}_trees.csv'
            assert source.exists() and target.exists(),'Run the original ETH extraction study first.'
            output=dest/f'{seed}_{a}-{b}.csv'
            jobs.append(f'{source} {target} {RAW/a}.ply {RAW}/groundtruth/{a}-{b}.tfm {output} {seed}_{a}-{b}')
        batch=RUNS/f'{name}_jobs.txt';batch.write_text('\n'.join(jobs)+'\n')
        args=[ROOT/'build/cm_register',f'--batch={batch}','--thresholds=0.10','--skip_point_error=1']
        if profile['mode']!='off':args += [f'--recovery={profile["mode"]}',f'--recovery_trigger={profile["trigger"]}']
        execute(f'discovery_{name}',args)
        if name=='baseline':
            for job in jobs:
                output=Path(job.split()[4])
                if any(r['solver']=='svd+ransac' and r['success']=='0' for r in read_csv(output)):
                    failed.append(job.replace('/baseline/','/failed/'))
    (RUNS/'failed').mkdir(exist_ok=True)
    batch=RUNS/'failed_jobs.txt';batch.write_text('\n'.join(failed)+'\n')
    if failed:
        execute('diagnostics',[ROOT/'build/cm_register',f'--batch={batch}','--thresholds=0.10',
                               '--skip_point_error=1',f'--diagnostics={RUNS}/traces'])


def freeze():
    plan=json.loads((OUT/'plan.json').read_text());summary=[]
    official={tuple(s.split()) for s in (RAW/'pairs.txt').read_text().splitlines() if s.strip()}
    for name in ['baseline']+[p['name'] for p in plan['candidate_recovery']]:
        rows=[r for p in sorted((RUNS/name).glob('*.csv')) for r in read_csv(p) if r['solver']=='svd+ransac']
        assert len(rows)==150 and len({r['label'] for r in rows})==150
        of=[r for r in rows if tuple(r['label'].split('_',1)[1].split('-')) in official]
        summary.append(dict(method=name,cases=150,success=sum(int(r['success']) for r in rows),
                            official_cases=len(of),official_success=sum(int(r['success']) for r in of),
                            recovery_triggered=sum(int(r.get('recovery_triggered',0)) for r in rows),
                            recovery_adopted=sum(int(r.get('recovery_accepted',0)) for r in rows)))
    write_csv(OUT/'discovery_summary.csv',summary)
    winner=max(summary,key=lambda r:r['success']) # stable; prefers baseline/earlier candidate on ties
    method=next((x for x in plan['candidate_recovery'] if x['name']==winner['method']),dict(name='baseline',mode='off',trigger=3))
    frozen=dict(method=method,discovery_cases=150,discovery_success=winner['success'],
                discovery_seeds=list(range(11,21)),validation_seeds=SEEDS,inference_sha256=hashes())
    path=OUT/'frozen_recovery.json'
    if path.exists():
        old=json.loads(path.read_text());assert {k:v for k,v in old.items() if k!='frozen_at_utc'}==frozen
    else:
        frozen['frozen_at_utc']=datetime.now(timezone.utc).isoformat();path.write_text(json.dumps(frozen,indent=2)+'\n')
    print(json.dumps(summary,indent=2));print('FROZEN',method,flush=True)


def load_frozen():
    f=json.loads((OUT/'frozen_recovery.json').read_text())
    assert f['inference_sha256']==hashes(),'Inference changed after freezing; do not reuse held-out validation.'
    return f


def execute(label,args):
    path=RUNS/'logs'/f'{label}.log';path.parent.mkdir(parents=True,exist_ok=True)
    cmd=([str(TOOL)] if TOOL else [])+list(map(str,args));start=time.monotonic()
    print('START',label,flush=True)
    with path.open('w') as log:r=subprocess.run(cmd,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
    rec=dict(label=label,command=cmd,exit_code=r.returncode,seconds=time.monotonic()-start)
    with LOCK:
        with (RUNS/'commands.jsonl').open('a') as log:log.write(json.dumps(rec)+'\n')
    print('DONE',label,r.returncode,round(rec['seconds'],2),flush=True)
    if r.returncode:raise RuntimeError(f'{label}: {path}')


def parallel(fn,jobs):
    with futures.ThreadPoolExecutor(max_workers=3) as p:return list(p.map(fn,jobs))


def extract():
    frozen=load_frozen()
    def one(scan):
        dest=RUNS/'validation_extraction'/scan;dest.mkdir(parents=True,exist_ok=True)
        args=[ROOT/'build/cm_extract_batch',f'--input={RAW/scan}.ply',f'--out={dest}',
              '--config=config/default.ini','--version=step','--experiment.seed_base=21','--experiment.n_seeds=10',
              '--cluster.tolerance=0.20','--scanner.x0=0','--scanner.y0=0','--scanner.z0=0']
        sig=hashlib.sha256((json.dumps(list(map(str,args)))+json.dumps(frozen['inference_sha256'],sort_keys=True)).encode()).hexdigest()
        marker=dest/'complete.json'
        outputs=[dest/f'step_seed{k}_{s}.csv' for k in SEEDS for s in ['trees','clusters','retention_bins']]
        if marker.exists() and json.loads(marker.read_text())['signature']==sig and all(p.exists() for p in outputs):return
        execute(f'extract_{scan}',args)
        assert all(p.exists() for p in outputs)
        for seed in SEEDS:
            assert all(math.isfinite(float(r[c])) for r in read_csv(dest/f'step_seed{seed}_trees.csv') for c in ['x','y','z'])
        marker.write_text(json.dumps(dict(signature=sig,outputs=len(outputs)))+'\n')
    parallel(one,SCANS)


def validate():
    frozen=load_frozen();rows=[]
    for method in ['baseline','recovery']:
        def one(scan):
            dest=RUNS/'validation_registration'/method;dest.mkdir(parents=True,exist_ok=True)
            jobs=[];paths=[]
            for seed,(a,b) in itertools.product(SEEDS,PAIRS):
                if a!=scan:continue
                output=dest/f'{seed}_{a}-{b}.csv';trees=RUNS/'validation_extraction'
                src=trees/a/f'step_seed{seed}_trees.csv';tgt=trees/b/f'step_seed{seed}_trees.csv'
                assert src.exists() and tgt.exists()
                jobs.append(f'{src} {tgt} {RAW/a}.ply {RAW}/groundtruth/{a}-{b}.tfm {output} {seed}_{a}-{b}')
                paths.append((seed,a,b,output))
            if not jobs:return []
            batch=dest/f'{scan}.txt';batch.write_text('\n'.join(jobs)+'\n')
            args=[ROOT/'build/cm_register',f'--batch={batch}','--thresholds=0.10']
            if method=='recovery':args += [f'--recovery={frozen["method"]["mode"]}',f'--recovery_trigger={frozen["method"]["trigger"]}']
            marker=dest/f'{scan}.complete.json';sig=hashlib.sha256(json.dumps(dict(command=list(map(str,args)),inference=frozen['inference_sha256'])).encode()).hexdigest()
            if not (marker.exists() and json.loads(marker.read_text())['signature']==sig and all(p.exists() for _,_,_,p in paths)):
                execute(f'reg_{method}_{scan}',args)
                marker.write_text(json.dumps(dict(signature=sig,cases=len(paths)))+'\n')
            result=[]
            for seed,a,b,path in paths:
                r=read_csv(path);assert len(r)==2
                result.extend(dict(method=method,seed=seed,pair=f'{a}-{b}',**x) for x in r)
            return result
        rows.extend(r for rr in parallel(one,SCANS) for r in rr)
    # Normalize optional recovery columns so the baseline remains byte/schema compatible.
    fields=list(dict.fromkeys(k for r in rows for k in r))
    write_csv(RUNS/'validation_rows.csv',[{k:r.get(k,'') for k in fields} for r in rows])
    print('VALIDATED',len(rows),'rows',flush=True)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('phase',choices=['discover','freeze','extract','validate']);a=p.parse_args()
    RUNS.mkdir(parents=True,exist_ok=True);globals()[a.phase]()


if __name__=='__main__':main()
