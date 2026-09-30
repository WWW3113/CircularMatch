# ETH Trees：接入與執行流程

資料：ETH Trees（Theiler et al., *Globally Consistent Registration of Multiple Point Clouds*）。
官方頁面：https://prs.igp.ethz.ch/research/completed_projects/automatic_registration_of_point_clouds.html
中的「Download Trees (ZIP, 1.2 GB)」。

範圍：只用現有的 keypoint extraction 與 `cm_compare`。不做 descriptor、matching、registration，
不計算 rotation / translation error，也不把 pairwise transformation 當參考樹位。
**本資料集沒有人工標註的參考樹位，所以不計算 precision、recall、樹位誤差**；結果只代表
「程式成功執行」，不代表「樹位準確度已驗證」。

## 目前狀態（2026-09-30）

**程式在 6 個 scan 上全部成功執行；樹位準確度未驗證。** 本資料集只有 pairwise transformation
ground truth，沒有人工標註的參考樹位，因此沒有計算 precision、recall 或樹位誤差。

### 資料

| 項目 | 值 |
|---|---|
| 官方連結 | 「Globally Consistent Registration of Multiple Point Clouds」段落的「Download Trees (ZIP, 1.2 GB)」 |
| 實際網址 | `https://ethz.ch/content/dam/ethz/special-interest/baug/igp/photogrammetry-remote-sensing-dam/documents/sourcecode-and-datasets/PascalTheiler/trees.zip` |
| 大小 / SHA-256 | 1,282,864,958 bytes / `ecb3f5e8791804341a3e7837c29f25789987c99fdf575aa3ebfbfd67d3acd37c`（伺服器 Last-Modified 2015-11-16） |
| 解壓 | `data/eth_trees/raw/trees/`：37 檔、1.94 GB |
| 內容 | `s1.ply`–`s6.ply`（binary little-endian PLY，float x y z intensity，19.6–21.6 M 點，由 PCL 產生，header 為 CRLF 換行）；`groundtruth/sX-sY.tfm`（30 個 4×4 pairwise 轉換）；`pairs.txt`（10 組重疊 pair） |
| 單位 / 座標 | 公尺；每站約 100 × 100 × 45 m；已整平（GT 旋轉幾乎只有繞 z 軸） |
| 讀取 | 現有 PCL PLY 讀取器直接讀取，**未轉檔**；header 點數與讀入點數一致、無非有限值 |

逐檔摘要見 `results/eth_trees/inspect/s*.txt`（`build/cm_inspect` 產生）。

下載時 proxy 會在約 9 分鐘後切斷長連線，所以 `download.sh` 用 HTTP range 續傳，並核對大小。

### 掃描儀站位：資料驗證，非 metadata

PLY header、官方頁面、官方原始碼（`global_consistency.zip`）與 sample program 都**沒有**寫出掃描儀位置。
各 scan 的站位 (0, 0, 0) 是用 `cm_inspect` 以三項可重現的檢查驗證的，**不是**文件或 metadata 的記載：

1. 所有點的 3-D 距離都在以 (0, 0, 0) 為中心的 50.0 m 內，超過的點數為 0（掃描儀端的距離截斷）；
2. 原點水平 0.5 m 內幾乎沒有點（腳架下方盲區）；
3. 仰角 lattice 分數在原點最高（0.314–0.341），偏移 ±2 cm 到 1 m 或換成 scan 重心後都 ≤ 0.261。

來源文字寫在 `stations.csv` 的 source 欄。若不接受資料驗證作為站位來源，以下 P(d) 結果應視為無效。

### 執行

```bash
python3 scripts/eth_trees/run_all.py --seeds 10      # 6 scans：config/default.ini、四種 P(d)、各 10 seed、baseline + improved
python3 scripts/eth_trees/aggregate.py               # -> results/eth_trees/summary.md
for p in baseline improved; do for s in s1 s2 s3 s4 s5 s6; do
  python3 scripts/eth_trees/dtm_check.py --ply data/eth_trees/raw/trees/$s.ply \
      --trees results/eth_trees/runs/$s/$p/step_raw_seed1_trees.csv; done; done   # 需要 numpy
```

每站 330–408 s、峰值記憶體 1.4–1.5 GB（4 核容器）。見 `results/eth_trees/run_status.csv`。

### 結果摘要（詳見 `results/eth_trees/summary.md`、`per_scan/`）

跨 6 站（每站 10 seed 平均的平均 ± 標準差），raw P(d)：

| 設定 | 版本 | 保留點數 | 樹幹數 | RANSAC 成功率 | 步驟 5–8 [ms] |
|---|---|---|---|---|---|
| 基準設定 | step | 321,144 ± 16,122 | 323.2 ± 34.3 | 0.377 ± 0.072 | 2713 ± 215 |
| 基準設定 | linear_a | 346,649 ± 16,229 | 330.9 ± 34.9 | 0.369 ± 0.069 | 3177 ± 329 |
| 基準設定 | linear_mid | 318,820 ± 14,735 | 324.9 ± 35.2 | 0.379 ± 0.073 | 2663 ± 251 |
| 基準設定 | physical | 270,981 ± 11,879 | 308.5 ± 33.0 | 0.401 ± 0.076 | 2037 ± 190 |
| 改良設定 | step | 321,144 ± 16,122 | 78.5 ± 21.2 | 0.094 ± 0.037 | 2716 ± 230 |
| 改良設定 | linear_a | 346,649 ± 16,229 | 78.9 ± 21.6 | 0.090 ± 0.036 | 3180 ± 345 |
| 改良設定 | linear_mid | 318,820 ± 14,735 | 78.5 ± 21.2 | 0.094 ± 0.037 | 2665 ± 274 |
| 改良設定 | physical | 270,981 ± 11,879 | 77.5 ± 21.4 | 0.103 ± 0.040 | 2038 ± 216 |

- 四種 P(d) 對樹幹數的影響很小（同一設定內差距 < 7%），主要差在保留點數與耗時；
  budget-matched 後三種形狀的樹幹數幾乎相同。
- 主要失敗原因：`radius_out_of_range`（每站 255–565 叢，多為 r < 3 cm 的細小叢，推測是林下灌木／枝條），
  基準設定其次為 `tilt_too_large`，改良設定其次為 `normal_inconsistent`。
- 共用步驟 1–4 每站約 30–35 s，其中法向量計算約 22 s。

### 已知問題（`results/eth_trees/dtm_check.txt`，step / raw / seed 1）

| 設定 | 樹幹數（6 站） | 基部高於鄰近最低點 > 1 m | 0.5 m 內重複偵測 |
|---|---|---|---|
| 基準設定 | 286–380 | 48–101 | 100–163 |
| 改良設定 | 53–119 | 13–35 | 8–35 |

1. **遠距離 DTM 高估。** 地形是坡地（s1 在 20–30 m 處各方位最低點差約 9 m）。遠處地面被遮擋時，
   DTM 可能比實際地面高出數公尺，0–3 m 高度切片於是包含樹冠或枝條，被擬合成「樹幹」。
   被標記的樹都在距掃描儀 ≥ 18 m 處。
2. **重複偵測。** 同一樹幹在高度方向被分成數個 cluster，各自擬合成一棵「樹」，樹幹數因此高估。
3. 以上兩項只是診斷，不是準確度驗證；「樹幹數」應解讀為「通過檢查的圓柱數」，不等於實際樹木數。

## 在 WSL2 重現

```bash
sudo apt-get install libpcl-dev libgtest-dev curl unzip python3 python3-numpy   # numpy 只給 dtm_check.py 用
git clone https://github.com/WWW3113/CircularMatch.git && cd CircularMatch
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
./build/cm_tests

# 1. 下載 + SHA-256 + 解壓（官方「Download Trees (ZIP, 1.2 GB)」連結）
scripts/eth_trees/download.sh 'https://ethz.ch/content/dam/ethz/special-interest/baug/igp/photogrammetry-remote-sensing-dam/documents/sourcecode-and-datasets/PascalTheiler/trees.zip'
#    -> data/eth_trees/trees.zip, data/eth_trees/raw/, results/eth_trees/download_record.txt

# 2. 填 scripts/eth_trees/stations.csv：每個 scan 的路徑、掃描儀位置、位置來源
#    （沒有可驗證來源的 scan 留空，runner 會跳過）

# 3. 逐站執行（config/default.ini、四種 P(d)、每種 10 seed、baseline + improved）
python3 scripts/eth_trees/run_all.py --seeds 10
#    單站 smoke test：python3 scripts/eth_trees/run_all.py --only <scan>

# 4. 彙整
python3 scripts/eth_trees/aggregate.py
#    -> results/eth_trees/summary.md
```

`data/` 與 `results/**/runs/`（每站完整輸出）不進 git；`results/eth_trees/` 下的
`download_record.txt`、`run_status.csv`、`summary.md`、`dtm_check.txt`、`inspect/`、`per_scan/`、`smoke_s1/`
是可提交的精簡結果。
