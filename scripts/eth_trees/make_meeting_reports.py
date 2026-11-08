#!/usr/bin/env python3
"""Six concise, auditable meeting workbooks; no experiment or seed selection."""
from pathlib import Path
from datetime import datetime
from zoneinfo import ZoneInfo
import argparse,csv,hashlib,json,math,statistics as st
import xlsxwriter
from openpyxl import load_workbook

ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'results/meeting_reports_20261108'
OLD=ROOT/'results/six_week_reports_20261028'
SCENES=['Plot1','Plot2','Plot3','Plot4','ETH']
PAPER=dict(zip(SCENES,[5.2558,11.4808,4.8756,4.3560,15.3427]))
TITLES={1:'CN容差與Solver',2:'弱配對補救',3:'DTM設定整組對照',4:'DTM與幾何微調',5:'擴大候選的退步',6:'結構驗證因素消融'}
PIN='b839f64ecc9385321168771847f1efea4b00d8ad'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def finite(v):
 try:x=float(v);return x if math.isfinite(x) else None
 except (TypeError,ValueError):return None
def key(r):return r['pair'],int(r['seed'])
def average(a):return st.mean(a) if a else None
def aggregate(rows,source):
 n=len(rows);assert n>0 and len({key(r) for r in rows})==n
 for r in rows:
  rot=finite(r.get('rot_err_deg'));t=finite(r.get('trans_err_m'))
  assert (r['success']=='1')==(rot is not None and t is not None and rot<2 and t<.5)
 errors=[finite(r['e_p_m']) for r in rows];errors=[x for x in errors if x is not None]
 return dict(rows=rows,n=n,ok=sum(r['success']=='1' for r in rows),finite=len(errors),ep=average(errors),rate=sum(r['success']=='1' for r in rows)/n,source=source)
class Data:
 def __init__(self,i):
  self.m=json.loads((OLD/f'report_{i:02d}_manifest.json').read_text());self.tables={};self.groups={}
  for sid,s in self.m['sources'].items():
   p=ROOT/s['path'];assert sha(p)==s['sha256'];self.tables[sid]=list(csv.DictReader(p.open()))
 def get(self,gid):
  if gid not in self.groups:
   g=self.m['groups'][gid];rows=[self.tables[g['source']][n-2] for n in g['csv_lines']]
   source=dict(self.m['sources'][g['source']],csv_lines=g['csv_lines'],selector=g['selector'],group=gid)
   x=aggregate(rows,source);assert x['ok']==g['ok'] and x['finite']==g['finite'] and x['n']==g['n']
   assert math.isclose(x['ep'],g['ep'],rel_tol=1e-11,abs_tol=1e-11);self.groups[gid]=x
  return self.groups[gid]
 def new(self,scene,variant):
  p=OUT/'verification_ablation_rows.csv';rr=list(csv.DictReader(p.open()));indices=[n for n,r in enumerate(rr,2) if r['scene']==scene and r['variant']==variant]
  x=aggregate([rr[n-2] for n in indices],dict(path=str(p.relative_to(ROOT)),sha256=sha(p),csv_lines=indices,selector=dict(scene=scene,variant=variant)))
  assert x['n']==(150 if scene=='ETH' else 100);return x
def paired(a,b):
 aa={key(r):r for r in a['rows']};bb={key(r):r for r in b['rows']};assert aa.keys()==bb.keys()
 both=[k for k in aa if aa[k]['success']==bb[k]['success']=='1']
 return dict(gained=sum(aa[k]['success']=='0' and bb[k]['success']=='1' for k in aa),lost=sum(aa[k]['success']=='1' and bb[k]['success']=='0' for k in aa),common=len(both),before=average([float(aa[k]['e_p_m'])*100 for k in both]),after=average([float(bb[k]['e_p_m'])*100 for k in both]))
def setup(i):
 d=Data(i);groups=[]
 if i==1:
  for name,th,method,solver in [('A｜5cm・SVD','0.05','reference5cm','svd'),('B｜5cm・RANSAC','0.05','reference5cm','svd+ransac'),('C｜10cm・SVD','0.1','baseline10cm','svd'),('D｜10cm・RANSAC','0.1','baseline10cm','svd+ransac')]:
   groups.append((name,{s:d.get('F:'+th+':'+solver) if s=='ETH' else d.get('T:'+s+':'+method+':'+solver) for s in SCENES}))
  primary=(0,3);question='只改容差與Solver，能否同時改善兩個dataset？';verdict='ETH：114→144/150；Tongji：309→304/400。兩項改動沒有跨場景一致改善。'
  settings='容差：5→10cm；Solver：SVD→RANSAC。四組均關閉新增補救；樹位、N=183、w=10、Thscore=40固定。'
 elif i==2:
  before={s:d.get('C:10R') if s=='ETH' else d.get('T:'+s+':10R') for s in SCENES}
  groups=[('補救關閉',before),('補救開啟',{s:d.get('U:'+s+':eth10_raw') for s in SCENES})];primary=(0,1)
  question='原CN配對不足時，新增補救能救回多少？';verdict='Tongji：304→329/400，新增25次全在Plot4；ETH：144→150/150。'
  settings='固定10cm＋RANSAC＋原DTM；只切換evidence補救。原CN少於6對啟動；top3／最多60邊／至少6支持為整組設定。'
 elif i==3:
  groups=[('原DTM・完整提取',{s:d.get('U:'+s+':eth10_raw') for s in SCENES}),('平滑DTM・完整提取',{s:d.get('FULL:'+s) for s in SCENES})];primary=(0,1)
  question='改地面估計，樹位與配準是否更穩？';verdict='Tongji：329→345/400；ETH：150→149/150。有改善，也有新失敗。'
  settings='DTM cell固定0.50m；fit_radius 2→6格、fill_max_radius 5→6格一起改。完整重提取；10cm／RANSAC／補救固定。'
 elif i==4:
  groups=[('原DTM・無微調',{s:d.get('U:'+s+':eth10_raw') for s in SCENES}),('原DTM・加微調',{s:d.get('U:'+s+':eth10_refined') for s in SCENES}),('平滑DTM・無微調',{s:d.get('FULL:'+s) for s in SCENES}),('平滑DTM・加微調',{s:d.get('SREF:'+s) for s in SCENES})];primary=(2,3)
  question='精度改善來自微調，還是改DTM？用四組拆開看。';verdict='承接第3週平滑DTM：494→547/550；原DTM加微調546/550，並非更好的分支。'
  settings='兩種DTM × 微調關閉／開啟。微調為6DOF、多尺度0.50→0.20→0.10m、30/25/20次；coarse與輸入點雲固定。'
 elif i==5:
  groups=[('單初值＋微調',{s:d.get('U:'+s+':eth10_refined') for s in SCENES}),('候選v1＋微調',{s:d.get('U:'+s+':multi_refined') for s in SCENES}),('候選v2＋微調',{s:d.get('U:'+s+':multi_v2_refined') for s in SCENES})];primary=(0,2)
  question='擴大候選範圍，是否一定選到更好的姿態？';verdict='Plot4：96→78/100（−18pp）。候選更多，舊幾何分數卻可能偏好嚴重錯位。'
  settings='沿原DTM支線與相同舊微調backend。v1→v2：top5→20、60→120邊、替代pose16→256、微調3→8，另加yaw初值；整組比較。'
 elif i==6:
  assert (OUT/'ABLATION_COMPLETE.json').exists(),'The two missing factor controls must finish before report6.'
  groups=[('都不改（v2）',{s:d.get('U:'+s+':multi_v2_refined') for s in SCENES}),('只篩垂直結構',{s:d.new(s,'v1g0') for s in SCENES}),('只收緊距離門檻',{s:d.new(s,'v0g1') for s in SCENES}),('兩項都改（v3）',{s:d.get('U:'+s+':multi_v3_refined') for s in SCENES})];primary=(0,3)
  question='垂直結構篩選與嚴格距離門檻，各自貢獻多少？';verdict='完整v3：550/550通過；相對v2新增22次成功，均在Plot4。兩個單項效果見四組矩陣。'
  settings='v2候選引擎、原DTM與6DOF optimizer固定。驗證表面：全方向／|normal.z|<0.30；gate：3h／min(h,0.20m)。'
 return d,groups,primary,question,verdict,settings

class Book:
 def __init__(self,i):
  OUT.mkdir(parents=True,exist_ok=True);self.path=OUT/f'week_{i:02d}_{TITLES[i]}.xlsx';self.w=xlsxwriter.Workbook(self.path);self.checks=[];self.sources={};self.i=i
  opts={'title':dict(bold=True,font_size=20),'verdict':dict(bold=True,font_size=15,bottom=1),'head':dict(bold=True,bg_color='#EDEDED',align='center'),'label':dict(bold=True,align='left'),'text':{},'note':dict(font_size=11,font_color='#444444'),'num':dict(num_format='0.00',align='center'),'integer':dict(num_format='0',align='center'),'count':dict(align='center'),'pct':dict(num_format='0.0%',align='center'),'pp':dict(num_format='+0.0" pp";-0.0" pp";0.0" pp"',align='center')}
  self.f={k:self.w.add_format(dict(font_name='Microsoft JhengHei',font_size=14,valign='vcenter',text_wrap=True)|v) for k,v in opts.items()}
  self.s={n:self.w.add_worksheet(n) for n in ['Meeting主頁','必要對照']}
  for name,s in self.s.items():
   s.hide_gridlines(2);s.set_zoom(85);s.set_column('A:A',15);s.set_column('B:B',13);s.set_column('C:L',12)
   s.set_landscape();s.set_paper(8);s.fit_to_pages(1,1);s.set_margins(.2,.2,.3,.3);s.set_footer('&L單位依欄名；pp＝百分點&R&P / &N');s.freeze_panes(5,2)
 def cell(self,s,r,c,value,style='text',formula=None,source=None):
  if value is None:value='不可比'
  ws=self.s[s]
  if formula:ws.write_formula(r,c,formula,self.f[style],value)
  else:ws.write(r,c,value,self.f[style])
  self.checks.append(dict(sheet=s,row=r+1,col=c+1,value=value,formula=formula))
  if source:
   h=source['sha256'];self.sources[h]=source
   ws.write_comment(r,c,'來源：'+source['path']+'\nSHA256：'+h+'\n篩選：'+json.dumps(source.get('selector',{}),ensure_ascii=False)+'\nCSV行號：'+','.join(map(str,source['csv_lines'])),{'width':480,'height':150})
 def merge(self,s,r,c,end,value,style='text',height=25):
  self.s[s].merge_range(r,c,r,end,value,self.f[style]);self.s[s].set_row(r,height)
 def line(self,s,r,value,style='note',height=30):self.merge(s,r,0,11,value,style,height)
 def matrix(self,s,row,title,groups,metric,source_map=None):
  self.line(s,row,title,'head',23);row+=1
  self.merge(s,row,0,1,'設定／版本','head',23)
  for j,scene in enumerate(SCENES):self.merge(s,row,2+j*2,3+j*2,'ETH-tree' if scene=='ETH' else 'Tongji '+scene,'head',23)
  row+=1;self.merge(s,row,0,1,'','head',20)
  for j in range(5):
   for k,name in enumerate(['成功/總數','成功率'] if metric=='success' else ['誤差(cm)','無輸出']):self.cell(s,row,2+j*2+k,name,'head')
  self.s[s].set_row(row,24);row+=1
  records=[('論文報告值',None)]+groups
  for name,gs in records:
   self.merge(s,row,0,1,name,'label',29)
   for j,scene in enumerate(SCENES):
    c=2+j*2
    if gs is None:
     self.cell(s,row,c,'未交代' if metric=='success' else PAPER[scene],'note' if metric=='success' else 'num')
     self.cell(s,row,c+1,1 if metric=='success' else '未交代','pct' if metric=='success' else 'note')
     for k in [c,c+1]:self.s[s].write_comment(row,k,'CircularMatch DOI10.1109/LGRS.2025.3650098，PDF p4 Table I。論文成功門檻／統計分母未齊，不能視為本地同條件重跑。')
    else:
     g=gs[scene]
     if metric=='success':
      self.cell(s,row,c,f'{g["ok"]}/{g["n"]}','count',source=g['source']);self.cell(s,row,c+1,g['rate'],'pct',f'={g["ok"]}/{g["n"]}',g['source'])
     else:
      self.cell(s,row,c,100*g['ep'],'num',source=g['source']);self.cell(s,row,c+1,g['n']-g['finite'],'integer',source=g['source'])
   row+=1
  return row
 def delta_table(self,s,row,title,groups,primary):
  a,z=[groups[k][1] for k in primary]
  self.line(s,row,title,'head',24);row+=1
  headers=[('場景',0,1),('vs修改前成功率',2,3),('新增成功',4,4),('退步失敗',5,5),('共同成功N',6,6),('共同成功前cm',7,8),('共同成功後cm',9,10),('誤差下降率',11,11)]
  for h,c,end in headers:
   if c==end:self.cell(s,row,c,h,'head')
   else:self.merge(s,row,c,end,h,'head',30)
  self.s[s].set_row(row,35);row+=1
  for scene in SCENES:
   x=paired(a[scene],z[scene]);self.merge(s,row,0,1,scene,'label',27)
   self.merge(s,row,2,3,'','text',27);self.cell(s,row,2,100*(z[scene]['rate']-a[scene]['rate']),'pp',f'=({z[scene]["ok"]}/{z[scene]["n"]}-{a[scene]["ok"]}/{a[scene]["n"]})*100')
   for c,k in [(4,'gained'),(5,'lost'),(6,'common')]:self.cell(s,row,c,x[k],'integer')
   for c,k in [(7,'before'),(9,'after')]:self.merge(s,row,c,c+1,'','text',27);self.cell(s,row,c,x[k],'num')
   v=(x['before']-x['after'])/x['before'] if x['before'] else None;rr=row+1;self.cell(s,row,11,v,'pct',f'=IF(AND(ISNUMBER(H{rr}),ISNUMBER(J{rr}),H{rr}<>0),(H{rr}-J{rr})/H{rr},"不可比")');row+=1
  self.line(s,row,'誤差下降率=(前−後)/前。共同成功只回答同一批成功案例的精度，不取代全體成功率。',height=27)
  return row+2
 def finish(self,last_main,last_backup,meta):
  self.s['Meeting主頁'].print_area(0,0,last_main,11);self.s['必要對照'].print_area(0,0,last_backup,11)
  self.w.close();meta.update(file=self.path.name,sha256=sha(self.path),checks=self.checks,sources=self.sources)
  audit=OUT/'audit';audit.mkdir(exist_ok=True);(audit/f'week_{self.i:02d}.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2)+'\n')
  validate(self.path,meta);print('BUILT',self.path.name,len(self.checks),flush=True)
  return meta

def validate(path,meta):
 f=load_workbook(path,data_only=False);v=load_workbook(path,data_only=True)
 assert f.sheetnames==['Meeting主頁','必要對照'] and not f._external_links
 for x in meta['checks']:
  c=v[x['sheet']].cell(x['row'],x['col']);a=c.value;b=x['value']
  assert math.isclose(a,b,rel_tol=1e-10,abs_tol=1e-9) if isinstance(b,(int,float)) else a==b,(x,a)
  if x['formula']:assert f[x['sheet']].cell(x['row'],x['col']).data_type=='f'
 for s in v:
  assert not any(c.data_type=='e' for row in s for c in row)
 f.close();v.close()

def build(i):
 d,groups,primary,q,verdict,settings=setup(i);b=Book(i);s='Meeting主頁'
 b.line(s,0,f'第{i}週｜{TITLES[i]}','title',32);b.line(s,1,q,'text',25);b.line(s,2,verdict,'verdict',37);b.line(s,3,settings,'note',37)
 row=b.matrix(s,5,'① 可靠性：成功／總次數與成功率（越高越好）',groups,'success');row+=1
 row=b.matrix(s,row,'② 精度與失敗代價：有輸出的全部案例平均誤差（含配錯）',groups,'error')
 b.line(s,row,'無輸出仍算失敗；誤差不填0。誤差平均分母＝總次數−無輸出次數；各組分母不同時，不直接計算整體誤差改善率。',height=32);row+=1
 b.line(s,row,'Tongji四Plot＝同一dataset；各100次（10配對×10seed）。ETH＝另一dataset，150次（15×10）。seed31–40；成功門檻：旋轉<2°且平移<0.5m。',height=33);row+=1
 b.line(s,row,'論文值僅供參考：成功門檻與分母未齊。GT只用於評分；曾用於開發選參。這是已知場景的比較，不是未見森林盲測。',height=29)
 last_main=row;s='必要對照';b.line(s,0,'同配對、同seed的消融證據','title',30);b.line(s,1,f'修改前：{groups[primary[0]][0]}　→　修改後：{groups[primary[1]][0]}','text',30)
 row=b.delta_table(s,3,'逐場景配對比較：新增與退步都保留',groups,primary)
 if i in [1,4,6]:
  contrasts={1:[('固定SVD：5→10cm',0,2),('固定RANSAC：5→10cm',1,3),('固定5cm：換Solver',0,1),('固定10cm：換Solver',2,3)],4:[('原DTM：加微調',0,1),('平滑DTM：加微調',2,3),('無微調：換DTM',0,2),('有微調：換DTM',1,3)],6:[('舊gate：篩垂直',0,1),('新gate：篩垂直',2,3),('全方向：收緊gate',0,2),('垂直：收緊gate',1,3)]}[i]
  b.line(s,row,'固定另一項：各因素的成功率變化（後−前，pp）','head',24);row+=1
  b.merge(s,row,0,1,'場景','head',32)
  for j,(name,_,_) in enumerate(contrasts):
   c=2+j*2;b.merge(s,row,c,c+1,name,'head',32)
  b.merge(s,row,10,11,'讀法','head',32);row+=1
  for scene in SCENES:
   b.merge(s,row,0,1,scene,'label',27)
   for j,(_,a,z) in enumerate(contrasts):
    x,y=groups[a][1][scene],groups[z][1][scene];c=2+j*2;b.merge(s,row,c,c+1,'','text',27);b.cell(s,row,c,100*(y['rate']-x['rate']),'pp',f'=({y["ok"]}/{y["n"]}-{x["ok"]}/{x["n"]})*100')
   b.merge(s,row,10,11,'正值改善；負值退步','note',27);row+=1
  row+=1
 if i==3:
  b.line(s,row,'機制控制：固定舊樹幹軸線，只重算DTM交點（不是完整重提取）','head',25);row+=1
  for scene in SCENES:
   a=groups[0][1][scene];z=d.get('EP:R') if scene=='ETH' else d.get('D:'+scene+':smooth')
   b.merge(s,row,0,1,scene,'label',27);b.merge(s,row,2,5,f'原：{a["ok"]}/{a["n"]} → 定位控制：{z["ok"]}/{z["n"]}','text',27);b.merge(s,row,6,11,'完整新流程結果見主頁；兩種實驗不可混稱。','note',27);row+=1
  row+=1
 if i==5:
  row=b.delta_table(s,row,'補充：候選v1→v2，不能將整組效果歸因於top-k單一參數',groups,(1,2))
  b.line(s,row,'反例｜Plot4 S4–S2 seed31：v1 overlap=16.7%、e_p=0.086m；v2 overlap=33.8%、e_p=45.94m。分數較高卻嚴重錯位。',height=37);row+=2
 if i==6:
  b.line(s,row,'同一v3 backend的公平控制：額外4次改善才是完整候選對單初值的增益','head',25);row+=1
  for scene in SCENES:
   gs=[d.get('U:'+scene+':'+m) for m in ['reference5_refined_v3','eth10_refined_v3','multi_v3_refined']]
   b.merge(s,row,0,1,scene,'label',27)
   for c,name,g in zip([2,5,8],['5cm/SVD','10cm/補救','完整候選'],gs):b.merge(s,row,c,c+2,f'{name}：{g["ok"]}/{g["n"]}','text',27)
   row+=1
  row+=1
 mechanisms={1:'機制：容差放寬可接納較不一致的距離，也可能增加錯配；RANSAC只在既有對應中找3D一致群。不能把兩項一起改的增益全算給容差。',2:'機制：原CN不足6對才啟動；回收CN候選證據，以相容三元組產生Kabsch姿態，再以一對一3D支持驗證。是新增方法整組，不是發明RANSAC。',3:'證據邊界：fit與fill一起變動，只能歸因DTM設定整組；尚無兩參數各自的完整流程消融。樹位數改變已觀察到，不能直接認定樹更多就是改善原因。',4:'機制：coarse提供初始姿態，表面point-to-plane微調估完整6DOF。四組完整控制顯示微調效果不靠回復DTM解釋；平滑＋微調也保留。第5週沿原DTM研究支線。',5:'證據邊界：候選池、yaw初值、支持分數與beam同時變動，是搜尋策略整組比較。v1/v2共用舊驗證分數；新候選可能放大假重疊的影響，不能只怪候選數。',6:'機制：驗證優先看垂直表面，減少共同地面的假重疊；收緊NN距離避免把偏移表面算作支持。兩項效果用四組隔離；改動作用於排序、接受與最終選擇，optimizer仍用全方向6DOF。'}
 b.line(s,row,mechanisms[i],height=44);row+=1
 b.line(s,row,'時間／代價：舊批次與新補跑的硬體或並行、計時範圍不齊，不計跨批次加速比。原論文i7-9700F 3GHz、實際threads未交代；本地原生OMP4。',height=36);row+=1
 b.line(s,row,'論文對照：Table I／PDF p4；時間見Table II／p4、Table III／p5。作者公開程式在2026-10-28查核僅README，未取得同環境作者baseline。',height=32);row+=1
 b.line(s,row,'追溯：主頁數值格的註解附CSV路徑、行號、篩選與SHA256。既有來源固定於Git commit '+PIN[:12]+'；新消融見ablation_protocol.json。',height=34)
 meta=dict(week=i,title=TITLES[i],generated_at=datetime.now(ZoneInfo('Asia/Taipei')).isoformat(),source_commit=PIN,groups=[dict(label=name,scenes={s:{k:v for k,v in g.items() if k!='rows'} for s,g in gs.items()}) for name,gs in groups],primary=primary,main_last_row=last_main+1,backup_last_row=row+1)
 return b.finish(last_main,row,meta)
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--weeks',nargs='+',type=int,default=list(range(1,7)));a=ap.parse_args()
 for i in a.weeks:build(i)
if __name__=='__main__':main()
