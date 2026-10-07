# ETH Trees CN 配準：結果解讀（keypoints = step、seed 1）

完整數字見 `registration_report.md`（每一對的原始旋轉誤差、平移誤差、e_p）與 `registration/*.csv`。
距離閾值 **5 cm 是論文設定**；10 / 15 cm 是敏感度分析，**不是論文設定**。求轉換的方法論文未指定（D7），svd 與 svd+ransac 分開報告。

## 論文設定（5 cm）

| keypoints | 求轉換 | 成功 | e_p 中位數 | 失敗原因 |
|---|---|---|---|---|
| baseline | svd | 1/10 | 19.94 m | 10 對全部走 triangle（最佳分數 13–35 < Th 40）；triangle 候選集只有 3–8 點，6 對得到 0 個配對，2 對配對全錯 |
| baseline | svd+ransac | 1/10 | 0.80 m | 同上 |
| improved | svd | 4/10 | 0.297 m | 匹配本身**全部正確**（每一對的正確配對 = 配對數）；失敗來自求轉換：5 對旋轉誤差 4.2–13.6°，1 對只有 2 個配對無法求解 |
| improved | svd+ransac | 4/10 | 0.297 m | 同上（成功的組合不同：s2-s3、s3-s4 改善，s1-s2、s2-s5 變差） |

論文 Table I（ETH-tree，Ours）：e_p = 0.153 m、成功率 100%（論文未給成功標準）。

## 主要發現

1. **improved 的 CN 匹配在 ETH 上是正確的，瓶頸在求轉換。** 旋轉誤差拆成繞 z 軸（yaw）與傾斜（tilt）：
   yaw 誤差 ≤ 1.0°（多數 < 0.4°），**幾乎全部是 tilt**（1.7–13.5°）。
   原因：用 3–10 個樹基點做 3D SVD，樹基 z（DTM 交點）有雜訊，且點數少，傾斜方向約束弱；
   只有 3 個配對的 s1-s3 tilt 達 13.5°。2D 距離診斷也顯示 3D 距離一致性明顯較差（`diag_pair_dist.md`）。
2. **baseline 的匹配失敗。** 重複偵測與假圓柱使描述子不穩，DC 低（最佳 3–8），FM 低（1–3），
   分數全部低於 Th = 40，依 Algorithm 1 只能用 triangle 比對，而候選集太小。
3. **敏感度（非論文設定）：** 放寬閾值對 baseline 幫助有限（15 cm：svd 1/10、svd+ransac 2/10）；
   improved 15 cm + svd+ransac 為 6/10（e_p 中位數 0.198 m），yaw 仍小、失敗仍主要來自 tilt。
4. **D12：** encode2 的 sector 0 最近點是 p1 的比例 baseline 3.5%、improved 6.8%；
   encode3 的 sector 0 最近點是 p1 或 p2 的比例 7.6% / 15.5%。

## 限制

- 只用 step、seed 1 的樹位；P(d) 其他版本與 seed 未跑配準。
- 成功標準（2°、0.5 m）是我們定的（D10）；以 e_p 比較，improved 成功的對 e_p 為 0.014–0.389 m。
- 沒有做 ICP（D11），結果是 coarse 配準。
- 一個可能的改善是利用掃描已整平，只估 yaw + 平移（4 自由度），可直接消除 tilt 誤差；
  但這**不是論文的方法**，依指示**未實作**，是否採用由使用者決定。
