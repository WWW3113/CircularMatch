# Liang 公開處理版的固定參數提取驗證

來源為 TreeTool 專案 README 公開的 Google Drive 樣本；TreeTool 明確連到 Liang 2018 原始資料公告。六份 PCD 每份對應一個樣區，另附六份 LHD 參考樹檔案。這是處理／降採樣版本，不是原始分站掃描，也沒有配準姿態或真值。來源網址、每檔位元組數與本地計算 SHA256 見 provenance.json；來源未提供可獨立對照的原廠 checksum。

plan.json 在評分前固定：沿用 ETH 的 clustering 0.20m、step 保留策略、seed11–20；比較 control、verticality 0.85、圓柱 RANSAC probability 0.999 / max_iterations 3000。共六樣區 × 三設定 × 十個 seed，沒有依新資料的評分重調參數。論文使用 Plot1/3/5；本報告也列出全部六樣區。

參考檔依 TreeID、X、Y、height、diameter 解讀。評分在地面樹位和參考 XY 間，以 0.5m 閾值作最大匹配數的一對一指派，再最小化匹配總距離；主要指標為匹配樹數除以所有提供的參考樹數。各樣區等權平均；seed 是同樣區的重複擬合，不能視為獨立森林。

限制：

- 點雲 PCD VIEWPOINT 為零，沿用該座標原點。角度格網檢查不能獨立確認真實掃描中心，因而不能宣稱已驗證原始 TLS 距離抽樣模型。
- 標註邊界不明，因此未匹配偵測不一定是誤報；不計算 precision/F1。
- 參考 XY 錨點高度未明確記載，位置誤差僅作該版本的 XY 比較；不據此評估 DBH 或樹高準確度。
- 不以單份點雲人工切割／施加已知變換來冒充跨站配準。10cm 匹配設定和圖路徑仍需另一份真正分站資料及配準真值驗證。

重現：

```bash
python3 scripts/eth_trees/download_liang_samples.py
OPENBLAS_NUM_THREADS=1 python3 scripts/eth_trees/validate_liang_trees.py
```

需要 NumPy 和 SciPy；C++ 工具使用與 ETH 相同的工具鏈。inspection.json 列實際點數與範圍；extraction_rows.csv 列全部180次結果，summary.csv 列每設定每樣區摘要。原始點雲和執行日誌在 Git 忽略的 data/、runs/ 內。

結果：六樣區等權平均的參考樹匹配比例，control為38.34%，vertical085為34.94%，ransac999為38.52%；論文使用的Plot1/3/5則為43.80%、39.48%、44.10%。所有樣區的vertical085均低於control；增加圓柱擬合資源的差異有限。建議保留verticality 0.90，先查漏樹與版本條件，不能以此宣稱10cm配準參數的跨資料集效果。驗證詳見validation.json。
