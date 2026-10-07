# Keypoint 變體：結論（ETH Trees，step、seed 1，配準 5 cm = 論文設定）

完整數字：`variants.md`（所有變體都列出）；每一對的原始結果：`registration/<變體>_<pair>.csv`。
重現：`python3 scripts/eth_trees/run_variants.py`。

> **所有變體都在 ETH Trees 上比較與挑選，沒有獨立驗證資料。** 下列「建議設定」是在 ETH 上選出的，
> 需要在另一個資料集（例如 Tongji-Trees）上確認。

## 1. baseline 差的原因

- **最貼近論文字面（5 個擬合後檢查全關）最差**：5032 個樹位，CN 配對 59 個只有 26 個正確，成功 2/10。
  論文沒寫怎麼排除壞圓柱；照字面做會產生大量假樹位（多為樹幹表面窄帶上的細圓柱、枝條），污染描述子
  （每個 sector 取最近的點）。所以 baseline 差**不是**因為加了論文沒有的篩選，而是缺少篩選。
- 三個「論文沒有」的檢查單獨開：只有**傾角**有幫助（2/10 → 4/10）；半徑、inlier 比例單獨開沒有改善。
- 0.3 m 重現率不適合當指標：半徑 < 3 cm 的窄帶擬合在 0.3 m 內有 41% 可重現，但位置在樹幹表面，
  兩站看到不同側，誤差遠大於描述子的 5 cm。
- baseline 調 `radius_min`（5、8 cm）或合併重複：沒有改善（1–3/10）。baseline 的主要錯誤是假樹位，不是重複。

## 2. improved 的優化

| 設定 | 成功（svd / svd+ransac） | e_p 中位數 | 說明 |
|---|---|---|---|
| improved（目前預設） | 8/10 / 8/10 | 0.037 m | 失敗的 2 對只有 1–2 個配對 |
| **improved + 合併 0.3 m（建議）** | **10/10 / 10/10** | 0.045 m | 77 個配對全部正確；每對 5–12 個配對；旋轉 ≤ 0.66°、e_p ≤ 0.103 m |
| improved + clustering 20 cm | 10/10 / 10/10 | 0.061 m | 也減少同一樹幹被拆成多個樹位；s3-s5 只有 3 個配對（邊緣） |
| improved + clustering 20 cm + 合併 | 10/10 / 10/10 | 0.070 m | 無額外好處 |
| 門檻放寬 / 收緊 | 9/10 / 9/10 | 0.083 / 0.069 m | 原門檻（合成資料上定的）在 ETH 上沒有明顯更好的選擇 |

**合併半徑的穩健性**：0.2 m → 9/10、0.3 m → 10/10、0.5 m → 10/10。0.3 m 是事先定的（與 GT 配對半徑相同），
不是調出來的；結果不是只在單一數值成立。

**為什麼合併有效（推測，未逐一驗證）**：improved 約 22% 的樹位在 0.5 m 內有另一個樹位；clustering 距離放大到 20 cm 也有同樣效果，推測同一根樹幹被拆成幾群、各自擬合出幾乎同位置的樹位。
重複點會在描述子的 sector 中互相搶「最近點」，兩站拆法不同就使描述子不一致。合併後一根樹幹只剩一個點。

## 3. 開關與標註（皆非論文）

| 開關 | 預設 | 類別 |
|---|---|---|
| `cylinder.check_radius` / `check_tilt` / `check_inlier_ratio` | 開 | 論文沒有的擬合後篩選（本次加入開關） |
| `cylinder.check_normal_consistency` / `check_arc_coverage` | 開（improved） | 我們新增 |
| `cm_register --merge_radius`、`run_registration.py --merge_radius` | **0（關）** | D9，論文沒有；依先前決定預設關。建議設定為 0.3 |
| `cluster.tolerance` | 0.10 m | 論文未指定的參數 |

預設值沒有改：要用建議設定，配準時加 `--merge_radius=0.3`。

> **更正（10 seed）**：本檔只用 step、seed 1。10 個 seed 的平均見 `../dtm_slope/retention_registration.md`：
> improved + 合併 0.3 m 在 15 對成功 10.9 ± 1.5、官方 10 對 7.8 ± 1.6（seed 1 的 12/15、10/10 偏幸運）；
> improved 15 對 9.7 ± 1.3。變體之間小於約 ±1.5 對的差異無法和 seed 變動區分。
