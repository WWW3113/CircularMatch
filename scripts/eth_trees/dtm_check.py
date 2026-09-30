#!/usr/bin/env python3
"""Post-hoc DTM consistency diagnostic for detected trees (needs numpy).

For every tree in a *_trees.csv, compare its base height (axis x DTM
intersection) with the lowest scanned points within `radius` m horizontally
(2nd percentile of z, robust to single below-ground noise points). A base far
ABOVE the lowest observed points means the DTM overestimates the ground there
(e.g. occluded ground at long range on a slope), so the 0-3 m height slice held
canopy / branches rather than stems.

This is a diagnostic of DTM failures only. It is NOT a validation of tree
positions: the dataset has no reference tree positions.

Usage:
  python3 scripts/eth_trees/dtm_check.py --ply data/eth_trees/raw/trees/s1.ply \
      --trees results/eth_trees/runs/s1/improved/step_raw_seed1_trees.csv [--radius 1.0] [--flag 1.0]
Also counts duplicate detections (trees within --dup_radius horizontally).
Prints summary lines; --detail prints every tree.
"""
import argparse
import csv
import math

import numpy as np


def read_ply_xyz(path):
    with open(path, "rb") as f:
        head = f.read(65536)
        end = head.find(b"end_header")
        if end < 0:
            raise SystemExit(f"{path}: no end_header")
        nl = head.index(b"\n", end) + 1          # handles LF and CRLF headers
        lines = head[:nl].decode("latin1").splitlines()
        if not any("binary_little_endian" in l for l in lines):
            raise SystemExit(f"{path}: only binary_little_endian PLY supported here")
        props = [l.split()[-1] for l in lines if l.startswith("property float")]
        n = int(next(l for l in lines if l.startswith("element vertex")).split()[-1])
        f.seek(nl)
        a = np.fromfile(f, dtype=np.dtype([(p, "<f4") for p in props]), count=n)
    return a["x"].astype(np.float64), a["y"].astype(np.float64), a["z"].astype(np.float64)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ply", required=True)
    ap.add_argument("--trees", required=True)
    ap.add_argument("--radius", type=float, default=1.0)
    ap.add_argument("--flag", type=float, default=1.0, help="flag if base - lowest observed z > this [m]")
    ap.add_argument("--offset", default="0,0,0", help="scanner offset used by cm_* (original = local + offset)")
    ap.add_argument("--dup_radius", type=float, default=0.5, help="trees closer than this count as one position")
    ap.add_argument("--detail", action="store_true")
    a = ap.parse_args()
    x, y, z = read_ply_xyz(a.ply)
    ox, oy, oz = (float(v) for v in a.offset.split(","))
    cell = a.radius
    ix = np.floor(x / cell).astype(np.int64)
    iy = np.floor(y / cell).astype(np.int64)
    key = ix * 1_000_003 + iy
    order = np.argsort(key, kind="stable")
    ks = key[order]
    trees = list(csv.DictReader(open(a.trees)))
    deltas, flagged = [], []
    for t in trees:
        px, py, pz = float(t["x"]), float(t["y"]), float(t["z"])
        cx, cy = math.floor(px / cell), math.floor(py / cell)
        idx = []
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                kk = (cx + dx) * 1_000_003 + (cy + dy)
                lo, hi = np.searchsorted(ks, kk, "left"), np.searchsorted(ks, kk, "right")
                idx.append(order[lo:hi])
        idx = np.concatenate(idx) if idx else np.array([], dtype=np.int64)
        m = (x[idx] - px) ** 2 + (y[idx] - py) ** 2 < a.radius ** 2
        zz = z[idx][m]
        if zz.size < 5:
            continue
        ground = float(np.percentile(zz, 2))
        d = pz - ground
        deltas.append(d)
        if d > a.flag:
            flagged.append((t["cluster_id"], round(px, 2), round(py, 2), round(math.hypot(px - ox, py - oy), 1),
                            round(pz, 2), round(ground, 2), round(d, 2), t["radius"], t["tilt_deg"]))
        if a.detail:
            print(t["cluster_id"], round(px, 2), round(py, 2), round(pz, 2), round(ground, 2), round(d, 2))
    # Duplicate detections: several trees within dup_radius horizontally
    # (typically one stem split vertically into several clusters).
    pos = [(float(t["x"]), float(t["y"])) for t in trees]
    parent = list(range(len(pos)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    for i in range(len(pos)):
        for j in range(i + 1, len(pos)):
            if math.hypot(pos[i][0] - pos[j][0], pos[i][1] - pos[j][1]) < a.dup_radius:
                parent[find(i)] = find(j)
    groups = len({find(i) for i in range(len(pos))})
    dl = np.array(deltas)
    far = [f[3] for f in flagged]
    print(f"trees={len(trees)} checked={dl.size} flagged(base > lowest observed z + {a.flag} m)={len(flagged)} "
          f"median_delta={np.median(dl) if dl.size else float('nan'):.2f} m "
          f"flagged_distance_m={sorted(far)}")
    print(f"distinct positions (trees merged within {a.dup_radius} m)={groups} -> duplicates={len(trees) - groups}")
    if flagged and not a.detail:
        print("flagged (cluster_id, x, y, dist, base_z, lowest_z_p2, delta, radius, tilt):")
        for f in flagged:
            print("  ", f)


if __name__ == "__main__":
    main()
