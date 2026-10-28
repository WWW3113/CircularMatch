#!/usr/bin/env python3
"""Resume-safe missing comparisons for the six teaching reports. No GT selection."""
from pathlib import Path
import argparse,csv,hashlib,json,os,subprocess,time,threading
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime,timezone

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'results/report_experiments_20261028'
RUNS=OUT/'runs';TOOL=Path('/workspace/.circularmatch-env/run');SEEDS=list(range(31,41));LOCK=threading.Lock()
MAN=json.loads((ROOT/'results/unified_20261025/dataset_manifest.json').read_text())

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return list(csv.DictReader(Path(p).open()))
def write(p,rows):
 p=Path(p);p.parent.mkdir(parents=True,exist_ok=True);temp=p.with_suffix('.tmp');fields=list(dict.fromkeys(k for r in rows for k in r))
 with temp.open('w',newline='') as f:w=csv.DictWriter(f,fields);w.writeheader();w.writerows(rows)
 temp.replace(p)
def features(scan):
 return ROOT/('results/unified_20261025/runs/extraction/ETH/'+scan['scan'] if scan['dataset']=='ETH' else 'results/tongji_trees/fixed_eth_20261025/runs/extraction/'+scan['scene']+'/'+scan['scan'])
def run(label,args):
 log=RUNS/'logs'/(label+'.log');log.parent.mkdir(exist_ok=True,parents=True);started=time.monotonic();cmd=[str(TOOL),*map(str,args)]
 print('START',label,flush=True)
 with log.open('w') as f:
  p=subprocess.Popen(cmd,cwd=ROOT,stdout=f,stderr=subprocess.STDOUT);_,status,u=os.wait4(p.pid,0);p.returncode=os.waitstatus_to_exitcode(status)
 record=dict(label=label,exit_code=p.returncode,wall_s=time.monotonic()-started,cpu_s=u.ru_utime+u.ru_stime,peak_child_rss_kib=u.ru_maxrss,command=cmd,utc=datetime.now(timezone.utc).isoformat())
 with LOCK:
  with (OUT/'resources.jsonl').open('a') as f:f.write(json.dumps(record)+'\n')
 if p.returncode:raise RuntimeError(str(log))
 print('DONE',label,round(record['wall_s'],1),flush=True)
def freeze():
 OUT.mkdir(exist_ok=True,parents=True);RUNS.mkdir(exist_ok=True)
 p=dict(seeds=SEEDS,datasets=['Tongji-trees','ETH-tree'],pairs=55,step='Original step retention; scanner origin zero; same extraction unless full-smooth',thresholds=[.05,.1],solvers=['svd','svd+ransac'],recovery='off for 4-cell coarse; evidence trigger6 for DTM',dtm=dict(baseline=dict(cell=.5,fit=2,fill=5),smooth=dict(cell=.5,fit=6,fill=6)),success='rotation<2deg AND translation<0.5m',ep='All source points, finite failures retained; no-output failures retained in success denominator',method_selection='No retuning or per-scene selection. Run frozen smooth profile on missing datasets, report regressions.',scope='Development/retrospective benchmark, not unseen forests. Native inference does not use GT to choose poses.',workers=2,omp_threads_per_child=4,missing_work=['ETH four coarse configurations seed31–40','ETH baseline/smooth fixed-axis DTM','ETH and Tongji Plot1/3/4 full smooth extraction and registration'],source_sha256={x:sha(ROOT/x) for x in ['src/cn_recovery.cpp','src/registration.cpp','src/dtm.cpp','config/default.ini','apps/cm_register.cpp','apps/cm_dtm_reposition.cpp']})
 dest=OUT/'protocol.json'
 if dest.exists():assert json.loads(dest.read_text())==p,'Protocol changed; create a new output directory.'
 else:dest.write_text(json.dumps(p,ensure_ascii=False,indent=2)+'\n')
 (OUT/'RESUME.md').write_text('Missing comparisons for six reports. Resume with:\n\npython scripts/eth_trees/complete_report_experiments.py all\n\nCompleted per-case CSVs and per-station .complete markers are reused. Never rerun based on GT failure. Raw clouds/tree coordinates remain local and are not published. protocol.json is frozen. Scalar aggregate outputs and resources are separate.\n')
def valid(p,n):
 try:return p.exists() and p.read_bytes().endswith(b'\n') and len(read(p))==n
 except Exception:return False
def register(tag,scenes,feature_map,recovery,thresholds):
 def source_job(scan):
  pairs=[p for p in MAN['pairs'] if p['scene']==scan['scene'] and p['source']==scan['scan']];dest=RUNS/tag/scan['scene'];dest.mkdir(parents=True,exist_ok=True);pending=[];outputs=[]
  count=len(thresholds)*2
  for pair in pairs:
   for seed in SEEDS:
    output=dest/f'{seed}_{pair["source"]}-{pair["target"]}.csv';outputs.append((pair,seed,output))
    if valid(output,count):continue
    src=feature_map[(pair['scene'],pair['source'])]/f'step_seed{seed}_trees.csv';tgt=feature_map[(pair['scene'],pair['target'])]/f'step_seed{seed}_trees.csv';assert src.exists() and tgt.exists()
    pending.append(f'{src} {tgt} {ROOT/scan["path"]} {ROOT/pair["gt"]} {output} {seed}')
  if pending:
   batch=dest/(scan['scan']+'.jobs');batch.write_text('\n'.join(pending)+'\n');args=[ROOT/'build/cm_register',f'--batch={batch}','--thresholds='+','.join(map(str,thresholds)),f'--recovery={recovery}']
   if recovery!='off':args+=['--recovery_trigger=6']
   run(tag+'_'+scan['scene']+'_'+scan['scan'],args)
  for pair,seed,output in outputs:
   assert valid(output,count)
  return [dict(r,dataset=pair['dataset'],scene=pair['scene'],pair=pair['source']+'-'+pair['target'],seed=seed,method=tag) for pair,seed,output in outputs for r in read(output)]
 scans=[s for s in MAN['scans'] if s['scene'] in scenes and any(p['scene']==s['scene'] and p['source']==s['scan'] for p in MAN['pairs'])]
 with ThreadPoolExecutor(max_workers=2) as pool:rows=[r for part in pool.map(source_job,scans) for r in part]
 write(OUT/(tag+'_rows.csv'),rows);return rows
def coarse():
 fmap={(s['scene'],s['scan']):features(s) for s in MAN['scans']}
 rows=register('eth_coarse',['ETH'],fmap,'off',[.05,.1]);assert len(rows)==600
 print('ETH COARSE COMPLETE',len(rows),flush=True)
def dtm():
 scans=[s for s in MAN['scans'] if s['dataset']=='ETH'];fmap={}
 def one(scan):
  dest=RUNS/'positions/ETH'/scan['scan'];dest.mkdir(parents=True,exist_ok=True);marker=dest/'complete.json'
  if not marker.exists():
   original=features(scan);batch=dest/'jobs.txt';batch.write_text(''.join(f'{original}/step_seed{seed}_trees.csv {original}/step_seed{seed}_clusters.csv\n' for seed in SEEDS))
   run('dtm_ETH_'+scan['scan'],[ROOT/'build/cm_dtm_reposition',f'--input={ROOT/scan["path"]}',f'--batch={batch}',f'--out={dest}','--profiles=baseline,smooth','--config=config/default.ini','--scanner.x0=0','--scanner.y0=0','--scanner.z0=0'])
   for seed in SEEDS:assert read(dest/'baseline'/f'step_seed{seed}_trees.csv')==read(original/f'step_seed{seed}_trees.csv')
   marker.write_text(json.dumps(dict(seed_files=20,baseline_exact=True))+'\n')
  return (scan['scene'],scan['scan']),dest/'smooth'
 with ThreadPoolExecutor(max_workers=2) as pool:fmap.update(pool.map(one,scans))
 rows=register('eth_dtm_position',['ETH'],fmap,'evidence',[.1]);assert len(rows)==300
 print('ETH DTM POSITION COMPLETE',len(rows),flush=True)
def full():
 def one(scan):
  if scan['scene']=='Plot2':return (scan['scene'],scan['scan']),ROOT/'results/tongji_trees/dtm_height_20261025/runs/full_extraction/Plot2'/scan['scan']
  dest=RUNS/'full_extraction'/scan['scene']/scan['scan'];dest.mkdir(parents=True,exist_ok=True);marker=dest/'complete.json'
  if not marker.exists():
   run('full_'+scan['scene']+'_'+scan['scan'],[ROOT/'build/cm_extract_batch',f'--input={ROOT/scan["path"]}',f'--out={dest}','--config=config/default.ini','--version=step','--cluster.tolerance=0.20','--scanner.x0=0','--scanner.y0=0','--scanner.z0=0','--experiment.seed_base=31','--experiment.n_seeds=10','--dtm.fit_radius=6','--dtm.fill_max_radius=6'])
   for seed in SEEDS:assert (dest/f'step_seed{seed}_trees.csv').exists()
   marker.write_text(json.dumps(dict(files=30,profile='frozen smooth'))+'\n')
  return (scan['scene'],scan['scan']),dest
 with ThreadPoolExecutor(max_workers=2) as pool:fmap=dict(pool.map(one,MAN['scans']))
 rows=register('full_dtm_smooth',['Plot1','Plot2','Plot3','Plot4','ETH'],fmap,'evidence',[.1]);assert len(rows)==1100
 print('FULL DTM COMPLETE',len(rows),flush=True)
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('phase',choices=['freeze','coarse','dtm','full','all']);a=ap.parse_args();freeze()
 for name,fn in [('coarse',coarse),('dtm',dtm),('full',full)]:
  if a.phase in [name,'all']:fn()
