#!/usr/bin/env python3
"""Finish, independently reaggregate/recalculate, and package the 13 requested files."""
from pathlib import Path
import csv,json,hashlib,math,subprocess,sys,zipfile,re
from datetime import datetime
from zoneinfo import ZoneInfo
from openpyxl import load_workbook
from make_six_week_reports import ROOT,OUT,TITLES,validate,report1,later

OVERVIEW='''六次教授報告安排總覽
每次預設10～15分鐘：主稿配合表格、圖表停頓與備答。每次各一份TXT與XLSX，共12份，另加本總覽＝13份；ZIP只放這13份。

統一資料口徑
Tongji-trees的Plot1～4是同一dataset四個場景，每場景五站、10個固定兩站配對。ETH-tree是另一dataset，六站、15個固定配對。每組使用seed31～40，前後同pair同seed；兩dataset不混算誤差平均。不是六站一起推論，也不是550片森林。
第一週舊ETH 118/123/135/142為seed11～20。本次為跨週對齊，補跑31～40四組600筆，為114/121/138/144；舊批完整保留，沒有選最好seed。第二週主比較因此是ETH144→150；舊21～30的142→150只作歷史補充。
本次另補ETH固定軸線DTM定位，以及ETH與Tongji Plot1/3/4的完整smooth提取與配準；Plot2既有完整提取與新流程共用同設定，不重挑結果。

第1次｜CN距離容差與Solver
只比較5cm/10cm×SVD/RANSAC四組；新增弱配對補救全部關閉。用兩dataset檢查簡單參數是否一致有效。ETH改善，但Tongji Plot4放寬門檻退步；不是所有森林都變好。

第2次｜弱配對補救
承接第1次共用10cm/RANSAC配置，對同配對與seed新增evidence recovery，其他條件固定。將此模組獨立說明，不放第一週。核心是CN證據回收、相容三元組、Kabsch與幾何共識；不是宣稱全新發明RANSAC。

第3次｜DTM平滑與樹位高度
前一次原DTM＋10cm/RANSAC/recovery，對照凍結smooth profile。主表使用兩dataset完整重提取，另列固定軸線只重算交點的控制，避免混稱。profile曾以Plot2開發GT選出；本次不為各場景重選，改善與退步全列。

第4次｜加入幾何微調
主表直接列第3次smooth完整流程，對照回復原DTM＋新增surface refinement。跨週同時改兩項，不把全部差異歸給ICP；另外列同一原DTM的raw/refined控制隔離後端整組。方法仍完整6DOF，原提取1cm與新表面0.5/0.2/0.1m分開。

第5次｜擴大候選搜尋的退步
主表承接第4次單初值refinement，對照多候選v2；另展開v1→v2。Plot4 v1 96→v2 78/100，保留這個負面結果。搜尋範圍、yaw初值、支持分數與beam一起改，不將效果獨立歸給top-k。

第6次｜結構驗證與最終比較
保留v2候選引擎，改用垂直表面驗證與收緊gate整組。主表v2→v3，另列同backend公平控制，整理最終與論文數值比較；Plot3小幅退步與Plot4共同成功子集的小幅退步也保留。缺單項消融，不說某個門檻單獨救回22次。

每份XLSX的讀法
第1次三頁為四組結果、設定與說明、精度時間與成對比較；後續三頁為結果比較、設定與說明、必要對照。每次都列論文報告值、本地5cm參考、前一次與本次；重要變數用少數幾個組合呈現，不列26項內部常數。
e_p用全部來源點Eq8，再跨案例取mean；finite mean包含可計算失敗，缺變換不填0且仍算失敗。共同成功另列，不能取代總成功率。誤差／時間下降率=(前−後)/前，成功率差用pp。有限案例集合或計時條件不同時，下降率留待補：這是不可比，不是缺實驗，不能用反覆重跑把失敗補成成功。

論文與真值界線
原作者repo在本次重新查核仍只有README，沒有可執行程式，因此同環境原作者baseline無法補跑；只能列論文Table I/II/III報告值與本地參考。論文分母、成功門檻、實際threads未齊，不宣稱嚴格超越原作者程式或公平速度排名。
推論不讀GT選候選／輸出，但離線開發用GT評分選過設定；兩dataset都已接觸。最終是已知benchmark的改善，不稱未見森林盲測。若要求任何開發階段都不能GT選參，現有研究不滿足強版本限制。
原始私人點雲、樹位座標與私人Drive識別碼不放入交付ZIP。數字來源與SHA列在Excel/TXT，詳細selector、CSV行號與驗證manifest另存專案同目錄。
'''

def main():
 OUT.mkdir(exist_ok=True,parents=True)
 assert (ROOT/'results/report_experiments_20261028/full_dtm_smooth_rows.csv').exists(),'Resume missing experiments first'
 # Count features without exporting any private tree coordinates.
 feature_rows=[];manifest=json.loads((ROOT/'results/unified_20261025/dataset_manifest.json').read_text())
 for scan in manifest['scans']:
  base=ROOT/('results/unified_20261025/runs/extraction/ETH/'+scan['scan'] if scan['dataset']=='ETH' else 'results/tongji_trees/fixed_eth_20261025/runs/extraction/'+scan['scene']+'/'+scan['scan'])
  full=ROOT/('results/tongji_trees/dtm_height_20261025/runs/full_extraction/Plot2/'+scan['scan'] if scan['scene']=='Plot2' else 'results/report_experiments_20261028/runs/full_extraction/'+scan['scene']+'/'+scan['scan'])
  for seed in range(31,41):
   before=base/f'step_seed{seed}_trees.csv';after=full/f'step_seed{seed}_trees.csv'
   feature_rows.append(dict(scene=scan['scene'],scan=scan['scan'],seed=seed,before_trees=len(list(csv.DictReader(before.open()))),after_trees=len(list(csv.DictReader(after.open()))),before_sha256=hashlib.sha256(before.read_bytes()).hexdigest(),after_sha256=hashlib.sha256(after.read_bytes()).hexdigest()))
 f=ROOT/'results/report_experiments_20261028/feature_count_rows.csv'
 with f.open('w',newline='') as stream:w=csv.DictWriter(stream,fieldnames=list(feature_rows[0]));w.writeheader();w.writerows(feature_rows)
 report1()
 for i in range(2,7):later(i)
 overview=OUT/'報告安排總覽.txt';overview.write_text(OVERVIEW,encoding='utf-8')
 run=OUT/'runs/recalculated';run.mkdir(exist_ok=True,parents=True)
 results=[];source_tables={};groups=0;checks=0
 for i in range(1,7):
  m=json.loads((OUT/f'report_{i:02d}_manifest.json').read_text());f=OUT/m['xlsx']
  for sid,s in m['sources'].items():
   p=ROOT/s['path'];assert hashlib.sha256(p.read_bytes()).hexdigest()==s['sha256'];source_tables[sid,s['path']]=list(csv.DictReader(p.open()))
  for gid,g in m['groups'].items():
   data=source_tables[g['source'],m['sources'][g['source']]['path']];rr=[data[n-2] for n in g['csv_lines']]
   assert len(rr)==g['n'] and len({(r.get('scene') or r.get('plot') or 'ETH',r['pair'],r['seed']) for r in rr})==len(rr)
   assert sum(r['success']=='1' for r in rr)==g['ok'];v=[float(r['e_p_m']) for r in rr if math.isfinite(float(r['e_p_m']))]
   assert len(v)==g['finite'];assert not v or math.isclose(sum(v)/len(v),g['ep'],rel_tol=1e-11,abs_tol=1e-10)
   groups+=1
  proc=subprocess.run(['soffice','--headless','--convert-to','xlsx','--outdir',str(run),str(f)],cwd=ROOT,capture_output=True,text=True,timeout=120);assert proc.returncode==0,proc.stderr
  validate(run/f.name,m);validate(f,m);checks+=len(m['checks'])
  wb=load_workbook(f,data_only=False);formulas=sum(c.data_type=='f' for ws in wb for row in ws for c in row);wb.close()
  text=(OUT/m['txt']).read_text();assert all(x in text for x in ['Tongji','ETH','GT','論文','問：','答：','修改'])
  if i>1:assert '前一次' in text or '上一週' in text
  for x in ['drive.google.com','1F9v5CyDlGkN1PRMipLdJzpuA4x1pAO_l','access_token','refresh_token']:assert x not in text
  results.append(dict(report=i,xlsx=m['xlsx'],txt=m['txt'],xlsx_sha256=m['xlsx_sha256'],txt_sha256=m['txt_sha256'],checks=len(m['checks']),formulas=formulas,charts=m['charts'],spoken_cjk=len(re.findall('[\u3400-\u9fff]',text.split('【教授追問備答】')[0]))))
 files=[overview]+[OUT/r[k] for r in results for k in ['txt','xlsx']];assert len(files)==13
 zip_path=OUT/'six_week_reports_20261028.zip'
 with zipfile.ZipFile(zip_path,'w',zipfile.ZIP_DEFLATED) as z:
  for f in files:z.write(f,f.name)
 with zipfile.ZipFile(zip_path) as z:assert len(z.namelist())==13 and z.testzip() is None
 validation=dict(validated_at=datetime.now(ZoneInfo('Asia/Taipei')).isoformat(),reports=results,distributed_files=13,zip_sha256=hashlib.sha256(zip_path.read_bytes()).hexdigest(),zip_bytes=zip_path.stat().st_size,independent_groups_reaggregated=groups,cell_checks=checks,libreoffice_independent_recalculation_passed=True,no_excel_error_cells=True,no_raw_private_clouds_in_bundle=True)
 (OUT/'validation.json').write_text(json.dumps(validation,ensure_ascii=False,indent=2)+'\n');print(json.dumps(validation,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
