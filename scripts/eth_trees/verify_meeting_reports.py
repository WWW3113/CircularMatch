#!/usr/bin/env python3
"""Independent CSV aggregation, Calc recalculation, and projection inspection."""
from pathlib import Path
from datetime import datetime
from zoneinfo import ZoneInfo
import argparse,csv,hashlib,json,math,statistics,subprocess,zipfile
from openpyxl import load_workbook
import fitz
from make_meeting_reports import ROOT,OUT,TITLES,validate

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def close(a,b):assert math.isclose(a,b,rel_tol=1e-10,abs_tol=1e-9),(a,b)
def verify(weeks):
 previous=OUT/'validation.json'
 prior={r['file']:r['sha256'] for r in json.loads(previous.read_text()).get('reports',[])} if previous.exists() else {}
 manifest=json.loads((ROOT/'results/unified_20261025/dataset_manifest.json').read_text())
 recalculated=OUT/'runs/recalculated';recalculated.mkdir(parents=True,exist_ok=True)
 previews=OUT/'runs/previews';previews.mkdir(parents=True,exist_ok=True)
 all_meta=[];source_cache={};group_checks=0
 for i in weeks:
  m=json.loads((OUT/f'audit/week_{i:02d}.json').read_text());p=OUT/m['file'];assert sha(p)==m['sha256'];validate(p,m)
  wb=load_workbook(p,data_only=True)
  assert not any('/550' in str(c.value) for s in wb for row in s for c in row)
  loaded=[]
  for group in m['groups']:
   by_scene={}
   for scene,g in group['scenes'].items():
    s=g['source'];path=ROOT/s['path'];assert sha(path)==s['sha256']
    if str(path) not in source_cache:source_cache[str(path)]=list(csv.DictReader(path.open()))
    rows=[source_cache[str(path)][n-2] for n in s['csv_lines']];by_scene[scene]=rows
    for r in rows:
     for k,wanted in s.get('selector',{}).items():
      actual=(r.get('scene') or ('Plot'+r['plot'] if r.get('plot') else 'ETH')) if k=='scene' else r[k]
      assert str(actual)==str(wanted),(k,actual,wanted)
    assert len(rows)==g['n']==(150 if scene=='ETH' else 100)
    assert len({(r['pair'],r['seed']) for r in rows})==g['n']
    assert {int(r['seed']) for r in rows}==set(range(31,41))
    pairs={p['source']+'-'+p['target'] for p in manifest['pairs'] if p['scene']==scene}
    assert {(r['pair'],int(r['seed'])) for r in rows}=={(p,k) for p in pairs for k in range(31,41)}
    ok=sum(r['success']=='1' for r in rows);assert ok==g['ok']
    ep=[float(r['e_p_m']) for r in rows if math.isfinite(float(r['e_p_m']))];assert len(ep)==g['finite'];close(sum(ep)/len(ep),g['ep'])
    rot=[float(r['rot_err_deg']) for r in rows if math.isfinite(float(r['rot_err_deg']))];assert len(rot)==len(ep);close(sum(rot)/len(rot),g['rot'])
    for r in rows:
     assert (r['success']=='1')==(float(r['rot_err_deg'])<2 and float(r['trans_err_m'])<.5)
    # Bind the actual main-page displayed cells directly to independent CSV sums.
    main=wb['Meeting主頁'];positions=[r[0].row for r in main if r[0].value==group['label']];assert len(positions)==2
    col=3+2*['Plot1','Plot2','Plot3','Plot4','ETH'].index(scene)
    assert main.cell(positions[0],col).value==f'{ok}/{len(rows)}\n無輸出{len(rows)-len(ep)}';close(main.cell(positions[0],col+1).value,ok/len(rows))
    close(main.cell(positions[1],col).value,100*sum(ep)/len(ep));close(main.cell(positions[1],col+1).value,sum(rot)/len(rot))
    group_checks+=1
   loaded.append(by_scene)
  for name,g in m.get('auxiliary_groups',{}).items():
   src=g['source'];path=ROOT/src['path'];assert sha(path)==src['sha256']
   if str(path) not in source_cache:source_cache[str(path)]=list(csv.DictReader(path.open()))
   rr=[source_cache[str(path)][n-2] for n in src['csv_lines']]
   assert len(rr)==g['n'] and sum(r['success']=='1' for r in rr)==g['ok']
   ep=[float(r['e_p_m']) for r in rr if math.isfinite(float(r['e_p_m']))];assert len(ep)==g['finite'];close(sum(ep)/len(ep),g['ep']);group_checks+=1
  a,z=[loaded[k] for k in m['primary']];sheet=wb['必要對照']
  for n,scene in enumerate(['Plot1','Plot2','Plot3','Plot4','ETH'],m['paired_data_first_row']):
   aa={(r['pair'],r['seed']):r for r in a[scene]};bb={(r['pair'],r['seed']):r for r in z[scene]};assert aa.keys()==bb.keys()
   gained=sum(aa[k]['success']=='0' and bb[k]['success']=='1' for k in aa);lost=sum(aa[k]['success']=='1' and bb[k]['success']=='0' for k in aa)
   both=[k for k in aa if aa[k]['success']==bb[k]['success']=='1']
   assert sheet.cell(n,5).value==gained and sheet.cell(n,6).value==lost and sheet.cell(n,7).value==len(both)
   close(sheet.cell(n,8).value,sum(float(aa[k]['e_p_m'])*100 for k in both)/len(both));close(sheet.cell(n,10).value,sum(float(bb[k]['e_p_m'])*100 for k in both)/len(both))
  wb.close()
  for fmt,dest in [('xlsx',recalculated),('pdf',previews)]:
   target=dest/(p.name if fmt=='xlsx' else p.stem+'.pdf')
   if prior.get(p.name)==sha(p) and target.exists():continue
   proc=subprocess.run(['soffice','--headless','--convert-to',fmt,'--outdir',str(dest),str(p)],capture_output=True,text=True,timeout=180)
   assert proc.returncode==0,(proc.stdout,proc.stderr)
  validate(recalculated/p.name,m)
  pdf=fitz.open(previews/(p.stem+'.pdf'));assert len(pdf)==2,(p.name,len(pdf))
  for n,page in enumerate(pdf):
   assert '###' not in page.get_text(),(p.name,n)
   page.get_pixmap(matrix=fitz.Matrix(1.3,1.3)).save(previews/f'week_{i:02d}_page{n+1}.png')
  all_meta.append(dict(week=i,file=p.name,sha256=sha(p),checks=len(m['checks']),pdf_pages=len(pdf),calc_recalculation_passed=True));print('VERIFIED',i,flush=True)
 audit=dict(validated_at=datetime.now(ZoneInfo('Asia/Taipei')).isoformat(),reports=all_meta,independent_groups=group_checks,cell_checks=sum(m['checks'] for m in all_meta),main_and_backup_one_page_each=True,independent_csv_and_paired_validation=True)
 talks=OUT/'talks_validation.json'
 if weeks==list(range(1,7)) and talks.exists():
  checked=[]
  for talk in json.loads(talks.read_text())['talks']:
   assert sha(OUT/talk['file'])==talk['sha256'] and sha(OUT/talk['xlsx'])==talk['xlsx_sha256']
   body=(OUT/talk['file']).read_text();assert '/550' not in body and talk['xlsx'] in body
   m=json.loads((OUT/f'audit/week_{talk["week"]:02d}.json').read_text())
   for group in m['groups']:
    for scene,g in group['scenes'].items():
     line=f'{scene}：成功{g["ok"]}/{g["n"]}；平均e_p={100*g["ep"]:.6f}cm；平均e_R={g["rot"]:.6f}°；有輸出{g["finite"]}次、無輸出{g["n"]-g["finite"]}次。'
     assert line in body,(talk['file'],line)
   checked.append(talk['week'])
  assert sorted(checked)==[4,6];audit['talks_matched_to_workbooks']=checked
 dest=OUT/('validation.json' if weeks==list(range(1,7)) else 'validation_partial.json');dest.write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n')
 if weeks==list(range(1,7)):
  zip_path=OUT/'meeting_6weeks.zip'
  with zipfile.ZipFile(zip_path,'w',zipfile.ZIP_DEFLATED) as z:
   for m in all_meta:z.write(OUT/m['file'],m['file'])
  with zipfile.ZipFile(zip_path) as z:
   assert len(z.namelist())==6
   for n in z.namelist():assert z.read(n)==(OUT/n).read_bytes()
  audit.update(zip_sha256=sha(zip_path),zip_bytes=zip_path.stat().st_size);dest.write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps(audit,ensure_ascii=False,indent=2))
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--weeks',nargs='+',type=int,default=list(range(1,7)));args=ap.parse_args();verify(args.weeks)
