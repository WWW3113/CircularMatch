#!/usr/bin/env python3
"""Compare with the paper's Table I (ETH-tree), 10 seeds (standard library only).

How many ETH pairs the paper used is NOT stated. Its success rates of the other
methods (93 / 87 / 67 %) rule out the official 10 pairs (pairs.txt); the
smallest consistent count is 15 (all pairs of 6 scans), but 30 (both
directions) fits as well. All three sets are reported:
  15 : sA-sB, A < B           30 : 15 + reverse direction     10 : official pairs.txt
Every configuration: step P(d), seeds 1..N, paper's 5 cm, SVD (paper does not specify the solver).

  python3 scripts/eth_trees/compare_paper.py [--seeds 10] [--jobs 3]
Tree positions: results/eth_trees/dtm_slope/seeds/{runs,literal/runs,ctol20/runs}/<scan>/<profile>/step_raw_seed<k>_trees.csv
Writes results/eth_trees/paper_comparison.{md,csv}.
"""
import argparse
import csv
import itertools
import os
import statistics as st
import subprocess
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RAW = os.path.join(ROOT, "data/eth_trees/raw/trees")
SEEDS = os.path.join(ROOT, "results/eth_trees/dtm_slope/seeds")
SCANS = ["s1", "s2", "s3", "s4", "s5", "s6"]

# key, label, runs dir, profile subdir, merge radius, note
CONFIGS = [
    ("literal", "照論文字面", "literal/runs", "baseline", 0.0, "擬合後檢查全關"),
    ("baseline", "baseline", "runs", "baseline", 0.0, "+ 半徑、傾角、inlier 比例檢查（論文沒有）"),
    ("improved", "improved", "runs", "improved", 0.0, "+ 法向一致性、圓弧覆蓋檢查（論文沒有）"),
    ("merge", "improved + 合併 0.3 m", "runs", "improved", 0.3, "+ 合併重複樹位（D9，論文沒有）"),
    ("ctol20", "improved + clustering 20 cm", "ctol20/runs", "improved", 0.0, "cluster.tolerance 0.20（論文未給值）"),
]
PAPER = [("論文 Ours（CircularMatch）", 0.153427, 100), ("論文 GM [9]", 0.153959, 100),
         ("論文 Eric [4]", 0.181602, 93), ("論文 FMP+BnB [5]", 0.163926, 87), ("論文 GROR [14]", 0.166293, 67)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", type=int, default=10)
    ap.add_argument("--jobs", type=int, default=3)
    ap.add_argument("--out", default=os.path.join(ROOT, "results/eth_trees/paper_comparison"))
    a = ap.parse_args()
    official = {tuple(l.split()) for l in open(os.path.join(RAW, "pairs.txt")) if l.strip()}
    fwd = list(itertools.combinations(SCANS, 2))
    allpairs = fwd + [(b, x) for x, b in fwd]
    regdir = os.path.join(SEEDS, "paper_cmp_registration", "runs")  # git-ignored
    os.makedirs(regdir, exist_ok=True)

    keys, jobs = [], {}
    for (key, _, rdir, prof, merge, _), seed, (sa, sb) in itertools.product(CONFIGS, range(1, a.seeds + 1), allpairs):
        tf = lambda s: os.path.join(SEEDS, rdir, s, prof, f"step_raw_seed{seed}_trees.csv")
        out = os.path.join(regdir, f"{key}_seed{seed}_{sa}-{sb}.csv")
        keys.append((key, seed, sa, sb, out))
        if not os.path.exists(out):
            jobs.setdefault((sa, merge), []).append(f"{tf(sa)} {tf(sb)} {RAW}/{sa}.ply {RAW}/groundtruth/{sa}-{sb}.tfm "
                                                    f"{out} {key}_seed{seed}_{sa}-{sb}")

    def run(k):
        sa, merge = k
        path = os.path.join(regdir, f"batch_{sa}_m{merge}.txt")
        open(path, "w").write("\n".join(jobs[k]) + "\n")
        subprocess.run([os.path.join(ROOT, "build/cm_register"), f"--batch={path}", "--thresholds=0.05",
                        f"--merge_radius={merge}"], check=True, stdout=subprocess.DEVNULL)
    with ThreadPoolExecutor(a.jobs) as ex:
        list(ex.map(run, sorted(jobs)))

    rows = []
    for key, seed, sa, sb, out in keys:
        for r in csv.DictReader(open(out)):
            if r["solver"] != "svd":
                continue
            rows.append(dict(config=key, seed=seed, pair=f"{sa}-{sb}", forward=int(sa < sb),
                             official=int((sa, sb) in official), n_matches=r["n_matches"],
                             n_correct=r["n_correct_matches"], transform_ok=int(r["transform_ok"]),
                             rot_err_deg=r["rot_err_deg"], trans_err_m=r["trans_err_m"], e_p_m=r["e_p_m"],
                             success=int(r["success"]), time_ms=r["time_ms"]))
    with open(a.out + ".csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)

    seeds = range(1, a.seeds + 1)
    sets = {"15": lambda r: r["forward"], "30": lambda r: True, "10": lambda r: r["official"]}
    size = {"15": 15, "30": 30, "10": 10}

    def stats(key, s):
        sel = [r for r in rows if r["config"] == key and sets[s](r)]
        per_seed = [sum(r["success"] for r in sel if r["seed"] == k) for k in seeds]
        ep_ok = [float(r["e_p_m"]) for r in sel if r["success"]]
        ep_t = [float(r["e_p_m"]) for r in sel if r["transform_ok"]]
        no_t = sum(1 for r in sel if not r["transform_ok"]) / a.seeds
        return per_seed, ep_ok, ep_t, no_t

    pct = lambda v, n: f"{st.mean(v) / n:.0%}（{st.mean(v):.1f} ± {st.stdev(v):.1f} / {n}；{min(v)}–{max(v)}）"
    f3 = lambda v, fn: f"{fn(v):.3f}" if v else "–"
    L = ["# 與論文 Table I（ETH-tree）比較（10 個 seed）", "",
         f"- 本實作：step P(d)、seed 1–{a.seeds}；距離閾值 5 cm、N = 183、w = 10、Th = 40（論文設定）；"
         "求轉換 SVD（論文未指定）；無 ICP。",
         "- 成功：旋轉 < 2° 且平移 < 0.5 m（**論文沒有給成功標準**，D10 是我們定的）。",
         "- 成功率：各 seed 的成功對數取平均（± 標準差；最少–最多）。",
         "- 論文的 e_p 沒說是平均或中位數、只算成功的對或全部；下表列「成功的對」的平均與中位數，以及「有求得轉換的全部對」的平均。",
         "- **論文沒寫 ETH 用幾對。** 其他方法的成功率（93 / 87 / 67 %）排除官方 10 對；15 對（6 站兩兩）是最小的可能，"
         "30 對（雙向）也符合。三種都列出，主表用 15 對。",
         "- **所有「論文沒有」的設定都是在 ETH 上選出的（只看 seed 1），沒有獨立驗證資料。**", "",
         "## 主表：15 對", "",
         "| 方法 / 設定 | 說明 | 成功率 | e_p 平均（成功）[m] | e_p 中位數（成功）[m] | e_p 平均（有轉換的全部對）[m] | 無轉換的對（每 seed） |",
         "|---|---|---|---|---|---|---|"]
    for name, ep, rate in PAPER:
        L.append(f"| {name} | 論文 Table I | {rate}% | {ep:.3f}（統計方式未說明） | – | – | – |")
    for key, label, _, _, _, note in CONFIGS:
        ps, ok, t, nt = stats(key, "15")
        L.append(f"| 本實作：{label} | {note} | {pct(ps, 15)} | {f3(ok, st.mean)} | {f3(ok, st.median)} | "
                 f"{f3(t, st.mean)} | {nt:.1f} |")
    L += ["", "## 三種配對集合的成功率", "", "| 設定 | 官方 10 對 | 15 對（單向） | 30 對（雙向） |", "|---|---|---|---|"]
    L.append("| 論文 Ours | – | 100% | 100% |")
    for key, label, _, _, _, _ in CONFIGS:
        L.append(f"| {label} | " + " | ".join(pct(stats(key, s)[0], size[s]) for s in ("10", "15", "30")) + " |")
    L += ["", "## 每一對的成功次數（10 個 seed 中；15 對單向）", "",
          "| pair | 官方 10 對 | 兩站距離 [m] | " + " | ".join(c[1] for c in CONFIGS) + " |",
          "|---|---|---|" + "---|" * len(CONFIGS)]
    for sa, sb in fwd:
        T = [[float(v) for v in l.split()] for l in open(os.path.join(RAW, "groundtruth", f"{sa}-{sb}.tfm")) if l.strip()]
        d = (T[0][3] ** 2 + T[1][3] ** 2) ** 0.5
        cells = [f"{sum(r['success'] for r in rows if r['config'] == c[0] and r['pair'] == f'{sa}-{sb}')}/{a.seeds}"
                 for c in CONFIGS]
        L.append(f"| {sa}-{sb} | {'是' if (sa, sb) in official else '否'} | {d:.1f} | " + " | ".join(cells) + " |")
    t = [float(r["time_ms"]) for r in rows if r["config"] == "improved"]
    L += ["", "## 執行時間（僅供參考，硬體不同）", "",
          "- 論文 Table II（ETH-tree，matching）：3.04 ms；Table III（總時間，含特徵擷取）：8.70 s（i7-9700F）。",
          f"- 本實作 improved：描述子 + 匹配 + SVD 中位數 {st.median(t):.1f} ms（每對）。樹位擷取每站約 37 s"
          "（含讀 300 MB PLY，4 核雲端容器），未最佳化，不與論文比較。", ""]
    open(a.out + ".md", "w").write("\n".join(L) + "\n")
    print("\n".join(L))


if __name__ == "__main__":
    main()
