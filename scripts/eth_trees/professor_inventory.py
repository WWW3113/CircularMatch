from pathlib import Path
import csv,json,statistics as st
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'results/professor_reports_20261026'
m=json.loads((OUT/'source_manifest.json').read_text())
def rows(sid):return list(csv.DictReader((OUT/m['sources'][sid]['snapshot']).open()))
t='''實驗盤點與數據口徑（不當作額外口述報告）

本次四次報告按方法主題分組。下列保留完成但未獨立拆篇的支線，避免只留下成功故事。數值來源ID及SHA見source_manifest.json；CSV scalar snapshots另存evidence/，不把大量原始資料放進投影Excel。

一、原論文與本地baseline
論文：DOI 10.1109/LGRS.2025.3650098。PDF p2方法、p3 Algorithm1、p4 Table I/II與Eq8、p5 Table III。
原作者未修改程式的重跑結果：待補。既有作者repo稽核只有README，不能把本地5cm/SVD叫嚴格原作重現。
本地基準：Thscore40/w10/N183、匹配0.05m、SVD；20cm分群、原DTM與額外圓柱檢查是本地實作選擇。提取voxel0.01m、法向半徑0.10m、verticality>0.9與距離step則有論文明列依據。
主要e_p是全來源點Eq8；先每個case平均，再跨case平均。finite mean含可計算失敗，缺輸出是失敗、非零誤差。共同成功另外列，避免集合變化造成錯誤歸因。

二、ETH探索，不當跨森林驗證（FS）
seed1–5各75次；既有seed6–10各75次。這是選參資料，不是最終驗收。
參數：threshold075=0.075m；threshold100=0.10m；score20/60=Thscore20/60；min_dc4=最低DC4；sectors93=N93；merge015/030=樹位合併半徑0.15/0.30m。其餘沿用control。
'''
for r in rows('FS'):t+=f"{r['method']}／{r['solver']}：開發{r['discovery_success']}/{r['discovery_trials']}；既有seed檢查{r['existing_seed_check_success']}/{r['existing_seed_check_trials']}。\n"
t+='''
三、ETH新seed11–20與提取負面結果（F/GS；第1次報告涵蓋主要證據）
這些仍是相同六站森林。vertical085只把提取verticality0.90→0.85；ransac999同時改圓柱iterations1000→3000、probability0.99→0.999；matching表示0.05→0.10m。solver分開列，不把組合效果歸因單一參數。
'''
for r in rows('GS'):
 if r['method']=='graph_mst':continue
 t+=f"{r['method']}／{r['solver']}：{r['fresh_success']}/{r['trials']}；成功e_p median={float(r['median_ep_success'])*100:.3f}cm（不是mean）。\n"
t+='\n補救開發seed11–20（FD）：\n'
for r in rows('FD'):t+=f"{r['method']}：{r['success']}/{r['cases']}；啟動{r['recovery_triggered']}、採用{r['recovery_adopted']}次。\n"
t+='''選定evidence_weak後凍結，另測21–30：固定10cm/RANSAC，142→150/150。第1次的統一主表另用31–40，三個cohort不拼合。詳細參數和新seed證據見第1次。

四、Tongji初次凍結ETH設定的遷移（第1次）
來源固定為results/tongji_trees/fixed_eth_20261025；四Plot合計400次。主要solver下5cm參考309/400、10cm但無補救304/400、10cm加補救329/400。SVD分支331/400不拿來替換事先選定的RANSAC主分支。主表U對同一樹位重算全部有限誤差，數值定義與舊summary的成功限定mean不同；不能混用兩種mean。

五、DTM（第2次）
baseline cell0.5/fit2/fill5；smooth0.5/6/6；coarse1.0/3/6；quantile1維持0.5/2/5但換第1百分位。開發只用Plot2 seed31–35，依規則選smooth。定位四Plot329→344/400，其中Plot3誤差退步。完整重提取僅Plot2，34→48/100；其餘三Plot完整新流程及ETH同組DTM對照待補。不把86%定位結果說成全流程。

六、統一9版本（U；第3、4次）
所有版本固定seed31–40、55配對；同一scanning source只算一個dataset，Tongji四Plot不是四來源。以下finite mean保留可計算失敗，不等於成功限定mean。
'''
for gid,g in sorted(m['groups'].items()):
 if not gid.startswith('U:'):continue
 s=g['stats'];t+=f"{gid}：{s['ok']}/{s['n']}；finite mean={s['ep']*100:.6f}cm，有限N={s['ep_n']}。\n"
t+='''
七、不納入兩站主比較的已完成支線
1. 六站graph_mst：150/150（GS）。使用整個六站圖，任務及可用資訊不同；不是獨立兩站baseline，也不另拆一個『超越』報告。
2. WHU-Park：不是WHU-FGI；以下是當時粗配準／密度探索與稀疏樹幹支線，不是最終v3測試。它不能替代本輪森林資料，也不能把不同配對清單的比率直接相減。
'''
for r in rows('K'):
 if r['solver']=='svd+ransac':t+=f"Park {r['preprocessing']}／{r['method']}：{r['success']}/{r['cases']}；有效變換{r['transforms_found']}。\n"
for r in rows('KS'):
 if r['solver']=='svd+ransac':t+=f"{r['group']}／{r['profile']}：{r['success']}/{r['cases']}；有效變換{r['transforms_found']}。\n"
t+='3. Liang／TreeTool處理後六樣區（L）：只完成樹位提取真值匹配，不是有GT變換的原始分站配準。六樣區等權reference-match-rate如下；單位為提取比例，不是registration成功率、也不是Eq8。\n'
for v in ['control','vertical085','ransac999']:
 rr=[r for r in rows('L') if r['variant']==v];t+=f"{v}：{st.mean(float(r['mean_reference_match_rate']) for r in rr)*100:.2f}%；6樣區各10seed。\n"
t+='''4. 早期ETH逐站distance retention：per_scan等提取摘要保存於原專案；step/linear_a/linear_mid/physical是提取保留率規則，不與本輪兩站配準e_p混算。四次主報告保持step，沒有聲稱其他保留函數已全面改進配準。

八、缺項與比較限制
原作者baseline、論文逐場景分母、論文成功門檻與OpenMP執行緒數：待補。
原論文Table I未列e_R/e_t；表格用待補保留欄位，不填0。
舊epoch硬體型號未完整記錄；R2跨批次、R4 v2/v3跨epoch，速度下降率待補。R1/R3只描述同批次的online前後代價，不拿來和論文跨硬體排名。
R4同新epoch冷啟動是每法每資料1次，不能當速度統計顯著差異。26站成本中4站重用相同設定第一筆冷啟動實測、22站新量測，不按耗時挑最佳值。
所有場景均已參與開發；原先第一次ETH→Tongji紀錄保留，最終benchmark改善不抹除資料接觸歷史。未完成的獨立森林驗證、單因素v3消融、DTM完整跨場景測試，均不補寫成既有成果。
'''
(OUT/'實驗盤點與數據口徑.txt').write_text(t,encoding='utf-8')
