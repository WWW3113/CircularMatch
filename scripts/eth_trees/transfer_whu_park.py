#!/usr/bin/env python3
"""Frozen ETH settings on the public YOHO WHU-Park derivative (not FGI-Forest)."""
import argparse
import concurrent.futures as futures
import hashlib
import itertools
import json
import math
from pathlib import Path
import re
import subprocess
import threading
import time

import numpy as np
from scipy.spatial import cKDTree
from tune_registration import ROOT, TOOL, read_csv, write_csv

OUT=ROOT/'results/whu_park/transfer_20261025'
RUNS=OUT/'runs'
DATA=ROOT/'data/whu_tls_yoho/extracted/WHU-TLS/Park/PointCloud'
SEEDS=list(range(21,31))
LOCK=threading.Lock()


def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def pairs():
    lines=[x for x in (DATA/'gt.log').read_text().splitlines() if x.strip()]
    assert len(lines)%5==0
    result=[]
    for k in range(0,len(lines),5):
        target,source,total=map(int,lines[k].split());assert total==32 and source!=target
        gt=np.array([[float(x) for x in row.split()] for row in lines[k+1:k+5]])
        assert gt.shape==(4,4) and np.allclose(gt[3],[0,0,0,1])
        assert np.allclose(gt[:3,:3].T@gt[:3,:3],np.eye(3),atol=5e-6)
        result.append((source,target,gt))
    assert len(result)==31 and len({(a,b) for a,b,_ in result})==31
    return result


def execute(label,args):
    path=RUNS/'logs'/f'{label}.log';path.parent.mkdir(parents=True,exist_ok=True)
    cmd=([str(TOOL)] if TOOL else [])+list(map(str,args));start=time.monotonic()
    print('START',label,flush=True)
    with path.open('w') as log:r=subprocess.run(cmd,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
    record=dict(label=label,command=cmd,exit_code=r.returncode,seconds=time.monotonic()-start)
    with LOCK:
        with (RUNS/'commands.jsonl').open('a') as f:f.write(json.dumps(record)+'\n')
    if r.returncode:raise RuntimeError(f'{label}: {path}')
    print('DONE',label,round(record['seconds'],2),flush=True)


def parallel(fn,jobs):
    with futures.ThreadPoolExecutor(max_workers=3) as pool:return list(pool.map(fn,jobs))


def read_park_points(index):
    path=DATA/f'cloud_bin_{index}.ply'
    with path.open('rb') as f:
        header=[]
        for count in range(1,201):
            line=f.readline();header.append(line)
            if line.strip()==b'end_header':break
        else:raise ValueError('incomplete PLY header')
        offset=f.tell()
    n=int(next(s for s in header if s.startswith(b'element vertex')).split()[-1])
    if b'format binary_little_endian 1.0\n' in header:
        props=[s.split()[1:] for s in header if s.startswith(b'property')]
        assert props==[[b'float',b'x'],[b'float',b'y'],[b'float',b'z']]
        xyz=np.fromfile(path,dtype='<f4',offset=offset,count=n*3).reshape(-1,3).astype(np.float64)
    else:
        assert b'format ascii 1.0\n' in header
        xyz=np.loadtxt(path,skiprows=count,dtype=np.float64,encoding='latin1',usecols=(0,1,2))
    assert xyz.shape==(n,3) and np.isfinite(xyz).all()
    return xyz


def prepare():
    OUT.mkdir(parents=True,exist_ok=True);RUNS.mkdir(exist_ok=True)
    pp=pairs();scans=sorted({i for a,b,_ in pp for i in [a,b]})
    assert scans==list(range(32))
    frozen=json.loads((ROOT/'results/eth_trees/failure_followup_20261025/frozen_recovery.json').read_text())
    for path,expected in frozen['inference_sha256'].items():assert digest(ROOT/path)==expected
    plan=dict(dataset='WHU-TLS / Park (YOHO public derivative)',not_fgi_forest=True,
              source='https://github.com/HpWang-whu/YOHO',download='https://drive.google.com/file/d/1QjlxIVMQPinNWt5LKhtaG9TTo2j3TGs_/view',
              archive_sha256='33a7a29577faa631c2a3bec828f9552182dbcbfd0da67ce572c8c28df60caf0a',
              pair_convention='YOHO utils/dataset.py: header target(id0),source(id1),N; matrix maps source to target',
              provided_pairs=[[a,b] for a,b,_ in pp],scans=scans,seeds=SEEDS,
              extraction=dict(config='config/default.ini',cluster_tolerance=.20,retention='step',verticality=.90,scanner_origin=[0,0,0]),
              registration=dict(threshold=.10,primary_solver='svd+ransac',recovery_mode='evidence',recovery_trigger=6),
              success='rotation<2deg AND translation<0.5m; full source-cloud e_p',
              no_new_dataset_tuning=True,physical_scanner_origin_independently_verified=False,
              limitations=['Public processed point clouds; source does not provide original scanner-center metadata',
                           'Park is a mixed park environment, not the FGI-Forest used in the paper',
                           'Keep fixed ETH settings even if density or extraction performance is poor'],
              inference_sha256=frozen['inference_sha256'],input_sha256={f.name:digest(f) for f in sorted(DATA.glob('*.ply'))},gt_sha256=digest(DATA/'gt.log'))
    dest=OUT/'plan.json'
    if dest.exists():assert json.loads(dest.read_text())==plan
    else:dest.write_text(json.dumps(plan,indent=2)+'\n')
    gt_dir=RUNS/'groundtruth';gt_dir.mkdir(exist_ok=True)
    for a,b,gt in pp:np.savetxt(gt_dir/f'{a}-{b}.tfm',gt,fmt='%.10g')
    def one(scan):
        path=RUNS/'logs'/f'inspect_{scan}.log'
        if not path.exists():execute(f'inspect_{scan}',[ROOT/'build/cm_inspect',f'--input={DATA}/cloud_bin_{scan}.ply','--stride=25'])
        text=path.read_text();n,skipped=map(int,re.search(r'points read: (\d+), skipped \(non-finite/unparsable\): (\d+)',text).groups())
        assert skipped==0 and 'matches read+skipped' in text
        return dict(scan=scan,points=n,skipped=skipped)
    inspection=parallel(one,scans);write_csv(OUT/'inspection.csv',inspection)
    # Direction is fixed by publisher code. This overlap audit does not select
    # a registration result or filter the benchmark pair list.
    sanity=[]
    for a,b,gt in pp[:3]:
        s=read_park_points(a);t=read_park_points(b);s=s[::max(1,len(s)//5000)]
        tree=cKDTree(t);mapped=s@gt[:3,:3].T+gt[:3,3]
        forward=tree.query(mapped,workers=1)[0]
        inverse=np.linalg.inv(gt);wrong=tree.query(s@inverse[:3,:3].T+inverse[:3,3],workers=1)[0]
        sanity.append(dict(source=a,target=b,provided_direction_share_within_030=float(np.mean(forward<.30)),
                           reversed_direction_share_within_030=float(np.mean(wrong<.30))))
    assert all(r['provided_direction_share_within_030']>r['reversed_direction_share_within_030'] for r in sanity)
    (OUT/'gt_direction_audit.json').write_text(json.dumps(sanity,indent=2)+'\n')
    print('PREPARED',len(scans),'scans',len(pp),'provided pairs',flush=True)


def load_plan():
    plan=json.loads((OUT/'plan.json').read_text())
    for path,expected in plan['inference_sha256'].items():assert digest(ROOT/path)==expected
    return plan


def density():
    plan=load_plan()
    def one(scan):
        xyz=read_park_points(scan);sample=xyz[::max(1,len(xyz)//5000)]
        distances=cKDTree(xyz).query(sample,k=5,workers=1)[0]
        return dict(scan=scan,points=len(xyz),sampled=len(sample),
                    median_nearest_m=float(np.median(distances[:,1])),
                    median_fifth_m=float(np.median(distances[:,4])),
                    fifth_p90_m=float(np.quantile(distances[:,4],.90)),
                    share_with_five_neighbours_within_010=float(np.mean(distances[:,4]<=.10)))
    rows=parallel(one,plan['scans']);write_csv(OUT/'density_audit.csv',rows)
    spacing=float(np.median([r['median_nearest_m'] for r in rows]));fifth=float(np.median([r['median_fifth_m'] for r in rows]))
    up=lambda x:round(math.ceil(x/.05)*.05,10)
    adaptation=dict(rule='normal radius=max(0.10,ceil_to_0.05(1.1*median fifth-neighbour distance)); cluster tolerance=max(0.20,ceil_to_0.05(1.5*median nearest-neighbour distance)); equal scan weights',
                    selected_from='input point spacing only; no GT poses, registration scores or tree labels',
                    median_nearest_m=spacing,median_fifth_m=fifth,
                    normal_radius_m=max(.10,up(1.1*fifth)),cluster_tolerance_m=max(.20,up(1.5*spacing)),
                    phase='exploratory density adaptation after unchanged ETH settings failed; not unchanged parameter transfer',
                    remaining_parameters='same frozen ETH settings; no further parameter search',
                    input_sha256=plan['input_sha256'])
    path=OUT/'density_adaptation.json'
    if path.exists():assert json.loads(path.read_text())==adaptation
    else:path.write_text(json.dumps(adaptation,indent=2)+'\n')
    print(json.dumps(adaptation,indent=2),flush=True)


def extract():
    plan=load_plan()
    adaptation=json.loads((OUT/'density_adaptation.json').read_text()) if RUNS.name=='adapted' else None
    def one(scan):
        dest=RUNS/'extraction'/str(scan);dest.mkdir(parents=True,exist_ok=True)
        marker=dest/'complete.json';outputs=[dest/f'step_seed{s}_{tail}.csv' for s in SEEDS for tail in ['trees','clusters','retention_bins']]
        if marker.exists() and all(p.exists() for p in outputs):return
        cluster=adaptation['cluster_tolerance_m'] if adaptation else .20
        args=[ROOT/'build/cm_extract_batch',f'--input={DATA}/cloud_bin_{scan}.ply',f'--out={dest}',
              '--config=config/default.ini','--version=step','--experiment.seed_base=21','--experiment.n_seeds=10',
              f'--cluster.tolerance={cluster}','--scanner.x0=0','--scanner.y0=0','--scanner.z0=0']
        if adaptation:args.append(f'--normals.radius={adaptation["normal_radius_m"]}')
        execute(f'extract_{scan}',args);assert all(p.exists() for p in outputs)
        marker.write_text(json.dumps(dict(outputs=30,input_sha256=plan['input_sha256'][f'cloud_bin_{scan}.ply'])))
    parallel(one,plan['scans'])


def register():
    plan=load_plan();allrows=[];pp=pairs()
    for method in ['baseline','recovery']:
        def one(source):
            dest=RUNS/'registration'/method;dest.mkdir(parents=True,exist_ok=True);jobs=[];outputs=[]
            for a,b,_ in pp:
                if a!=source:continue
                for seed in SEEDS:
                    src=RUNS/'extraction'/str(a)/f'step_seed{seed}_trees.csv';tgt=RUNS/'extraction'/str(b)/f'step_seed{seed}_trees.csv'
                    assert src.exists() and tgt.exists()
                    output=dest/f'{seed}_{a}-{b}.csv'
                    jobs.append(f'{src} {tgt} {DATA}/cloud_bin_{a}.ply {OUT}/runs/groundtruth/{a}-{b}.tfm {output} {seed}_{a}-{b}')
                    outputs.append((seed,a,b,output))
            if not jobs:return []
            batch=dest/f'{source}.txt';batch.write_text('\n'.join(jobs)+'\n')
            args=[ROOT/'build/cm_register',f'--batch={batch}','--thresholds=0.10']
            if method=='recovery':args += ['--recovery=evidence','--recovery_trigger=6']
            marker=dest/f'{source}.complete.json'
            if not marker.exists() or not all(p.exists() for _,_,_,p in outputs):
                execute(f'reg_{method}_{source}',args);marker.write_text(json.dumps(dict(pairs=len(outputs))))
            rows=[]
            for seed,a,b,path in outputs:
                rr=read_csv(path);assert len(rr)==2
                rows.extend(dict(dataset='WHU-Park-YOHO',method=method,seed=seed,pair=f'{a}-{b}',**r) for r in rr)
            return rows
        allrows.extend(r for rr in parallel(one,sorted({a for a,_,_ in pp})) for r in rr)
    headers=list(dict.fromkeys(k for r in allrows for k in r));normalized=[{k:r.get(k,'') for k in headers} for r in allrows]
    write_csv(RUNS/'registration_rows.csv',normalized)
    print('REGISTERED',len(normalized),'rows',flush=True)


def main():
    global RUNS
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('phase',choices=['prepare','density','extract','register']);p.add_argument('--adapted',action='store_true');a=p.parse_args()
    if a.adapted:
        assert a.phase in ['extract','register'];RUNS=OUT/'runs/adapted';RUNS.mkdir(parents=True,exist_ok=True)
    globals()[a.phase]()


if __name__=='__main__':main()
