#!/usr/bin/env python3
"""Score the ETH tail independently; never changes inference or case selection."""
import json
from pathlib import Path
import shutil
import unified_study as s

for pair in reversed([p for p in s.manifest()['pairs'] if p['dataset']=='ETH']):
    name=f'{pair["source"]}-{pair["target"]}'
    dest=s.RUNS/'v3_evaluation'/pair['scene']/(name+'.csv')
    marker=Path(str(dest)+'.complete')
    if marker.exists():continue
    estimates=s.RUNS/'v3'/pair['scene']/(name+'.csv')
    source=next(scan for scan in s.manifest()['scans'] if scan['scene']==pair['scene'] and scan['scan']==pair['source'])
    out=s.RUNS/'v3_evaluation_assist'/pair['scene']/(name+'.csv')
    out.parent.mkdir(parents=True,exist_ok=True)
    s.execute('evaluate_v3tail_'+pair['scene']+'_'+name,[s.ROOT/'build/cm_evaluate_transforms',f'--src_cloud={s.ROOT/source["path"]}',f'--gt={s.ROOT/pair["gt"]}',f'--estimates={estimates}',f'--out={out}'])
    rows=s.read_csv(out);assert len(rows)==30
    if marker.exists():
        assert rows==s.read_csv(dest),'Duplicate deterministic score disagrees'
    else:
        dest.parent.mkdir(parents=True,exist_ok=True)
        temp=dest.with_suffix('.assist.tmp');shutil.copyfile(out,temp);temp.replace(dest)
        marker.write_text('same frozen estimates and exact full-source evaluator; tail assist\n')
    print('CANONICAL SCORES COMPLETE',pair['scene'],name,flush=True)
