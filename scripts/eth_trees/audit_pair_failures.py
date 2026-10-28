#!/usr/bin/env python3
"""Evaluation-only failure audit; oracle proximity is not tree annotation truth."""
import csv
import json
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'results/eth_trees/failure_followup_20261025'
RUNS = OUT/'runs'
RAW = ROOT/'data/eth_trees/raw/trees'
TREES = ROOT/'results/eth_trees/tuning_20261025/runs/extraction/control'


def read(path):
    with path.open() as f: return list(csv.DictReader(f))


def points(path):
    return np.array([[float(r[c]) for c in ['x','y','z']] for r in read(path)])


def main():
    old = [r for r in read(ROOT/'results/eth_trees/tuning_20261025/runs/fresh_rows.csv')
           if r['method']=='fresh_matching']
    fields = ['n_src_kp','n_tgt_kp','gate_passed','candidates','best_score','n_matches',
              'n_correct_matches','ransac_inliers','success','transform_ok','rot_err_deg','trans_err_m']
    fields += [f'r{i}{j}' for i in range(3) for j in range(3)] + [f't{i}' for i in range(3)]
    for r in old:
        n = next(x for x in read(RUNS/'baseline'/f'{r["seed"]}_{r["pair"]}.csv') if x['solver']==r['solver'])
        assert all(r[k]==n[k] for k in fields), (r['seed'],r['pair'])
    official={tuple(s.split()) for s in (RAW/'pairs.txt').read_text().splitlines() if s.strip()}
    rows=[]
    for r in old:
        if r['solver']!='svd+ransac' or r['success']=='1': continue
        a,b=r['pair'].split('-');seed=r['seed'];stem=f'{seed}_{r["pair"]}_thr0.100000'
        s=points(TREES/a/f'step_seed{seed}_trees.csv');t=points(TREES/b/f'step_seed{seed}_trees.csv')
        gt=np.loadtxt(RAW/'groundtruth'/f'{a}-{b}.tfm');mapped=s@gt[:3,:3].T+gt[:3,3]
        d=np.linalg.norm(mapped[:,None,:2]-t[None,:,:2],axis=2)
        nearest=d.argmin(axis=1);back=d.argmin(axis=0)
        mutual=[(i,j) for i,j in enumerate(nearest) if back[j]==i and d[i,j]<.30]
        desc=read(RUNS/'traces'/f'{stem}_descriptors.csv')
        ev=read(RUNS/'traces'/f'{stem}_evidence.csv')
        is_true=lambda x:float(x['gt_xy_m'])<.30
        corr=[x for x in desc if is_true(x)]
        top3={ (int(x['source']),int(x['target'])) for x in ev if int(x['rank'])<3 and is_true(x)}
        used=[x for x in desc if x['accepted']=='1']
        ii=[int(x['source']) for x in used]
        # Spatial spread is descriptive; it is not used to relabel success.
        spread=np.linalg.svd(s[ii,:2]-s[ii,:2].mean(axis=0),compute_uv=False) if len(ii)>=2 else []
        rows.append(dict(seed=int(seed),pair=r['pair'],official=int((a,b) in official),
                         n_src=len(s),n_tgt=len(t),gt_proximity_mutual_trees=len(mutual),
                         gt_proximity_pairs=len(corr),true_gate_pass=sum(x['gate_pass']=='1' for x in corr),
                         true_dc3=sum(x['gate_pass']=='1' and int(x['dc'])>=3 for x in corr),
                         true_retained_candidates=sum(x['retained_candidate']=='1' for x in corr),
                         accepted_pairs=int(r['n_matches']),true_accepted_pairs=int(r['n_correct_matches']),
                         true_top3_evidence_pairs=len(top3),best_score=float(r['best_score']),used_triangle=int(r['used_triangle']),
                         matched_xy_minor_spread_m=float(spread[1]) if len(spread)>1 else '',
                         matched_gt_xyz_median_m=float(np.median([float(x['gt_xyz_m']) for x in used])) if used else '',
                         mutual_gt_z_median_m=float(np.median([abs(mapped[i,2]-t[j,2]) for i,j in mutual])) if mutual else '',
                         diagnosis='too_few_matches' if int(r['n_matches'])<3 else 'transform_error_with_five_matches'))
    with (OUT/'failure_audit.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
    validation=dict(previous_rows_checked=len(old),instrumentation_changes_transforms_or_matches=False,
                    failed_cases=len(rows),fewer_than_three_matches=sum(r['accepted_pairs']<3 for r in rows),
                    gt_use='diagnosis only; nearby cross-scan stems are optimistic proxies, not annotated tree truth')
    (OUT/'audit_validation.json').write_text(json.dumps(validation,indent=2)+'\n')
    print(json.dumps(rows,indent=2))


if __name__=='__main__': main()
