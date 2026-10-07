#!/usr/bin/env python3
"""Cross-scan consistency of tree positions (diagnostic; standard library only).

For every overlapping pair (pairs.txt) the source trees are mapped into the
target frame with the GT transform; mutual 2-D nearest neighbours closer than
--match are matched trees. Reports the horizontal and the vertical (tree base z)
difference of matched trees, per profile, for one or more runs directories:

  python3 scripts/eth_trees/tree_consistency.py --runs before=results/eth_trees/runs \
      after=results/eth_trees/dtm_slope/runs [--out file.md]

Same caveat as diag_pair_dist.py: only trees within --match are counted, so the
numbers are optimistic, and tree positions are not validated against tree truth.
"""
import argparse
import csv
import math
import os
import statistics as st

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def load(p):
    return [(float(r["x"]), float(r["y"]), float(r["z"])) for r in csv.DictReader(open(p))]


def tfm(p):
    return [[float(v) for v in l.split()] for l in open(p) if l.strip()][:3]


def apply(T, p):
    return tuple(sum(T[i][j] * p[j] for j in range(3)) + T[i][3] for i in range(3))


def nn(p, Q):
    return min(range(len(Q)), key=lambda j: math.hypot(p[0] - Q[j][0], p[1] - Q[j][1]))


def pct(xs, q):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(q * (len(xs) - 1)))]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", nargs="+", required=True, help="label=dir")
    ap.add_argument("--gt", default=os.path.join(ROOT, "data/eth_trees/raw/trees"))
    ap.add_argument("--trees", default="step_raw_seed1_trees.csv")
    ap.add_argument("--match", type=float, default=0.3)
    ap.add_argument("--out")
    a = ap.parse_args()
    pairs = [l.split() for l in open(os.path.join(a.gt, "pairs.txt")) if l.strip()]
    L = [f"樹位：`{a.trees}`；配對樹 = GT 對齊後 2D 互為最近鄰且 < {a.match} m（只統計這些樹，結果偏樂觀）。", "",
         "| runs | keypoints | 樹位數（6 站合計） | 配對樹（10 對） | 水平差中位數 [cm] | z 差中位數 [cm] | z 差 p90 [cm] | z 差 > 0.5 m | z 差 > 1 m |",
         "|---|---|---|---|---|---|---|---|---|"]
    for spec in a.runs:
        label, d = spec.split("=", 1)
        for prof in ("baseline", "improved"):
            dxy, dz = [], []
            n_trees = sum(len(load(os.path.join(d, s, prof, a.trees))) for s in sorted({x for p in pairs for x in p}))
            for sa, sb in pairs:
                A = load(os.path.join(d, sa, prof, a.trees))
                B = load(os.path.join(d, sb, prof, a.trees))
                T = tfm(os.path.join(a.gt, "groundtruth", f"{sa}-{sb}.tfm"))
                At = [apply(T, p) for p in A]
                for i, p in enumerate(At):
                    j = nn(p, B)
                    dd = math.hypot(p[0] - B[j][0], p[1] - B[j][1])
                    if dd < a.match and nn(B[j], At) == i:
                        dxy.append(dd)
                        dz.append(abs(p[2] - B[j][2]))
            L.append(f"| {label} | {prof} | {n_trees} | {len(dz)} | {st.median(dxy) * 100:.1f} | {st.median(dz) * 100:.1f} | "
                     f"{pct(dz, 0.9) * 100:.1f} | {sum(x > 0.5 for x in dz) / len(dz):.1%} | {sum(x > 1 for x in dz) / len(dz):.1%} |")
    txt = "\n".join(L) + "\n"
    if a.out:
        open(a.out, "w").write(txt)
    print(txt)


if __name__ == "__main__":
    main()
