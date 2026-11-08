#!/usr/bin/env python3
"""Frozen, resumable 2x2 verification-factor ablation. No GT in inference."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import argparse,csv,hashlib,io,json,os,shlex,subprocess,threading,time
import numpy as np

ROOT=Path(__file__).resolve().parents[2]
LEGACY=Path('/workspace/CircularMatch')
OUT=ROOT/'results/meeting_reports_20261108';RUNS=OUT/'runs/ablation'
BASE=LEGACY/'build';WRAPPER='/workspace/.circularmatch-env/run'
LOADER='/workspace/.circularmatch-env/rootfs/usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2'
LIB='/workspace/.circularmatch-env/rootfs/usr/lib/x86_64-linux-gnu:/workspace/.circularmatch-env/rootfs/usr/lib'
MAN=json.loads((ROOT/'results/unified_20261025/dataset_manifest.json').read_text());LOCK=threading.Lock()

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):
 if not Path(p).exists():return []
 s=Path(p).read_text();s=s if s.endswith('\n') else s.rsplit('\n',1)[0]+'\n'
 return [r for r in csv.DictReader(io.StringIO(s)) if None not in r and all(v is not None for v in r.values())]
def write(p,rows):
 p=Path(p);p.parent.mkdir(parents=True,exist_ok=True);tmp=p.with_suffix(p.suffix+'.tmp')
 with tmp.open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
 tmp.replace(p)
def native(exe,args):return [LOADER,'--library-path',LIB,str(exe),*map(str,args)]
def execute(label,cmd):
 log=RUNS/'logs'/(label+'.log');log.parent.mkdir(parents=True,exist_ok=True)
 env=os.environ.copy();env.update(OMP_NUM_THREADS='4',OMP_WAIT_POLICY='PASSIVE',LC_ALL='C')
 start=time.monotonic()
 with log.open('w') as f:
  child=subprocess.Popen(cmd,cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,env=env)
  _,status,usage=os.wait4(child.pid,0);child.returncode=os.waitstatus_to_exitcode(status)
 rec=dict(label=label,exit_code=child.returncode,wall_s=time.monotonic()-start,cpu_s=usage.ru_utime+usage.ru_stime,peak_rss_kib=usage.ru_maxrss)
 with LOCK:
  with (OUT/'ablation_resources.jsonl').open('a') as f:f.write(json.dumps(rec)+'\n')
 if child.returncode:raise RuntimeError(str(log))
 return rec
def build():
 RUNS.mkdir(parents=True,exist_ok=True)
 names=[f'cm_geometry_ablation_v{v}g{g}' for v,g in [(0,0),(1,1),(1,0),(0,1)]]
 if not all((ROOT/'build'/n).exists() for n in names):
  execute('configure',[WRAPPER,'/usr/bin/cmake','-S',str(ROOT),'-B',str(ROOT/'build'),'-DCM_BUILD_TESTS=OFF','-DCMAKE_BUILD_TYPE=Release'])
  execute('build',[WRAPPER,'/usr/bin/cmake','--build',str(ROOT/'build'),'--target',*names,'-j2'])
 import shutil
 for name in names:shutil.copy2(ROOT/'build'/name,RUNS/name.removeprefix('cm_geometry_ablation_'))
 protocol=dict(frozen_at=datetime.now(timezone.utc).isoformat(),factors={'vertical':['all orientations','abs(normal.z)<0.30; fallback if either side<30 points'],'gate':['3*voxel','min(voxel,0.20m)']},missing=['v1g0','v0g1'],existing_endpoints={'v0g0':'multi_v2_refined','v1g1':'multi_v3_refined'},seeds=list(range(31,41)),pairs=55,trials_per_variant=550,workers=4,omp_threads=4,omp_wait_policy='PASSIVE',gt_scope='Evaluation only; no retuning, pair filtering or GT pose selection',source_sha256={p:sha(ROOT/p) for p in ['apps/geometry_refinement_ablation.cpp','apps/cm_geometry_register_v3.cpp','config/default.ini']},binary_sha256={n:sha(RUNS/n) for n in ['v0g0','v1g1','v1g0','v0g1']},timing='Concurrent controls; no fair runtime ranking versus old epochs')
 p=OUT/'ablation_protocol.json'
 if p.exists():assert {k:v for k,v in json.loads(p.read_text()).items() if k!='frozen_at'}=={k:v for k,v in protocol.items() if k!='frozen_at'}
 else:p.write_text(json.dumps(protocol,indent=2)+'\n')
def paths(pair):
 scene=pair['scene'];base=ROOT/('results/unified_20261025/runs/extraction/ETH' if scene=='ETH' else 'results/tongji_trees/fixed_eth_20261025/runs/extraction/'+scene)
 return base,ROOT/'results/unified_20261025/runs/geometry'/scene
def infer(pair,variant,seeds=range(31,41),tag='full'):
 name=pair['source']+'-'+pair['target'];dest=RUNS/tag/variant/pair['scene'];dest.mkdir(parents=True,exist_ok=True);pose=dest/(name+'.poses.csv');score=dest/(name+'.scores.csv');want=set(map(str,seeds))
 if {r['label'] for r in read(score)}==want:
  return [dict(r,scene=pair['scene'],dataset=pair['dataset'],pair=name,seed=r['label'],variant=variant) for r in read(score)]
 existing={r['label']:r for p in [pose,dest/(name+'.pending.csv')] for r in read(p) if r.get('label') in want}
 if set(existing)!=want:
  base,cache=paths(pair);batch=dest/(name+'.jobs');batch.write_text(''.join(f'multi_v3_refined {base}/{pair["source"]}/step_seed{s}_trees.csv {base}/{pair["target"]}/step_seed{s}_trees.csv {s}\n' for s in seeds if str(s) not in existing))
  pending=dest/(name+'.pending.csv')
  execute(tag+'_'+variant+'_'+pair['scene']+'_'+name,native(RUNS/variant,[f'--src_cache={cache}/{pair["source"]}.bin',f'--tgt_cache={cache}/{pair["target"]}.bin',f'--batch={batch}',f'--out={pending}']))
  existing.update({r['label']:r for r in read(pending)});assert set(existing)==want
  write(pose,[existing[str(s)] for s in seeds])
 if {r['label'] for r in read(pose)}!=want:
  write(pose,[existing[str(s)] for s in seeds])
 if {r['label'] for r in read(score)}!=want:
  scan=next(s for s in MAN['scans'] if s['scene']==pair['scene'] and s['scan']==pair['source'])
  execute('eval_'+tag+'_'+variant+'_'+pair['scene']+'_'+name,native(BASE/'cm_evaluate_transforms',[f'--src_cloud={ROOT/scan["path"]}',f'--gt={ROOT/pair["gt"]}',f'--estimates={pose}',f'--out={score}']))
 assert {r['label'] for r in read(score)}==want
 return [dict(r,scene=pair['scene'],dataset=pair['dataset'],pair=name,seed=r['label'],variant=variant) for r in read(score)]
def validate_endpoints():
 # Includes the documented catastrophic v2 failure and a non-Tongji scene.
 chosen=[next(p for p in MAN['pairs'] if p['scene']==s and (p['source']+'-'+p['target']==name)) for s,name in [('Plot4','S4-S2'),('ETH','s1-s2')]]
 old=read(ROOT/'results/professor_reports_20261026/evidence/U_registration_rows.csv');checks=[]
 for p in chosen:
  for variant,method in [('v0g0','multi_v2_refined'),('v1g1','multi_v3_refined')]:
   r=infer(p,variant,[31],tag='endpoint')[0];a=next(x for x in old if x['scene']==p['scene'] and x['pair']==r['pair'] and x['seed']=='31' and x['method']==method)
   assert r['success']==a['success'];assert abs(float(r['e_p_m'])-float(a['e_p_m']))<1e-7,(r,a)
   checks.append(dict(scene=p['scene'],pair=r['pair'],variant=variant,success=r['success'],ep_difference_m=float(r['e_p_m'])-float(a['e_p_m'])))
 (OUT/'endpoint_validation.json').write_text(json.dumps(checks,indent=2)+'\n')
 print('ENDPOINTS VALIDATED',flush=True)
def run():
 restore_partial();build();validate_endpoints();tasks=[(p,v) for p in MAN['pairs'] for v in ['v1g0','v0g1']]
 def job(t):
  p,v=t;r=infer(p,v)
  with LOCK:print('DONE',v,p['scene'],r[0]['pair'],'success',sum(x['success']=='1' for x in r),flush=True)
  return r
 with ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(job,tasks))
 aggregate()
def restore_partial():
 saved={}
 for r in read(OUT/'ablation_partial_rows.csv'):saved.setdefault((r['variant'],r['scene'],r['pair']),[]).append(r)
 for (variant,scene,pair),rows in saved.items():
  if {r['label'] for r in rows}!=set(map(str,range(31,41))):continue
  p=RUNS/'full'/variant/scene/(pair+'.scores.csv')
  if len(read(p))!=10:write(p,rows)
def aggregate():
 rows=[r for p in MAN['pairs'] for v in ['v1g0','v0g1'] for r in infer(p,v)]
 assert len(rows)==1100 and len({(r['variant'],r['scene'],r['pair'],r['seed']) for r in rows})==1100
 write(OUT/'verification_ablation_rows.csv',rows)
 (OUT/'ABLATION_COMPLETE.json').write_text(json.dumps(dict(rows=1100,sha256=sha(OUT/'verification_ablation_rows.csv'),completed_at=datetime.now(timezone.utc).isoformat()),indent=2)+'\n');print('ABLATION COMPLETE 1100',flush=True)
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('phase',choices=['build','run','aggregate']);a=ap.parse_args();globals()[a.phase]()
