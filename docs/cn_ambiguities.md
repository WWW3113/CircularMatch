# CircularMatch 實作：論文未指定或矛盾之處（歧義表）

以論文為準；論文沒有寫的地方採下表的預設，並且不加入論文沒有的方法。
下表標「使用者決定」的項目是經使用者確認的選擇。

## Keypoint extraction

| # | 項目 | 論文 | 本實作 |
|---|---|---|---|
| C1 | Euclidean clustering 距離閾值 | Fig. 1 與 §III-A 提到 clustering，§II 正文未提，**未給參數** | `cluster.tolerance = 0.10 m`（實作選擇） |
| C2 | clustering 最小點數 | 未給 | `cluster.min_points = 30`（實作選擇） |
| C3 | clustering 最大點數 | 未給 | `cluster.max_points = 10^7`（實作選擇） |

Pipeline 順序與 Fig. 1 一致：DTM → 0–3 m 高度濾波 → 1 cm voxel → verticality > 0.9 → 距離機率降採樣 →
**Euclidean clustering** → 每叢 RANSAC 圓柱 → 軸與 DTM 交點。其他 keypoint 階段的未指定參數見 README。

## CN descriptor 與 matching（§II-A、§II-B、Algorithm 1）

論文給定：N = 183、距離閾值 5 cm、w = 10、Th_score = 40。

| # | 項目 | 論文 | 本實作 |
|---|---|---|---|
| D1 | 距離與角度的維度 | 未明說；式 (6) 用 z 軸判斷順逆時針 | **一律 2D 水平**（忽略 z），不提供 3D 選項（使用者決定） |
| D2 | 空 sector 的值 | 只說「empty」 | 值 0、索引 −1；空維度永遠不算符合 |
| D3 | gatekeeper 三個 bit 的內容與「completely mismatched」 | 「p1, p2, p3 與其索引作為 gatekeeper bits」；「完全不符」者剔除 | bit = 到 p1、p2、p3 的距離；source bit k 對 target bit k，**三個中有一個差 < 閾值即通過**，三個都不符才剔除（使用者決定） |
| D4 | DC 篩選門檻 | **矛盾**：內文「DC **below 3** 的配對無法形成有效拓撲而剔除」；Algorithm 1 第 17 行寫 `S_maxDC > 3` | 採內文：保留 **DC ≥ 3**（參數 `min_dc`，可改為 4 對應 Algorithm 1） |
| D5 | FM score 的定義 | 「matching index pair 在整個匹配過程中出現的次數」 | 每個保留的候選對（Alg. 1 l. 18 `UpdateFMscore`）中，每個符合維度的 (source 索引, target 索引) 計一次；配對 (i, j) 的 FM = FM[i][j] |
| D6 | triangle bit comparison | 最佳配對分數 < Th 時，取其所有符合維度的點索引為候選集，以點為頂點組三角形，邊長相符者為匹配三角形，頂點成為配對點；Fig. 4：source 的 bit 2 可對 target 的 bit 3 | 候選集 = 最佳配對所有符合維度的索引；兩組三角形邊長（2D）在某種頂點對應下**三邊都**差 < 閾值才算匹配；**頂點順序可置換**（使用者決定） |
| D7 | 由配對點求轉換的方法 | 只寫「calculate the Euclidean transform」，**未指定** | 兩種都跑、分開報告：SVD（Kabsch，3D 樹位）與 SVD + RANSAC（最小樣本 3、inlier 0.3 m、1000 次、seed 1，皆為我們的參數）（使用者決定）；兩者都估完整 3D 剛體轉換，**不使用只估 yaw + 平移的 4 自由度解法**（論文沒有，使用者決定不加） |
| D8 | 輸入 keypoints | — | baseline 與 improved 兩組都跑（step、seed 1），並排比較（使用者決定） |
| D9 | 是否合併重複偵測 | 論文沒有此步驟 | baseline 不合併；合併為選項（`--merge_radius`），**預設關**（使用者決定）。ETH 上 improved + 0.3 m 合併為 10/10（`results/eth_trees/variants/findings.md`，在 ETH 上選出） |
| D10 | 一對 scan 的成功標準 | Table I 有成功率但**未給標準** | 旋轉誤差 < 2° **且** 平移誤差 < 0.5 m；每一對另保留旋轉誤差、平移誤差、e_p 原始數字 |
| D11 | 精配準（ICP） | 論文方法為描述子匹配求轉換 | 不做 ICP，只評估由匹配求得的轉換 |
| D12 | encode2 的 sector 0 中離 p0 最近的點可能是 p1（encode3 可能是 p1 或 p2） | 未說明 | 照字面：每個 sector 取最近的點；統計發生次數並輸出 |
| D13 | triangle 比對產生的配對若互相衝突 | 未說明 | 依該頂點對應出現在多少個匹配三角形中排序，取一對一（票數高者優先） |
| D14 | 描述子的鄰域範圍 | 未給半徑 | 使用 scan 中所有其他 keypoint |
| D15 | DC 是否計入 gatekeeper 維度 | 「數值差 < 閾值的維度數」，未說是否含 gatekeeper | DC 只計 sector 維度（3..N−1）；gatekeeper 只用於 D3 的篩選 |
| D16 | 同分時的選擇 | 未說明 | 最佳 target：DC 相同取索引小者；候選排序：MatchScore → DC → source 索引 |
| D17 | Algorithm 1 第 26–30 行 | 只有「第一對」分數 < Th 時做 triangle；其餘只收分數 > Th 者 | 照字面；分數 = Th 的配對不收 |

## 評估

- e_p（式 8）：對 **source scan 的所有點**計算 `mean |R_est p + t_est − (R_gt p + t_gt)|`，不是只對樹位置。
- 旋轉誤差：`acos((trace(R_gtᵀ R_est) − 1) / 2)`；平移誤差：`|t_est − t_gt|`。
- ETH Trees 的 GT：`sA-sB.tfm` 把 sA 座標轉到 sB（已用樹位驗證方向，見 `results/eth_trees/diag_pair_dist.md`）。
- 敏感度分析：距離閾值 5 / 10 / 15 cm（同時套用在 gatekeeper、DC、triangle）。**只有 5 cm 是論文設定**，
  10 與 15 cm 不是論文設定。
