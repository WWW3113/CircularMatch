# DTM 坡度濾波：修改與 ETH Trees 結果

## 修改內容（只改論文未指定的細節）

論文只寫「先產生 DTM，保留離 DTM 0–3 m 的點；圓柱與 DTM 的交點為樹位」，**DTM 的產生方法未指定**。
流程（DTM → 0–3 m → voxel → verticality → P(d) → clustering → RANSAC 圓柱 → 軸與 DTM 交點）與所有論文參數都沒有改。

新增 DTM 步驟 1b：坡度濾波（Vosselman 2000）。某格的地面樣本若比 `slope_radius` 內任一樣本高出
`max_slope · d + slope_tol`，就不可能是地面，剔除。參數：`max_slope = 0.5`、`slope_tol = 0.3 m`、`slope_radius = 5 m`。

- **原因**：離掃描站 15 m 以上，地面多被遮蔽，格內最低點是枝葉。原本只和鄰格平面比較（鄰格也是枝葉），剔除不掉，
  DTM 被抬高數公尺，0–3 m 切到樹幹較高的段落，樹基 z 錯（`../gap_to_paper.md`）。
- **參數依據**：ETH 6 站近站 12 m 內地形，整體坡度 5–11°，相鄰格坡度中位數 0.11–0.16。0.5（約 27°）遠大於實際地形。
  **參數是依地形量測決定，不是用配準結果調整。**
- **限制**：地形比 `max_slope` 陡的地方，上坡的地面樣本會被誤刪。

## 樹位一致性（`tree_consistency.md`）

| runs | keypoints | 樹位數（6 站合計） | 配對樹（10 對） | 水平差中位數 [cm] | z 差中位數 [cm] | z 差 p90 [cm] | z 差 > 0.5 m | z 差 > 1 m |
|---|---|---|---|---|---|---|---|---|
| 舊DTM | baseline | 1932 | 721 | 7.3 | 24.1 | 226.5 | 37.7% | 23.7% |
| 舊DTM | improved | 470 | 268 | 4.7 | 29.2 | 202.5 | 39.9% | 25.7% |
| 坡度濾波 | baseline | 1798 | 746 | 6.4 | 4.6 | 34.8 | 5.5% | 0.7% |
| 坡度濾波 | improved | 420 | 261 | 3.8 | 4.1 | 26.8 | 3.4% | 1.5% |

## 配準（論文設定 5 cm；完整數字見 `registration_report.md`）

| keypoints | 求轉換 | 舊 DTM 成功 | 坡度濾波 成功 | 舊 DTM e_p 中位數 | 坡度濾波 e_p 中位數 |
|---|---|---|---|---|---|
| baseline | svd | 1/10 | 3/10 | 19.94 m | 0.181 m |
| baseline | svd+ransac | 1/10 | 3/10 | 0.80 m | 0.181 m |
| improved | svd | 4/10 | **8/10** | 0.297 m | **0.037 m** |
| improved | svd+ransac | 4/10 | **8/10** | 0.297 m | **0.037 m** |

論文 Table I（ETH-tree）：e_p 0.153 m，成功率 100%（成功標準未給）。

- improved 成功的 8 對 e_p 為 0.022–0.067 m，旋轉誤差 0.08–0.85°；**所有配對都正確**。
- improved 失敗的 2 對（s2-s4、s2-s5）：配對正確但只有 2 對與 1 對，不足 3 對無法求轉換（Algorithm 1 只收分數 > Th 的配對，D17）。
- baseline：10 對的最佳分數仍全部 < Th = 40，都走 triangle；其中 6 對沒有得到配對。baseline 水平差中位數 6.4 cm 仍大於 5 cm 閾值。
- 敏感度（非論文設定）：improved 10 cm 9/10、15 cm 10/10；baseline 10 cm 5–6/10、15 cm 4/10。

## 範圍與限制

- 只重跑 step、seed 1（兩組 keypoints）；`../runs/` 下的 10 seed、四個 P(d) 版本結果與其報告（`../summary.md`、
  `../baseline_report.md`、`../improved_report.md`）仍是**舊 DTM** 的結果，未重跑。
- 成功標準（2°、0.5 m）仍是我們定的（D10）；仍無樹木真值。
