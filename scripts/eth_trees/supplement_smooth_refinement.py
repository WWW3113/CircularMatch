#!/usr/bin/env python3
"""The missing fourth DTM x refinement cell; fixed v1 backend, no retuning."""
from pathlib import Path
import csv,json
from datetime import datetime,timezone
from complete_report_experiments import ROOT,OUT,RUNS,MAN,SEEDS,run,read,write,sha,valid

def main():
 assert (OUT/'full_dtm_smooth_rows.csv').exists()
 protocol=dict(created_before_execution_utc=None,source='smooth full extraction from frozen profile',method='same cm_geometry_register eth10_refined v1 backend, no multistart',seeds=SEEDS,pairs=55,parameters_reselected=False,geometry='reuse identical original raw-cloud geometry caches; independent of DTM',selection='one fixed policy on all scenes; GT evaluation only',sha256={p:sha(ROOT/p) for p in ['build/cm_geometry_register','apps/geometry_refinement.cpp','config/default.ini']})
 p=OUT/'smooth_refinement_protocol.json'
 if not p.exists():protocol['created_before_execution_utc']=datetime.now(timezone.utc).isoformat();p.write_text(json.dumps(protocol,ensure_ascii=False,indent=2)+'\n')
 else:assert {k:v for k,v in json.loads(p.read_text()).items() if k!='created_before_execution_utc'}=={k:v for k,v in protocol.items() if k!='created_before_execution_utc'}
 rows=[]
 for pair in MAN['pairs']:
  scene=pair['scene'];name=pair['source']+'-'+pair['target'];dest=RUNS/'smooth_refinement'/scene;dest.mkdir(parents=True,exist_ok=True);scored=dest/(name+'.scores.csv')
  if not valid(scored,10):
   base=ROOT/'results/tongji_trees/dtm_height_20261025/runs/full_extraction/Plot2' if scene=='Plot2' else RUNS/'full_extraction'/scene
   infer=dest/(name+'.poses.csv');batch=dest/(name+'.jobs.txt')
   if not valid(infer,10):
    prior={}
    if infer.exists():
     import io
     text=infer.read_text();text=text if text.endswith('\n') else text.rsplit('\n',1)[0]+'\n'
     for r in csv.DictReader(io.StringIO(text)):
      if None not in r and r.get('label') in set(map(str,SEEDS)):prior[r['label']]=r
    missing=[s for s in SEEDS if str(s) not in prior]
    batch.write_text(''.join(f'eth10_refined {base}/{pair["source"]}/step_seed{s}_trees.csv {base}/{pair["target"]}/step_seed{s}_trees.csv {s}\n' for s in missing))
    caches=ROOT/'results/unified_20261025/runs/geometry'/scene
    pending=dest/(name+'.pending.csv')
    if missing:
     run('smooth_refine_'+scene+'_'+name,[ROOT/'build/cm_geometry_register',f'--src_cache={caches}/{pair["source"]}.bin',f'--tgt_cache={caches}/{pair["target"]}.bin',f'--batch={batch}',f'--out={pending}'])
     for r in read(pending):
      if r['label'] not in prior:prior[r['label']]=r
    write(infer,[prior[str(s)] for s in SEEDS])
   assert {int(r['label']) for r in read(infer)}==set(SEEDS)
   scan=next(x for x in MAN['scans'] if x['scene']==scene and x['scan']==pair['source'])
   run('smooth_refine_eval_'+scene+'_'+name,[ROOT/'build/cm_evaluate_transforms',f'--src_cloud={ROOT/scan["path"]}',f'--gt={ROOT/pair["gt"]}',f'--estimates={infer}',f'--out={scored}'])
  assert valid(scored,10)
  rows.extend(dict(r,dataset=pair['dataset'],scene=scene,pair=name,seed=r['label'],solver='svd+ransac',method='smooth_refined') for r in read(scored))
 assert len(rows)==550 and len({(r['scene'],r['pair'],r['seed']) for r in rows})==550;write(OUT/'smooth_refined_rows.csv',rows);print('SMOOTH REFINEMENT COMPLETE 550',flush=True)
if __name__=='__main__':main()
