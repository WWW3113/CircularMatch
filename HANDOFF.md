# 交接文件：CircularMatch C++ 復現（ETH Trees）

最後更新：2026-10-24。本文件整理到目前為止做過的所有事、做過的決定、結果在哪裡、哪些結果可信，以及還沒做完的事。
細節以各報告與程式為準；本文件是入口。

---

## 1. 一句話現況

論文（Sun et al., *CircularMatch*, IEEE GRSL 23, 2026, DOI 10.1109/LGRS.2025.3650098）的 keypoint 擷取、CN 描述子、
Algorithm 1 匹配與配準都已用 C++ 實作並在 **ETH Trees** 上評估。**目前沒有達到論文的成績**：
論文 ETH-tree 成功率 100%；我們最好的設定（improved + clustering 20 cm）在 15 對上平均 **84%**、官方 10 對 **92%**
（10 個 seed 平均）。差距主要在兩站距離遠、重疊少的配對。

---

## 2. 位置

| 項目 | 位置 |
|---|---|
| 程式碼 repo | GitHub `WWW3113/CircularMatch`（remote 顯示為 `WWW3113/circularmatch`，GitHub 會轉址），分支 `main`，直接提交在 main |
| 本機路徑（雲端容器） | `/home/user/circularmatch` |
| 論文 PDF | 使用者上傳，不在 repo 內。Table I–III 數字已抄進 `results/eth_trees/paper_comparison.md` |
| 原始資料 | `data/eth_trees/raw/trees/`（s1–s6.ply、`groundtruth/*.tfm`、`pairs.txt`），**不提交**，用 `scripts/eth_trees/download.sh` 下載 |
| 大型中間結果 | 所有 `results/**/runs/` 目錄，**不提交**（.gitignore） |
| 另一個 repo `6.24-6.29-` | 與本專案無關（使用者指定不要用） |

---

## 3. 環境與建置

```bash
sudo apt-get install libpcl-dev libgtest-dev        # PCL 1.14.0 驗證過；Eigen3、CMake ≥ 3.16、C++17
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/cm_tests                                    # 目前 75 個測試全過
```

- 產生／重算 xlsx 需要 `openpyxl`（pip）與 **LibreOffice Calc**（`apt-get install libreoffice-calc`）。
  只裝 `libreoffice-core` 時讀不了 xlsx，重算會一直逾時（曾踩過）。
- Python 腳本只用標準函式庫（`dtm_check.py` 需要 numpy；xlsx 腳本需要 openpyxl）。

### 執行檔

| 程式 | 用途 |
|---|---|
| `cm_extract` | 單次樹位擷取（一個 P(d) 版本、一個 seed）。`--version=step|linear_a|linear_mid|physical|none` |
| `cm_compare` | 比較實驗：baseline／improved 兩種 profile × 4 個 P(d) 版本 × N seed，含 budget-matched |
| `cm_register` | 一對 scan 的 CN 描述子匹配 + 配準 + 對 GT 評估。`--batch=jobs.txt` 可一次跑多對（點雲只讀一次） |
| `cm_inspect` | 檢查點雲檔（格式、點數、範圍、掃描儀是否在原點） |
| `cm_synth` | 產生合成測試場景 |

所有參數可用 `--section.key=value` 覆寫；每次執行輸出 `effective_config.ini`，論文未指定的項目標 `[UNSPECIFIED]`。

---

## 4. 實作內容與「論文 vs 我們」

### 4.1 Pipeline

DTM → 保留離 DTM 0–3 m → 1 cm voxel（保留最靠近重心的原始點）→ 法向量（octree 半徑 10 cm）→ verticality > 0.9 →
距離降採樣 P(d)（Bernoulli，共用隨機數 CRN）→ Euclidean clustering → 每群 RANSAC 圓柱（PCL）+ LM refit →
擬合後檢查 → 圓柱軸與 DTM 交點 = 樹位 → CN 描述子 → Algorithm 1 匹配 → SVD 求轉換。

### 4.2 分類

| 項目 | 論文 | 本實作 | 類別 |
|---|---|---|---|
| 0–3 m、1 cm voxel、法向半徑 10 cm、verticality 0.9 | 有 | 同 | 論文 |
| P(d) step（式 5） | 有（開區間） | d<5:0.5、5≤d<10:0.75、d≥10:1（端點歸屬為我們補的） | 論文 |
| P(d) linear_a / linear_mid / physical | 無 | 可切換 | 我們的修改 |
| P(d) none（=1，不降採樣） | 無 | `--version=none`，對照組 | 拿掉論文步驟 |
| DTM 方法 | 只寫「generated a DTM」 | 格網 supported_lowest + **坡度濾波** + 局部平面 | 實作選擇 |
| clustering 參數 | 未給 | 0.10 m / 30 / 1e7（最佳組合用 0.20 m） | 實作選擇 |
| RANSAC 參數 | 未給 | inlier 2 cm、normal weight 0.1、1000 次 | 實作選擇 |
| 擬合後檢查：半徑 3 cm–1 m、傾角 ≤ 20°、inlier 比例 ≥ 0.5 | **無** | 預設開，`cylinder.check_radius/check_tilt/check_inlier_ratio` | **論文沒有的方法** |
| 擬合後檢查：法向一致性、圓弧覆蓋角 | **無** | improved 開、baseline 關 | **論文沒有的方法（我們新增）** |
| LM refit、幾何距離計 inlier | 未提 | 有 | 實作選擇（LM refit 為使用者最初規格） |
| CN 描述子 N=183、閾值 5 cm、w=10、Th=40 | 有 | 同 | 論文 |
| 描述子／匹配的未指定細節 | — | 見 `docs/cn_ambiguities.md`（D1–D17） | 實作選擇 |
| 求轉換方法 | 未指定 | SVD（Kabsch）；另跑 SVD+RANSAC | 實作選擇 |
| 合併重複樹位 | 無 | `--merge_radius`，預設關（D9） | 論文沒有的方法 |
| 成功標準 | 未給 | 旋轉 < 2° 且平移 < 0.5 m（D10） | 我們定的 |

**baseline 與 improved 只差兩個開關**：`check_normal_consistency`、`check_arc_coverage`（baseline 關、improved 開）。
兩者都不是「論文原法」。最接近論文字面的是 5 個擬合後檢查全關（下稱「照論文字面」）。

---

## 5. 使用者做過的決定（依時間）

1. P(d) 四個版本：step、linear_a = min(1, 0.5+0.05d)、linear_mid = min(1, 0.375+0.05d)、physical = clip((d/d0)², Pmin, 1)；比較時含 budget-matched；CRN 可重現。
2. verticality 維持 0.9（0.8 只是可選參數）；DTM 預設 supported_lowest；法向一致性與圓弧覆蓋檢查是我們加的、可關閉；報告分 baseline（未啟用額外檢查）與 improved。
3. ETH 掃描儀位置接受由資料推得的 (0,0,0)（`scripts/eth_trees/stations.csv`）。
4. CN 描述子：「以論文為準，不要加論文沒有的方法」。D1 一律 2D；D3 gatekeeper 三個 bit 有一個符合即通過；D4 採內文（DC ≥ 3）；D6 triangle 頂點可置換、三邊都要符合；D7 SVD 與 SVD+RANSAC 分開報告；D8 baseline 與 improved 都跑；D9 合併預設關；D12 照字面並記錄頻率；e_p 用 source 的所有點。
5. 距離閾值：論文 5 cm 完整跑，另跑 5/10/15 cm 敏感度並標明不是論文設定。
6. **不加 4 自由度（只估 yaw + 平移）**，維持論文的 3D 剛體轉換。
7. **修 DTM，但只改論文沒指定的細節** → 加坡度濾波。
8. 「先盡量優化，然後標注並可開關」→ 加入三個擬合後檢查的開關、變體實驗、合併開關。
9. P(d) 比較的 xlsx：只比 P(d)，固定一個最佳組合，要有官方 10 對與 15 對；之後加入 none（不降採樣）。
10. 不做「降採樣 vs RANSAC 隨機來源」實驗（中途停掉並刪除腳本與中間檔）。

---

## 6. ETH Trees 資料

- 官方 trees.zip，解壓後約 1.9 GB：6 個 PLY，每個約 300 MB、約 2000 萬點，`download.sh` 可續傳（代理會切斷長連線）。
- 6 站 s1–s6；`pairs.txt` 列 10 對；`groundtruth/` 有全部 30 個方向的轉換。**`sA-sB.tfm` 把 sA 座標轉到 sB**（已用樹位驗證）。
- **沒有樹木真值**（沒有人工量的樹位），所以只能評估配準，不能算樹位 precision／recall。

---

## 7. 做過的事（時間線）與結論

| 階段 | 做了什麼 | 主要結論 | 報告 |
|---|---|---|---|
| A. keypoint 擷取 | C++ pipeline、4 種 P(d)、budget-matched、CRN、合成場景測試 | 合成場景上樹位誤差 < 2 cm | `README.md`、`tests/` |
| B. ETH 樹位（**舊 DTM**） | 6 站 × 4 P(d) × 10 seed，baseline／improved | 四種 P(d) 的圓柱數差異在 seed 標準差內 | `summary.md`、`baseline_report.md`、`improved_report.md` |
| C. CN 描述子與配準（**舊 DTM**） | 描述子、Algorithm 1、SVD／SVD+RANSAC | improved 4/10、baseline 1/10（官方 10 對，seed 1）；失敗主因是 tilt | `registration_report.md`、`registration_findings.md` |
| D. 找差距原因 | oracle 實驗（把對上的樹換成 GT 位置） | 樹位精確時 9–10/10 → 匹配程式本身沒問題；問題在樹位：水平誤差接近 5 cm，DTM 遠處被枝葉抬高造成 z 誤差（中位數 24–29 cm） | `gap_to_paper.md` |
| E. 修 DTM | 坡度濾波（Vosselman 2000，max_slope 0.5、tol 0.3 m、半徑 5 m；依近站地形量測決定） | 配對樹 z 誤差中位數 24–29 → 4–5 cm；improved 官方 10 對 4/10 → 8/10（seed 1） | `dtm_slope/README.md`、`dtm_slope/tree_consistency.md` |
| F. 變體與優化 | 20 個變體（照論文字面、各檢查單開、radius_min、門檻、clustering 距離、合併） | 照論文字面最差；只有傾角檢查有用；合併與 clustering 20 cm 有幫助（**只看 seed 1**） | `variants/variants.md`、`variants/findings.md` |
| G. 和論文比（10 seed） | 5 種設定 × 10 seed × 官方 10 對／15 對／30 對 | 最好 84%（15 對），論文 100% | `paper_comparison.md` |
| H. P(d) 配準比較（10 seed） | 6 種 P(d) × 3 種設定；最佳組合另做一次並加 none | 沒有一個版本顯著勝過 step；none（不降採樣）傾向較差 | `dtm_slope/retention_registration*.md`、`retention_comparison.xlsx` |

### 重要更正（不要被舊數字誤導）

- **單一 seed 的結果偏樂觀。** 之前說「improved + 合併 = 官方 10 對 10/10」只是 seed 1；10 seed 平均是 7.8/10。
  變體之間小於約 ±1.5 對的差異分不出是不是 seed 變動。
- **論文 ETH 用幾對不確定。** 其他方法的成功率 93/87/67% 用 10 對做不出來；15 對（6 站兩兩）最合理，30 對也符合。
  「官方 10 對 × 重複跑平均」不太可能（其他資料集的成功率全是 10 的倍數）。論文自述共 68 對，但用我們的假設兜不攏。
- **B、C、D 階段與 `variants/` 用的是舊 DTM 或單一 seed**，數字只供歷史參考。

---

## 8. 目前最可信的結果（新 DTM、10 seed、論文設定 5 cm、SVD）

### 8.1 與論文比較（`results/eth_trees/paper_comparison.md`）

| 設定 | 官方 10 對 | 15 對 | 30 對 |
|---|---|---|---|
| 論文 CircularMatch | – | 100% | 100% |
| 照論文字面（擬合後檢查全關） | 28% | 20% | 21% |
| baseline | 21% | 15% | 15% |
| improved | 73% | 65% | 65% |
| improved + 合併 0.3 m | 78% | 73% | 73% |
| **improved + clustering 20 cm（最佳）** | **92%** | **84%** | **84%** |

成功對的 e_p 中位數約 0.06–0.07 m（論文 0.153 m，但論文沒說怎麼算，且我們只平均成功的對，**不可直接比**）。
最難的是 s1-s5、s1-s6（兩站距離 26–32 m），最佳設定也只有約一半 seed 成功。

### 8.2 P(d) 比較（最佳組合；`results/eth_trees/dtm_slope/retention_comparison.xlsx`）

| P(d) | 官方 10 對 | 15 對 | 勝／輸 step | p |
|---|---|---|---|---|
| step（論文） | 92% | 84% | – | – |
| linear_a | 91% | 81% | 11／16 | 0.44 |
| linear_mid | 88% | 74% | 11／26 | 0.02 |
| physical | 91% | 83% | 15／17 | 0.86 |
| linear（同點數） | 89% | 79% | 12／19 | 0.28 |
| physical（同點數） | 94% | 83% | 13／15 | 0.85 |
| none（不降採樣） | 86% | 75% | 8／21 | 0.024 |

Bonferroni 門檻 0.05/6 ≈ 0.008，**沒有一個達到顯著**。none 沒有抽點隨機性，seed 間只差 RANSAC，但成功對數標準差仍約 1.6 對
→ RANSAC 本身就會造成明顯的 seed 間變動。

---

## 9. 結果檔案索引（`results/eth_trees/`）

| 檔案 | 內容 | 狀態 |
|---|---|---|
| `paper_comparison.md/.csv` | 與論文 Table I 比較，10 seed，10/15/30 對 | **最新** |
| `dtm_slope/retention_comparison.xlsx` | P(d) 比較試算表（最佳組合，含 none），全部用公式 | **最新** |
| `dtm_slope/retention_registration_ctol20.md/.csv` | 同上的原始數字 | **最新** |
| `dtm_slope/retention_registration.md/.csv` | P(d) 比較：baseline、improved、improved+合併 | 新 DTM、10 seed |
| `dtm_slope/README.md`、`tree_consistency.md` | DTM 坡度濾波的修改與效果 | 新 DTM |
| `dtm_slope/registration_report.md` | 新 DTM 的配準（seed 1，官方 10 對） | 單一 seed |
| `variants/variants.md`、`findings.md` | 20 個變體 | **單一 seed**，已加更正註記 |
| `gap_to_paper.md` | oracle 診斷 | 舊 DTM（結論仍成立） |
| `registration_report.md`、`registration_findings.md` | 第一次配準 | **舊 DTM** |
| `summary.md`、`baseline_report.md`、`improved_report.md` | 4 種 P(d) 的樹位層級比較 | **舊 DTM**，未重跑 |
| `diag_pair_dist.md`、`dtm_check.txt` | 配對樹距離、DTM 檢查 | 舊 DTM |
| `inspect/`、`per_scan/`、`smoke_s1/`、`run_status.csv`、`download_record.txt` | 資料檢查與早期輸出 | 歷史紀錄 |

`docs/cn_ambiguities.md`：描述子與匹配的所有歧義與決定（C1–C3、D1–D17）。

---

## 10. 如何重現

```bash
bash scripts/eth_trees/download.sh                        # 下載並解壓到 data/eth_trees/raw/
python3 scripts/eth_trees/run_all.py --seeds 10           # cm_compare 全部 6 站（舊流程；輸出 results/eth_trees/runs）

# 新 DTM、10 seed 的樹位（預設設定已含坡度濾波）
build/cm_compare --input=data/eth_trees/raw/trees/s1.ply --out=results/eth_trees/dtm_slope/seeds/runs/s1 \
    --config=config/default.ini --experiment.n_seeds=10 --scanner.x0=0 --scanner.y0=0 --scanner.z0=0 --quiet
#   最佳組合另加 --experiment.profiles=improved --cluster.tolerance=0.20，輸出到 seeds/ctol20/runs/<scan>
#   照論文字面：--experiment.profiles=baseline --cylinder.check_radius=false --cylinder.check_tilt=false
#               --cylinder.check_inlier_ratio=false，輸出到 seeds/literal/runs/<scan>
#   none：build/cm_extract --version=none --seed=K --cluster.tolerance=0.20 ...，輸出到 seeds/none_ctol20/runs/<scan>，
#         再以絕對路徑 symlink 成 seeds/ctol20/runs/<scan>/improved/none_raw_seed<K>_trees.csv

python3 scripts/eth_trees/compare_paper.py --seeds 10              # → paper_comparison.md/.csv
python3 scripts/eth_trees/compare_retention.py                     # → dtm_slope/retention_registration.*
python3 scripts/eth_trees/compare_retention.py --runs results/eth_trees/dtm_slope/seeds/ctol20/runs \
    --profiles improved:0.0 --with_none --out results/eth_trees/dtm_slope/retention_registration_ctol20
python3 scripts/eth_trees/make_retention_xlsx.py                   # → retention_comparison.xlsx
python3 <xlsx skill>/scripts/recalc.py results/eth_trees/dtm_slope/retention_comparison.xlsx 120
python3 scripts/eth_trees/run_variants.py                          # 20 個變體（單一 seed）
python3 scripts/eth_trees/oracle_check.py <空目錄>                  # oracle 診斷
python3 scripts/eth_trees/tree_consistency.py --runs 名稱=目錄 ...   # 兩站樹位一致性
```

耗時參考（4 核容器）：cm_compare 10 seed 6 站約 15 分鐘（3 平行）；cm_extract 每站約 37 秒；
cm_register 每次約 4 秒（主要在讀 300 MB 點雲），用 `--batch` 後每對只需毫秒級。

---

## 11. 已知限制與未解問題

1. **沒有達到論文成績**，困難在低重疊配對（s1 與 s4/s5/s6）。論文說 triangle 比對可處理「重疊在周邊」的情況，我們的 triangle 路徑在這些對上沒有發揮作用。尚未深入分析原因。
2. **所有論文沒有的設定都在 ETH 上挑選**，沒有獨立驗證資料 → 結果偏樂觀。
3. **成功標準是我們定的**；論文的 e_p 統計方式不明，e_p 不能直接比。
4. **論文 ETH 用幾對不確定**（推測 15）；論文內也有不一致（內文 Plot1、3、5 vs 表格 Plot1–4；列了 Tongji-tree 但 Table I 沒有；68 對兜不攏）。
5. **樹位沒有真值驗證**；圓柱數含重複偵測（improved 約 22% 在 0.5 m 內有另一個樹位）。
6. **seed 間變動大**（約 ±1–2 對 / 15 對），只有 10 seed 平均的數字可信；RANSAC 本身就會造成這個變動。
7. 舊 DTM 的樹位層級報告（`summary.md` 等）沒有用新 DTM 重跑。
8. 坡度濾波在比 27° 更陡的地形會誤刪上坡地面（`max_slope = 0.5`）。

---

## 12. 待辦／建議下一步

- **其他資料集驗證**（最重要，用來確認在 ETH 上挑的設定是否過擬合）：
  - Tongji-Trees：使用者本機有資料，需先在本機跑 `cm_inspect`（步驟見 `scripts/OTHER_DATASETS.md`）。
  - Liang et al. 2018：laserscanning.fi 當時維護中，web.archive.org 被網路政策擋住，尚未下載。
- 分析低重疊配對失敗的原因（描述子的 sector 被不重疊區的樹佔據？triangle 候選集太小？）。
- 視需要用新 DTM 重跑樹位層級報告（`summary.md`、`baseline_report.md`、`improved_report.md`）。
- 若要確認論文的配對數，可聯絡作者或找公開程式碼。

---

## 13. 注意事項（踩過的坑）

- **不要用 `pkill -f <字串>`**：會連同執行它的 shell 一起砍掉；改用 `pkill -f "[s]office"` 這種寫法。
- `cm_compare` 最後才寫檔，跑的時候輸出目錄看起來是空的屬正常。
- 中斷的抽取可能留下不完整檔案；`run_variants.py` 等腳本會跳過已存在的檔案，重跑前要先刪掉不完整的輸出。
- symlink 請用絕對路徑（相對路徑層數容易算錯）。
- 提交只放小檔（報告、CSV、腳本）；原始資料與 `runs/` 永遠不提交。
- `cm_register` 讀點雲很慢，大量配準一定要用 `--batch` 並依 source 掃描分組。
