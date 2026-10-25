#!/usr/bin/env python3
"""Auditable two-dataset benchmark. Inference binaries never accept GT."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime,timezone
import csv
import hashlib
import json
import os
from pathlib import Path
import subprocess
import threading
import time
import numpy as np
from tune_registration import ROOT,TOOL,read_csv
from tongji_study import write_csv,sha

OUT=ROOT/'results/unified_20261025';RUNS=OUT/'runs';SEEDS=list(range(31,41));LOCK=threading.Lock()

def execute(label,args):
    logfile=RUNS/'logs'/f'{label}.log';logfile.parent.mkdir(parents=True,exist_ok=True)
    command=([str(TOOL)] if TOOL else [])+list(map(str,args));started=time.monotonic()
    print('START',label,flush=True)
    with logfile.open('w') as log:
        child=subprocess.Popen(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
        _,status,usage=os.wait4(child.pid,0);child.returncode=os.waitstatus_to_exitcode(status)
    record=dict(label=label,command=command,exit_code=child.returncode,wall_s=time.monotonic()-started,cpu_s=usage.ru_utime+usage.ru_stime,peak_rss_kib=usage.ru_maxrss)
    with LOCK:
        with (RUNS/'resources.jsonl').open('a') as f:f.write(json.dumps(record)+'\n')
    if child.returncode:raise RuntimeError(str(logfile))
    print('DONE',label,round(record['wall_s'],2),record['peak_rss_kib'],flush=True)
    return record

def parallel(fn,jobs,workers=4):
    with ThreadPoolExecutor(max_workers=workers) as pool:return list(pool.map(fn,jobs))

def audit():
    OUT.mkdir(parents=True,exist_ok=True);RUNS.mkdir(exist_ok=True)
    tongue=json.loads((ROOT/'results/tongji_trees/fixed_eth_20261025/plan.json').read_text());scans=[];pairs=[]
    for dataset,scenes in [('Tongji',[f'Plot{i}' for i in range(1,5)]),('ETH',['ETH'])]:
        for scene in scenes:
            directory=ROOT/'data/tongji_trees/received/Tongji-Trees'/scene if dataset=='Tongji' else ROOT/'data/eth_trees/raw/trees'
            for i in range(1,6 if dataset=='Tongji' else 7):
                scan=f'S{i}' if dataset=='Tongji' else f's{i}';cloud=directory/f'{scan}.ply'
                scans.append(dict(dataset=dataset,scene=scene,scan=scan,path=str(cloud.relative_to(ROOT)),bytes=cloud.stat().st_size,sha256=sha(cloud),units='m',development_exposure='ETH developed earlier; all Tongji baseline results reviewed; Plot2 used for DTM development'))
            for i in range(1,6 if dataset=='Tongji' else 7):
                for j in range(i+1,6 if dataset=='Tongji' else 7):
                    source,target=(f'S{j}',f'S{i}') if dataset=='Tongji' else (f's{i}',f's{j}')
                    gt=directory/f'{source}_{target}.txt' if dataset=='Tongji' else directory/'groundtruth'/f'{source}-{target}.tfm'
                    matrix=np.loadtxt(gt);assert matrix.shape==(4,4) and np.allclose(matrix[3],[0,0,0,1])
                    pairs.append(dict(dataset=dataset,scene=scene,source=source,target=target,gt=str(gt.relative_to(ROOT)),gt_sha256=sha(gt),direction='source to target'))
    manifest=dict(datasets=['Tongji','ETH'],scans=scans,pairs=pairs,seed_list=SEEDS,independent_unseen_scenes=[],raw_drive_ids_not_published=True)
    (OUT/'dataset_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    source=ROOT/'data/author_source/CircularMatch'
    baseline=dict(repository='https://github.com/MercerSU/CircularMatch',commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip(),tracked_files=subprocess.check_output(['git','ls-tree','-r','--name-only','HEAD'],cwd=source,text=True).splitlines(),readme=(source/'README.md').read_text(),branches_checked=['main'],tags=[],releases=[],author_executable_available=False,strict_author_baseline_comparison='blocked: author repository has README only; reference implementation must be labeled separately',checked_at_utc=datetime.now(timezone.utc).isoformat())
    (OUT/'author_source_audit.json').write_text(json.dumps(baseline,indent=2)+'\n')
    protocol=dict(base_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),frozen_at_utc=datetime.now(timezone.utc).isoformat(),seeds=SEEDS,pairs=55,scene_cases={'Plot1':100,'Plot2':100,'Plot3':100,'Plot4':100,'ETH':150},
        history='Both datasets now participated in development; new seeds are not independent forests. First ETH-to-Tongji transfer and DTM follow-up remain immutable.',
        previous_fixed_transfer='results/tongji_trees/fixed_eth_20261025',scope='Each inference sees only the two current station clouds and tree CSVs. No GT arguments, no graph, no other station map.',
        success='project threshold rotation<2deg AND translation<0.5m; paper definition not found',ep='Eq8 full source points; preserve finite errors for failures; all-case and success-only summaries',
        voxel_scales_m=[.5,.2,.1],max_source_points=20000,iterations=[30,25,20],normal_radius_factor=2.5,normal_cosine_min=.7,max_correspondence_factor=3.,trim_fraction=.7,huber_scale_factor=2.,huber_floor_fraction=.05,
        hypotheses='ETH frozen coarse; if initial CN pairs<6, bounded top5 descriptor evidence triples; 16 diverse proposals, coarse geometry ranking, refine top3; no GT selection',
        ablations=['reference5_raw','reference5_refined','eth10_raw','eth10_refined','multi_refined'],
        computational_budget='one prespecified ICP backend and one multistart extension; diagnose on ETH pairs before full evaluation; no scene-specific parameter choices',
        runtime='fixed 4 OpenMP threads; cache preparation/extraction separated from pair cost; process wall,cpu,maxRSS captured with wait4; concurrent workers recorded; evaluator excluded from inference timings')
    (OUT/'protocol.json').write_text(json.dumps(protocol,indent=2)+'\n')
    print('AUDIT COMPLETE',len(scans),len(pairs),flush=True)

def manifest():return json.loads((OUT/'dataset_manifest.json').read_text())
def feature_dir(scan):
    return (ROOT/'results/tongji_trees/fixed_eth_20261025/runs/extraction'/scan['scene']/scan['scan']) if scan['dataset']=='Tongji' else RUNS/'extraction/ETH'/scan['scan']
def extract_eth():
    def one(scan):
        out=feature_dir(scan);out.mkdir(parents=True,exist_ok=True);marker=out/'complete.json'
        outputs=[out/f'step_seed{seed}_{kind}.csv' for seed in SEEDS for kind in ['trees','clusters','retention_bins']]
        if marker.exists() and all(p.exists() for p in outputs):return
        execute('extract_ETH_'+scan['scan'],[ROOT/'build/cm_extract_batch',f'--input={ROOT/scan["path"]}',f'--out={out}','--config=config/default.ini','--version=step','--cluster.tolerance=0.20','--scanner.x0=0','--scanner.y0=0','--scanner.z0=0','--experiment.seed_base=31','--experiment.n_seeds=10'])
        assert all(p.exists() for p in outputs);marker.write_text(json.dumps(dict(files=30)))
    parallel(one,[s for s in manifest()['scans'] if s['dataset']=='ETH'])

def geometry(datasets=None):
    def one(scan):
        directory=RUNS/'geometry'/scan['scene'];directory.mkdir(parents=True,exist_ok=True);cache=directory/(scan['scan']+'.bin');marker=Path(str(cache)+'.complete')
        if marker.exists() and cache.exists():return
        execute('geometry_'+scan['scene']+'_'+scan['scan'],[ROOT/'build/cm_geometry_register',f'--input={ROOT/scan["path"]}',f'--prepare={cache}'])
        assert cache.exists() and Path(str(cache)+'.timing.csv').exists();marker.write_text(sha(cache)+'\n')
    parallel(one,[s for s in manifest()['scans'] if datasets is None or s['dataset'] in datasets])

def freeze_method():
    paths=['apps/cm_geometry_register.cpp','apps/geometry_refinement.cpp','include/cm/geometry_refinement.hpp','build/cm_geometry_register','build/cm_extract_batch','config/default.ini']
    frozen=dict(frozen_at_utc=datetime.now(timezone.utc).isoformat(),protocol_sha256=sha(OUT/'protocol.json'),sha256={p:sha(ROOT/p) for p in paths},normal_min_neighbors=6,normal_max_curvature=.1,hypothesis_limit=10000,pose_cluster_angle_deg=5,pose_cluster_translation_m=.5,geometry_selection='bidirectional overlap/(0.03+NN_RMSE) times log1p(CN XY support); GT never enters inference')
    path=OUT/'method_frozen.json'
    if path.exists():assert {k:v for k,v in json.loads(path.read_text()).items() if k!='frozen_at_utc'}=={k:v for k,v in frozen.items() if k!='frozen_at_utc'}
    else:path.write_text(json.dumps(frozen,indent=2)+'\n')

def infer_pair(pair,seeds=SEEDS,methods=None,tag='final'):
    methods=methods or ['reference5_raw','reference5_refined','eth10_raw','eth10_refined','multi_refined']
    directory=RUNS/tag/pair['scene'];directory.mkdir(parents=True,exist_ok=True);name=f'{pair["source"]}-{pair["target"]}'
    out=directory/(name+'.csv');batch=directory/(name+'.jobs');marker=directory/(name+'.complete')
    if marker.exists() and out.exists():return
    scans={s['scan']:s for s in manifest()['scans'] if s['scene']==pair['scene']};src=feature_dir(scans[pair['source']]);tgt=feature_dir(scans[pair['target']])
    batch.write_text(''.join(f'{method} {src}/step_seed{seed}_trees.csv {tgt}/step_seed{seed}_trees.csv {seed}\n' for seed in seeds for method in methods))
    execute(tag+'_'+pair['scene']+'_'+name,[ROOT/'build/cm_geometry_register',f'--src_cache={RUNS}/geometry/{pair["scene"]}/{pair["source"]}.bin',f'--tgt_cache={RUNS}/geometry/{pair["scene"]}/{pair["target"]}.bin',f'--batch={batch}',f'--out={out}'])
    assert len(read_csv(out))==len(seeds)*len(methods);marker.write_text('complete\n')

def diagnose():
    pairs=[p for p in manifest()['pairs'] if p['scene']=='ETH' and (p['source'],p['target']) in [('s1','s2'),('s1','s6')]]
    for pair in pairs:infer_pair(pair,[31],tag='diagnose')

def infer():
    frozen=json.loads((OUT/'method_frozen.json').read_text())
    for path,digest in frozen['sha256'].items():assert sha(ROOT/path)==digest
    parallel(infer_pair,manifest()['pairs'],workers=1)

def evaluate(tag='final'):
    def one(pair):
        name=f'{pair["source"]}-{pair["target"]}';est=RUNS/tag/pair['scene']/(name+'.csv')
        if not est.exists():return
        directory=RUNS/(tag+'_evaluation')/pair['scene'];directory.mkdir(parents=True,exist_ok=True);out=directory/(name+'.csv');marker=Path(str(out)+'.complete')
        if marker.exists() and out.exists():return
        source=next(s for s in manifest()['scans'] if s['scene']==pair['scene'] and s['scan']==pair['source'])
        execute('evaluate_'+tag+'_'+pair['scene']+'_'+name,[ROOT/'build/cm_evaluate_transforms',f'--src_cloud={ROOT/source["path"]}',f'--gt={ROOT/pair["gt"]}',f'--estimates={est}',f'--out={out}'])
        assert len(read_csv(est))==len(read_csv(out));marker.write_text('complete\n')
    parallel(one,manifest()['pairs'],workers=1)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('phase',choices=['audit','extract_eth','geometry','freeze_method','diagnose','infer','evaluate']);args=parser.parse_args();globals()[args.phase]()
