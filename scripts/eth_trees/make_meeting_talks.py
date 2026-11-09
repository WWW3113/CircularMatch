#!/usr/bin/env python3
"""Current week4/week6 talks, generated from the same groups as meeting XLSX."""
from datetime import datetime
from zoneinfo import ZoneInfo
import hashlib,json
from make_meeting_reports import OUT,SCENES,TITLES,setup,paired,dataset_counts,dataset_change

def fmt(v,n=3):return f'{v:.{n}f}'
def checks(groups):
 lines=[]
 for label,gs in groups:
  lines.append(label+'：'+dataset_counts(gs))
  for scene,g in gs.items():
   lines.append(f'  {scene}：成功{g["ok"]}/{g["n"]}；平均e_p={100*g["ep"]:.6f}cm；平均e_R={g["rot"]:.6f}°；有輸出{g["finite"]}次、無輸出{g["n"]-g["finite"]}次。')
 return '\n'.join(lines)
def pair_lines(groups,a,z):
 lines=[]
 for scene in SCENES:
  x,y=groups[a][1][scene],groups[z][1][scene];p=paired(x,y)
  lines.append(f'{scene}：{x["ok"]}→{y["ok"]}/{x["n"]}；新增成功{p["gained"]}次、退步{p["lost"]}次；平均e_p {100*x["ep"]:.3f}→{100*y["ep"]:.3f}cm；平均e_R {x["rot"]:.3f}→{y["rot"]:.3f}°。')
 return '\n'.join(lines)
def common():
 return '''統計與比較口徑
Tongji-trees的Plot1到Plot4是同一dataset的四個場景；ETH-tree是另一dataset。只累計同dataset的成功數，不把兩者合成性能總分，也不混算誤差平均。
Tongji每個Plot為10個固定兩站配對×10個seed；ETH為15個固定配對×10個seed，均使用seed31～40。每次推論只使用當前兩站；重複seed不是新增獨立森林。
成功定義固定為旋轉誤差<2°且平移誤差<0.5m。e_p先對全部來源點計算估計／GT變換位置差，再跨有輸出的案例取平均；e_R是估計與GT旋轉之間的角度，再跨相同有輸出案例平均。可計算的失敗保留，無輸出仍算失敗但誤差不填0。
共同成功子集只回答同一批成功案例的精度，不能取代全體成功率。不同輸出集合的平均值，不直接稱作同一批案例的精度提升。
論文Table I／PDF p4報告值僅作參考，e_R未單獨報告，成功門檻與分母未齊；未取得同環境作者程式baseline，不宣稱嚴格超越原作者。不同硬體、並行與計時範圍不做速度排名。
兩個dataset都已參與過開發，GT曾用於離線選參；GT不參與當次推論選輸出，但這不是未見森林盲測。'''
def talk4(groups):
 a,z=groups[2][1],groups[3][1];p4=z['Plot4'];orig=groups[1][1]
 between=paired(orig['Plot4'],z['Plot4'])
 return f'''第4週新版口述稿｜DTM與幾何微調
對應：week_04_DTM與幾何微調.xlsx（Meeting主頁／必要對照兩頁）。本稿取代舊版「回復原DTM再加微調」的主比較說法。主稿約3～5分鐘，後附核對數字不用逐筆念。

【可直接口述】
老師好，上一週使用平滑DTM後，部分場景改善，但粗配準仍不夠準。這次要回答的是：如果利用兩站的點雲表面，再做幾何微調，能不能改善結果？為了分清楚是DTM還是微調帶來影響，我保留原DTM／平滑DTM，以及微調關閉／開啟的四組對照。

先看第一頁。承接第3週的平滑DTM，只加微調後，{dataset_change(a,z)}。其中Plot2由{a['Plot2']['ok']}增加到{z['Plot2']['ok']}/100；ETH原本少通過的一次也被修正。
同時看原DTM支線，加入相同微調後，{dataset_change(groups[0][1],orig)}。所以微調的效果不是只能靠換回原DTM來解釋，兩種DTM內都有受控對照。

四組共用10cm、RANSAC與弱配對補救。新增的微調使用當前兩站表面，由0.50、0.20到0.10m，最多30、25、20輪，估計完整六個自由度。原本提取樹幹的1cm voxel沒有改，也沒有使用多站圖或GT來挑姿態。
直覺上，樹位先提供大致位置；微調再利用更多表面資料修正剩餘偏差。距離與法向篩選、trimming及Huber等一起形成這個後端，本週只能說新增後端整組的效果，不能把全部改善單獨算給其中一個常數。

結果也有界線。平滑DTM加微調在Plot4仍只有{p4['ok']}/100，有{p4['n']-p4['ok']}次未通過，其中{p4['n']-p4['finite']}次沒有有效姿態，另外{p4['finite']-p4['ok']}次有輸出但失敗。包含這些可計算失敗的平均e_p仍為{100*p4['ep']:.3f}cm。局部微調需要可用初值，不能保證把很遠的錯誤初值拉回正確位置。

兩種DTM都加微調時，平滑DTM為{dataset_counts(z)}；原DTM為{dataset_counts(orig)}。成功數淨差一次，出現在Tongji Plot4。逐筆比較是新增{between['gained']}次成功、同時退步{between['lost']}次，因此不是每個案例都由平滑DTM單向改善，也不能說原DTM整體比較好。
下一週回到原DTM支線，是因為候選搜尋原本就在原DTM上開發。為讓既有單初值、v1、v2與v3比較的其他條件一致，後續固定這條支線。六週按方法主題整理，不是每週都選上一週最高分的分支繼續。我們尚未完整驗證平滑DTM搭配所有候選版本，所以也不能宣稱已找到全域最佳組合。

本次結論是：同一DTM內新增幾何微調有明顯作用，但沒有解決全部初始化失敗；兩種DTM的較好與較差結果都保留。第5週接著看候選搜尋，並保留候選變多反而退步的結果。

【與Excel同配對、同seed的核對】
前一次平滑DTM、無微調 → 相同平滑DTM、加微調：
{pair_lines(groups,2,3)}

【四組完整核對數字；平均包含可計算失敗】
{checks(groups)}

【教授可能追問】
問：為什麼下一週不沿著平滑DTM走？
答：候選研究原先用原DTM，固定這條支線才能隔離既有候選與驗證改動。平滑DTM較好的結果保留；尚未測完平滑DTM搭全部候選，不認定原DTM較好。
問：這次是不是同時換地面又加ICP？
答：新版主比較固定前一週的平滑DTM，只加微調；另外完整列原／平滑DTM×微調關閉／開啟四組，沒有把跨分支差異全部歸給微調。
問：是不是所有點的實際量測精度都達到表上的毫米或公分？
答：不是。e_p是估計變換與GT變換作用於來源點後的位置差，不能等同雷射單點的物理量測精度。

{common()}
'''
def talk6(d,groups):
 p=paired(groups[0][1]['Plot4'],groups[3][1]['Plot4'])
 fair=[(name,{s:d.get('U:'+s+':'+method) for s in SCENES}) for name,method in [('5cm/SVD＋同v3後端','reference5_refined_v3'),('10cm/補救＋同v3後端','eth10_refined_v3'),('完整候選＋同v3後端','multi_v3_refined')]]
 diff={s:1000*(groups[3][1][s]['ep']-groups[1][1][s]['ep']) for s in SCENES}
 return f'''第6週新版口述稿｜結構驗證因素消融
對應：week_06_結構驗證因素消融.xlsx（Meeting主頁／必要對照兩頁）。本稿已加入這次補完的兩個單項組合，取代舊稿「缺少表面×gate消融」的說法。主稿約3～5分鐘，後附核對數字不用逐筆念。

【可直接口述】
老師好，第5週擴大候選後，Plot4成功數從96降到78/100。這次保留相同v2候選引擎與原DTM支線，只檢查驗證規則的兩個因素：是否優先看垂直表面，以及是否收緊驗證距離。我把兩者拆成四組，避免將整組改善同時歸功於每一項。

第一個因素是驗證表面。原本使用全方向表面；新設定優先保留|normal.z|<0.30的表面。目的在減少共同地面造成的假重疊。這裡0.30是法向量分量門檻，不是距離，也不是把提取階段的垂直度0.9改成0.3。任一站少於30個垂直表面點時，兩站都退回全方向；這個設計依賴掃描大致直立。
第二個因素是驗證最近鄰距離。對應0.50、0.20、0.10m三尺度，舊門檻為1.50、0.60、0.30m，新門檻為0.20、0.20、0.10m。新分數用於候選排序、微調接受與最後選擇；optimizer本身仍使用全方向表面、原本3h門檻與完整6DOF，沒有同時改成只用垂直點優化。

看Plot4，舊規則是{groups[0][1]['Plot4']['ok']}/100；只篩垂直結構為{groups[1][1]['Plot4']['ok']}/100；只收緊距離門檻為{groups[2][1]['Plot4']['ok']}/100；兩項合用為{groups[3][1]['Plot4']['ok']}/100。
分dataset列，四組依序是：
{chr(10).join(name+'：'+dataset_counts(gs)+'。' for name,gs in groups)}

所以這批資料支持：垂直結構篩選本身已使兩個dataset的所有測試案例通過；收緊gate單獨也有幫助，但在已篩垂直結構時，沒有再增加成功案例。不能說兩項都必要，也不能將兩個單項增益直接相加。這是這次四組消融補完後，與舊口述稿不同的重要結論。

成功率相同也不表示逐筆精度相同。兩項合用相對只篩垂直，Plot3平均e_p變化為{diff['Plot3']:+.3f}mm，Plot4為{diff['Plot4']:+.3f}mm，其餘場景小於0.001mm。主頁按0.01cm顯示，所以看起來相同；不能因此聲稱數值完全一致，也不把這種微小差異直接解讀成穩定精度優勢。
另外，v2到v3在Plot4新增{p['gained']}次成功、退步{p['lost']}次；但兩者共同成功的{p['common']}次，平均e_p仍由{p['before']:.6f}變成{p['after']:.6f}cm。提高整體可靠性與部分原本好案例的輕微退步，可以同時存在。

最後看第二頁同一v3 backend的公平控制：
{chr(10).join(name+'：'+dataset_counts(gs)+'。' for name,gs in fair)}
完整候選比10cm／補救的單初值方法額外救回4次，全部在Tongji Plot4；ETH沒有額外增加。不能將從最早基準到最終的全部增益都歸給多候選。

第5、6週都固定原DTM，是因為候選搜尋原先就在這條支線開發，需讓候選與驗證改動可比；不是認定它優於第4週的平滑DTM加微調。平滑DTM搭全部候選版本尚未完整驗證。
本次結論是：這批資料的成功率改善主要可由垂直結構篩選單項達成，額外收緊gate不是達到全部通過的必要條件。這仍是已知場景的消融結果，不能推論任意新森林也會如此；後續才需要固定版本比較未參與開發的森林。

【與Excel同配對、同seed的核對】
v2舊驗證 → v3兩項合用：
{pair_lines(groups,0,3)}

【四組完整核對數字；平均包含可計算失敗】
{checks(groups)}

【教授可能追問】
問：兩個改動是不是都必要？
答：本批資料的成功率不支持這個說法。只篩垂直結構已全部通過；gate單項有幫助，但在垂直篩選開啟後沒有增加成功數，精度則仍需分場景看。
問：是否已證明所有失敗都是地面造成？
答：沒有。四組控制支持改變驗證表面這個因素有作用；地面假重疊是設計機制與部分反例相容的解釋，沒有逐案排除所有其他原因。
問：是不是單獨證明0.30是最佳門檻？
答：沒有。測試的是既定垂直表面規則關閉／開啟，不是0.30對所有其他門檻的完整搜尋。

{common()}
'''
def main():
 records=[]
 for i,fn in [(4,talk4),(6,talk6)]:
  d,g,*_=setup(i);text=fn(g) if i==4 else fn(d,g)
  assert '/550' not in text and '三頁' not in text
  file=OUT/f'week_{i:02d}_{TITLES[i]}_口述稿_新版.txt';file.write_text(text,encoding='utf-8')
  book=OUT/f'week_{i:02d}_{TITLES[i]}.xlsx';record=dict(week=i,file=file.name,sha256=hashlib.sha256(file.read_bytes()).hexdigest(),xlsx=book.name,xlsx_sha256=hashlib.sha256(book.read_bytes()).hexdigest(),groups={name:{s:{k:v for k,v in x.items() if k!='rows'} for s,x in gs.items()} for name,gs in g})
  records.append(record);print('TALK',file.name,flush=True)
 (OUT/'talks_validation.json').write_text(json.dumps(dict(generated_at=datetime.now(ZoneInfo('Asia/Taipei')).isoformat(),talks=records,no_cross_dataset_performance_totals=True),ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':main()
