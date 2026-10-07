# 為什麼 ETH 配準結果和論文不同（診斷）

論文 Table I（ETH-tree）：e_p = 0.153 m，成功率 100%。本實作（5 cm，論文設定）：improved 4/10、baseline 1/10。

## 1. 對照實驗：程式本身（）

保留 target 的全部樹位，只把「GT 對齊後 0.3 m 內互為最近鄰」的 target 樹換成 GT 轉換後的 source 樹位，
其餘（含多餘、錯誤的樹）不動，再用同一個 `cm_register`、論文參數（5 cm）跑。只用於診斷（用到 GT）。

```
xyz baseline svd        success 9/10  e_p median 0.000  matches/correct 9/9 4/4 21/21 6/6 16/15 6/6 4/4 13/13 17/17 28/28
xyz baseline svd+ransac success 10/10  e_p median 0.000  matches/correct 9/9 4/4 21/21 6/6 16/15 6/6 4/4 13/13 17/17 28/28
xyz improved svd        success 10/10  e_p median 0.000  matches/correct 10/10 9/9 13/13 10/10 7/7 14/14 8/8 13/13 13/13 16/16
xyz improved svd+ransac success 10/10  e_p median 0.000  matches/correct 10/10 9/9 13/13 10/10 7/7 14/14 8/8 13/13 13/13 16/16
xy  baseline svd        success 3/10  e_p median 0.288  matches/correct 9/9 4/4 21/21 6/6 16/15 6/6 4/4 13/13 17/17 28/28
xy  baseline svd+ransac success 5/10  e_p median 0.518  matches/correct 9/9 4/4 21/21 6/6 16/15 6/6 4/4 13/13 17/17 28/28
xy  improved svd        success 5/10  e_p median 0.301  matches/correct 10/10 9/9 13/13 10/10 7/7 14/14 8/8 13/13 13/13 16/16
xy  improved svd+ransac success 6/10  e_p median 0.277  matches/correct 10/10 9/9 13/13 10/10 7/7 14/14 8/8 13/13 13/13 16/16
```
（matches/correct：每一對的配對數 / 正確配對數）

- **xyz 精確**：baseline 9–10/10、improved 10/10，e_p ≈ 0。描述子、Algorithm 1 與求轉換在真實樹分布上可以正常運作。
- **只有 xy 精確、z 保留實測值**：配對仍幾乎全對，但成功只有 3–6/10 → 3D 求轉換被樹基高度 z 的誤差拉歪（tilt）。

## 2. 樹位誤差的大小（GT 對齊後的配對樹）

| keypoints | 配對樹 | 水平誤差中位數 | z 誤差中位數 | z 誤差 p90 |
|---|---|---|---|---|
| baseline | 721 | 7.3 cm | 24.1 cm | 2.27 m |
| improved | 268 | 4.7 cm | 29.2 cm | 2.10 m |

- **水平誤差**：baseline 7.3 cm 大於論文 5 cm 閾值，所以 baseline 的最佳分數都 < Th = 40，走 triangle 而失敗；improved 4.7 cm 剛好在閾值附近。
- **z 誤差**不是固定偏移，而是個別樹差 1–15 m。這些樹在兩站的「樹段中心」高度也差了同樣的量：
  兩站各自的 DTM 在該處差好幾公尺，0–3 m 切到的是同一根樹幹的不同高度段。這些樹多在離掃描站 15 m 以上，
  與 `dtm_check.txt` 標記的「樹基高於最低實測點 1 m 以上」（每站約 20–30% 的樹）一致。

## 結論

- 在已檢查的範圍內，差距**不是匹配程式的錯誤**，而是**我們的樹位不夠準**：水平誤差接近 5 cm 閾值，且 DTM 在遠處高估地面造成 z 誤差。
- 論文的 DTM 方法未指定（本實作的 DTM 細節是實作選擇），論文也聲稱其樹位「accurate and reliable」，但沒有給精度數字。
- 無法排除的其他差異：歧義表 D1–D17 的解讀可能和作者不同；論文成功標準未給（D10）；論文沒說 ETH 用哪些 scan pair。
- 依使用者指示不做「改善樹位精度」（選項 3），此處只記錄原因。
