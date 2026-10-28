#!/usr/bin/env python3
"""Station-disjoint Park optimization; keep original dense ETH inference unchanged."""
import argparse
import concurrent.futures as futures
from datetime import datetime,timezone
import hashlib
import itertools
import json
from pathlib import Path
import subprocess
import threading
import time

from tune_registration import ROOT,TOOL,read_csv,write_csv
from transfer_whu_park import DATA,pairs

OUT=ROOT/'results/whu_park/sparse_stems_20261025';RUNS=OUT/'runs';LOCK=threading.Lock()
PROFILES=[
 dict(name='baseline',arc='occupied',args=[]),
 dict(name='normal_soft',arc='occupied',args=['--sparse_normal_weight=0.02']),
 dict(name='geometry_soft',arc='occupied',args=['--sparse_normal_weight=0.02','--sparse_dist_threshold=0.05']),
 dict(name='arc60',arc='occupied',args=['--sparse_min_arc=60']),
 dict(name='loose_support',arc='occupied',args=['--sparse_normal_weight=0.02','--sparse_dist_threshold=0.05','--sparse_min_arc=60','--sparse_min_normal_ratio=0.50']),
 dict(name='trimmed90',arc='auto',args=[]),
 dict(name='trimmed90_soft',arc='auto',args=['--sparse_normal_weight=0.02']),
]
HASH_PATHS=['apps/cm_sparse_extract.cpp','include/cm/sparse_arc.hpp','build/cm_sparse_extract',
            'build/cm_register','build/cm_extract_batch','config/default.ini']


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def hashes():return {p:sha(ROOT/p) for p in HASH_PATHS}


def execute(label,args):
    log=RUNS/'logs'/f'{label}.log';log.parent.mkdir(parents=True,exist_ok=True)
    cmd=([str(TOOL)] if TOOL else [])+list(map(str,args));start=time.monotonic();print('START',label,flush=True)
    with log.open('w') as f:r=subprocess.run(cmd,cwd=ROOT,stdout=f,stderr=subprocess.STDOUT)
    rec=dict(label=label,command=cmd,exit_code=r.returncode,seconds=time.monotonic()-start)
    with LOCK:
        with (RUNS/'commands.jsonl').open('a') as f:f.write(json.dumps(rec)+'\n')
    if r.returncode:raise RuntimeError(f'{label}: {log}')
    print('DONE',label,round(rec['seconds'],2),flush=True)


def parallel(fn,jobs):
    with futures.ThreadPoolExecutor(max_workers=3) as pool:return list(pool.map(fn,jobs))


def extraction(profile,phase,scans,seeds,eth=False):
    def one(scan):
        dest=RUNS/phase/'extraction'/profile['name']/str(scan);dest.mkdir(parents=True,exist_ok=True)
        cloud=(ROOT/f'data/eth_trees/raw/trees/s{scan}.ply') if eth else DATA/f'cloud_bin_{scan}.ply'
        args=[ROOT/'build/cm_sparse_extract',f'--input={cloud}',f'--out={dest}',f'--arc_mode={profile["arc"]}',
              '--config=config/default.ini',f'--normals.radius={.10 if eth else .20}','--cluster.tolerance=0.20',
              f'--experiment.seed_base={min(seeds)}',f'--experiment.n_seeds={len(seeds)}',
              '--scanner.x0=0','--scanner.y0=0','--scanner.z0=0',*profile['args']]
        sig=sha(ROOT/'build/cm_sparse_extract')+json.dumps(list(map(str,args)))
        marker=dest/'complete.json';expected=[dest/f'step_seed{s}_{t}.csv' for s in seeds for t in ['trees','clusters','retention_bins']]
        if marker.exists() and json.loads(marker.read_text())['signature']==sig and all(p.exists() for p in expected):return
        execute(f'{phase}_extract_{profile["name"]}_{scan}',args)
        assert all(p.exists() for p in expected)
        marker.write_text(json.dumps(dict(signature=sig,outputs=len(expected)))+'\n')
    parallel(one,scans)


def registration(profile,phase,pp,seeds,full,eth=False):
    dest=RUNS/phase/'registration'/profile['name'];dest.mkdir(parents=True,exist_ok=True)
    def one(source):
        jobs=[];outputs=[];treehash={}
        for a,b in pp:
            if a!=source:continue
            for seed in seeds:
                prefix=RUNS/phase/'extraction'/profile['name']
                src=prefix/str(a)/f'step_seed{seed}_trees.csv';tgt=prefix/str(b)/f'step_seed{seed}_trees.csv'
                treehash[str(src)]=sha(src);treehash[str(tgt)]=sha(tgt)
                cloud=ROOT/f'data/eth_trees/raw/trees/s{a}.ply' if eth else DATA/f'cloud_bin_{a}.ply'
                gt=ROOT/f'data/eth_trees/raw/trees/groundtruth/s{a}-s{b}.tfm' if eth else ROOT/f'results/whu_park/transfer_20261025/runs/groundtruth/{a}-{b}.tfm'
                output=dest/f'{seed}_{a}-{b}.csv';jobs.append(f'{src} {tgt} {cloud} {gt} {output} {seed}_{a}-{b}');outputs.append((seed,a,b,output))
        batch=dest/f'{source}.txt';batch.write_text('\n'.join(jobs)+'\n')
        args=[ROOT/'build/cm_register',f'--batch={batch}','--thresholds=0.10','--recovery=evidence','--recovery_trigger=6']
        if not full:args.append('--skip_point_error=1')
        sig=json.dumps(dict(args=list(map(str,args)),inputs=treehash,tool=sha(ROOT/'build/cm_register')),sort_keys=True)
        marker=dest/f'{source}.complete.json'
        if not (marker.exists() and json.loads(marker.read_text())['signature']==sig and all(p.exists() for _,_,_,p in outputs)):
            execute(f'{phase}_reg_{profile["name"]}_{source}',args);marker.write_text(json.dumps(dict(signature=sig))+'\n')
        rows=[]
        for seed,a,b,path in outputs:
            rr=read_csv(path);assert len(rr)==2
            rows.extend(dict(profile=profile['name'],phase=phase,seed=seed,pair=f'{a}-{b}',**r) for r in rr)
        return rows
    return [r for rr in parallel(one,sorted({a for a,b in pp})) for r in rr]


def screen():
    plan=json.loads((OUT/'plan.json').read_text());plan['profiles']=PROFILES
    plan['selection']='maximum svd+ransac success on development15pairs x seeds21-25; ties prefer earlier simpler profile; never select by number of detected trees'
    plan['density_gate']='sparse fitting/arc changes only when median nearest distance in height-filtered voxel cloud >0.05m; dense inputs retain original fitting checks'
    plan['eth_regression']='Use existing ETH seeds21-30 to check all three output CSVs byte-for-byte and rerun registration; input normal radius0.10m (Park0.20m from previous density adaptation), cluster0.20m shared'
    (OUT/'plan.json').write_text(json.dumps(plan,indent=2)+'\n')
    allrows=[];summary=[]
    for profile in PROFILES:
        extraction(profile,'development',plan['development_scans'],plan['development_seeds'])
        rr=registration(profile,'development',plan['development_pairs'],plan['development_seeds'],False);allrows+=rr
        primary=[r for r in rr if r['solver']=='svd+ransac'];assert len(primary)==75
        summary.append(dict(profile=profile['name'],cases=len(primary),success=sum(int(r['success']) for r in primary),
                            transforms_found=sum(int(r['transform_ok']) for r in primary),mean_matches=sum(int(r['n_matches']) for r in primary)/len(primary)))
    write_csv(RUNS/'development_rows.csv',allrows);write_csv(OUT/'development_summary.csv',summary)
    winner=max(summary,key=lambda r:r['success']);profile=next(p for p in PROFILES if p['name']==winner['profile'])
    frozen=dict(profile=profile,development_summary=summary,inference_sha256=hashes(),validation_scans=plan['validation_scans'],
                validation_pairs=plan['validation_pairs'],validation_seeds=plan['validation_seeds'])
    path=OUT/'frozen.json'
    if path.exists():assert {k:v for k,v in json.loads(path.read_text()).items() if k!='frozen_at_utc'}==frozen
    else:frozen['frozen_at_utc']=datetime.now(timezone.utc).isoformat();path.write_text(json.dumps(frozen,indent=2)+'\n')
    print('FROZEN',profile,flush=True)


def frozen():
    f=json.loads((OUT/'frozen.json').read_text());assert f['inference_sha256']==hashes();return f


def validate():
    f=frozen();rows=[]
    for profile in [PROFILES[0],dict(f['profile'],name='selected')]:
        extraction(profile,'validation',f['validation_scans'],f['validation_seeds'])
        rows+=registration(profile,'validation',f['validation_pairs'],f['validation_seeds'],True)
    write_csv(RUNS/'validation_rows.csv',rows);print('VALIDATED',len(rows),flush=True)


def eth():
    f=frozen();profile=dict(f['profile'],name='selected');seeds=list(range(21,31))
    extraction(profile,'eth_regression',list(range(1,7)),seeds,True)
    checks=[]
    for scan in range(1,7):
        dest=RUNS/'eth_regression/extraction/selected'/str(scan)
        policy=json.loads((dest/'arc_policy.json').read_text());same=0
        for seed,t in itertools.product(seeds,['trees','clusters','retention_bins']):
            n=f'step_seed{seed}_{t}.csv';old=ROOT/'results/eth_trees/failure_followup_20261025/runs/validation_extraction'/f's{scan}'/n
            same+=int((dest/n).read_bytes()==old.read_bytes())
        checks.append(dict(scan=scan,sparse_span_used=policy['sparse_span_used'],median_nearest_m=policy['median_nearest_m'],identical_files=same,total_files=30))
    rr=registration(profile,'eth_regression',list(itertools.combinations(range(1,7),2)),seeds,True,True)
    write_csv(RUNS/'eth_rows.csv',rr)
    (OUT/'eth_regression.json').write_text(json.dumps(dict(scans=checks,primary_cases=150,primary_success=sum(int(r['success']) for r in rr if r['solver']=='svd+ransac'),
                                                        outputs_identical=all(c['identical_files']==30 for c in checks)),indent=2)+'\n')
    print('ETH REGRESSION complete',flush=True)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('phase',choices=['screen','validate','eth']);a=p.parse_args();globals()[a.phase]()


if __name__=='__main__':main()
