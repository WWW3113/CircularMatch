# 統一森林配準benchmark

本輪只計Tongji-trees（四Plot）及ETH-tree兩個獨立來源。全部55個固定配對、seed31–40；每次推論只使用當前兩站，不讀GT、不用其他站圖。兩資料均已參與開發，不能稱未見森林驗證。

主要統一版本為v3：保留ETH粗配準，弱CN時擴大有限候選，使用垂直結構驗證降低水平地面錯配偏差，再做完整6DOF點對平面refinement。5cm參考與ETH固定版也套相同v3 refinement作公平對照。沒有依Plot或dataset切換方法。

## 逐場景主要結果

| 場景 | 成功試驗 | 所有seed皆成功配對 | 平均有限e_p m | 論文報告e_p m | 降低比例 |
|---|---:|---:|---:|---:|---:|
| Plot1 | 100/100 | 10/10 | 0.028866 | 0.052558 | 45.1% |
| Plot2 | 100/100 | 10/10 | 0.011854 | 0.114808 | 89.7% |
| Plot3 | 100/100 | 10/10 | 0.011183 | 0.048756 | 77.1% |
| Plot4 | 100/100 | 10/10 | 0.033920 | 0.043560 | 22.1% |
| ETH | 150/150 | 15/15 | 0.006038 | 0.153427 | 96.1% |

成功門檻為本專案rotation<2°且translation<0.5m，未聲稱是論文門檻。e_p依Eq8計算全部來源點；有限失敗也納入平均，無變換仍計失敗且另列無法計算誤差數。550次是55個配對重複10次，不能當作550個獨立場景。

本輪五場景全成功：True；五場景誤差數值低於論文：True；五場景各降低至少10%且全成功：True。這些是固定benchmark的數值判定，論文配對及成功率門檻未完全核對。

## 尚未完成的研究驗收

- 作者原作baseline未重跑：論文指定 https://github.com/MercerSU/CircularMatch 的完整history僅README，無程式、tags、releases或forks。5cm參考是我們實作，不冒稱未修改作者原作。
- ETH及全部Tongji已參與開發／失敗分析；沒有完全獨立未見森林。相同固定方法的兩來源benchmark改善，不等同更廣泛一般化。
- 不以整體平均掩蓋場景退步；所有九版本逐場景及逐案例保留。

## 版本與消融

v1在Plot4為96/100；v2擴大候選後退到78/100，說明候選多而幾何評分偏好重疊地面會造成退步。v3改結構驗證，不改全6DOF求解。全55配對均重新評估，沒有只補失敗。

相對v2仍有的平均誤差退步：Plot3平均e_p比v2增加0.127毫米，成功率100/100→100/100。統一版保留此結果，沒有為該場景換回其他版本。

第一個ETH凍結後的Tongji遷移仍在 `../tongji_trees/fixed_eth_20261025`；DTM定位／完整Plot2追測仍在 `../tongji_trees/dtm_height_20261025`。本輪未覆寫這些紀錄。配置、程式hash與各候選預算見protocol、method_frozen、method_v2_frozen、method_v3_frozen、unified_config及versions。

## 報告及成本

- comparison_simple.xlsx：白話結論、五場景、論文數值與參數。
- comparison_full.xlsx：全部4950列、消融、Tongji等權／配對加權、ETH單列、時間／記憶體、失敗、seed變異、一般化限制。
- registration_rows.csv與scene_summary.csv保留可計算失敗；status及finite_error_cases避免把缺輸出當零誤差。
- v3的相同refinement控制與統一版冷啟動在同一新runtime epoch比較。舊epoch時間只作歷史紀錄；並行準確度採集online時間不是獨占速度基準。
- 每站幾何快取與seed31完整提取另列，不當作免費；其中4站重用當前epoch第一筆相同設定的冷啟動提取實測（串行），其餘22站以最多4工作並行量測，每程序4threads。此為共用成本重用，不依耗時挑選；不同並行數不可當獨占速度直接比較。12筆冷啟動重建兩站應用快取，OS檔案快取未重置並反向排序。記憶體為最大子程序RSS，不含Python父程序及OS page cache。
- GT全點評分成本獨立列出，不混入推論；不同硬體的論文matching-only時間不作直接比較。

## 續跑與重現

先看RESUME.md。原始點雲及私人Drive識別碼不隨Git分發；manifest記錄26站、55配對、checksum、方向、單位與開發接觸歷史。checkpoint可恢復完整及部分seed，SHA先核驗。環境重啟會殺背景程序，需先核對活程序再啟動resume_unified.py。

核驗文件為evaluator_audit、evaluator_full_cloud_audit、matrix_direction_audit、validation及workbook_validation。DELIVERY.json提供核對過下載SHA的固定commit連結。FINAL_COMPLETE只標示本輪計算及交付結束，不代表整個研究已驗收。

報表生成時間：2026-10-26T03:47:03+08:00（Asia/Taipei）。
