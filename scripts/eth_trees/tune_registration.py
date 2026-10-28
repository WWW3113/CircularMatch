#!/usr/bin/env python3
"""Prespecified ETH tuning study. Raw runs are ignored; compact reports use Git.

Run phases: screen, extract, fresh, graph, report. Existing extraction/registration
defaults are unchanged. Discovery uses seeds 1-5; fresh fitting seeds are 11-20.
Both sets still use the same six ETH scans, not an independent forest dataset.
"""
import argparse
import concurrent.futures as futures
import csv
import hashlib
import itertools
import json
import math
import os
from pathlib import Path
import statistics as stats
import subprocess
import threading
import time

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'results/eth_trees/tuning_20261025'
RUNS = OUT / 'runs'
RAW = ROOT / 'data/eth_trees/raw/trees'
_cloud_wrapper = Path('/workspace/.circularmatch-env/run')
_wrapper = os.environ.get('CM_TOOLCHAIN_WRAPPER', str(_cloud_wrapper) if _cloud_wrapper.exists() else '')
TOOL = Path(_wrapper) if _wrapper else None
OLD = ROOT / 'results/eth_trees/dtm_slope/seeds/ctol20/runs'
SCANS = [f's{i}' for i in range(1, 7)]
PAIRS = list(itertools.combinations(SCANS, 2))
PROFILES = [
    dict(name='control', threshold=.05),
    dict(name='threshold075', threshold=.075),
    dict(name='threshold100', threshold=.10),
    dict(name='score20', threshold=.05, th_score=20),
    dict(name='score60', threshold=.05, th_score=60),
    dict(name='min_dc4', threshold=.05, min_dc=4),
    dict(name='sectors93', threshold=.05, N=93),
    dict(name='merge015', threshold=.05, merge_radius=.15),
    dict(name='merge030', threshold=.05, merge_radius=.30),
]
EXTRACTION = {
    'control': [],
    'vertical085': ['--normals.vert_threshold=0.85'],
    'ransac999': ['--cylinder.max_iterations=3000', '--cylinder.probability=0.999'],
}
LOCK = threading.Lock()


def read_csv(path):
    with Path(path).open() as f:
        return list(csv.DictReader(f))


def write_csv(path, rows):
    with Path(path).open('w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)


def execute(label, args):
    log = RUNS / 'logs' / f'{label}.log'; log.parent.mkdir(parents=True, exist_ok=True)
    command = ([str(TOOL)] if TOOL else []) + list(map(str, args))
    start = time.monotonic(); print('START', label, flush=True)
    with log.open('w') as f:
        result = subprocess.run(command, cwd=ROOT, stdout=f, stderr=subprocess.STDOUT)
    record = dict(label=label, command=command, exit_code=result.returncode,
                  seconds=round(time.monotonic()-start, 3))
    with LOCK:
        with (RUNS / 'commands.jsonl').open('a') as f: f.write(json.dumps(record)+'\n')
    print('DONE', label, record['exit_code'], record['seconds'], flush=True)
    if result.returncode: raise RuntimeError(f'{label} failed: {log}')


def parallel(fn, jobs):
    with futures.ThreadPoolExecutor(max_workers=3) as pool: return list(pool.map(fn, jobs))


def tree_path(kind, scan, seed):
    if kind == 'cached': return OLD / scan / 'improved' / f'step_raw_seed{seed}_trees.csv'
    return RUNS / 'extraction' / kind / scan / f'step_seed{seed}_trees.csv'


def register(tag, profile, kind, seeds):
    directory = RUNS / 'registration' / tag; directory.mkdir(parents=True, exist_ok=True)
    def one(scan):
        jobs=[]; inputs=set()
        for seed, (a,b) in itertools.product(seeds, PAIRS):
            if a != scan: continue
            source, target = tree_path(kind,a,seed), tree_path(kind,b,seed)
            inputs.update([source,target]); output=directory/f'{seed}_{a}-{b}.csv'
            jobs.append((seed,a,b,output, f'{source} {target} {RAW/a}.ply {RAW}/groundtruth/{a}-{b}.tfm {output} {tag}_{seed}_{a}-{b}'))
        if not jobs:return []
        batch=directory/f'{scan}.txt'; batch.write_text('\n'.join(j[-1] for j in jobs)+'\n')
        args=[ROOT/'build/cm_register', f'--batch={batch}', f'--thresholds={profile["threshold"]}']
        args += [f'--{k}={v}' for k,v in profile.items() if k not in ('name','threshold')]
        h=hashlib.sha256(json.dumps(list(map(str,args))).encode())
        for path in sorted(inputs): h.update(str(path).encode()); h.update(path.read_bytes())
        signature=h.hexdigest(); marker=directory/f'{scan}.complete.json'
        valid=marker.exists() and json.loads(marker.read_text()).get('signature')==signature
        if valid:
            try:valid=all(len(read_csv(j[3]))==2 for j in jobs)
            except (OSError,KeyError,csv.Error):valid=False
        if not valid:
            execute(f'reg_{tag}_{scan}', args)
            for j in jobs:
                rr=read_csv(j[3]); assert len(rr)==2 and {r['solver'] for r in rr}=={'svd','svd+ransac'}
            marker.write_text(json.dumps(dict(signature=signature, jobs=len(jobs))))
        rows=[]
        for seed,a,b,path,_ in jobs:
            for row in read_csv(path): rows.append(dict(method=tag,seed=seed,pair=f'{a}-{b}',**row))
        return rows
    return [r for group in parallel(one, SCANS) for r in group]


def screen():
    rows=[]
    for profile in PROFILES: rows += register(profile['name'], profile, 'cached', range(1,11))
    write_csv(RUNS/'screen_rows.csv', rows)
    summary=[]
    for p, solver in itertools.product(PROFILES, ['svd','svd+ransac']):
        r=[r for r in rows if r['method']==p['name'] and r['solver']==solver]
        train=sum(int(x['success']) for x in r if x['seed']<=5)
        check=sum(int(x['success']) for x in r if x['seed']>5)
        summary.append(dict(method=p['name'],solver=solver,discovery_success=train,discovery_trials=75,
                            existing_seed_check_success=check,existing_seed_check_trials=75))
    write_csv(OUT/'screen_summary.csv',summary)
    # Freeze on discovery seeds ONLY; prefer earlier/simpler candidates on ties.
    winner=max(summary,key=lambda r:r['discovery_success'])
    frozen=dict(profile=next(p for p in PROFILES if p['name']==winner['method']),solver=winner['solver'],
                discovery_success=winner['discovery_success'],discovery_seeds=list(range(1,6)),
                fresh_seeds=list(range(11,21)))
    path=OUT/'frozen_matching.json'
    if path.exists(): assert json.loads(path.read_text())==frozen, 'Frozen selection changed; investigate.'
    else:path.write_text(json.dumps(frozen,indent=2)+'\n')
    print('FROZEN',frozen,flush=True)


def extract():
    def one(job):
        variant,scan=job;out=RUNS/'extraction'/variant/scan;out.mkdir(parents=True,exist_ok=True)
        marker=out/'complete.json'
        args=[ROOT/'build/cm_extract_batch',f'--input={RAW/scan}.ply',f'--out={out}',
              '--config=config/default.ini','--version=step','--experiment.seed_base=11','--experiment.n_seeds=10',
              '--cluster.tolerance=0.20','--scanner.x0=0','--scanner.y0=0','--scanner.z0=0',*EXTRACTION[variant]]
        signature=hashlib.sha256(json.dumps(list(map(str,args))).encode()).hexdigest()
        outputs=[out/f'step_seed{k}_{suffix}.csv' for k in range(11,21) for suffix in ['trees','clusters','retention_bins']]
        if marker.exists() and json.loads(marker.read_text()).get('signature')==signature and all(p.exists() for p in outputs): return
        execute(f'extract_{variant}_{scan}',args)
        for k in range(11,21):
            rr=read_csv(out/f'step_seed{k}_trees.csv')
            assert all(math.isfinite(float(r[c])) for r in rr for c in ['x','y','z','radius','tilt_deg'])
        marker.write_text(json.dumps(dict(signature=signature, outputs=len(outputs))))
    parallel(one,itertools.product(EXTRACTION,SCANS))


def fresh():
    frozen=json.loads((OUT/'frozen_matching.json').read_text());rows=[]
    rows+=register('fresh_control',PROFILES[0],'control',range(11,21))
    rows+=register('fresh_matching',frozen['profile'],'control',range(11,21))
    for kind in ['vertical085','ransac999']:
        rows+=register('fresh_'+kind,PROFILES[0],kind,range(11,21))
        rows+=register('fresh_'+kind+'_matching',frozen['profile'],kind,range(11,21))
    write_csv(RUNS/'fresh_rows.csv',rows)


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('phase',choices=['screen','extract','fresh','graph','report']);args=ap.parse_args()
    RUNS.mkdir(parents=True,exist_ok=True)
    if args.phase in ['screen','extract','fresh']:globals()[args.phase]()
    else:
        from tuning_graph import run_graph, report
        (run_graph if args.phase=='graph' else report)(ROOT,OUT)


if __name__=='__main__':main()
