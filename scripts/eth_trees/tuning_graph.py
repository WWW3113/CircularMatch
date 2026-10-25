"""Experimental multiview route and reporting, independent of core defaults.

Forest construction receives no ground truth. GT is read only for final errors.
All transformations map source coordinates to target coordinates, in full 6DOF.
"""
import csv
import itertools
import json
import math
from pathlib import Path
import statistics as stats

import numpy as np


def read_csv(path):
    with Path(path).open() as f: return list(csv.DictReader(f))


def write_csv(path, rows):
    keys=list(dict.fromkeys(k for r in rows for k in r))
    with Path(path).open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerows(rows)


def matrix(row):
    t=np.eye(4)
    t[:3,:3]=[[float(row[f'r{i}{j}']) for j in range(3)] for i in range(3)]
    t[:3,3]=[float(row[f't{i}']) for i in range(3)]
    return t


def quality(transform, source, target, matches):
    """GT-free quality: mutual 3D neighbours, support and median residual."""
    empty=dict(score=0.,support=0,overlap=0.,median_residual=None)
    if matches < 4 or len(source)==0 or len(target)==0:return empty
    moved=source@transform[:3,:3].T+transform[:3,3]
    distance=np.linalg.norm(moved[:,None,:]-target[None,:,:],axis=2)
    st=distance.argmin(axis=1);ts=distance.argmin(axis=0)
    residual=np.array([distance[i,j] for i,j in enumerate(st) if ts[j]==i and distance[i,j]<.30])
    if len(residual)==0:return empty
    median=float(np.median(residual));support=len(residual);overlap=support/min(len(source),len(target))
    score=support*overlap/(1+median/.10) if support>=6 and median<=.15 else 0.
    return dict(score=score,support=support,overlap=overlap,median_residual=median)


def infer_forest(nodes, edges):
    """Maximum spanning forest and unique composed paths; no GT argument."""
    parent={n:n for n in nodes};adj={n:[] for n in nodes};selected=[]
    def find(n):
        while parent[n]!=n:n=parent[n]
        return n
    for edge in sorted(edges,key=lambda e:(-e['score'],e['a'],e['b'])):
        a,b=edge['a'],edge['b']
        if edge['score']<=0 or find(a)==find(b):continue
        parent[find(a)]=find(b);selected.append(edge)
        adj[a].append((b,edge['T']));adj[b].append((a,np.linalg.inv(edge['T'])))
    transforms={};paths={}
    for source in nodes:
        todo=[(source,np.eye(4),[source])];seen={source}
        while todo:
            current,t,path=todo.pop()
            if current!=source:transforms[source,current]=t;paths[source,current]=path
            for target,step in adj[current]:
                if target not in seen:
                    seen.add(target);todo.append((target,step@t,path+[target]))
    return transforms,paths,selected


def errors(estimate, truth):
    c=np.clip((np.trace(truth[:3,:3].T@estimate[:3,:3])-1)/2,-1,1)
    rotation=math.degrees(math.acos(float(c)))
    translation=float(np.linalg.norm(estimate[:3,3]-truth[:3,3]))
    return rotation,translation


def ply_map(path):
    """Map scalar little-endian vertex data used by the verified ETH files."""
    types={'float':'<f4','float32':'<f4','double':'<f8','float64':'<f8',
           'char':'i1','uchar':'u1','int8':'i1','uint8':'u1','short':'<i2',
           'ushort':'<u2','int':'<i4','uint':'<u4'}
    with Path(path).open('rb') as f:
        assert f.readline().strip()==b'ply';props=[];count=None;vertex=False;first=True;fmt=None
        while True:
            line=f.readline()
            if not line:raise ValueError('truncated PLY header')
            p=line.decode('ascii').split()
            if not p:continue
            if p[0]=='format':fmt=p[1]
            if p[0]=='element':
                if first and p[1]!='vertex':raise ValueError('vertices must be first')
                first=False;vertex=p[1]=='vertex'
                if vertex:count=int(p[2])
            if p[0]=='property' and vertex:
                if p[1] not in types:raise ValueError('unsupported vertex property')
                props.append((p[2],types[p[1]]))
            if p[0]=='end_header':break
        assert fmt=='binary_little_endian' and count is not None
        dtype=np.dtype(props);assert all(k in dtype.names for k in ['x','y','z'])
        assert Path(path).stat().st_size>=f.tell()+count*dtype.itemsize
        return np.memmap(path,dtype=dtype,mode='r',offset=f.tell(),shape=(count,))


def point_errors(rows, raw):
    """Exact paper-style mean point error over EVERY source point, in chunks."""
    for source in sorted({r['pair'].split('-')[0] for r in rows}):
        selected=[r for r in rows if r['pair'].split('-')[0]==source and r['transform_ok']=='1']
        if not selected:continue
        cloud=ply_map(raw/f'{source}.ply');deltas=[]
        for row in selected:
            truth=np.loadtxt(raw/'groundtruth'/f'{row["pair"]}.tfm')
            t=matrix(row);deltas.append((t[:3,:3]-truth[:3,:3],t[:3,3]-truth[:3,3]))
        sums=np.zeros(len(selected))
        for start in range(0,len(cloud),262144):
            block=cloud[start:start+262144]
            xyz=np.column_stack([block[k] for k in ['x','y','z']]).astype(np.float64)
            for i,(r,t) in enumerate(deltas):
                diff=xyz@r.T+t;sums[i]+=np.sqrt(np.einsum('ij,ij->i',diff,diff)).sum()
        for row,total in zip(selected,sums):row['e_p_m']=float(total/len(cloud))
        print('FULL CLOUD ERRORS',source,len(selected),len(cloud),flush=True)


def run_graph(root, out, source_rows=None):
    runs=out/'runs';raw=root/'data/eth_trees/raw/trees'
    fresh=read_csv(runs/'fresh_rows.csv') if source_rows is None else source_rows
    nodes=[f's{i}' for i in range(1,7)];pairs=list(itertools.combinations(nodes,2));rows=[];edge_rows=[]
    for seed in range(11,21):
        observations=[r for r in fresh if r['method']=='fresh_control' and r['solver']=='svd' and int(r['seed'])==seed]
        points={s:np.array([[float(r[k]) for k in ['x','y','z']] for r in read_csv(runs/'extraction/control'/s/f'step_seed{seed}_trees.csv')]).reshape(-1,3) for s in nodes}
        edges=[]
        for row in observations:
            if row['transform_ok']!='1':continue
            a,b=row['pair'].split('-');t=matrix(row);q=quality(t,points[a],points[b],int(row['n_matches']))
            edges.append(dict(a=a,b=b,T=t,**q))
        transforms,paths,chosen=infer_forest(nodes,edges)
        selected={(e['a'],e['b']) for e in chosen}
        for edge in edges:edge_rows.append(dict(seed=seed,pair=f'{edge["a"]}-{edge["b"]}',selected=int((edge['a'],edge['b']) in selected),**{k:edge[k] for k in ['score','support','overlap','median_residual']}))
        # Inference above is complete before any ground truth is loaded below.
        for a,b in pairs:
            row=dict(method='graph_mst',seed=seed,pair=f'{a}-{b}',solver='graph_mst',threshold=.05,
                     transform_ok='0',success=0,rot_err_deg='',trans_err_m='',e_p_m='',path='')
            if (a,b) in transforms:
                t=transforms[a,b];truth=np.loadtxt(raw/'groundtruth'/f'{a}-{b}.tfm');rotation,translation=errors(t,truth)
                row.update(transform_ok='1',success=int(rotation<2 and translation<.5),rot_err_deg=rotation,
                           trans_err_m=translation,path='>'.join(paths[a,b]))
                row.update({f'r{i}{j}':float(t[i,j]) for i in range(3) for j in range(3)})
                row.update({f't{i}':float(t[i,3]) for i in range(3)})
            rows.append(row)
    # Also verify this NumPy evaluator against one existing C++ full-cloud error.
    reference=next(dict(r) for r in fresh if r['method']=='fresh_control' and r['transform_ok']=='1')
    expected=float(reference['e_p_m']);reference['method']='ep_reference'
    point_errors(rows+[reference],raw)
    assert abs(float(reference['e_p_m'])-expected)<1e-6,(reference['e_p_m'],expected)
    write_csv(runs/'graph_rows.csv',rows);write_csv(runs/'graph_edges.csv',edge_rows)
    (out/'graph_validation.json').write_text(json.dumps(dict(full_source_cloud_ep=True,cpp_ep_reference_error=abs(reference['e_p_m']-expected),inference_uses_ground_truth=False),indent=2)+'\n')


def sign_p(wins,losses):
    n=wins+losses
    return min(1.,2*sum(math.comb(n,k) for k in range(min(wins,losses)+1))/2**n) if n else 1.


def report(root, out):
    runs=out/'runs';fresh=read_csv(runs/'fresh_rows.csv');graph=read_csv(runs/'graph_rows.csv');rows=fresh+graph
    official={tuple(line.split()) for line in (root/'data/eth_trees/raw/trees/pairs.txt').read_text().splitlines() if line.strip()}
    control=[r for r in fresh if r['method']=='fresh_control' and r['solver']=='svd']
    frozen=json.loads((out/'frozen_matching.json').read_text());summary=[]
    for method,solver in list(dict.fromkeys((r['method'],r['solver']) for r in rows)):
        selected=[r for r in rows if r['method']==method and r['solver']==solver];assert len(selected)==150
        counts=[sum(int(r['success']) for r in selected if int(r['seed'])==seed) for seed in range(11,21)]
        base=[sum(int(r['success']) for r in control if int(r['seed'])==seed) for seed in range(11,21)]
        counts10=[sum(int(r['success']) for r in selected if int(r['seed'])==seed and tuple(r['pair'].split('-')) in official) for seed in range(11,21)]
        ep=[float(r['e_p_m']) for r in selected if r['success']=='1']
        wins=sum(a>b for a,b in zip(counts,base));losses=sum(a<b for a,b in zip(counts,base))
        baseline={(r['seed'],r['pair']):r for r in control}
        common=[(r,baseline[r['seed'],r['pair']]) for r in selected
                if r['success']=='1' and baseline[r['seed'],r['pair']]['success']=='1']
        summary.append(dict(method=method,solver=solver,fresh_success=sum(counts),trials=150,official_success=sum(counts10),official_trials=100,
                            mean15=stats.mean(counts),sd15=stats.stdev(counts),min15=min(counts),max15=max(counts),
                            median_ep_success=stats.median(ep) if ep else '',mean_seed_gain=stats.mean(a-b for a,b in zip(counts,base)),
                            wins=wins,losses=losses,ties=10-wins-losses,p_seed_sign=sign_p(wins,losses),
                            common_success_cases=len(common),
                            common_ep_control=stats.median(float(b['e_p_m']) for _,b in common) if common else '',
                            common_ep_method=stats.median(float(a['e_p_m']) for a,_ in common) if common else ''))
    # Exploratory family: every reported non-control method/solver comparison.
    comparisons=[r for r in summary if (r['method'],r['solver'])!=('fresh_control','svd')]
    for r in summary:r['p_bonferroni']=min(1.,r['p_seed_sign']*len(comparisons))
    write_csv(out/'fresh_summary.csv',summary)
    L=['# ETH Trees 改進路徑比較','',
       '固定原本最佳擷取設定（improved、clustering 20 cm、step）；原始程式預設值保持不變。',
       '參數在既有 seed 1–5 上篩選，6–10 只作穩定性檢查；下表使用新擷取的 seed 11–20。所有資料仍來自同一片 ETH 森林，不能當成獨立森林驗證。',
       '成功標準固定為旋轉誤差 < 2° 且平移誤差 < 0.5 m。每個 seed 為 15 個單向配對，另列官方 10 對；原始逐對數據含失敗案例。',
       f'配準參數固定選擇：`{frozen["profile"]}`，solver=`{frozen["solver"]}`；在看新 seed 結果以前已保存於 frozen_matching.json。','',
       '| 路徑 | solver | 官方10對 | 全15對 | 每seed成功對數 | Δ對數/seed | 成功案例 e_p 中位數(m) | seed勝/負/同 | 校正p |',
       '|---|---|---:|---:|---:|---:|---:|---:|---:|']
    for r in summary:
        ep=f'{r["median_ep_success"]:.4f}' if r['median_ep_success']!='' else '–'
        L.append(f'| {r["method"]} | {r["solver"]} | {r["official_success"]}% | {r["fresh_success"]/150:.1%} | {r["mean15"]:.1f} ± {r["sd15"]:.1f} | {r["mean_seed_gain"]:+.1f} | {ep} | {r["wins"]}/{r["losses"]}/{r["ties"]} | {r["p_bonferroni"]:.4f} |')
    L+=['','## 共同成功案例的精度','',
        '只比較此方法與control SVD都成功的相同(seed,pair)，避免將不同成功集合的中位數當成精度改善。','',
        '| 方法 | solver | 共同成功筆數 | control e_p中位數(m) | 此方法e_p中位數(m) |','|---|---|---:|---:|---:|']
    for r in summary:
        base_ep=f'{r["common_ep_control"]:.4f}' if r['common_ep_control']!='' else '–'
        method_ep=f'{r["common_ep_method"]:.4f}' if r['common_ep_method']!='' else '–'
        L.append(f'| {r["method"]} | {r["solver"]} | {r["common_success_cases"]} | {base_ep} | {method_ep} |')
    L+=['','## 既有 seed 的篩選結果','',
        '選擇只依seed 1–5；6–10沒有參與本次參數選擇。原本20 cm設定已在ETH上挑選，因此兩組都不能稱為獨立森林測試。','',
        '| 設定 | solver | seed1–5 成功/75 | seed6–10 成功/75 |','|---|---|---:|---:|']
    for r in read_csv(out/'screen_summary.csv'):
        L.append(f'| {r["method"]} | {r["solver"]} | {r["discovery_success"]} | {r["existing_seed_check_success"]} |')
    L+=['','## 路徑差異','',
        '- `fresh_control`：新 seed 的原本最佳設定，5 cm；同時列 SVD 與 SVD+RANSAC，供既有 solver 對照。',
        '- `fresh_matching`：只修改配準參數；10 cm 等非 5 cm 設定皆是我們的敏感度實驗，不是論文原設定。',
        '- `fresh_vertical085`：verticality 0.90 → 0.85；增加可見樹幹點，仍用 5 cm 配準。',
        '- `fresh_ransac999`：圓柱擬合迭代上限 1000 → 3000，成功機率 0.99 → 0.999；仍用 5 cm 配準。',
        '- 名稱後綴 `_matching`：擷取變體加上預先固定的配準參數。這兩個組合在新seed配準前即列入計畫，檢查擷取與匹配改善能否相加。',
        '- `graph_mst`：六站多視角流程。從 control 的兩站 SVD 建邊，只用估計變換後的三維互為最近鄰與匹配數決定品質；至少4個CN匹配、6個3D支持點，支持點距離 < 0.30 m、中位殘差 ≤ 0.15 m。建立最大生成森林，再沿唯一路徑串接完整6DOF變換。無路徑即失敗。選邊不讀GT。',
        '- 圖路徑依賴中間站，與獨立兩站配準的輸入條件不同，不能將其數字直接當作超越論文的證據。沒有使用4DOF或ICP。',
        '- e_p 均使用 source 點雲的所有點；圖路徑以分塊 NumPy 計算，已與同一筆 C++ 結果比對。中位數只對成功案例，會有成功集合不同的影響。',
        '- seed勝/負/同：比較每個seed成功對數與control SVD；符號檢定以10個seed為單位，Bonferroni涵蓋表中所有非control比較。這只反映本場景的亂數敏感度，不提供跨森林的不確定性。','',
        '## 失敗配對','', '| pair | '+' | '.join(r['method']+'/'+r['solver'] for r in summary)+' |',
        '|---|'+'---|'*len(summary)]
    for a,b in itertools.combinations([f's{i}' for i in range(1,7)],2):
        pair=f'{a}-{b}';cells=[]
        for r in summary:cells.append(str(sum(int(x['success']) for x in rows if x['method']==r['method'] and x['solver']==r['solver'] and x['pair']==pair))+'/10')
        L.append('| '+pair+' | '+' | '.join(cells)+' |')
    L+=['','## 下一步判準','',
        '若多站圖路徑有改善，適合以整片森林的多站配準為產品目標；若必須單獨兩站完成，優先獨立資料驗證固定的配準參數。擷取閾值與RANSAC資源是否值得改動，應同時看成功率、時間與新增樹位，不只挑最好seed。',
        '目前仍需要 Tongji-Trees 等另一片森林作外部驗證；本輪沒有根據新seed結果反覆再調參。','',
        '重現：依序執行 `python3 scripts/eth_trees/tune_registration.py screen|extract|fresh|graph|report`（每次一個phase，graph使用OPENBLAS_NUM_THREADS=1）。C++工具由CM_TOOLCHAIN_WRAPPER啟用；大型中間檔在git忽略的runs/，小型計畫與結論保留在本目錄。']
    (out/'report.md').write_text('\n'.join(L)+'\n')
    plot_summary(out, summary, rows, frozen)
    print('\n'.join(L[:22]))


def plot_summary(out, summary, rows, frozen):
    import os
    os.environ.setdefault('MPLCONFIGDIR',str(out/'runs/mplconfig'))
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    keys=[('fresh_control','svd'),('fresh_control','svd+ransac'),
          ('fresh_matching',frozen['solver']),('fresh_vertical085','svd'),
          ('fresh_ransac999','svd'),('fresh_vertical085_matching',frozen['solver']),
          ('fresh_ransac999_matching',frozen['solver']),('graph_mst','graph_mst')]
    labels=['Control: 5 cm + SVD','5 cm + registration RANSAC',
            f'Selected: {frozen["profile"]["threshold"]*100:g} cm + {frozen["solver"]}',
            'Verticality 0.85','Cylinder fitting p=0.999','Verticality + selected matching',
            'Cylinder effort + selected matching','MULTIVIEW: six-scan spanning tree']
    selected=[next(r for r in summary if (r['method'],r['solver'])==k) for k in keys]
    fig,ax=plt.subplots(figsize=(11,5.6));y=np.arange(len(keys));height=.34
    ax.barh(y-height/2,[r['official_success'] for r in selected],height,label='Official 10 pairs')
    ax.barh(y+height/2,[100*r['fresh_success']/150 for r in selected],height,label='All 15 pairs')
    ax.set_yticks(y,labels);ax.invert_yaxis();ax.set_xlim(0,105)
    ax.set_xlabel('Success rate (%), fresh seeds 11-20');ax.axhline(len(keys)-1.5,color='black',ls='--',lw=1)
    ax.set_title('ETH parameter study: pairwise routes and a separate multiview route')
    ax.legend(loc='lower right');ax.grid(axis='x',alpha=.2);fig.tight_layout()
    fig.savefig(out/'success_rates.png',dpi=160);plt.close(fig)
    pairs=[f'{a}-{b}' for a,b in itertools.combinations([f's{i}' for i in range(1,7)],2)]
    values=np.array([[sum(int(r['success']) for r in rows if r['method']==m and r['solver']==s and r['pair']==p) for m,s in keys] for p in pairs])
    fig,ax=plt.subplots(figsize=(11,7.5));im=ax.imshow(values,vmin=0,vmax=10,cmap='YlGnBu',aspect='auto')
    ax.set_yticks(range(15),pairs);ax.set_xticks(range(len(keys)),labels,rotation=25,ha='right')
    for i in range(15):
        for j in range(len(keys)):ax.text(j,i,str(values[i,j]),ha='center',va='center',color='white' if values[i,j]>=7 else 'black')
    ax.axvline(len(keys)-1.5,color='black',ls='--');ax.set_title('Successful seeds per pair (out of 10); last column uses all six scans')
    fig.colorbar(im,ax=ax,label='Successful seeds');fig.tight_layout();fig.savefig(out/'pair_success.png',dpi=160);plt.close(fig)
    with (out/'report.md').open('a') as f:f.write('\n## 圖表\n\n![Success rates](success_rates.png)\n\n![Pair success](pair_success.png)\n')
