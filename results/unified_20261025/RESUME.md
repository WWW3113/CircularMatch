# 可續跑狀態與指令

這是進行中的統一方法研究，尚未宣稱整個專案完成。先讀 `checkpoints/status.json` 的保存時間與實際列數；不要憑本文的數字推測最新進度。

## 已固定、不得覆寫的紀錄

- 第一個ETH選參後的Tongji遷移：`../tongji_trees/fixed_eth_20261025`。後續已看過所有Tongji與ETH場景，不能再說完全未見資料。
- 統一v1程式commit：`b8eefb0c677a56d55428c86cb43839eec74639a0`；`method_frozen.json`。全部55配對、10seed、5版本已完成，主要版本Plot4為96/100，未達100%。
- 統一v2程式commit：見 `method_v2_frozen.json`（以8b9b1ae開頭）；擴大候選與水平初始值，最後仍為完整6DOF。必須全部五場景評估，不能只補失敗配對。
- 作者repository `MercerSU/CircularMatch` 只有兩次README提交，沒有可執行程式、tag、release或fork；嚴格原作者baseline比較仍受阻。5cm參考實作不能冒稱原作。

## Checkpoint內容

`checkpoints/completed_predictions.csv` 保存已完整寫出的每一列估計變換，包含尚未完成整批的部分seed；`completed_scores.csv` 保存已完成整批的全點e_p評分；`resources.jsonl` 保存量測紀錄。`status.json` 含SHA256，恢復前會核驗。

沒有把原始點雲、Google分享識別碼或樹位座標上傳Git。原始資料與較大的幾何／提取快取留在忽略的 `data/`、`runs/`。同一環境可直接使用；若容器資料遺失，需從已授權來源補齊原始檔並核對dataset_manifest，再重建所需快取。已保存的推論與評分可以恢復，不必全部重算。

## 先檢查是否仍在運算

目前可能有v2冷啟動benchmark控制器，以及v3後續控制器。v3會等待v2冷啟動完成，再建置與測試結構驗證修正、重跑全部55配對的三個公平版本、評分、補v3冷啟動、最後跑26站成本。若本地 `data/unified/live_jobs.json` 存在，先核對PID、父程序與日誌；**不要在既有控制器還運作時啟動第二份同階段工作**。

日誌：`data/unified/v2-study.log`、`feature-benchmark-controller.log`。主要工作依序為v2全55配對推論→v2全點評分→8個冷啟動benchmark→26站seed31完整提取成本重測。

## 沒有既有工作運作時續跑

```bash
cd /workspace/CircularMatch
python scripts/eth_trees/unified_study.py restore_checkpoint
```

若native工具不存在，使用已驗證工具鏈wrapper執行CMake建置；不要改寫frozen檔來掩蓋版本不符：

```bash
/workspace/.circularmatch-env/run /usr/bin/cmake -S . -B build
/workspace/.circularmatch-env/run /usr/bin/cmake --build build --target cm_extract_batch cm_geometry_register cm_geometry_register_v2 cm_evaluate_transforms cm_geometry_tests -j2
```

若還有尚未完成的推論，先補所需ETH樹位／幾何快取（已存在者跳過；Tongji樹位來自fixed_eth的runs）：

```bash
OPENBLAS_NUM_THREADS=1 python scripts/eth_trees/unified_study.py extract_eth
OPENBLAS_NUM_THREADS=1 python scripts/eth_trees/unified_study.py geometry
OPENBLAS_NUM_THREADS=1 python scripts/eth_trees/unified_study.py infer
OPENBLAS_NUM_THREADS=1 python scripts/eth_trees/unified_study.py infer_v2
```

完成標記會跳過完整批次；部分批次按 `(seed,method)` 只補缺少列，合併時採原子寫入。若所有推論已恢復，可直接評分，不必重建幾何快取。

```bash
OPENBLAS_NUM_THREADS=1 python scripts/eth_trees/unified_study.py evaluate
OPENBLAS_NUM_THREADS=1 PYTHONPATH=scripts/eth_trees python -c 'import unified_study as s; s.evaluate("v2")'
OPENBLAS_NUM_THREADS=1 python scripts/eth_trees/unified_study.py cold_benchmark
OPENBLAS_NUM_THREADS=1 python scripts/eth_trees/unified_study.py feature_benchmark
OPENBLAS_NUM_THREADS=1 python scripts/eth_trees/unified_report.py
python scripts/eth_trees/unified_study.py checkpoint
```

各benchmark有自己的完成標記，會沿用已完成工作。成本必須與準確度工作串行，避免互相爭CPU。原有55配對、seed31–40、project門檻rotation<2°且translation<0.5m保持固定。

## 完成前尚需核對

1. v3若完成，九版本合計4950列（v1五版本2750＋v2版本550＋v3三版本1650），保留所有失敗；完整e_p使用全部來源點。
2. 逐場景成功率、平均／中位／P90；Tongji場景等權與配對加權，ETH單列。
3. baseline加相同refinement的公平消融；v1→v2改善／退步與計算代價。
4. 8個冷啟動benchmark與26站成本重測；不要把快取成本當作0，也不把online時間當總時間。
5. 驗證完整及精簡XLSX數字、私人識別碼未外洩，再推送GitHub與檢查下載連結。
6. 最終區分「低於論文報告值」「優於參考實作」「嚴格原作baseline（尚缺程式）」「未見森林一般化（尚無資料）」；不能宣布全專案已完成。

## 新增v3續跑

完整來源凍結規則見v3_protocol.json；原地面overlap會選錯位置，因此另測垂直結構驗證。原優化器仍用所有方向並解完整6DOF。v3也對5cm參考及ETH固定版套相同refinement，不能省略公平控制。若既有v3控制器不在執行，先建置cm_geometry_register_v3及cm_geometry_structural_tests並執行測試，再freeze_v3、infer_v3、evaluate("v3")；最後補cold_benchmark及feature_benchmark。phase publish_checkpoint會核驗並推送小型結果；不會上傳點雲或樹位。

## 定時遠端備份

checkpoint_watch.py會每120秒保存並推送小型結果，使用Git鎖，且只提交checkpoint與已完成成本JSON，不包含原始點雲／樹位。若FINAL_COMPLETE.json出現則停止；推送失敗會記錄於data/unified/checkpoint-watch.log並保留本地資料。不要重複啟動watcher。
