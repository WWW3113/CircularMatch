# ETH Trees：接入與執行流程

資料：ETH Trees（Theiler et al., *Globally Consistent Registration of Multiple Point Clouds*）。
官方頁面：https://prs.igp.ethz.ch/research/completed_projects/automatic_registration_of_point_clouds.html
中的「Download Trees (ZIP, 1.2 GB)」。

範圍：只用現有的 keypoint extraction 與 `cm_compare`。不做 descriptor、matching、registration，
不計算 rotation / translation error，也不把 pairwise transformation 當參考樹位。
**本資料集沒有人工標註的參考樹位，所以不計算 precision、recall、樹位誤差**；結果只代表
「程式成功執行」，不代表「樹位準確度已驗證」。

## 目前狀態

- 官方頁面已確認：「Globally Consistent Registration of Multiple Point Clouds」段落的
  「Download Trees (ZIP, 1.2 GB)」指向
  `https://ethz.ch/content/dam/ethz/special-interest/baug/igp/photogrammetry-remote-sensing-dam/documents/sourcecode-and-datasets/PascalTheiler/trees.zip`
  （頁面說明：6 scans acquired in a forest with large amount of underwood and medium overlap；
  ground truth 為 pairwise transformation matrices）。
- ZIP 實際放在 `ethz.ch` 網域；雲端開發環境目前仍擋下該網域（proxy 回 403），尚未下載。
- 下列工具已在合成資料上測試過：下載／驗證腳本（語法檢查）、`run_all.py`（有站位才執行、無站位會跳過並記錄）、
  `aggregate.py`（每站表格與跨站平均 ± 標準差）。

## 在 WSL2 重現

```bash
sudo apt-get install libpcl-dev libgtest-dev curl unzip python3
git clone https://github.com/WWW3113/CircularMatch.git && cd CircularMatch
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
./build/cm_tests

# 1. 下載 + SHA-256 + 解壓（官方「Download Trees (ZIP, 1.2 GB)」連結）
scripts/eth_trees/download.sh 'https://ethz.ch/content/dam/ethz/special-interest/baug/igp/photogrammetry-remote-sensing-dam/documents/sourcecode-and-datasets/PascalTheiler/trees.zip'
#    -> data/eth_trees/trees.zip, data/eth_trees/raw/, results/eth_trees/download_record.txt

# 2. 填 scripts/eth_trees/stations.csv：每個 scan 的路徑、掃描儀位置、位置來源
#    （沒有可驗證來源的 scan 留空，runner 會跳過）

# 3. 逐站執行（config/default.ini、四種 P(d)、每種 10 seed、baseline + improved）
python3 scripts/eth_trees/run_all.py --seeds 10
#    單站 smoke test：python3 scripts/eth_trees/run_all.py --only <scan>

# 4. 彙整
python3 scripts/eth_trees/aggregate.py
#    -> results/eth_trees/summary.md
```

`data/` 與 `results/**/runs/`（每站完整輸出）不進 git；`results/eth_trees/` 下的
`download_record.txt`、`run_status.csv`、`summary.md` 是可提交的精簡結果。
