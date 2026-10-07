#!/usr/bin/env python3
"""Diagnostic before implementing CN matching (standard library only).

For every overlapping scan pair (pairs.txt), take the tree positions of both
scans, map the source trees into the target frame with the ground-truth
transform (sA-sB.tfm maps sA coordinates into sB), and keep mutual nearest
neighbours closer than --match (2-D) as matched trees. For every two matched
trees i, j compare their distance in the source scan (d_ij) with their distance
in the target scan (d'_ij):

  2D raw    : |d_ij - d'_ij| with 2-D distances, each in its own scan frame
              (the quantity the CN descriptor compares; rigid-invariant except
              for the small tilt in the GT rotation)
  2D GT-rot : 2-D distance of the source trees AFTER applying the GT transform
              (removes the tilt effect; pure position noise of the two scans)
  3D        : 3-D distances (rigid-invariant, but includes tree-base height noise)

Reports per profile: matched trees per pair, number of (i, j) pairs, median
|delta d| and the share below 5 cm (also 10 / 20 cm).
"""
import argparse
import csv
import math
import os
import statistics as st

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def load_trees(path):
    return [(float(r["x"]), float(r["y"]), float(r["z"])) for r in csv.DictReader(open(path))]


def load_tfm(path):
    rows = [[float(v) for v in l.split()] for l in open(path) if l.strip()]
    return rows[:3]


def apply(T, p):
    return tuple(sum(T[i][j] * p[j] for j in range(3)) + T[i][3] for i in range(3))


def mutual_nn(src_t, dst, radius):
    def nn(p, pts):
        best, bj = 1e18, -1
        for j, q in enumerate(pts):
            d = math.hypot(p[0] - q[0], p[1] - q[1])
            if d < best:
                best, bj = d, j
        return bj, best
    pairs = []
    for i, p in enumerate(src_t):
        j, d = nn(p, dst)
        if d < radius and nn(dst[j], src_t)[0] == i:
            pairs.append((i, j))
    return pairs


def pct(xs, q):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(q * (len(xs) - 1)))]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", default=os.path.join(ROOT, "results/eth_trees/runs"))
    ap.add_argument("--gt", default=os.path.join(ROOT, "data/eth_trees/raw/trees"))
    ap.add_argument("--trees", default="step_raw_seed1_trees.csv")
    ap.add_argument("--match", type=float, default=0.3)
    ap.add_argument("--out", default=os.path.join(ROOT, "results/eth_trees/diag_pair_dist.md"))
    a = ap.parse_args()
    pairs = [l.split() for l in open(os.path.join(a.gt, "pairs.txt")) if l.strip()]
    L = ["# 診斷：配對樹之間的距離差 |d_ij − d'_ij|", "",
         f"樹位：`{a.trees}`（各站、各設定）。配對樹：以 GT 轉換把 source 樹位轉到 target 後，"
         f"2D 互為最近鄰且距離 < {a.match} m。對每一對配對樹 (i, j) 比較 source 中的距離 d_ij 與 target 中的 d'_ij。",
         "", "- **2D**：各自 scan 座標下的 2D 水平距離（CN 描述子比較的量）",
         "- **2D（GT 旋轉後）**：source 先套用 GT 轉換再算 2D 距離（排除 GT 旋轉中微小傾斜的影響）",
         "- **3D**：3D 距離（剛體不變，但包含樹基高度誤差）", ""]
    for prof in ("baseline", "improved"):
        allv = {"2d": [], "2drot": [], "3d": []}
        rows = []
        for sa, sb in pairs:
            A = load_trees(os.path.join(a.runs, sa, prof, a.trees))
            B = load_trees(os.path.join(a.runs, sb, prof, a.trees))
            T = load_tfm(os.path.join(a.gt, "groundtruth", f"{sa}-{sb}.tfm"))
            At = [apply(T, p) for p in A]
            m = mutual_nn(At, B, a.match)
            v = {"2d": [], "2drot": [], "3d": []}
            for x in range(len(m)):
                for y in range(x + 1, len(m)):
                    (i, j), (k, l) = m[x], m[y]
                    d2 = math.hypot(A[i][0] - A[k][0], A[i][1] - A[k][1])
                    e2 = math.hypot(B[j][0] - B[l][0], B[j][1] - B[l][1])
                    r2 = math.hypot(At[i][0] - At[k][0], At[i][1] - At[k][1])
                    d3 = math.dist(A[i], A[k])
                    e3 = math.dist(B[j], B[l])
                    v["2d"].append(abs(d2 - e2))
                    v["2drot"].append(abs(r2 - e2))
                    v["3d"].append(abs(d3 - e3))
            for key in v:
                allv[key] += v[key]
            n = len(v["2d"])
            rows.append((f"{sa}-{sb}", len(A), len(B), len(m), n,
                         st.median(v["2d"]) if n else float("nan"),
                         sum(x < 0.05 for x in v["2d"]) / n if n else float("nan")))
        L += [f"## {prof}", "", "| pair | source 樹位數 | target 樹位數 | 配對樹 | (i, j) 對數 | 2D 中位數 [cm] | 2D < 5 cm |",
              "|---|---|---|---|---|---|---|"]
        for r in rows:
            L.append(f"| {r[0]} | {r[1]} | {r[2]} | {r[3]} | {r[4]} | {r[5] * 100:.1f} | {r[6]:.1%} |")
        L += ["", "全部 10 對合計：", "", "| 距離 | (i, j) 對數 | 中位數 [cm] | p90 [cm] | < 5 cm | < 10 cm | < 20 cm |",
              "|---|---|---|---|---|---|---|"]
        for key, name in (("2d", "2D"), ("2drot", "2D（GT 旋轉後）"), ("3d", "3D")):
            xs = allv[key]
            L.append(f"| {name} | {len(xs)} | {st.median(xs) * 100:.1f} | {pct(xs, 0.9) * 100:.1f} | "
                     f"{sum(x < 0.05 for x in xs) / len(xs):.1%} | {sum(x < 0.10 for x in xs) / len(xs):.1%} | "
                     f"{sum(x < 0.20 for x in xs) / len(xs):.1%} |")
        L.append("")
    open(a.out, "w").write("\n".join(L) + "\n")
    print("\n".join(L))


if __name__ == "__main__":
    main()
