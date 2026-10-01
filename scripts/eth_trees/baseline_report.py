#!/usr/bin/env python3
"""Baseline-profile report for the ETH Trees runs (standard library only).

Reads results/eth_trees/runs/<scan>/baseline/ and writes
results/eth_trees/baseline_report.md with, per station s1-s6:
  - raw P(d) table (step, linear_a, linear_mid, physical) and the separate
    budget-matched table (step, linear*, physical*): kept points, cylinders
    that passed all checks, RANSAC success rate, stage 5-8 time (mean +- std
    over seeds);
  - where the kept points come from (candidate / kept points < 10 m and
    >= 10 m from the scanner, seed 1);
  - how the final tree positions of each version relate to step's (same seed):
    identical (< 1 mm), shifted (< 0.5 m), or without counterpart, split by
    horizontal distance to the scanner (< 10 m / >= 10 m).

Cylinder counts are NOT validated against tree ground truth (none exists).
"""
import csv
import glob
import math
import os
import statistics as st

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RUNS = os.path.join(ROOT, "results/eth_trees/runs")
OUT = os.path.join(ROOT, "results/eth_trees/baseline_report.md")
SCANS = ["s1", "s2", "s3", "s4", "s5", "s6"]
RAW = ["step", "linear_a", "linear_mid", "physical"]
MATCHED = ["step", "linear*", "physical*"]
NEAR = 10.0  # band split for the position comparison (step, linear_a, physical reach P = 1 here)


def summary(path):
    rows = list(csv.reader(open(path)))
    labels = [h[: -len("_mean")] for h in rows[0][1::2]]
    return {r[0]: {l: (float(r[1 + 2 * i]), float(r[2 + 2 * i])) for i, l in enumerate(labels)} for r in rows[1:]}


def ms(m, s, p):
    return f"{m:,.{p}f} ± {s:,.{p}f}"


def trees(path):
    return [(float(r["x"]), float(r["y"]), float(r["radius"])) for r in csv.DictReader(open(path))]


def compare(ref, other):
    """Classify `other` trees against `ref` (same seed). Returns counters per band."""
    out = {b: {"identical": 0, "shifted": 0, "new": 0, "lost": 0, "shifts": []} for b in ("near", "far")}
    used = set()
    for x, y, r in other:
        band = "near" if math.hypot(x, y) < NEAR else "far"
        best, bj = 1e9, -1
        for j, (a, b_, _) in enumerate(ref):
            d = math.hypot(x - a, y - b_)
            if d < best:
                best, bj = d, j
        if best < 1e-3 and abs(r - ref[bj][2]) < 1e-3:
            out[band]["identical"] += 1
            used.add(bj)
        elif best < 0.5:
            out[band]["shifted"] += 1
            out[band]["shifts"].append(best)
            used.add(bj)
        else:
            out[band]["new"] += 1
    for j, (a, b_, _) in enumerate(ref):
        if j not in used:
            out["near" if math.hypot(a, b_) < NEAR else "far"]["lost"] += 1
    return out


def bins(path):
    near_c = near_k = far_c = far_k = 0
    for r in csv.DictReader(open(path)):
        n, k = int(r["n"]), int(r["kept"])
        if float(r["d_lo"]) < NEAR:
            near_c += n
            near_k += k
        else:
            far_c += n
            far_k += k
    return near_c, near_k, far_c, far_k


def fname(label, table):
    return label.replace("*", "") + ("_raw" if table == "raw" else "_matched")


def main():
    L = ["# ETH Trees — baseline 設定：四種 P(d) 的逐站比較", "",
         "> **圓柱數尚未經樹木真值驗證。** 本資料集沒有人工標註的參考樹位；「通過檢查的圓柱數」是通過全部擬合後檢查"
         "（baseline：半徑、傾角、inlier 比例、DTM 交點；未啟用法向一致性與圓弧覆蓋角）並求得樹位的圓柱數量，"
         "不等於實際樹木數，也不代表樹位正確。", "",
         "設定：`config/default.ini`，`cylinder.check_normal_consistency=false`、`cylinder.check_arc_coverage=false`；"
         "掃描儀位置 (0,0,0)（資料檢查推定，使用者已接受）；每種 P(d) 10 個 seed（1–10）。"
         "數值為 10 seed 的平均 ± 樣本標準差。", ""]
    agg = {}
    for table, labels, title in (("raw", RAW, "Raw：P(d) 依定義"),
                                 ("matched", MATCHED, "保留點數相同（budget-matched）：linear*、physical* 的參數調整到與同 seed 的 step 保留相同點數")):
        L += [f"## {title}", ""]
        if table == "matched":
            L += ["linear_a 與 linear_mid 只差截距，保留點數對齊後是同一個函數，所以合併為一欄 `linear*`。", ""]
        L += ["| 站 | P(d) | 保留點數 | 通過檢查的圓柱數 | RANSAC 成功率 | 第 5–8 階段時間 [ms] |", "|---|---|---|---|---|---|"]
        for s in SCANS:
            d = summary(os.path.join(RUNS, s, "baseline", f"summary_{table}.csv"))
            for l in labels:
                L.append(f"| {s} | {l} | {ms(*d['kept points'][l], 0)} | {ms(*d['trees'][l], 1)} | "
                         f"{ms(*d['RANSAC success rate'][l], 3)} | {ms(*d['time total 5-8 [ms]'][l], 0)} |")
                agg.setdefault((table, l), []).append(d)
        L += ["", "跨站（6 站各自 10 seed 平均值的平均 ± 標準差）：", "",
              "| P(d) | 保留點數 | 通過檢查的圓柱數 | RANSAC 成功率 | 第 5–8 階段時間 [ms] |", "|---|---|---|---|---|"]
        for l in labels:
            ds = agg[(table, l)]
            cols = []
            for m, p in (("kept points", 0), ("trees", 1), ("RANSAC success rate", 3), ("time total 5-8 [ms]", 0)):
                v = [x[m][l][0] for x in ds]
                cols.append(ms(st.mean(v), st.stdev(v), p))
            L.append(f"| {l} | " + " | ".join(cols) + " |")
        L.append("")

    # Where the kept points are (seed 1, raw).
    L += ["## 保留點在哪裡（raw，seed 1）", "",
          "P(d) 達到 1（之後全部保留）的距離：step、linear_a、physical 為 10 m；linear_mid（0.375 + 0.05 d）為 12.5 m。"
          "budget-matched 的 linear* 約 11.7 m（截距約 0.41），physical* 約 6.8 m（d0 約 6.8 m）。"
          "因此 step、linear_a、physical 的保留差異全部在 10 m 以內；linear_mid 另外在 10–12.5 m 少保留一些點。", "",
          "| 站 | 垂直候選點 < 10 m | ≥ 10 m | 保留 < 10 m：step | linear_a | linear_mid | physical | 保留 ≥ 10 m：step / linear_a / physical | linear_mid |",
          "|---|---|---|---|---|---|---|---|---|"]
    for s in SCANS:
        vals = {l: bins(os.path.join(RUNS, s, "baseline", f"{fname(l, 'raw')}_seed1_retention_bins.csv")) for l in RAW}
        nc, _, fc, fk = vals["step"]
        same3 = vals["linear_a"][3] == fk == vals["physical"][3] == fc
        L.append(f"| {s} | {nc:,} | {fc:,} | " + " | ".join(f"{vals[l][1]:,} ({vals[l][1] / nc:.0%})" for l in RAW)
                 + f" | {fk:,}（{'= 全部候選點' if same3 else '不一致！'}） | {vals['linear_mid'][3]:,} ({vals['linear_mid'][3] / fc:.0%}) |")
    L.append("")

    # Relationship between kept points and final tree positions.
    L += ["## 保留點與最終樹位的關係：各版本樹位與同 seed 的 step 比較", "",
          "對每站每個 seed，把該版本的每個樹位對到 step 最近的樹位：**相同**＝水平距離 < 1 mm 且半徑差 < 1 mm；"
          "**位移**＝< 0.5 m 但不相同；**新增**＝0.5 m 內沒有 step 樹位；**消失**＝step 樹位沒有被對到。"
          f"依樹位到掃描儀的水平距離分成 < {NEAR:.0f} m 與 ≥ {NEAR:.0f} m。數字為 6 站 × 10 seed 的合計。"
          "第一列是同一個 step、不同 seed 的比較，代表只有抽樣雜訊時的變動量。", "",
          "| 比較 | 距離 | step 樹位數 | 相同 | 位移 | 位移量中位數 / p90 [cm] | 新增 | 消失 |", "|---|---|---|---|---|---|---|---|"]
    # Reference row: step seed k vs step seed k+1 (same P(d), different random
    # draws) = how much tree positions change from sampling noise alone.
    for table, labels in (("noise", ["step(seed k+1)"]), ("raw", RAW[1:]), ("matched", MATCHED[1:])):
        for l in labels:
            tot = {b: {"identical": 0, "shifted": 0, "new": 0, "lost": 0, "ref": 0, "shifts": []} for b in ("near", "far")}
            for s in SCANS:
                for seed in range(1, 11):
                    ref = trees(os.path.join(RUNS, s, "baseline", f"step_raw_seed{seed}_trees.csv"))
                    if table == "noise":
                        oth = trees(os.path.join(RUNS, s, "baseline", f"step_raw_seed{seed % 10 + 1}_trees.csv"))
                    else:
                        oth = trees(os.path.join(RUNS, s, "baseline", f"{fname(l, table)}_seed{seed}_trees.csv"))
                    c = compare(ref, oth)
                    for b in c:
                        for k in c[b]:
                            tot[b][k] += c[b][k]
                    for x, y, _ in ref:
                        tot["near" if math.hypot(x, y) < NEAR else "far"]["ref"] += 1
            for b, name in (("near", f"< {NEAR:.0f} m"), ("far", f"≥ {NEAR:.0f} m")):
                t = tot[b]
                sh = sorted(t["shifts"])
                q = (f"{sh[len(sh) // 2] * 100:.1f} / {sh[int(0.9 * (len(sh) - 1))] * 100:.1f}" if sh else "–")
                tag = "step seed k vs step seed k+1（抽樣雜訊參考）" if table == "noise" else f"step vs {l}（{table}）"
                L.append(f"| {tag} | {name} | {t['ref']} | {t['identical']} | {t['shifted']} | {q} | "
                         f"{t['new']} | {t['lost']} |")
    L.append("")
    open(OUT, "w").write("\n".join(L) + "\n")
    print("wrote", OUT)


if __name__ == "__main__":
    main()
