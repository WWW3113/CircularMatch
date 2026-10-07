# CircularMatch — keypoint extraction（樹幹位置擷取）C++ 復現

復現 Sun et al., *CircularMatch: Efficient Registration of Forest Terrestrial Point
Clouds Using Circular Neighborhood Descriptor*, IEEE GRSL 23, 2026 的 **keypoint extraction**
（第 II 節前半），並讓距離相關降採樣的保留機率 P(d) 可切換四個版本做比較。
**本階段不含 CN descriptor 與 matching。**

> **驗證狀態：只在合成資料上驗證過，尚未在真實森林 TLS 資料上驗證。**
> 目前沒有真實掃描與參考樹位；下文所有數字（誤差、recall、門檻值）都來自合成場景
> （斜坡 + 5 根已知樹幹），門檻值也是在同一個合成場景上定出來的。

## 論文設定與實作選擇

論文只給出部分參數。本專案把每個決定分成三類，程式的 `effective_config.ini` 也把
「論文未指定」的項目標成 `[UNSPECIFIED]`。

| 項目 | 論文設定 | 本專案 | 類別 |
|---|---|---|---|
| 高度範圍 | 0–3 m | 0–3 m（負向容差 5 cm） | 論文（容差為實作選擇） |
| Voxel | 1 cm，保留最靠近重心的點 | 同左 | 論文 |
| 法向量鄰域 | 半徑 10 cm | 同左 | 論文 |
| Verticality | > 0.9 | **預設 > 0.9**；0.8 僅為可選參數，不是論文設定 | 論文 |
| 距離降採樣 P(d) | 階梯式（開區間） | 四個版本可切換（見下） | 論文 + 我們的修改 |
| DTM 產生方法 | 未指定 | **預設 `supported_lowest`**；`percentile` 保留供比較 | 實作選擇 |
| inlier 計數方式 | 未指定 | 幾何點到圓柱面距離 | 實作選擇 |
| LM refit | 未提及 | 在幾何 inlier 上 refit 2 次 | 實作選擇（任務規格要求 LM refit） |
| 法向一致性檢查 | 無 | 可關閉，`cylinder.check_normal_consistency` | 我們新增 |
| 圓弧覆蓋角檢查 | 無 | 可關閉，`cylinder.check_arc_coverage` | 我們新增 |
| 其他數值參數 | 未指定 | 見「論文未指定的參數」 | 實作選擇 |

**本專案沒有任何一種設定可稱為「論文原法」**：即使關閉兩項新增檢查，DTM、inlier 計數、
LM refit 仍是我們的實作選擇。

### 兩種報告設定

`cm_compare` 預設同時跑兩種設定，結果分開列表、分開存放：

| 設定 | 目錄 | 法向一致性 | 圓弧覆蓋角 |
|---|---|---|---|
| **未啟用額外檢查的基準設定**（baseline） | `baseline/` | 關 | 關 |
| **啟用檢查的改良設定**（improved） | `improved/` | 開 | 開 |

兩者的步驟 1–4 相同（只算一次），差別只在步驟 7 的擬合後檢查。`--experiment.profiles=baseline`
或 `=improved` 可只跑其中一種。`cm_extract` 與程式庫的預設為兩項檢查開啟，可用
`--cylinder.check_normal_consistency=false --cylinder.check_arc_coverage=false` 關閉。

## 建置

需求：PCL ≥ 1.10（已在 1.14.0 驗證）、Eigen3、GoogleTest、CMake ≥ 3.16、C++17。

```bash
# Ubuntu 24.04
sudo apt-get install libpcl-dev libgtest-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/cm_tests            # 或 ctest --test-dir build
```

## 使用

```bash
# 單次執行（一個版本、一個 seed）
./build/cm_extract --input=scan.las --out=out --version=step --seed=1 \
    --scanner.x0=512345.678 --scanner.y0=4412345.678 --scanner.z0=123.4

# 比較實驗：baseline / improved 兩種設定 × 4 版本 × N seed，各有原始表 + budget-matched 表
./build/cm_compare --input=scan.las --out=cmp --ref=ref_trees.csv --experiment.n_seeds=10 \
    --config=config/default.ini --scanner.x0=... --scanner.y0=... --scanner.z0=...

# 合成測試場景（斜坡 + 5 根樹幹，其中 2 根傾斜）
./build/cm_synth --out=syn.las --truth=syn_truth.csv [--offset_x=512345.678 ...]
```

任何參數都可用 `--section.key=value` 覆寫，或寫在 INI 檔（`--config=`）。每次執行都會把
**實際生效的完整參數**寫到 `effective_config.ini`，其中「論文未指定」的項目標
`[UNSPECIFIED]`。

### 輸入格式

| 副檔名 | 讀取方式 |
|---|---|
| `.xyz .txt .csv .asc` | ASCII，每行前三個數值欄為 x y z；空白、tab、逗號、分號皆可；非數值行（標頭、註解）略過並計數 |
| `.pcd` | PCL（ascii / binary / binary_compressed）；x/y/z 為 float64 時保留 double 精度 |
| `.ply` | PCL（ascii / binary） |
| `.las` | **自寫精簡讀取器**：未壓縮 LAS 1.0–1.4、點格式 0–10，只讀 XYZ（scale/offset 套用後為 double） |
| `.laz` | **不支援**，直接報錯並提示先解壓（`laszip -i in.laz -o out.las` 或 `pdal translate`） |

LAS 會明確報錯的情況：非 `LASF` 簽章、版本不在 1.0–1.4、點格式 > 10、**點格式超出該版本允許範圍**
（1.0/1.1 ≤ 1、1.2 ≤ 3、1.3 ≤ 5、1.4 ≤ 10）、壓縮位元（format byte bit 6/7，即 LAZ）、
record length 小於該格式最小值、header size 過小、檔案截斷、1.4 的 legacy 與 64-bit 點數不一致、
scale ≤ 0。

參考樹位檔（`--ref`）：ASCII，每行 `x,y[,z]`（原始座標），非數值行略過。

## 座標、單位與掃描儀位置

- 單位公尺；假設 z 軸鉛直（掃描已整平）。
- 讀檔一律用 double，再平移到**以掃描儀為原點的局部座標**：`local = original − (x0, y0, z0)`，
  之後才轉成 PCL 的 float。輸出 CSV 再加回 offset，也就是原始座標。
- **大型測量座標防護**：若 |局部座標| > `scanner.max_local_coord`（預設 1e4 m，float 精度約 1 mm）就**停止並報錯**：
  - 掃描儀位置仍是預設原點 → 要求用 `--scanner.x0/y0/z0` 提供正確位置；
  - 已提供但仍過遠 → 提示位置可能錯誤（座標系、x/y 對調、單位）。
  - 不會在精度不足的情況下繼續跑。
- 讀檔後印出 bbox、最近點到掃描儀的水平距離；最近點 > `scanner.nearest_warn`（預設 3 m）時警告。
  範圍 < 1 m、> 5 km 或高差 > 200 m 時也會警告（可能單位或軸向錯誤）。
- d = 水平距離 `sqrt(x² + y²)`（局部座標），`--scanner.use_3d_distance=true` 可改為 3D 距離。

## Pipeline

| # | 步驟 | 實作 | 依據 |
|---|---|---|---|
| 1 | DTM + 高度濾波 | 網格法：每格選一個地面樣本點（保留 xyz）。預設 `supported_lowest`：上方 5 cm 內至少有 3 點的最低點；可選 `percentile`：第 k 百分位的點。→ **坡度濾波**（`slope_filter`，預設開；Vosselman 2000）：若某樣本比 `slope_radius` 內任一樣本高出 `max_slope·d + slope_tol`，剔除（遠處地面被遮蔽時，格內最低點是枝葉）→ 剔除高於鄰格平面 `outlier_above` 的樣本 → 每格以 (2r+1)² 鄰格樣本做最小平方平面（同時平滑與補洞，坡地無偏）→ 查詢時對 4 個最近格的平面做雙線性混合。保留 `zmin − neg_tol ≤ z − DTM ≤ zmax` | 論文：0–3 m；**DTM 方法為實作選擇** |
| 2 | Voxel 1 cm | 每格保留**最靠近格內重心的原始點**（自寫，PCL VoxelGrid / UniformSampling 皆不符） | 論文 |
| 3 | 法向量 | octree 半徑 10 cm 鄰域、共變異數矩陣、最小特徵值對應的特徵向量 e3 | 論文 eq. 1–3 |
| 4 | Verticality | `1 − |z·e3| > 0.9`（0.8 為可選參數，非論文設定） | 論文 eq. 4 |
| 5 | 距離相關降採樣 | 依 P(d) 做 Bernoulli 抽樣（見下） | 論文 eq. 5（已改） |
| 6 | Euclidean clustering | PCL `EuclideanClusterExtraction` | 論文 §III 提及 |
| 7 | RANSAC 圓柱 | PCL `SampleConsensusModelCylinder` + `RandomSampleConsensus`（論文）；LM refit 於幾何 inlier、幾何 inlier 計數（實作選擇）；法向一致性、圓弧覆蓋角檢查（我們新增，可關閉） | 論文 + 實作選擇 |
| 8 | 樹位 | 圓柱軸與 DTM 的交點，沿軸迭代求解 | 論文 |

步驟 1–4 與 P(d)、seed 無關，比較實驗中**只算一次、所有版本共用**；步驟 5–8 每個版本 × seed 各跑一次。

### 步驟 8：軸與 DTM 交點

軸 `x(t) = p + t·v`，解 `f(t) = p_z + t·v_z − DTM(p_x + t·v_x, p_y + t·v_y) = 0`。
軸傾斜時**不可**只取 (p_x, p_y) 處的 DTM 高度。
先用不動點迭代 `t ← (DTM(x(t), y(t)) − p_z) / v_z`，在 |坡度 · tan(傾角)| < 1 時收斂；
不收斂時改為沿軸 ±`search_range` 找變號區間再二分。找不到交點則記為失敗 `no_dtm_intersection`。

## P(d)：四個版本

P(d) 是**保留**機率：近處少留、遠處多留，因為遠處點本來就稀。每點獨立做 Bernoulli 抽樣。

| 版本 | 公式 | 來源 |
|---|---|---|
| `step` | d<5 → 0.5；5≤d<10 → 0.75；d≥10 → 1 | 論文原式（端點為我們的補法） |
| `linear_a` | min(1, 0.5 + 0.05·d) | 我們的修改 |
| `linear_mid` | min(1, 0.375 + 0.05·d) | 我們的修改 |
| `physical` | clip((d/d0)², P_min, 1)，d0 = 10 m，P_min = 0.1 | 我們的修改 |

- **step**：論文用開區間，d = 0、5、10 處未定義。上表的閉／開端點是我們的補法。
- **linear_a**：在 d = 0、5、10 與 step 各段起點相同，但在所有 d 都 ≥ step，所以保留總量必然較多。
- **linear_mid**：只有在「點沿 d 均勻分布」時，[0,5)、[5,10) 兩段的平均才與 step 相同；d ≥ 10 則不同。
  真實點雲並非均勻分布，實際保留量不會相同。
- **physical**：假設 TLS 點密度約隨 1/d² 衰減，使各距離保留後的密度接近一致。
- **不要假設各版本保留量相同**：程式一律實測並輸出保留點數，並附每 1 m 距離分組的
  「實際保留率 vs 理論平均 P(d)」對照（`*_retention_bins.csv`）。
- 所有版本的輸出都 clamp 到 [0, 1]（budget matching 時截距可能變成負值）。

### Common random numbers（可重現性）

- 每點 `u = splitmix64(seed, round(x/0.1mm), round(y/0.1mm), round(z/0.1mm))` ∈ [0, 1)，
  只由 seed 與座標決定，與處理順序無關。
- 四個版本對同一點用**同一個 u**，點被保留的條件為 `u < P(d)`。版本間差異只來自 P(d)。
- 因此只要逐點 P_A ≤ P_B，A 保留的點集必為 B 的子集。預設參數下 step ⊂ linear_a、linear_mid ⊂ linear_a。

### Budget-matched

- 目標 = 同一 seed 下 step 的**實際**保留點數。linear 版本調截距，physical 調 d0（P_min 固定），用二分法。
- 容差 `max(budget.abs_tol, budget.rel_tol · target)`，預設 max(1, 0.1%)，每個 seed 各自校準。
- linear_a 與 linear_mid 只差截距，對齊保留量後會變成**同一個函數**，因此 budget-matched 表合併為一欄 `linear*`。

## PCL RANSAC 的亂數（已在安裝版本 PCL 1.14.0 驗證）

直接讀已安裝的 PCL 1.14.0 標頭檔，並用測試實際確認：

- `SampleConsensusModel` 在 `random=false`（預設）時，以常數 `12345u` 初始化 protected 的
  `boost::mt19937 rng_alg_`；`rng_gen_` 持有 `rng_alg_` 的**參考**。
- `RandomSampleConsensus` 的所有抽樣都經由 model 的產生器；它的 `threads_` 預設 −1（不開 OpenMP）。
- 結論：PCL 預設**可重現但固定**（不隨我們的 seed 改變）。本專案繼承 cylinder model 並提供
  `reseed()`，每叢 RANSAC seed = hash(run seed, 量化後的叢重心)，與叢的處理順序無關。
  並明確設定 `setNumberOfThreads(-1)`。
- 測試 `CylinderSeed.*` 驗證：
  1. 同 seed 抽樣序列相同、不同 seed 不同；
  2. 未 reseed 的 PCL model 抽樣序列 = `reseed(12345)`，證實上述預設行為；
  3. 同 seed 擬合結果逐位元相同。
- `cylinder.seed_mode=fixed` 可讓 RANSAC seed 不隨 run seed 變動，用來把抽樣變異和 RANSAC 變異分開。

## 論文未指定的參數（全部可調，預設見 `config/default.ini`）

| 參數 | 預設 | 說明 |
|---|---|---|
| `dtm.cell` | 0.5 m | DTM 網格大小 |
| `dtm.ground_select` | `supported_lowest` | **實作選擇**。`supported_lowest`（預設）或 `percentile`（保留供比較，見「已知限制」） |
| `dtm.percentile` | 5 | 僅 `percentile` 使用：第 k 百分位數 |
| `dtm.min_points` | 3 | 格內點數少於此則無地面樣本 |
| `dtm.fit_radius` | 2 格 | 平面擬合視窗半寬（5×5） |
| `dtm.fill_max_radius` | 5 格 | 補洞時視窗最大半寬 |
| `dtm.outlier_above` | 0.5 m | 高於鄰格平面此值的地面樣本剔除 |
| `dtm.slope_filter` | true | **實作選擇**。坡度濾波（Vosselman 2000）；在 ETH Trees 上發現遠處 DTM 被枝葉抬高數公尺後加入（`results/eth_trees/dtm_slope/`） |
| `dtm.max_slope` / `slope_tol` / `slope_radius` | 0.5 / 0.3 m / 5 m | 坡度濾波參數。0.5（約 27°）依 ETH 近站地形量測決定（相鄰格坡度中位數 0.11–0.16），**不是**以配準結果調整；地形比 `max_slope` 陡時上坡地面會被誤刪 |
| `dtm.support_count` / `support_dz` | 3 / 0.05 m | 僅 `supported_lowest` 使用：候選點上方 `support_dz` 內至少 `support_count` 點 |
| `height.neg_tol` | 0.05 m | 高度濾波負向容差 |
| `normals.octree_res` | 0.05 m | octree 解析度 |
| `normals.min_neighbors` | 5 | PCA 最少鄰點數，不足則丟棄該點 |
| `cluster.tolerance` / `min_points` / `max_points` | 0.10 m / 30 / 1e7 | Euclidean clustering |
| `cylinder.dist_threshold` | 0.02 m | RANSAC 與幾何 inlier 距離 |
| `cylinder.max_iterations` / `probability` | 1000 / 0.99 | RANSAC |
| `cylinder.radius_min` / `radius_max` | 0.03 / 1.0 m | 擬合後檢查 |
| `cylinder.normal_weight` | 0.1 | RANSAC normal distance weight |
| `cylinder.max_tilt_deg` | 20° | 軸傾角上限 |
| `cylinder.min_inlier_ratio` | 0.5 | inlier 比例；**inlier 以幾何距離計數，為實作選擇** |
| `cylinder.check_normal_consistency` | true | **我們新增**，可關閉：法向一致性檢查 |
| `cylinder.min_normal_ratio` / `normal_max_angle_deg` | 0.75 / 30° | 法向一致性門檻（合成場景上定出） |
| `cylinder.check_arc_coverage` | true | **我們新增**，可關閉：圓弧覆蓋角檢查 |
| `cylinder.min_arc_deg` | 100° | 圓弧覆蓋角門檻（合成場景上定出） |
| `cylinder.lm_passes` | 2 | **實作選擇**：在幾何 inlier 上做 LM refit 的次數 |
| `intersect.tol` / `max_iter` / `search_range` / `scan_step` | 1 mm / 50 / 5 m / 0.1 m | 軸與 DTM 交點 |
| `scanner.x0/y0/z0` | 原點 | 掃描儀位置（原始座標） |
| `scanner.nearest_warn` / `max_local_coord` | 3 m / 1e4 m | 防護閾值 |

### 擬合後檢查與失敗原因

RANSAC 內**不**套用半徑／傾角限制。否則 PCL 會默默丟掉不合格的模型，只會顯示成「無解」。
所有限制都在擬合後檢查，每叢記錄第一個失敗原因（依序）和全部失敗的 bitmask：

`too_few_points`（點數不足）→ `no_model`（RANSAC 無解）→ `low_inlier_ratio`（inlier 比例過低）→
`radius_out_of_range`（半徑超出範圍）→ `tilt_too_large`（軸傾角過大）→
`normal_inconsistent`（法向量不一致，**新增**）→ `arc_coverage_low`（覆蓋角過小，**新增**）→
`no_dtm_intersection`（軸與 DTM 無交點）

RANSAC 成功率 = 通過所有檢查且有樹位的叢數 / 送進 RANSAC 的叢數。

`normal_inconsistent` 與 `arc_coverage_low` 只在對應檢查開啟時才會成為失敗原因；關閉時兩項指標
仍會計算並寫進 `*_clusters.csv`（`n_normal_ok`、`arc_deg`），方便比較。

### 實作選擇與新增檢查的理由（皆在合成資料上觀察到）

1. **inlier 以幾何距離計數（實作選擇，非論文原法）。** 論文未說明 inlier 如何計數。PCL 的 inlier 距離是
   `w·法向夾角 + (1−w)·歐氏距離`；w = 0.1 時，法向誤差 11° 就超過 2 cm，而細樹幹（r = 8 cm）用 10 cm 鄰域
   算出的法向量正是這麼差，正確擬合會被誤判為 inlier 過少（0.46）。現在比例檢查和輸出的 `n_inliers` 用
   ‖q − 軸‖ − r 的幾何距離；PCL 的加權計數另存為 `n_inliers_sac`。
2. **LM refit 在幾何 inlier 上做（實作選擇）。** 用 PCL 加權 inlier refit 時，10° 傾斜樹幹的半徑系統性偏低
   1.1–1.5 cm；改用幾何 inlier 後，合成資料上誤差 ≤ 0.64 cm。
3. **法向一致性檢查（我們新增，可關閉）。** 15° 傾斜樹幹經 verticality > 0.9 過濾後碎成窄條，RANSAC 會在碎片上
   擬合出 r ≈ 4–6 cm 的假樹幹，而且幾何 inlier 比例很高。正確擬合的法向一致比例為 0.89–1.00，碎片為 0.15–0.65。
4. **圓弧覆蓋角檢查（我們新增，可關閉）。** 窄弧無法約束半徑：15° 樹幹的主條帶曾以 r = 0.17（真值 0.30）
   通過其他所有檢查。正確擬合的覆蓋角為 120–195°，錯誤的條帶擬合為 5–80°。

兩項新增檢查的門檻都是在合成場景上定出來的，**尚未在真實資料上確認**。真實樹幹受遮擋、可見弧 < 100°
時會被拒絕，recall 可能下降。

### 合成場景結果：兩種設定分開列出

合成場景 10 seed，每種設定 60 次執行（raw 4 版本 + budget-matched 2 版本），UTM 級座標 + 掃描儀位置：

| 指標 | 未啟用額外檢查的基準設定 | 啟用檢查的改良設定 |
|---|---|---|
| 直立樹幹（3 根 × 60）最大水平誤差 / 最大半徑誤差 | 1.81 cm / 1.12 cm | 1.81 cm / 1.12 cm |
| 10° 樹幹（60 次）最大水平誤差 / 最大半徑誤差 | 0.70 cm / 0.64 cm | 0.70 cm / 0.64 cm |
| 15° 樹幹附近（0.5 m 內）的偵測數 | 155（每次約 2.6 個） | 14 |
| 其中超過 2 cm 或 1 cm 容差 | 146 | 5 |
| 15° 附近偵測的最大水平誤差 / 最大半徑誤差 | 29.0 cm / 27.0 cm | 2.1 cm / 2.0 cm |
| precision（step，raw） | 0.767 ± 0.096 | 1.000 ± 0.000 |
| recall（step，raw） | 1.000 ± 0.000 | 0.820 ± 0.063 |

- 兩項檢查不影響直立與 10° 樹幹（兩欄相同），只影響 15° 樹幹的條帶擬合。
- 基準設定的 recall = 1.0 是因為 15° 樹幹的碎片擬合落在 0.5 m 配對半徑內而被算成命中，
  但這些擬合多數誤差很大（146/155 超過容差），同時造成多餘偵測（precision 0.77）。
- 完整表格見 `cm_compare` 輸出的 `summary.md`。

## 輸出

`cm_compare` 的輸出：頂層 `summary.md`（兩種設定分節）、`effective_config.ini`；
每種設定各一個子目錄 `baseline/`、`improved/`，內含：

- `<version>_<raw|matched>_seed<k>_trees.csv`：`cluster_id,x,y,z,radius,tilt_deg,n_points,n_inliers`（原始座標，z 為交點）
- `<version>_<raw|matched>_seed<k>_clusters.csv`：每叢的狀態、失敗 bitmask、各 inlier 數、覆蓋角、半徑、傾角、重心、軸、RANSAC seed
- `*_retention_bins.csv`：每 1 m 的實際保留率 vs 平均 P(d)
- `summary_raw.csv`、`summary_matched.csv`（與頂層 `summary.md` 的對應節）：每欄一個版本，平均 ± 樣本標準差（n − 1）
  - 保留點數、叢數、樹幹數、RANSAC 成功率、各失敗原因數、步驟 5–8 耗時
  - 有參考樹位時另列 recall、precision、水平誤差平均與 RMSE
- `runs.log`：每次執行的各階段點數與耗時、樹幹數、成功率、失敗原因
- `effective_config.ini`（該設定實際使用的參數）

檔名中的 `linear*`、`physical*`（budget-matched）寫成 `linear`、`physical`，並放在 `matched` 表。

### 參考樹位配對

只允許水平距離 ≤ `experiment.match_radius`（預設 0.5 m）的候選。先取**配對數最多**的一對一配對，
再在其中取**總距離最小**者（Hungarian 演算法，不可配對的組合給大懲罰）。結果與處理順序無關。

## 已知限制

0. **尚未在真實森林資料上驗證。** 目前只有合成資料；門檻值（`min_normal_ratio`、`min_arc_deg` 等）
   也是在同一個合成場景上定出來的，可能過度貼合合成資料。
1. **傾斜樹幹與 verticality > 0.9（論文設定）。** 這個門檻只保留法向量與水平面夾角 < 5.7° 的點，
   傾斜 > ~6° 的樹幹會失去正面與背面，只剩兩側條帶。合成場景中：10° 樹幹可準確還原；
   15° 樹幹在改良設定下 60 次中找到 14 次（其餘不產生假陽性），在基準設定下會產生多個不準確的條帶擬合。
   `normals.vert_threshold=0.8` 可穩定還原 15° 樹幹（測試 `Synthetic.LooserVerticalityRecoversAllStems`），
   但**這不是論文設定**，只作為可選參數。
2. **DTM `percentile`（非預設）。** 若某格內樹幹或冠層點遠多於地面點，第 k 百分位會落在地面以上。
   合成測試中，密集樹幹旁的最大誤差：`percentile` 16.6 mm，`supported_lowest`（預設）0.7 mm
   （測試 `Dtm.DenseStemCellBiasesPercentile`）。`supported_lowest` 在真實地形（落葉、倒木、低矮植被）
   上的表現尚未驗證。
3. **軸傾角誤差會外推到樹位。** 樹位是軸往下延伸到地面的交點，擬合的軸傾角誤差會被放大。
   合成場景 180 次直立樹幹偵測中，最大水平誤差 1.81 cm；有 1 次半徑誤差 1.12 cm，超過 1 cm 容差。
   合成測試只跑 seed 1–3，這個尾端不在測試範圍內。
4. **不支援 LAZ。** 只讀 LAS 的 XYZ。LAS 讀取器只用本專案寫出的 LAS 檔測試過，尚未用掃描儀廠商軟體輸出的
   檔案（含 VLR）測試。

## 測試

`./build/cm_tests`（GoogleTest，約 20 秒，62 項）。所有測試都使用合成或人工資料，**沒有真實森林資料測試**：

| 測試 | 內容 |
|---|---|
| `Retention.*` | 四個版本在 d = 0、5、10、20 的值；所有 d 下 P ∈ [0, 1]；單調不減；physical(0) = P_min；linear_a ≥ step 與 linear_mid；linear_mid 在均勻 d 下兩段平均 = 0.5 / 0.75 |
| `CRN.*` | u ∈ [0, 1)；與處理順序無關；不同 seed 抽樣不同；子集關係（step ⊂ linear_a、linear_mid ⊂ linear_a）；每 2 m 分組的實際保留率在 5σ 內等於 P(d) |
| `Budget.*` | 調整後保留數與 step 差距在容差內（隨機 d 與合成場景 pipeline）；回報數 = 實際數；linear_a / linear_mid 收斂到同一截距 |
| `IO.*` | ASCII（標頭、逗號、分號、多欄）；PCD ascii / binary / compressed；PCD float64 精度；PLY ascii / binary；LAS 1.0–1.4 × 所有合法點格式 × 額外 bytes；不支援格式、LAZ 位元、截斷、錯誤簽章、record length 過短、.laz、未知副檔名皆報錯 |
| `Sanity.*` | 大座標 + 預設掃描儀 → 停止；給正確位置 → 通過；給錯位置 → 停止；最近點過遠 → 警告；參數讀寫；dump / 讀回不會誤標掃描儀位置 |
| `Dtm.*` | 坡地加空洞時誤差 < 1 cm（兩種 ground_select）；地面被遮蔽、只有樹腳可見時，坡度濾波使 DTM 誤差 < 5 cm（關閉時 > 1 m）；預設為 `supported_lowest`；密集樹幹旁 `percentile` 的偏差（記錄限制）；範圍外回傳 nullopt；0–3 m 高度濾波 |
| `TreePosition.*` | 坡地上傾斜 0–30°、4 個方位的交點與解析解誤差 < 3 mm；陡坡時退回二分法；範圍外無解 |
| `CylinderSeed.*` | PCL RANSAC 亂數行為（見上）；擬合可重現且準確；失敗原因分類 |
| `CylinderChecks.*` | 兩項新增檢查可關閉：關閉時不再造成失敗，但指標仍計算；檢查不改變擬合本身 |
| `Matching.*` | 最佳配對優於貪婪配對；在最大配對中取最小總距離；半徑限制與順序無關 |
| `Synthetic.*`（seed 1–3） | 斜坡（12.6°）+ 5 樹幹（含 10° 上坡、15° 下坡傾斜），1/d² 密度，3 mm 雜訊。DTM 誤差 < 2 cm；verticality 去除地面。**改良設定**：四個版本 × 3 seed 無假陽性；直立與 10° 樹幹（必須找到）樹位誤差 < 2 cm、半徑 < 1 cm、交點 z < 2 cm、傾角 ±2°；15° 樹幹（非必須）若找到則樹位 < 5 cm、半徑 < 3 cm。**基準設定**：直立與 10° 樹幹仍準確找到、無重複；15° 條帶擬合只計數不斷言。verticality 0.8（非論文設定）時五根全部找到。測量座標（UTM 級 offset）+ 掃描儀位置的完整流程 |
| `Reproducibility.*` | 同參數、同 seed 跑兩次，CSV 逐位元組相同；輸入點順序反轉後保留點集相同；pipeline 內的 CRN 子集關係 |
