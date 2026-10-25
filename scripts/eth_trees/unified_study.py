#!/usr/bin/env python3
"""Auditable two-dataset benchmark. Inference binaries never accept GT."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime,timezone
import csv
import io
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

def complete_csv_rows(path):
    text=Path(path).read_text()
    if not text.endswith('\n'):text=text.rsplit('\n',1)[0]+'\n' if '\n' in text else ''
    return list(csv.DictReader(io.StringIO(text))) if text else []

def execute(label,args):
    logfile=RUNS/'logs'/f'{label}.log';logfile.parent.mkdir(parents=True,exist_ok=True)
    command=([str(TOOL)] if TOOL else [])+list(map(str,args));started=time.monotonic()
    print('START',label,flush=True)
    with logfile.open('w') as log:
        child=subprocess.Popen(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
        _,status,usage=os.wait4(child.pid,0);child.returncode=os.waitstatus_to_exitcode(status)
    record=dict(label=label,command=command,exit_code=child.returncode,wall_s=time.monotonic()-started,cpu_s=usage.ru_utime+usage.ru_stime,peak_rss_kib=usage.ru_maxrss)
    epoch=OUT/'current_epoch.json'
    record['runtime_epoch']=json.loads(epoch.read_text())['id'] if epoch.exists() else 'legacy_before_environment_restart'
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
    manifest_path=OUT/'dataset_manifest.json'
    if manifest_path.exists():
        previous=json.loads(manifest_path.read_text())
        assert all(previous[k]==manifest[k] for k in ['datasets','scans','pairs','seed_list'])
    else:manifest_path.write_text(json.dumps(manifest,indent=2)+'\n')
    source=ROOT/'data/author_source/CircularMatch'
    baseline=dict(repository='https://github.com/MercerSU/CircularMatch',commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip(),tracked_files=subprocess.check_output(['git','ls-tree','-r','--name-only','HEAD'],cwd=source,text=True).splitlines(),readme=(source/'README.md').read_text(),branches_checked=['main'],tags=[],releases=[],author_executable_available=False,strict_author_baseline_comparison='blocked: author repository has README only; reference implementation must be labeled separately',checked_at_utc=datetime.now(timezone.utc).isoformat())
    author_path=OUT/'author_source_audit.json'
    if not author_path.exists():author_path.write_text(json.dumps(baseline,indent=2)+'\n')
    protocol=dict(base_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),frozen_at_utc=datetime.now(timezone.utc).isoformat(),seeds=SEEDS,pairs=55,scene_cases={'Plot1':100,'Plot2':100,'Plot3':100,'Plot4':100,'ETH':150},
        history='Both datasets now participated in development; new seeds are not independent forests. First ETH-to-Tongji transfer and DTM follow-up remain immutable.',
        previous_fixed_transfer='results/tongji_trees/fixed_eth_20261025',scope='Each inference sees only the two current station clouds and tree CSVs. No GT arguments, no graph, no other station map.',
        success='project threshold rotation<2deg AND translation<0.5m; paper definition not found',ep='Eq8 full source points; preserve finite errors for failures; all-case and success-only summaries',
        voxel_scales_m=[.5,.2,.1],max_source_points=20000,iterations=[30,25,20],normal_radius_factor=2.5,normal_cosine_min=.7,max_correspondence_factor=3.,trim_fraction=.7,huber_scale_factor=2.,huber_floor_fraction=.05,
        hypotheses='ETH frozen coarse; if initial CN pairs<6, bounded top5 descriptor evidence triples; 16 diverse proposals, coarse geometry ranking, refine top3; no GT selection',
        ablations=['reference5_raw','reference5_refined','eth10_raw','eth10_refined','multi_refined'],
        computational_budget='one prespecified ICP backend and one multistart extension; diagnose on ETH pairs before full evaluation; no scene-specific parameter choices',
        runtime='fixed 4 OpenMP threads; cache preparation/extraction separated from pair cost; process wall,cpu,maxRSS captured with wait4; concurrent workers recorded; evaluator excluded from inference timings')
    protocol_path=OUT/'protocol.json'
    if protocol_path.exists():
        previous=json.loads(protocol_path.read_text());ignored={'base_commit','frozen_at_utc'}
        assert {k:v for k,v in previous.items() if k not in ignored}=={k:v for k,v in protocol.items() if k not in ignored}
    else:protocol_path.write_text(json.dumps(protocol,indent=2)+'\n')
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

def infer_pair(pair,seeds=SEEDS,methods=None,tag='final',version='v1'):
    methods=methods or ['reference5_raw','reference5_refined','eth10_raw','eth10_refined','multi_refined']
    directory=RUNS/tag/pair['scene'];directory.mkdir(parents=True,exist_ok=True);name=f'{pair["source"]}-{pair["target"]}'
    out=directory/(name+'.csv');batch=directory/(name+'.jobs');marker=directory/(name+'.complete')
    expected={(str(seed),method) for seed in seeds for method in methods}
    existing={}
    if out.exists():
        for row in complete_csv_rows(out):
            if None not in row and all(v is not None for v in row.values()) and (row.get('label'),row.get('method')) in expected:existing[(row['label'],row['method'])]=row
    if marker.exists() and len(existing)==len(expected):return
    scans={s['scan']:s for s in manifest()['scans'] if s['scene']==pair['scene']};src=feature_dir(scans[pair['source']]);tgt=feature_dir(scans[pair['target']])
    batch.write_text(''.join(f'{method} {src}/step_seed{seed}_trees.csv {tgt}/step_seed{seed}_trees.csv {seed}\n' for seed in seeds for method in methods if (str(seed),method) not in existing))
    binary={'v1':'cm_geometry_register','v2':'cm_geometry_register_v2','v3':'cm_geometry_register_v3'}[version]
    if len(existing)<len(expected):
        pending=out.with_suffix('.pending.csv')
        execute(tag+'_'+pair['scene']+'_'+name,[ROOT/'build'/binary,f'--src_cache={RUNS}/geometry/{pair["scene"]}/{pair["source"]}.bin',f'--tgt_cache={RUNS}/geometry/{pair["scene"]}/{pair["target"]}.bin',f'--batch={batch}',f'--out={pending}'])
        for row in read_csv(pending):existing[(row['label'],row['method'])]=row
    assert existing.keys()==expected
    merged=out.with_suffix('.merged.csv');write_csv(merged,[existing[(str(seed),method)] for seed in seeds for method in methods]);merged.replace(out);marker.write_text('complete\n')

def diagnose():
    pairs=[p for p in manifest()['pairs'] if p['scene']=='ETH' and (p['source'],p['target']) in [('s1','s2'),('s1','s6')]]
    for pair in pairs:infer_pair(pair,[31],tag='diagnose')

def infer():
    frozen=json.loads((OUT/'method_frozen.json').read_text())
    for path,digest in frozen['sha256'].items():assert sha(ROOT/path)==digest
    parallel(infer_pair,manifest()['pairs'],workers=1)

def freeze_v2():
    record=dict(frozen_at_utc=datetime.now(timezone.utc).isoformat(),reason='v1 missed four weak-correspondence cases; retain v1 and evaluate broader coverage on all scenes',code_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),top_descriptor_candidates=20,max_evidence_edges=120,max_pair_seeds=7140,max_3d_triples=10000,max_diverse_poses=256,refined_candidates=8,initialization='upright yaw from two horizontal correspondences; vertical translation median; existing full6DOF coarse estimate retained; final refinement full6DOF',selection='bidirectional geometry quality times log1p(global horizontal tree support); no GT',sha256={p:sha(ROOT/p) for p in ['apps/cm_geometry_register_v2.cpp','build/cm_geometry_register_v2','apps/geometry_refinement.cpp','include/cm/geometry_refinement.hpp']})
    path=OUT/'method_v2_frozen.json'
    if path.exists():assert {k:v for k,v in json.loads(path.read_text()).items() if k not in ['frozen_at_utc','code_commit']}=={k:v for k,v in record.items() if k not in ['frozen_at_utc','code_commit']}
    else:path.write_text(json.dumps(record,indent=2)+'\n')

def infer_v2():
    record=json.loads((OUT/'method_v2_frozen.json').read_text())
    for path,digest in record['sha256'].items():assert sha(ROOT/path)==digest
    for pair in manifest()['pairs']:infer_pair(pair,methods=['multi_v2_refined'],tag='v2',version='v2')

def freeze_v3():
    paths=['apps/cm_geometry_register_v3.cpp','apps/geometry_refinement_structural.cpp','build/cm_geometry_register_v3','include/cm/geometry_refinement.hpp']
    record=dict(code_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),frozen_at_utc=datetime.now(timezone.utc).isoformat(),protocol_sha256=sha(OUT/'v3_protocol.json'),sha256={p:sha(ROOT/p) for p in paths},status='one globally fixed structure-verification revision, no scene-specific choices')
    path=OUT/'method_v3_frozen.json'
    if path.exists():assert {k:v for k,v in json.loads(path.read_text()).items() if k not in ['frozen_at_utc','code_commit']}=={k:v for k,v in record.items() if k not in ['frozen_at_utc','code_commit']}
    else:path.write_text(json.dumps(record,indent=2)+'\n')

def infer_v3():
    record=json.loads((OUT/'method_v3_frozen.json').read_text())
    for path,digest in record['sha256'].items():assert sha(ROOT/path)==digest
    for pair in manifest()['pairs']:infer_pair(pair,methods=['reference5_refined_v3','eth10_refined_v3','multi_v3_refined'],tag='v3',version='v3')

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

def cold_benchmark():
    """Application caches are rebuilt from raw inputs for each version, serially."""
    chosen=[next(p for p in manifest()['pairs'] if p['scene']==scene) for scene in ['ETH','Plot1']]
    protocol=dict(pairs=chosen,seed=31,methods=['reference5_raw','reference5_refined','multi_refined','multi_v2_refined','reference5_refined_v3','multi_v3_refined'],order='ETH forward; Tongji reverse (counterbalanced)',application_cache='cold: re-extract both stations and rebuild geometry if needed',filesystem_cache='uncontrolled/warm; no OS cache reset',threads=4,workers=1)
    (OUT/'cold_benchmark_protocol.json').write_text(json.dumps(protocol,indent=2)+'\n')
    results=[]
    for pair in chosen:
        methods=protocol['methods'][:]
        if pair['dataset']=='Tongji':methods.reverse()
        for method in methods:
            dest=RUNS/'cold_benchmark'/pair['scene']/method;dest.mkdir(parents=True,exist_ok=True);marker=dest/'complete.json'
            if marker.exists():results.append(json.loads(marker.read_text()));continue
            start=time.monotonic();resources=[];caches=[];features=[]
            for scan_name in [pair['source'],pair['target']]:
                scan=next(s for s in manifest()['scans'] if s['scene']==pair['scene'] and s['scan']==scan_name)
                directory=dest/scan_name;directory.mkdir(exist_ok=True);features.append(directory/'step_seed31_trees.csv')
                resources.append(execute('cold_extract_'+pair['scene']+'_'+method+'_'+scan_name,[ROOT/'build/cm_extract_batch',f'--input={ROOT/scan["path"]}',f'--out={directory}','--config=config/default.ini','--version=step','--cluster.tolerance=0.20','--scanner.x0=0','--scanner.y0=0','--scanner.z0=0','--experiment.seed_base=31','--experiment.n_seeds=1']))
                assert read_csv(features[-1])==read_csv(feature_dir(scan)/'step_seed31_trees.csv')
                cache=directory/'geometry.bin';caches.append(cache)
                if method!='reference5_raw':resources.append(execute('cold_geometry_'+pair['scene']+'_'+method+'_'+scan_name,[ROOT/'build/cm_geometry_register',f'--input={ROOT/scan["path"]}',f'--prepare={cache}']))
                else:
                    import struct
                    with cache.open('wb') as f:
                        f.write(b'CMGEO001')
                        for h in [.5,.2,.1]:f.write(struct.pack('<dQ',h,0))
            batch=dest/'jobs.txt';batch.write_text(f'{method} {features[0]} {features[1]} 31\n')
            binary='cm_geometry_register_v3' if method.endswith('_v3') or method=='multi_v3_refined' else ('cm_geometry_register_v2' if method=='multi_v2_refined' else 'cm_geometry_register')
            resources.append(execute('cold_pair_'+pair['scene']+'_'+method,[ROOT/'build'/binary,f'--src_cache={caches[0]}',f'--tgt_cache={caches[1]}',f'--batch={batch}',f'--out={dest}/pose.csv']))
            result=dict(dataset=pair['dataset'],scene=pair['scene'],pair=f'{pair["source"]}-{pair["target"]}',method=method,wall_s=time.monotonic()-start,cpu_s=sum(r['cpu_s'] for r in resources),peak_child_rss_kib=max(r['peak_rss_kib'] for r in resources),peak_scope='largest native child RSS, excludes OS page cache and Python parent; processes run serially',resources=[r['label'] for r in resources])
            result['runtime_epoch']=resources[-1].get('runtime_epoch','legacy_before_environment_restart')
            marker.write_text(json.dumps(result,indent=2)+'\n');results.append(result)
    (OUT/'cold_benchmark.json').write_text(json.dumps(results,indent=2)+'\n')

def feature_benchmark():
    """Fresh one-seed cost for every station; shared by all method variants."""
    def one(scan):
        out=RUNS/'feature_benchmark'/scan['scene']/scan['scan'];out.mkdir(parents=True,exist_ok=True);marker=out/'complete.json'
        if marker.exists():return json.loads(marker.read_text())
        result=execute('feature_benchmark_'+scan['scene']+'_'+scan['scan'],[ROOT/'build/cm_extract_batch',f'--input={ROOT/scan["path"]}',f'--out={out}','--config=config/default.ini','--version=step','--cluster.tolerance=0.20','--scanner.x0=0','--scanner.y0=0','--scanner.z0=0','--experiment.seed_base=31','--experiment.n_seeds=1'])
        assert read_csv(out/'step_seed31_trees.csv')==read_csv(feature_dir(scan)/'step_seed31_trees.csv')
        result.update(dataset=scan['dataset'],scene=scan['scene'],scan=scan['scan'],seed=31,concurrent_jobs=4)
        marker.write_text(json.dumps(result,indent=2)+'\n');return result
    records=parallel(one,manifest()['scans']);(OUT/'feature_benchmark.json').write_text(json.dumps(records,indent=2)+'\n')

def checkpoint():
    """Publishable poses/scores only; raw clouds and derived tree locations stay local."""
    import shutil
    destination=OUT/'checkpoints';destination.mkdir(exist_ok=True);counts={};files=[]
    for kind,tags in [('predictions',['final','v2','v3']),('scores',['final_evaluation','v2_evaluation','v3_evaluation'])]:
        gathered_map={}
        for tag in tags:
            groups=set()
            for path in sorted((RUNS/tag).glob('*/*.csv')):
                if path.name.endswith('.merged.csv'):continue
                if kind=='scores' and not Path(str(path)+'.complete').exists():continue
                pair=path.stem.removesuffix('.pending')
                valid=[]
                for r in complete_csv_rows(path):
                    if None not in r and all(v is not None for v in r.values()) and r.get('label','').isdigit() and r.get('pose_ok') in ['0','1']:valid.append(r)
                if valid:
                    groups.add((path.parent.name,pair))
                    for r in valid:gathered_map.setdefault((tag,path.parent.name,pair,r['label'],r['method']),dict(run=tag,scene=path.parent.name,pair=pair,**r))
            counts[tag]=dict(pair_files=len(groups),rows=sum(r['run']==tag for r in gathered_map.values()))
        gathered=list(gathered_map.values())
        if gathered:
            keys=list(dict.fromkeys(k for r in gathered for k in r));target=destination/f'completed_{kind}.csv';temp=target.with_suffix('.tmp')
            write_csv(temp,[{k:r.get(k,'') for k in keys} for r in gathered]);temp.replace(target);files.append(target.name)
    if (RUNS/'resources.jsonl').exists():
        target=destination/'resources.jsonl';temp=destination/'resources.tmp';valid=[]
        for line in (RUNS/'resources.jsonl').read_text().splitlines():
            try:json.loads(line);valid.append(line)
            except json.JSONDecodeError:pass
        temp.write_text('\n'.join(valid)+'\n');temp.replace(target);files.append(target.name)
    state=dict(saved_at_utc=datetime.now(timezone.utc).isoformat(),counts=counts,files={name:sha(destination/name) for name in files},raw_clouds_uploaded=False,tree_coordinates_uploaded=False,cold_benchmark_complete=(OUT/'cold_benchmark.json').exists(),feature_benchmark_complete=(OUT/'feature_benchmark.json').exists())
    temp=destination/'status.tmp';temp.write_text(json.dumps(state,indent=2)+'\n');temp.replace(destination/'status.json');print(json.dumps(counts),flush=True)

def restore_checkpoint(destination=None):
    import shutil
    target=Path(destination) if destination else RUNS;source=OUT/'checkpoints';state=json.loads((source/'status.json').read_text())
    for name,digest in state['files'].items():assert sha(source/name)==digest,'checkpoint integrity failure'
    for name in ['completed_predictions.csv','completed_scores.csv']:
        if not (source/name).exists():continue
        grouped={}
        for raw in read_csv(source/name):
            r=dict(raw);run,scene,pair=[r.pop(k) for k in ['run','scene','pair']];grouped.setdefault((run,scene,pair),[]).append(r)
        for (run,scene,pair),rows in grouped.items():
            directory=target/run/scene;directory.mkdir(parents=True,exist_ok=True);out=directory/(pair+'.csv')
            marker=Path(str(out)+'.complete') if 'evaluation' in run else out.with_suffix('.complete')
            if marker.exists():continue
            existing={(r['label'],r['method']):r for r in read_csv(out)} if out.exists() else {}
            for r in rows:existing.setdefault((r['label'],r['method']),r)
            write_csv(out,list(existing.values()))
            expected=50 if run.startswith('final') else (30 if run.startswith('v3') else 10)
            if len(existing)==expected:marker.write_text('restored complete\n')
    if not (target/'resources.jsonl').exists() and (source/'resources.jsonl').exists():target.mkdir(parents=True,exist_ok=True);shutil.copyfile(source/'resources.jsonl',target/'resources.jsonl')
    print('CHECKPOINT RESTORED',target,flush=True)

def publish_checkpoint():
    import fcntl
    RUNS.mkdir(parents=True,exist_ok=True)
    with (RUNS/'checkpoint-git.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX);checkpoint()
        paths=[str((OUT/'checkpoints').relative_to(ROOT))]
        paths.extend(str((OUT/name).relative_to(ROOT)) for name in ['cold_benchmark.json','cold_benchmark_protocol.json','feature_benchmark.json','method_v3_frozen.json'] if (OUT/name).exists())
        subprocess.run(['git','add','--',*paths],cwd=ROOT,check=True)
        changed=subprocess.run(['git','diff','--cached','--quiet','--',*paths],cwd=ROOT).returncode
        if changed:subprocess.run(['git','commit','--only','-m','Save resumable unified experiment checkpoint','--',*paths],cwd=ROOT,check=True)
        subprocess.run(['git','push','origin','HEAD'],cwd=ROOT,check=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('phase',choices=['audit','extract_eth','geometry','freeze_method','diagnose','infer','freeze_v2','infer_v2','freeze_v3','infer_v3','evaluate','cold_benchmark','feature_benchmark','checkpoint','restore_checkpoint','publish_checkpoint']);args=parser.parse_args();globals()[args.phase]()
