# 用同一套工具跑其他資料集（例如 Tongji-Trees）

`scripts/eth_trees/` 的工具不綁定 ETH，只要提供路徑即可。以下在本機（Linux / WSL2）執行。

1. 放資料（不要提交到 git）：`data/<dataset>/raw/...`
2. 逐檔檢查格式、點數、範圍、掃描儀是否在原點：
   ```bash
   for f in data/<dataset>/raw/*.<ply|las|pcd|xyz>; do
     ./build/cm_inspect --input="$f" > "results/<dataset>/inspect/$(basename "$f").txt"
   done
   ```
   支援 xyz/txt/csv、pcd、ply、未壓縮 las；`.laz` 需先解壓。
   檔案若是大型測量座標，`--origin=x,y,z` 給候選站位再檢查。
3. 寫 `scripts/<dataset>/stations.csv`（欄位同 `scripts/eth_trees/stations.csv`）：
   每個 scan 的路徑、掃描儀位置、位置來源。沒有可驗證來源的 scan 留空，runner 會跳過。
4. 執行與彙整：
   ```bash
   python3 scripts/eth_trees/run_all.py --stations scripts/<dataset>/stations.csv \
       --out results/<dataset>/runs --status results/<dataset>/run_status.csv --seeds 10
   python3 scripts/eth_trees/aggregate.py --runs results/<dataset>/runs --out results/<dataset>/summary.md
   python3 scripts/eth_trees/baseline_report.py --runs results/<dataset>/runs \
       --out results/<dataset>/baseline_report.md --title "<dataset>"
   ```
5. 只有在資料集附**人工量測的參考樹位**時，才在 `run_all.py` 後面加 `--ref=...`（cm_compare 的參數），
   計算 precision / recall / 樹位誤差。只有 registration ground truth（轉換矩陣）時不要計算。
