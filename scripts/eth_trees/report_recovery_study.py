#!/usr/bin/env python3
"""Report one frozen comparison, including regressions, common-case errors and cost."""
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import statistics as stats

from openpyxl import Workbook
from make_simple_workbook import sheet, note
from recovery_study import ROOT, OUT, RUNS, RAW, SEEDS, PAIRS, load_frozen
from tune_registration import read_csv, write_csv


def numeric(value):
    try:
        n=float(value);return n if math.isfinite(n) else None
    except (ValueError,TypeError):return None


def main():
    frozen=load_frozen();rows=read_csv(RUNS/'validation_rows.csv')
    assert len(rows)==600 and {int(r['seed']) for r in rows}==set(SEEDS)
    assert len({(r['method'],r['solver'],r['seed'],r['pair']) for r in rows})==600
    official={tuple(s.split()) for s in (RAW/'pairs.txt').read_text().splitlines() if s.strip()}
    for r in rows:
        if r['transform_ok']=='1':assert all(numeric(r[k]) is not None for k in ['rot_err_deg','trans_err_m','e_p_m'])
        assert int(r['success'])==int(r['transform_ok']=='1' and float(r['rot_err_deg'])<2 and float(r['trans_err_m'])<.5)
    extraction_counts=[]
    for scan in [f's{i}' for i in range(1,7)]:
        path=RUNS/'validation_extraction'/scan
        previous=ROOT/'results/eth_trees/tuning_20261025/runs/extraction/control'/scan/'effective_config.ini'
        current=(path/'effective_config.ini').read_text()
        # Only seed range differs; the scanner and extraction settings stay fixed.
        normal=lambda s:re.sub(r'(?m)^seed_base\s*=.*$', 'seed_base = RANGE',s)
        assert normal(current)==normal(previous.read_text())
        counts=[len(read_csv(path/f'step_seed{seed}_trees.csv')) for seed in SEEDS]
        log=(RUNS/'logs'/f'extract_{scan}.log').read_text()
        assert counts==list(map(int,re.findall(r'trees: (\d+) ;',log)))
        extraction_counts.append(dict(scan=scan,seeds=10,mean_trees=stats.mean(counts),min_trees=min(counts),max_trees=max(counts)))
    write_csv(OUT/'validation_extraction_summary.csv',extraction_counts)
    summary=[]
    for method in ['baseline','recovery']:
        for solver in ['svd','svd+ransac']:
            rr=[r for r in rows if r['method']==method and r['solver']==solver]
            assert len(rr)==150
            oo=[r for r in rr if tuple(r['pair'].split('-')) in official]
            good=[float(r['e_p_m']) for r in rr if r['success']=='1'];ms=[float(r['time_ms']) for r in rr]
            per_seed=[sum(int(r['success']) for r in rr if int(r['seed'])==s) for s in SEEDS]
            summary.append(dict(method=method,solver=solver,cases=len(rr),success=sum(int(r['success']) for r in rr),
                                official_cases=len(oo),official_success=sum(int(r['success']) for r in oo),
                                median_success_ep_m=stats.median(good) if good else '',
                                mean_match_solve_ms=stats.mean(ms),median_match_solve_ms=stats.median(ms),
                                p95_match_solve_ms=sorted(ms)[math.ceil(.95*len(ms))-1],
                                recovery_triggered=sum(int(r.get('recovery_triggered') or 0) for r in rr),
                                recovery_adopted=sum(int(r.get('recovery_accepted') or 0) for r in rr),
                                min_success_per_seed=min(per_seed),max_success_per_seed=max(per_seed)))
    write_csv(OUT/'validation_summary.csv',summary)
    primary={method:{(int(r['seed']),r['pair']):r for r in rows if r['method']==method and r['solver']=='svd+ransac'} for method in ['baseline','recovery']}
    old,new=primary['baseline'],primary['recovery']
    rescued=[k for k in old if old[k]['success']=='0' and new[k]['success']=='1']
    regressed=[k for k in old if old[k]['success']=='1' and new[k]['success']=='0']
    common=[k for k in old if old[k]['success']==new[k]['success']=='1']
    seed_gains=[sum(int(new[s,p]['success'])-int(old[s,p]['success']) for p in [f'{a}-{b}' for a,b in PAIRS]) for s in SEEDS]
    wins=sum(d>0 for d in seed_gains);losses=sum(d<0 for d in seed_gains);n=wins+losses
    sign_p=min(1.,2*sum(math.comb(n,i) for i in range(min(wins,losses)+1))/2**n) if n else 1.
    paired=dict(primary_solver='svd+ransac',rescued_cases=[dict(seed=s,pair=p) for s,p in rescued],
                regressed_cases=[dict(seed=s,pair=p) for s,p in regressed],seed_gains=seed_gains,
                winning_seeds=wins,losing_seeds=losses,tied_seeds=10-n,paired_seed_sign_p=sign_p,
                common_success_cases=len(common),
                common_ep_baseline_m=stats.median(float(old[k]['e_p_m']) for k in common) if common else None,
                common_ep_recovery_m=stats.median(float(new[k]['e_p_m']) for k in common) if common else None,
                interpretation='one prespecified frozen primary comparison; seeds repeat the same forest, not independent forests')
    (OUT/'paired_validation.json').write_text(json.dumps(paired,indent=2)+'\n')
    pair_rows=[]
    for a,b in PAIRS:
        pair=f'{a}-{b}';os=sum(int(old[s,pair]['success']) for s in SEEDS);ns=sum(int(new[s,pair]['success']) for s in SEEDS)
        pair_rows.append(dict(pair=pair,official=int((a,b) in official),baseline_success=os,recovery_success=ns,change=ns-os))
    write_csv(OUT/'pair_validation.csv',pair_rows)
    b=next(r for r in summary if r['method']=='baseline' and r['solver']=='svd+ransac')
    r=next(r for r in summary if r['method']=='recovery' and r['solver']=='svd+ransac')
    lines=['# 兩站配準失敗分析與新seed驗證','',
           '本輪只使用當前兩站。step降採樣、clustering 0.20m、垂直度0.90、匹配閾值0.10m和配準RANSAC保持固定。新方案為選用的補救功能，原程式預設關閉；沒有加入六站圖路徑、ICP或4DOF。','',
           '## 原因與修改','',
           '舊seed11–20的8次失敗中，7次不足3對匹配；其中5次最高分已過門檻，按原流程不會觸發三角形備援。另1次有5對匹配，但變換誤差超標。GT對齊後的近鄰樹位診斷與描述子證據顯示，這些案例仍有可用的跨站候選；此診斷不是人工樹木真值，也不參與推論。','',
           '三種候選都只在舊seed上比較：三角形補救149/150；匹配少於3對時做證據一致性補救149/150；少於6對時補救150/150。舊seed已用於設計，不能再稱盲測。','',
           '凍結方案：少於6對時，收集前三個描述子候選的中心對應與維度對應；每個候選去重後投票，最多保留60條證據。由相容三元組估完整6DOF，使用同一證據池的3D殘差<0.30m與一對一支持選擇，至少6對且比原匹配多才採用。無有效補救就保留原結果。推論不讀GT。','',
           f'固定時間：{frozen["frozen_at_utc"]}；程式、可執行檔與設定的SHA256見frozen_recovery.json。之後才提取seed21–30；驗證結果出來後未重新調參。','',
           '## 新seed21–30結果','',
           '| 方法 | 全15對×10seed | 官方10對×10seed | 成功e_p中位數m | 平均匹配＋求解ms |',
           '|---|---:|---:|---:|---:|']
    for x in summary:
        lines.append(f'| {x["method"]}/{x["solver"]} | {x["success"]}/150 | {x["official_success"]}/100 | {x["median_success_ep_m"]:.4f} | {x["mean_match_solve_ms"]:.3f} |')
    lines += ['',f'主要比較（配準RANSAC）：補回{len(rescued)}次，退步{len(regressed)}次；每seed勝/負/同={wins}/{losses}/{10-n}。每seed成功對數符號檢定p={sign_p:.4g}，僅描述本森林的亂數敏感度。',
              f'共同成功的{len(common)}個案例，e_p中位數：baseline={paired["common_ep_baseline_m"]:.4f}m，recovery={paired["common_ep_recovery_m"]:.4f}m。整體成功集合不同時，不能只憑成功案例誤差推論精度改善。',
              f'補救觸發{r["recovery_triggered"]}/150次，採用{r["recovery_adopted"]}/150次。計時只含描述子、匹配與求解，不含提取及完整點雲誤差；主機有並行工作，非獨占效能基準。','',
              '## 如何解讀','',
              '成功標準仍為旋轉<2°且平移<0.5m；原論文未明確給定成功門檻，ETH配對集合也未完全確認。即使本輪達100%，仍是加入新補救策略後、在我們成功定義下的結果，不能稱為原論文100%的嚴格重現。','',
              'seed21–30仍來自同一片ETH森林，不是另一資料集。Liang處理版沒有原始站點與配準真值，本輪無法據此證明跨森林配準泛化。','',
              '## 驗證與重現','',
              '81項C++測試通過，含6項新補救測試：預設不變、足夠匹配時不啟動、最高分已過門檻仍可補救、真實6DOF、拒絕僅XY相符而Z錯誤、支持不足保留原結果。舊300列變換/匹配已比對；兩個實際案例把評分GT平移100m後，推論結果與支持數不變。','',
              '先執行recovery_study.py discover產生舊seed候選比較，再執行audit_pair_failures.py與recovery_study.py freeze、extract、validate，最後執行report_recovery_study.py。資料及執行日誌保留在忽略的runs/；小型摘要與試算表由Git保存。']
    lines += ['', '## 試算表', '',
              '[完整版 comparison_full.xlsx](comparison_full.xlsx)：歷次比較、本輪全部逐次結果、變換、診斷與驗證紀錄。',
              '[精簡版 comparison_simple.xlsx](comparison_simple.xlsx)：改動重點、各配對改善與主要逐次比較。']
    (OUT/'report.md').write_text('\n'.join(lines)+'\n')
    wb=Workbook();wb.remove(wb.active)
    overview=[['使用資料','每次只用兩站','每次只用兩站','沒有使用六站資訊'],
              ['距離降採樣','step：50%／75%／100%','相同','本輪沒有改降採樣'],
              ['匹配設定','10cm＋配準RANSAC','相同','保留上一輪推薦組'],
              ['新增步驟','最高分不足才做三角形備援','匹配少於6對時，額外找3D一致的候選','至少6對支持，且比原匹配多才採用'],
              ['全15對成功率',f'{b["success"]}/150（{b["success"]/150:.1%}）',f'{r["success"]}/150（{r["success"]/150:.1%}）',f'補回{len(rescued)}次；退步{len(regressed)}次'],
              ['官方10對成功率',f'{b["official_success"]}/100',f'{r["official_success"]}/100','同樣使用seed21–30'],
              ['共同成功案例的點雲誤差中位數',f'{paired["common_ep_baseline_m"]*100:.2f} cm',f'{paired["common_ep_recovery_m"]*100:.2f} cm','比較同一批成功案例，越低越好'],
              ['匹配＋求解平均時間',f'{b["mean_match_solve_ms"]:.2f} ms',f'{r["mean_match_solve_ms"]:.2f} ms','不含提取；並行主機計時僅供參考']]
    ws=sheet(wb,'重點與改動','本表只看未參與設計的新seed21–30。舊seed用來選方案，兩者分開；成功定義與上一輪相同。',
             ['比較項目','原本推薦組','加上補救後','怎麼解讀'],overview,[33,43,49,49])
    for i in range(5,13):ws.row_dimensions[i].height=48
    note(ws,'結論：'+('新seed的成功次數增加。' if r['success']>b['success'] else '新seed未顯示成功次數增加。')+'這是同一森林的亂數驗證；尚未證明跨森林泛化，也不是原論文100%的嚴格重現。')
    ws=sheet(wb,'哪些配對改善','每個配對都試10次，最高10次成功。「官方」指ETH下載資料內的pairs.txt，不代表已確認作者使用同一列表。',
             ['配對','官方列表','原本成功次數／10','補救後成功次數／10','多成功幾次','變好或變壞'],
             [[x['pair'],'是' if x['official'] else '否',x['baseline_success'],x['recovery_success'],x['change'],
               '變好' if x['change']>0 else ('變壞' if x['change']<0 else '相同')] for x in pair_rows],[19,17,28,30,22,23])
    for i in range(5,20):ws.row_dimensions[i].height=24
    detail=[]
    for s in SEEDS:
        for a,t in PAIRS:
            p=f'{a}-{t}';x,y=old[s,p],new[s,p]
            detail.append([s,p,'成功' if x['success']=='1' else '失敗','成功' if y['success']=='1' else '失敗',
                           numeric(x['rot_err_deg']),numeric(y['rot_err_deg']),numeric(x['trans_err_m']),numeric(y['trans_err_m']),
                           '是' if y['recovery_accepted']=='1' else '否'])
    ws=sheet(wb,'逐次結果','成功＝旋轉誤差<2度且平移誤差<0.5公尺。空白誤差代表沒有求出變換，不是零誤差。',
             ['seed','配對','原本結果','補救後結果','原本旋轉誤差°','補救旋轉誤差°','原本平移誤差m','補救平移誤差m','採用補救'],detail,[10,16,16,18,23,23,24,24,18])
    for row in ws.iter_rows(min_row=5):
        ws.row_dimensions[row[0].row].height=22
        for c in row[4:8]:c.number_format='0.000'
    ws.page_setup.fitToHeight=0
    target=OUT/'comparison_simple.xlsx';wb.save(target)
    checks=dict(validation_rows=600,primary_cases_per_method=150,validation_extractions_checked=60,
                extraction_settings_unchanged_except_seed=True,full_source_cloud_ep=True,success_threshold_unchanged=True,
                inference_sha256_unchanged_since_freeze=True,cpp_tests=81,workbook_sha256=hashlib.sha256(target.read_bytes()).hexdigest())
    (OUT/'validation.json').write_text(json.dumps(checks,indent=2)+'\n')
    print(json.dumps(summary,indent=2));print(target)


if __name__=='__main__':main()
