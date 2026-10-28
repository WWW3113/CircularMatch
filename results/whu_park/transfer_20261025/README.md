# WHU-Park 外部場景測試

後續完成了 [稀疏提取分站研究](../sparse_stems_20261025/README.md)：開發組選出的放寬擬合組合，在本輪保留站點僅0/120→2/120；ETH原180份輸出與300列配準結果保持一致。新方案仍不足以可靠使用；本目錄保留先前全場測試快照。

資料為YOHO專案提供的WHU-TLS公開整理版Park，不是原論文WHU-FGI。32站、31個提供的GT配對，每對使用seed21–30。兩種前處理×原推薦組／三維補救×SVD／配準RANSAC，共2480列；640次提取均核對樹位CSV與日誌。

## 結果

ETH參數直接遷移：兩種匹配方法皆0/310成功。32站有效法向量都是0，後面無法提取樹木，三維匹配補救沒有輸入。

診斷：點的最近鄰距離中位數約0.1115m，第五近鄰約0.1685m；10cm鄰域不足以取得PCA所需的5個鄰居。

另作探索性密度適配：只用輸入點間距的固定規則，法向半徑升至0.20m；分群距離仍0.20m，其他參數不改，且未用GT選值。法向量恢復，但樹木提取依然很少；原推薦組與三維補救皆2/310成功（0.6%）。沒有在看到適配結果後繼續搜尋參數。

因此目前沒有可靠的跨資料集配準效果。這份資料為處理後混合公園場景，實際掃描中心也未獨立驗證；不能用本次結果聲稱原始森林資料的泛化表現。

## 來源與真值

公開來源：https://github.com/HpWang-whu/YOHO；壓縮檔SHA256：33a7a29577faa631c2a3bec828f9552182dbcbfd0da67ce572c8c28df60caf0a。只提取既有Park點雲與gt.log/gt.info。原始GT方向依YOHO utils/dataset.py：header target,source,N，矩陣把source映至target。前三對的原始點雲重疊亦確認此方向，未依GT刪選配對。

另找到FinnWoodlands、Lin3D及Treeseg/3D Forest公開來源，但目前README沒有確認可直接使用的分站配準真值；Tongji仍為申請表、公開WHU整理版仍不含FGI；Liang處理版僅能作提取驗證。

## 重現

依序執行transfer_whu_park.py prepare、extract、register，再執行density、extract --adapted、register --adapted，最後執行report_whu_park.py。prepare要求data/whu_tls_yoho/extracted/WHU-TLS/Park/PointCloud已有核驗壓縮檔內的32份點雲與GT。密度適配計畫在試跑前保存於density_adaptation.json，固定ETH參數計畫保存於plan.json。

[跨資料集比較Excel](cross_dataset_park.xlsx)含主要結果、ETH與Park比較、31個配對、提取瓶頸、參數來源與2480筆原始逐次結果。
