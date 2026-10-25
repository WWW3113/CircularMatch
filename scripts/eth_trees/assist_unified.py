#!/usr/bin/env python3
"""Compute a disjoint tail first; canonical results are never selected using GT."""
from datetime import datetime,timezone
from pathlib import Path
import json
import os
import shutil
import unified_study as s

record=json.loads((s.OUT/'method_v3_frozen.json').read_text())
for path,digest in record['sha256'].items():assert s.sha(s.ROOT/path)==digest
(s.OUT/'acquisition_parallelism.json').write_text(json.dumps(dict(started_utc=datetime.now(timezone.utc).isoformat(),extra_workers=1,process_threads=4,cpu_quota_cores=4,scope='accuracy collection only; cold timing remains serial; no algorithm/parameter changes',possible_duplicate_tail='deduplicated by scene,pair,seed,method; never increases accuracy sample count'),indent=2)+'\n')
for pair in reversed([p for p in s.manifest()['pairs'] if p['dataset']=='ETH']):
    name=f'{pair["source"]}-{pair["target"]}';dest=s.RUNS/'v3'/pair['scene']/(name+'.csv');marker=dest.with_suffix('.complete')
    if marker.exists():continue
    s.infer_pair(pair,methods=['reference5_refined_v3','eth10_refined_v3','multi_v3_refined'],tag='v3_assist',version='v3')
    source=s.RUNS/'v3_assist'/pair['scene']/(name+'.csv');rows=s.read_csv(source);assert len(rows)==30
    if marker.exists():
        old={(r['label'],r['method']):r for r in s.read_csv(dest)}
        for r in rows:
            original=old[(r['label'],r['method'])];assert original['pose_ok']==r['pose_ok']
            if r['pose_ok']=='1':assert all(abs(float(original[k])-float(r[k]))<1e-10 for k in [f'r{i}{j}' for i in range(3) for j in range(3)]+[f't{i}' for i in range(3)])
    else:
        dest.parent.mkdir(parents=True,exist_ok=True);temporary=dest.with_suffix('.assist.tmp');shutil.copyfile(source,temporary);temporary.replace(dest);marker.write_text('assist complete; same frozen method\n')
    print('CANONICAL TAIL COMPLETE',pair['scene'],name,flush=True)
