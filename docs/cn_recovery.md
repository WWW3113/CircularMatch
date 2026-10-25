# 兩站匹配補救實驗

此功能是我們新增的選用實驗，不是 CircularMatch 論文原方法。預設 `--recovery=off`，原本 score 門檻與三角形備援行為不變。設定、選法、凍結時間與結果見 `results/eth_trees/failure_followup_20261025/`。

## 為何修改

原流程只在最高分低於 `Th_score` 時啟動三角形備援；最高分已過門檻但最後只有一兩對匹配時，仍無法估計完整剛體變換。舊 seed11–20 的推薦組有8次失敗，其中7次少於3對匹配，另1次有5對但誤差過大。真值僅用於失敗診斷和事後評分，不作為補救推論的輸入。

## 兩種選用方法

- `--recovery=triangles --recovery_trigger=3`：不足3對時，取前三個描述子候選的維度點與中心點，分別作既有的三角形比對；彙總票數並保留一對一匹配。所得匹配至少3對且比原本多才採用。
- `--recovery=evidence --recovery_trigger=6`：不足6對時，彙總前三個描述子候選的維度對應及中心對應。每個候選內去重投票，最多60條證據；枚舉三元組，先檢查對應邊的水平長度差小於0.20m，再估完整6DOF變換。以同一證據池內的三維殘差小於0.30m、一對一支持最多為優先；同支持數時依殘差中位數及總和排序。至少6對支持且比原匹配多才採用，否則保留原結果。最後仍由既有 SVD / SVD+RANSAC 求解。

兩種方法都只使用當前兩站的樹位、描述子與固定參數；沒有六站圖路徑、ICP、迭代最近鄰重配對、4DOF或真值選解。所有數值在舊seed上選定，凍結後才產生seed21–30的驗證結果。新seed仍是同一片森林的亂數驗證，並非跨森林泛化。

## 執行

```bash
./build/cm_register \
  --src_trees=results/eth_trees/failure_followup_20261025/runs/validation_extraction/s4/step_seed21_trees.csv \
  --tgt_trees=results/eth_trees/failure_followup_20261025/runs/validation_extraction/s5/step_seed21_trees.csv \
  --src_cloud=data/eth_trees/raw/trees/s4.ply \
  --gt=data/eth_trees/raw/trees/groundtruth/s4-s5.tfm \
  --thresholds=0.10 --recovery=evidence --recovery_trigger=6 \
  --out=results/eth_trees/failure_followup_20261025/runs/example.csv
```

雲端命令使用 `/workspace/.circularmatch-env/run` 前綴。先完成上一輪ETH研究以保留舊seed11–20樹位與逐次CSV，然後執行：

```bash
OPENBLAS_NUM_THREADS=1 python3 scripts/eth_trees/recovery_study.py discover
OPENBLAS_NUM_THREADS=1 python3 scripts/eth_trees/audit_pair_failures.py
OPENBLAS_NUM_THREADS=1 python3 scripts/eth_trees/recovery_study.py freeze
OPENBLAS_NUM_THREADS=1 python3 scripts/eth_trees/recovery_study.py extract
OPENBLAS_NUM_THREADS=1 python3 scripts/eth_trees/recovery_study.py validate
OPENBLAS_NUM_THREADS=1 python3 scripts/eth_trees/report_recovery_study.py
```

discover只讀舊seed，產生三組候選比較及失敗追蹤。freeze保存固定方案；extract產生新seed樹位；validate執行配對比較。報告產生CSV、Markdown及精簡xlsx，並檢查原始成功門檻、完整source點雲誤差、60次提取與有效設定。重建若改動推論程式或可執行檔，凍結雜湊檢查會停止；不可把原驗證結果當作改動後方法的驗證。

使用 `--diagnostics=DIR` 可輸出所有描述子配對及候選維度證據；附加的GT距離欄位僅為事後診斷。`--skip_point_error=1` 僅供診斷，省略讀取原始點雲及完整點誤差，該欄為NaN；正式驗證不使用此選項。

新增模式才附加 recovery 系列CSV欄位；預設模式維持原本欄位。診斷已核對舊300列的匹配與變換保持一致；兩個實際補救案例把評分GT平移100m後，匹配、支持數與估計變換保持不變。
