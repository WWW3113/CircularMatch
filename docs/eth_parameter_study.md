# ETH Trees 參數與多站路徑實驗

本實驗使用 `results/eth_trees/tuning_20261025/plan.json` 固定的比較計畫。
小型計畫、篩選表、結論與圖表可由 Git 追蹤；點雲與逐次結果放在已忽略的 `data/`、`results/**/runs/`。
原本 `config/default.ini` 與核心演算法預設保持不變。

## 前提

- 依 HANDOFF 建置 PCL/Eigen/CMake/GoogleTest，並完成 ETH 原始資料下載與 SHA-256 校驗。
- 既有 clustering 20 cm 的 seed 1–10 樹位須在 `results/eth_trees/dtm_slope/seeds/ctol20/runs/<scan>/improved/`。
- Python 的篩選、擷取與配準只用標準函式庫；圖路徑與報告需要 NumPy、Matplotlib。
- 雲端使用既有 `/workspace/.circularmatch-env/run`，也可用 `CM_TOOLCHAIN_WRAPPER` 指定工具鏈啟動腳本。
  一般已安裝依賴的 Linux 主機會直接執行 `build/` 下的工具。

## 執行

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
python3 -m unittest discover -s tests -p 'test_*.py'
python3 scripts/eth_trees/tune_registration.py screen
python3 scripts/eth_trees/tune_registration.py extract
python3 scripts/eth_trees/tune_registration.py fresh
OPENBLAS_NUM_THREADS=1 python3 scripts/eth_trees/tune_registration.py graph
OPENBLAS_NUM_THREADS=1 python3 scripts/eth_trees/tune_registration.py report
```

雲端的 CMake 命令須加工具鏈啟動腳本前綴。
`screen` 只用既有 seed 1–5 選參數，再保存 `frozen_matching.json`；6–10 另列穩定性檢查。
`extract` 對三個預先指定的擷取設定產生新 seed 11–20。
`fresh` 比較控制組、固定的配準參數、兩個擷取變體，以及擷取變體加固定配準參數的組合；每組都有 SVD / SVD+RANSAC。
`graph` 只以控制組的估計變換及三維樹位支持點選邊，完成推論後才用真值評分。
`report` 同時列出全部新 seed 結果、逐對成功次數與以 seed 為單位的配對比較。

`cm_extract_batch` 共用同一站、同一設定的步驟1–4，步驟5–8仍逐 seed 呼叫既有程式庫。
整合測試會確認兩個seed的樹位、叢集、保留率CSV與單次`cm_extract`逐位元組一致。

```bash
./build/cm_extract_batch --input=data/eth_trees/raw/trees/s1.ply --out=results/eth_trees/example/runs/s1 \
  --config=config/default.ini --cluster.tolerance=0.20 \
  --experiment.seed_base=11 --experiment.n_seeds=10 --scanner.x0=0 --scanner.y0=0 --scanner.z0=0
```

## 如何解讀

新 seed 只檢驗同場景下的抽樣與擬合敏感度，並非另一片森林的外部驗證。
圖路徑需要六站資料，與獨立兩站的 CircularMatch 任務不同；報告分別標示。
閾值10 cm、verticality 0.85等變體均不是論文原參數。
成功標準固定為旋轉小於2°且平移小於0.5 m；e_p對完整source點雲計算。
圖路徑的分塊NumPy評估還會對照一筆C++完整點雲誤差。

## 外部資料與試算表

已另取得 Liang2018 的 TreeTool 公開處理版六樣區與 LHD 參考樹；固定 ETH 的三組擷取設定直接轉用。流程需要 NumPy、SciPy，執行 `scripts/eth_trees/download_liang_samples.py` 後再執行 `scripts/eth_trees/validate_liang_trees.py`。這份資料僅能驗證提取階段，未提供分站點雲與配準真值。版本與評分限制見 `results/liang_trees/transfer_20261025/README.md`。

`OPENBLAS_NUM_THREADS=1 python3 scripts/eth_trees/make_study_workbook.py` 產生未重算的 `results/cross_dataset_20261025/runs/input/comparison.xlsx`。再用 LibreOffice Calc 重算至上層報告目錄；大型輸入保持在 Git 忽略目錄，結果摘要、來源、方法與全部逐對數據保存在白底灰表頭的試算表。

後續兩站失敗診斷與選用的匹配補救方法見 `docs/cn_recovery.md`。舊seed11–20用於本輪設計；固定方案後另用seed21–30比較原本推薦組與補救組，輸出於 `results/eth_trees/failure_followup_20261025/`，不覆寫原研究報表。
