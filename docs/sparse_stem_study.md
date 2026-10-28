# 稀疏公園資料的提取研究

此研究在 `results/whu_park/sparse_stems_20261025/` 保存完整／精簡xlsx、候選群圖、開發比較、固定方案及ETH回歸。原本 `cm_extract_batch`、`cm_register`、核心預設與 `config/default.ini` 保持不變。

## 分組與結果

Park前16站開發，15個提供的配對使用seed21–25；後16站保留，12個配對使用seed31–40；跨組4個配對排除。Park先前已全場評估，本次分組只是本輪不參與調參的保留資料，不是新森林。

7個候選在開發組比較；四項放寬的組合12/75最佳，但保留站點只有2/120，對照0/120，僅一個配對在兩個seed成功。此結果不足以採用為可靠公園配準方法。

稀疏組合：圓柱法向權重0.1→0.02、距離容差2→5cm、佔格覆蓋100→60°、法向一致性75→50%。僅在高度濾波與voxel後的點間距中位數>5cm時啟用；密集輸入保留原設定。Park法向半徑仍20cm，ETH仍10cm，這些沿用既有資料密度設定；分群均20cm，配準閾值10cm和三維補救保持固定。

ETH沿用既有seed21–30做回歸：180份提取CSV逐位元組一致，重跑後300列匹配／變換／誤差仍與原值相同，主要solver維持150/150。計時因主機並行負載不同，不作跨輪效能宣稱。

## 新增工具

- `cm_candidate_export` 匯出垂直候選點與每seed分群索引，不做GT選擇。已核對第0、7站全部分群點數與重心符合既有輸出。只有XYZ幾何，無法把所有圓弧候選確定標成樹木。
- `cm_sparse_extract` 預設 `--arc_mode=occupied`，保持原行為；附加的 `--sparse_*` 參數只在稀疏資料啟用。
- `--arc_mode=auto` 是另一個角度量法實驗：稀疏時取能涵蓋90%內點的最短環形角跨度，避免少數離群點放大覆蓋。此候選開發組沒有勝出，未被最終方案採用。

新選項範例：

```bash
./build/cm_sparse_extract --input=SCAN.ply --out=OUTPUT \
  --config=config/default.ini --normals.radius=0.20 --cluster.tolerance=0.20 \
  --arc_mode=occupied --sparse_normal_weight=0.02 --sparse_dist_threshold=0.05 \
  --sparse_min_arc=60 --sparse_min_normal_ratio=0.50 \
  --experiment.seed_base=31 --experiment.n_seeds=10
```

## 建置與重現

需要先完成WHU-Park提取，以及ETH前輪seed21–30，作為診斷與回歸來源。雲端命令使用 `/workspace/.circularmatch-env/run` 前綴。

```bash
cmake -S . -B build
cmake --build build --target cm_candidate_export cm_sparse_extract cm_sparse_arc_tests -j4
ctest --test-dir build -R '^SparseArc\.' --output-on-failure
OPENBLAS_NUM_THREADS=1 python3 scripts/eth_trees/sparse_stem_study.py screen
OPENBLAS_NUM_THREADS=1 python3 scripts/eth_trees/sparse_stem_study.py validate
OPENBLAS_NUM_THREADS=1 python3 scripts/eth_trees/sparse_stem_study.py eth
OPENBLAS_NUM_THREADS=1 python3 scripts/eth_trees/report_sparse_stems.py
```

screen不讀保留站點結果；選定後保存程式、二進位與設定雜湊，validate/eth會核對它們。新增4項角度測試已通過；原模式2個seed的6份輸出CSV也與既有提取逐位元組一致。原81項測試未因本輪新增工具而修改或停用。
