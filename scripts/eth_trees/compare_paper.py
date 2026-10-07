#!/usr/bin/env python3
"""Compare with the paper's Table I (ETH-tree) on all 15 scan pairs. Standard library only.

The paper's ETH-tree success rates of the other methods (93 / 87 / 67 %) are
14/15, 13/15, 10/15, so the paper most likely used all 15 pairs of the 6 scans
(the official pairs.txt lists 10 overlapping pairs). Ground truth exists for all
15 (groundtruth/sA-sB.tfm). This script registers every pair sA-sB (A < B) for
each configuration, at the paper's 5 cm, and reports success rate and e_p
(mean and median; the paper does not say which statistic or which pairs enter e_p).

  python3 scripts/eth_trees/compare_paper.py [--out results/eth_trees/paper_comparison.md]
Uses tree positions from results/eth_trees/variants/runs (run_variants.py first).
"""
import argparse
import csv
import itertools
import os
import statistics as st
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RAW = os.path.join(ROOT, "data/eth_trees/raw/trees")
VAR = os.path.join(ROOT, "results/eth_trees/variants")

# label, runs directory under variants/runs, merge radius, note
CONFIGS = [
    ("paper_literal", "paper_literal", 0.0, "擬合後檢查全關（最接近論文字面）"),
    ("baseline", "baseline", 0.0, "半徑、傾角、inlier 比例檢查（論文沒有）"),
    ("improved", "improved", 0.0, "baseline + 法向一致性、圓弧覆蓋檢查（論文沒有）"),
    ("improved + 合併 0.3 m", "improved", 0.3, "improved + 合併重複樹位（D9，論文沒有）"),
    ("improved + clustering 20 cm", "improved_ctol20", 0.0, "improved + cluster.tolerance 0.20（論文未給值）"),
]

PAPER = [("論文 Ours（CircularMatch）", 0.153427, 100), ("論文 GM [9]", 0.153959, 100),
         ("論文 Eric [4]", 0.181602, 93), ("論文 FMP+BnB [5]", 0.163926, 87), ("論文 GROR [14]", 0.166293, 67)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "results/eth_trees/paper_comparison.md"))
    a = ap.parse_args()
    scans = ["s1", "s2", "s3", "s4", "s5", "s6"]
    official = {tuple(l.split()) for l in open(os.path.join(RAW, "pairs.txt")) if l.strip()}
    pairs = list(itertools.combinations(scans, 2))
    regdir = os.path.join(VAR, "registration15")
    os.makedirs(regdir, exist_ok=True)
    res = {}
    for label, runs, merge, _ in CONFIGS:
        rows = []
        for sa, sb in pairs:
            tag = f"{runs}_m{merge}_{sa}-{sb}"
            out = os.path.join(regdir, tag + ".csv")
            if not os.path.exists(out):
                tf = lambda s: os.path.join(VAR, "runs", runs, s, "step_seed1_trees.csv")
                subprocess.run([os.path.join(ROOT, "build/cm_register"), f"--src_trees={tf(sa)}", f"--tgt_trees={tf(sb)}",
                                f"--src_cloud={RAW}/{sa}.ply", f"--gt={RAW}/groundtruth/{sa}-{sb}.tfm",
                                "--thresholds=0.05", f"--merge_radius={merge}", f"--label={tag}", f"--out={out}"],
                               check=True, stdout=subprocess.DEVNULL)
            for r in csv.DictReader(open(out)):
                r["pair"], r["official"] = f"{sa}-{sb}", (sa, sb) in official
                rows.append(r)
        res[label] = rows
        print(label, "done", flush=True)

    def summ(rows, solver, subset):
        sel = [r for r in rows if r["solver"] == solver and (subset == "all" or r["official"])]
        ok = [r for r in sel if r["success"] == "1"]
        ep_all = [float(r["e_p_m"]) for r in sel if r["transform_ok"] == "1"]
        ep_ok = [float(r["e_p_m"]) for r in ok]
        t = [float(r["time_ms"]) for r in sel]
        return len(sel), len(ok), ep_ok, ep_all, sum(r["transform_ok"] != "1" for r in sel), t

    f = lambda v, p=3: f"{v:.{p}f}" if v == v else "–"
    mean = lambda v: st.mean(v) if v else float("nan")
    med = lambda v: st.median(v) if v else float("nan")
    L = ["# 與論文 Table I（ETH-tree）比較", "",
         "- 本實作：step、seed 1；距離閾值 5 cm、N = 183、w = 10、Th = 40（論文設定）；求轉換用 SVD（論文未指定）；無 ICP。",
         "- 成功：旋轉 < 2° 且平移 < 0.5 m（**論文沒有給成功標準**，D10 是我們定的）。",
         "- 論文的 e_p 沒說是平均或中位數、也沒說只算成功的對或全部；下表各種算法都列出。",
         "- 論文 ETH-tree 很可能用 15 對（其他方法成功率 93 / 87 / 67 % = 14 / 13 / 10 對），本表用全部 15 對；"
         "另列官方 pairs.txt 的 10 對。",
         "- **所有「論文沒有」的設定都是在 ETH 上選出的，沒有獨立驗證資料。**", "",
         "## 15 對（與論文最可比）", "",
         "| 方法 / 設定 | 說明 | 成功率 | e_p 平均（成功的對）[m] | e_p 中位數（成功的對）[m] | e_p 平均（有轉換的全部對）[m] | 無轉換的對 |",
         "|---|---|---|---|---|---|---|"]
    for name, ep, rate in PAPER:
        L.append(f"| {name} | 論文 Table I | {rate}% | {ep:.3f}（統計方式未說明） | – | – | – |")
    for label, _, _, note in CONFIGS:
        n, k, ep_ok, ep_all, none, _ = summ(res[label], "svd", "all")
        L.append(f"| 本實作：{label} | {note} | {k}/{n} = {k / n:.0%} | {f(mean(ep_ok))} | {f(med(ep_ok))} | "
                 f"{f(mean(ep_all))} | {none} |")
    L += ["", "## 官方 pairs.txt 的 10 對", "",
          "| 設定 | 成功 | e_p 平均（成功的對）[m] | e_p 中位數（成功的對）[m] |", "|---|---|---|---|"]
    for label, _, _, _ in CONFIGS:
        n, k, ep_ok, _, _, _ = summ(res[label], "svd", "official")
        L.append(f"| {label} | {k}/{n} | {f(mean(ep_ok))} | {f(med(ep_ok))} |")
    L += ["", "## svd+ransac（15 對）", "", "| 設定 | 成功 | e_p 平均（成功的對）[m] |", "|---|---|---|"]
    for label, _, _, _ in CONFIGS:
        n, k, ep_ok, _, _, _ = summ(res[label], "svd+ransac", "all")
        L.append(f"| {label} | {k}/{n} | {f(mean(ep_ok))} |")
    L += ["", "## 每一對（svd，15 對）", "",
          "| pair | 官方 10 對 | " + " | ".join(c[0] for c in CONFIGS) + " |", "|---|---|" + "---|" * len(CONFIGS)]
    for sa, sb in pairs:
        cells = []
        for label, _, _, _ in CONFIGS:
            r = next(r for r in res[label] if r["pair"] == f"{sa}-{sb}" and r["solver"] == "svd")
            cells.append(("✓ " if r["success"] == "1" else "✗ ") + (f(float(r["e_p_m"])) if r["transform_ok"] == "1"
                                                                     else f"無轉換（配對 {r['n_matches']}）"))
        L.append(f"| {sa}-{sb} | {'是' if (sa, sb) in official else '否'} | " + " | ".join(cells) + " |")
    _, _, _, _, _, t = summ(res["improved"], "svd", "all")
    L += ["", "## 執行時間（僅供參考，硬體不同）", "",
          f"- 論文 Table II（ETH-tree，matching）：3.04 ms；Table III（總時間，含特徵擷取）：8.70 s（i7-9700F）。",
          f"- 本實作 improved：描述子 + 匹配 + SVD 中位數 {st.median(t):.1f} ms（每對）。樹位擷取每站約 37 s"
          "（含讀 300 MB PLY，4 核雲端容器），未最佳化，不與論文比較。", ""]
    open(a.out, "w").write("\n".join(L) + "\n")
    print("\n".join(L))


if __name__ == "__main__":
    main()
