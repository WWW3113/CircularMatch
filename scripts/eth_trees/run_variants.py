#!/usr/bin/env python3
"""Keypoint-extraction variants on ETH Trees: extract (step, seed 1) -> tree
consistency -> CN registration at the paper's 5 cm. Standard library only.

Every variant and its result is reported (no hidden selection). Variants are
chosen and compared ON ETH TREES ITSELF (no held-out data): treat the "best"
variant as tuned on ETH, not as validated.

  python3 scripts/eth_trees/run_variants.py [--only NAME ...] [--jobs 2]
Writes results/eth_trees/variants/{runs (git-ignored), registration/<variant>_<pair>.csv, variants.csv, variants.md}.
"""
import argparse
import csv
import math
import os
import statistics as st
import subprocess
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RAW = os.path.join(ROOT, "data/eth_trees/raw/trees")
OUT = os.path.join(ROOT, "results/eth_trees/variants")
SCANS = ["s1", "s2", "s3", "s4", "s5", "s6"]

OFF5 = ["--cylinder.check_radius=false", "--cylinder.check_tilt=false", "--cylinder.check_inlier_ratio=false",
        "--cylinder.check_normal_consistency=false", "--cylinder.check_arc_coverage=false"]
BASE = ["--cylinder.check_normal_consistency=false", "--cylinder.check_arc_coverage=false"]
IMP = []  # default config = all checks on

# name, category, description, extract args, merge radius for registration, reuse runs of
VARIANTS = [
    ("paper_literal", "診斷", "5 個擬合後檢查全關（最接近論文字面）", OFF5, 0.0, None),
    ("literal+radius", "診斷", "只開半徑檢查（3 cm–1 m）", OFF5 + ["--cylinder.check_radius=true"], 0.0, None),
    ("literal+tilt", "診斷", "只開傾角檢查（≤ 20°）", OFF5 + ["--cylinder.check_tilt=true"], 0.0, None),
    ("literal+ratio", "診斷", "只開 inlier 比例檢查（≥ 0.5）", OFF5 + ["--cylinder.check_inlier_ratio=true"], 0.0, None),
    ("baseline", "既有", "半徑、傾角、inlier 比例開；法向一致性、圓弧覆蓋關", BASE, 0.0, None),
    ("improved", "既有", "5 個檢查全開（預設）", IMP, 0.0, None),
    ("baseline_rmin5", "優化", "baseline + radius_min 5 cm", BASE + ["--cylinder.radius_min=0.05"], 0.0, None),
    ("baseline_rmin8", "優化", "baseline + radius_min 8 cm", BASE + ["--cylinder.radius_min=0.08"], 0.0, None),
    ("baseline_merge", "優化", "baseline + 合併 0.3 m 內重複樹位（D9 開關）", BASE, 0.3, "baseline"),
    ("improved_merge", "優化", "improved + 合併 0.3 m 內重複樹位（D9 開關）", IMP, 0.3, "improved"),
    ("improved_rmin5", "優化", "improved + radius_min 5 cm", ["--cylinder.radius_min=0.05"], 0.0, None),
    ("improved_loose", "優化", "improved，門檻放寬：法向比例 0.65、覆蓋角 80°",
     ["--cylinder.min_normal_ratio=0.65", "--cylinder.min_arc_deg=80"], 0.0, None),
    ("improved_strict", "優化", "improved，門檻收緊：法向比例 0.85、覆蓋角 120°",
     ["--cylinder.min_normal_ratio=0.85", "--cylinder.min_arc_deg=120"], 0.0, None),
    ("improved_ctol5", "優化", "improved + clustering 距離 5 cm", ["--cluster.tolerance=0.05"], 0.0, None),
    ("improved_ctol20", "優化", "improved + clustering 距離 20 cm", ["--cluster.tolerance=0.20"], 0.0, None),
    # robustness of the merge radius (0.3 m was set a priori, equal to the GT match radius)
    ("improved_merge20", "穩健性", "improved + 合併 0.2 m", IMP, 0.2, "improved"),
    ("improved_merge50", "穩健性", "improved + 合併 0.5 m", IMP, 0.5, "improved"),
    ("improved_loose_merge", "優化", "improved_loose + 合併 0.3 m",
     ["--cylinder.min_normal_ratio=0.65", "--cylinder.min_arc_deg=80"], 0.3, "improved_loose"),
    ("literal_merge", "診斷", "paper_literal + 合併 0.3 m", OFF5, 0.3, "paper_literal"),
    ("improved_ctol20_merge", "優化", "improved + clustering 20 cm + 合併 0.3 m",
     ["--cluster.tolerance=0.20"], 0.3, "improved_ctol20"),
]


def load(p):
    return [(float(r["x"]), float(r["y"]), float(r["z"])) for r in csv.DictReader(open(p))]


def tfm(p):
    return [[float(v) for v in l.split()] for l in open(p) if l.strip()][:3]


def apply(T, p):
    return tuple(sum(T[i][j] * p[j] for j in range(3)) + T[i][3] for i in range(3))


def nn(p, Q):
    return min(range(len(Q)), key=lambda j: math.hypot(p[0] - Q[j][0], p[1] - Q[j][1]))


def extract(name, args, jobs):
    def one(s):
        d = os.path.join(OUT, "runs", name, s)
        f = os.path.join(d, "step_seed1_trees.csv")
        if os.path.exists(f):
            return
        os.makedirs(d, exist_ok=True)
        cmd = [os.path.join(ROOT, "build/cm_extract"), f"--input={RAW}/{s}.ply", f"--out={d}",
               f"--config={ROOT}/config/default.ini", "--scanner.x0=0", "--scanner.y0=0", "--scanner.z0=0"] + args
        with open(os.path.join(d, "log.txt"), "w") as log:
            subprocess.run(cmd, check=True, stdout=log, stderr=subprocess.STDOUT)
    with ThreadPoolExecutor(jobs) as ex:
        list(ex.map(one, SCANS))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", nargs="*")
    ap.add_argument("--jobs", type=int, default=2)
    a = ap.parse_args()
    pairs = [l.split() for l in open(os.path.join(RAW, "pairs.txt")) if l.strip()]
    os.makedirs(os.path.join(OUT, "registration"), exist_ok=True)
    rows = []
    for name, cat, desc, args, merge, reuse in VARIANTS:
        if a.only and name not in a.only:
            continue
        runs = reuse or name
        print("variant", name, flush=True)
        extract(runs, args, a.jobs)
        tf = lambda s: os.path.join(OUT, "runs", runs, s, "step_seed1_trees.csv")
        n_trees = sum(len(load(tf(s))) for s in SCANS)
        dxy, dz, rep_n, rep_hit = [], [], 0, 0
        succ = {"svd": 0, "svd+ransac": 0}
        ep = {"svd": [], "svd+ransac": []}
        n_m, n_c = 0, 0
        for sa, sb in pairs:
            A, B = load(tf(sa)), load(tf(sb))
            T = tfm(os.path.join(RAW, "groundtruth", f"{sa}-{sb}.tfm"))
            At = [apply(T, p) for p in A]
            for i, p in enumerate(At):
                if math.hypot(p[0], p[1]) > 40:  # outside the target scan's coverage
                    continue
                j = nn(p, B)
                d = math.hypot(p[0] - B[j][0], p[1] - B[j][1])
                rep_n += 1
                rep_hit += d < 0.3
                if d < 0.3 and nn(B[j], At) == i:
                    dxy.append(d)
                    dz.append(abs(p[2] - B[j][2]))
            out = os.path.join(OUT, "registration", f"{name}_{sa}-{sb}.csv")
            subprocess.run([os.path.join(ROOT, "build/cm_register"), f"--src_trees={tf(sa)}", f"--tgt_trees={tf(sb)}",
                            f"--src_cloud={RAW}/{sa}.ply", f"--gt={RAW}/groundtruth/{sa}-{sb}.tfm",
                            "--thresholds=0.05", f"--merge_radius={merge}", f"--label={name}_{sa}-{sb}", f"--out={out}"],
                           check=True, stdout=subprocess.DEVNULL)
            for r in csv.DictReader(open(out)):
                succ[r["solver"]] += int(r["success"])
                if r["transform_ok"] == "1":
                    ep[r["solver"]].append(float(r["e_p_m"]))
                if r["solver"] == "svd":
                    n_m += int(r["n_matches"])
                    n_c += int(r["n_correct_matches"])
        med = lambda v: st.median(v) if v else float("nan")
        rows.append(dict(variant=name, category=cat, description=desc, merge_radius=merge, trees=n_trees,
                         repeatable=f"{rep_hit / rep_n:.3f}", matched_trees=len(dz), h_med_cm=f"{med(dxy) * 100:.1f}",
                         z_med_cm=f"{med(dz) * 100:.1f}", matches=n_m, correct=n_c,
                         svd=succ["svd"], ransac=succ["svd+ransac"],
                         ep_svd=f"{med(ep['svd']):.3f}", ep_ransac=f"{med(ep['svd+ransac']):.3f}"))
        print(rows[-1], flush=True)
    path = os.path.join(OUT, "variants.csv")
    old = {r["variant"]: r for r in csv.DictReader(open(path))} if os.path.exists(path) and a.only else {}
    for r in rows:
        old[r["variant"]] = r
    order = [v[0] for v in VARIANTS]
    allrows = sorted(old.values(), key=lambda r: order.index(r["variant"]) if r["variant"] in order else 99)
    with open(path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(allrows[0].keys()))
        w.writeheader()
        w.writerows(allrows)
    L = ["# Keypoint 變體比較（ETH Trees，step、seed 1，配準閾值 5 cm = 論文設定）", "",
         "**所有變體都在 ETH Trees 上比較與挑選，沒有獨立的驗證資料；最佳變體視為「在 ETH 上調出」，不是已驗證。**", "",
         "- 重現率：source 樹位（在 target 掃描 40 m 範圍內）經 GT 轉換後，target 在 0.3 m 內有樹位的比例。",
         "- 水平差、z 差：GT 對齊後 0.3 m 內互為最近鄰的配對樹（偏樂觀）。",
         "- 成功：旋轉 < 2° 且平移 < 0.5 m（D10，我們定的）。e_p 中位數只對求得轉換的對計算。",
         "- 合併（`--merge_radius`，D9，論文沒有）只在配準前套用；樹位數、重現率、水平差、z 差欄位是合併**前**的數字。",
         "- 0.3 m 重現率偏寬鬆：半徑 < 3 cm 的「樹幹表面窄帶」擬合在 0.3 m 內常可重現，但位置在樹幹表面而非中心。", "",
         "| 變體 | 類別 | 說明 | 樹位數 | 重現率 | 配對樹 | 水平差 [cm] | z 差 [cm] | CN 配對（正確） | 成功 svd | 成功 svd+ransac | e_p 中位數 svd [m] |",
         "|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for r in allrows:
        L.append(f"| {r['variant']} | {r['category']} | {r['description']} | {r['trees']} | {float(r['repeatable']):.0%} | "
                 f"{r['matched_trees']} | {r['h_med_cm']} | {r['z_med_cm']} | {r['matches']} ({r['correct']}) | "
                 f"{r['svd']}/10 | {r['ransac']}/10 | {r['ep_svd']} |")
    open(os.path.join(OUT, "variants.md"), "w").write("\n".join(L) + "\n")
    print("\n".join(L))


if __name__ == "__main__":
    main()
