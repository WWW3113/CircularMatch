#!/usr/bin/env python3
"""Frozen ETH extraction settings on public TreeTool/Liang processed samples.

This evaluates reference-tree coverage, not scan registration or precision:
the mirror supplies neither individual station poses nor annotation boundaries.
"""
import concurrent.futures as futures
import hashlib
import itertools
import json
from pathlib import Path
import subprocess
import time

import numpy as np
from scipy.optimize import linear_sum_assignment
from tune_registration import EXTRACTION, ROOT, TOOL, read_csv, write_csv

OUT = ROOT / 'results/liang_trees/transfer_20261025'
RUNS = OUT / 'runs'
DATA = ROOT / 'data/liang_treetool/TreeToolSamples'


def execute(label, args):
    path = RUNS / 'logs' / (label + '.log'); path.parent.mkdir(parents=True, exist_ok=True)
    command = ([str(TOOL)] if TOOL else []) + list(map(str, args))
    start = time.monotonic(); print('START', label, flush=True)
    with path.open('w') as f:
        r = subprocess.run(command, cwd=ROOT, stdout=f, stderr=subprocess.STDOUT)
    if r.returncode: raise RuntimeError(f'{label} exit {r.returncode}; {path}')
    return dict(label=label, command=command, seconds=round(time.monotonic()-start, 3))


def main():
    OUT.mkdir(parents=True, exist_ok=True); RUNS.mkdir(exist_ok=True)
    provenance = json.loads((DATA/'DOWNLOAD.json').read_text())
    plan = dict(dataset='Liang2018 / TreeTool processed samples', plots=list(range(1, 7)),
                seeds=list(range(11, 21)), variants=EXTRACTION, cluster_tolerance_m=.20,
                retention='step', scanner_origin=[0, 0, 0], match_radius_m=.5,
                reference_columns=['id', 'x', 'y', 'height', 'diameter'],
                matching='maximum-cardinality one-to-one XY assignment; minimize total distance within gate',
                primary='matched_reference_trees / all_provided_reference_trees',
                limitations=['Processed/downsampled clouds; not original station scans',
                             'PCD VIEWPOINT is zero but physical sensor origin is not independently verified',
                             'Unknown annotation boundary; unmatched detections are not confirmed false positives',
                             'Reference XY anchor height is not documented; no DBH/height accuracy claim',
                             'No registration ground truth; no registration success rate'],
                input_sha256={f['name']: f['computed_sha256'] for f in provenance['files']})
    plan_path = OUT/'plan.json'
    if plan_path.exists(): assert json.loads(plan_path.read_text()) == plan, 'Frozen plan changed'
    else: plan_path.write_text(json.dumps(plan, indent=2)+'\n')
    # Verify point count, binary payload, finite coordinates and documented frame
    # before any extraction or comparison to reference locations.
    inspections=[]
    for plot in plan['plots']:
        path=DATA/f'downsampledlesscloudEURO{plot}.pcd'
        with path.open('rb') as f:
            for _ in range(200):
                line=f.readline()
                if not line: raise ValueError('Incomplete PCD header')
                if line.strip()==b'DATA binary': break
            else: raise ValueError('Unsupported PCD header')
            offset=f.tell()
        a=np.memmap(path,dtype='<f4',mode='r',offset=offset).reshape(-1,3)
        assert np.isfinite(a).all()
        inspections.append(dict(plot=plot, points=len(a), bbox_min=a.min(axis=0).tolist(), bbox_max=a.max(axis=0).tolist()))
        log=RUNS/'logs'/f'inspect_{plot}.log'
        if not log.exists(): execute(f'inspect_{plot}', [ROOT/'build/cm_inspect', f'--input={path}', '--stride=100'])
    (OUT/'inspection.json').write_text(json.dumps(inspections,indent=2)+'\n')
    def one(job):
        kind,plot=job; dest=RUNS/'extraction'/kind/f'plot{plot}'; dest.mkdir(parents=True,exist_ok=True)
        args=[ROOT/'build/cm_extract_batch', f'--input={DATA}/downsampledlesscloudEURO{plot}.pcd', f'--out={dest}',
              '--config=config/default.ini', '--version=step', '--experiment.seed_base=11', '--experiment.n_seeds=10',
              '--cluster.tolerance=0.20', '--scanner.x0=0', '--scanner.y0=0', '--scanner.z0=0', *EXTRACTION[kind]]
        signature=hashlib.sha256((json.dumps(list(map(str,args)))+json.dumps(plan['input_sha256'],sort_keys=True)).encode()).hexdigest()
        marker=dest/'complete.json'
        outputs=[dest/f'step_seed{s}_{suffix}.csv' for s in plan['seeds'] for suffix in ['trees','clusters','retention_bins']]
        if marker.exists() and json.loads(marker.read_text()).get('signature')==signature and all(p.exists() for p in outputs):return
        rec=execute(f'extract_{kind}_plot{plot}',args)
        assert all(p.exists() for p in outputs)
        marker.write_text(json.dumps(dict(signature=signature,outputs=len(outputs),**rec),indent=2)+'\n')
    with futures.ThreadPoolExecutor(max_workers=2) as pool:
        list(pool.map(one,itertools.product(EXTRACTION,plan['plots'])))
    # Reference files are first used for scoring after all extraction has ended.
    rows=[]
    for kind,plot,seed in itertools.product(EXTRACTION,plan['plots'],plan['seeds']):
        ref=np.loadtxt(DATA/f'TLS_Benchmarking_Plot_{plot}_LHD.txt',ndmin=2)
        detected=read_csv(RUNS/'extraction'/kind/f'plot{plot}'/f'step_seed{seed}_trees.csv')
        xy=np.array([[float(r['x']),float(r['y'])] for r in detected]).reshape(-1,2)
        assert ref.shape[1]==5 and np.isfinite(ref).all() and np.isfinite(xy).all()
        assert len(np.unique(ref[:,0]))==len(ref)
        distance=np.linalg.norm(ref[:,None,1:3]-xy[None,:,:],axis=2)
        i,j=linear_sum_assignment(np.where(distance<=plan['match_radius_m'],distance,1e6))
        valid=distance[i,j]<=plan['match_radius_m']; errors=distance[i[valid],j[valid]]
        rows.append(dict(dataset='Liang2018-TreeTool',plot=plot,variant=kind,seed=seed,
                         n_reference=len(ref),n_detected=len(xy),n_matched=len(errors),reference_match_rate=len(errors)/len(ref),
                         matched_xy_rmse_m=float(np.sqrt(np.mean(errors**2))) if len(errors) else '',
                         matched_xy_median_m=float(np.median(errors)) if len(errors) else ''))
    write_csv(OUT/'extraction_rows.csv',rows)
    summary=[]
    for kind,plot in itertools.product(EXTRACTION,plan['plots']):
        r=[x for x in rows if x['variant']==kind and x['plot']==plot]
        summary.append(dict(variant=kind,plot=plot,n_seeds=len(r),n_reference=r[0]['n_reference'],
                            mean_detected=float(np.mean([x['n_detected'] for x in r])),
                            mean_matched=float(np.mean([x['n_matched'] for x in r])),
                            mean_reference_match_rate=float(np.mean([x['reference_match_rate'] for x in r])),
                            sd_reference_match_rate=float(np.std([x['reference_match_rate'] for x in r],ddof=1))))
    write_csv(OUT/'summary.csv',summary)
    print(json.dumps(summary,indent=2),flush=True)


if __name__=='__main__':main()
