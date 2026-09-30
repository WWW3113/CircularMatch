#!/usr/bin/env python3
"""Aggregate cm_compare outputs of several scans into per-station tables.

Usage: python3 scripts/eth_trees/aggregate.py [--runs results/eth_trees/runs]
                                              [--out results/eth_trees/summary.md]

For each profile (baseline / improved) and table (raw / budget-matched) it
reports, per station and P(d) version: kept points, clusters, trees, RANSAC
success rate (mean ± std over seeds), the two most frequent failure reasons,
and the stage 5-8 time; plus shared stage 1-4 point counts and times, and the
mean ± std ACROSS stations of the per-station seed means.

Only execution statistics are reported. No tree precision / recall / position
error: there are no reference tree positions for this dataset.
"""
import argparse
import csv
import glob
import math
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PROFILES = [("baseline", "未啟用額外檢查的基準設定 (baseline: extra checks OFF)"),
            ("improved", "啟用檢查的改良設定 (improved: extra checks ON)")]
TABLES = [("raw", "Raw (P(d) as defined)"), ("matched", "Budget-matched (kept count = step's)")]


def read_summary_csv(path):
    """-> {metric: {label: (mean, std)}}, labels in column order."""
    with open(path, newline="") as f:
        rows = list(csv.reader(f))
    header = rows[0]
    labels = [h[: -len("_mean")] for h in header[1::2]]
    out = {}
    for r in rows[1:]:
        out[r[0]] = {l: (float(r[1 + 2 * i]), float(r[2 + 2 * i])) for i, l in enumerate(labels)}
    return out, labels


def read_shared_stages(md_path):
    stages = []
    with open(md_path) as f:
        for line in f:
            m = re.match(r"\| (\d [\w+ ]+?) \| (\d+) \| ([\d.eE+-]+) \|", line)
            if m:
                stages.append((m.group(1), int(m.group(2)), float(m.group(3))))
    return stages


def fmt(m, s, p):
    if m is None or (isinstance(m, float) and math.isnan(m)):
        return "–"
    return f"{m:.{p}f}" + ("" if s is None or math.isnan(s) else f" ± {s:.{p}f}")


def mean_std(xs):
    xs = [x for x in xs if not math.isnan(x)]
    if not xs:
        return float("nan"), float("nan")
    m = sum(xs) / len(xs)
    if len(xs) < 2:
        return m, float("nan")
    return m, math.sqrt(sum((x - m) ** 2 for x in xs) / (len(xs) - 1))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", default=os.path.join(ROOT, "results/eth_trees/runs"))
    ap.add_argument("--out", default=os.path.join(ROOT, "results/eth_trees/summary.md"))
    a = ap.parse_args()

    scans = sorted(d for d in os.listdir(a.runs) if os.path.isfile(os.path.join(a.runs, d, "summary.md")))
    lines = ["# ETH Trees: keypoint extraction runs (execution statistics only)", "",
             "> 程式成功執行 ≠ 樹位準確度已驗證。此資料集沒有人工標註的參考樹位，",
             "> 因此不計算 precision、recall 或樹位誤差。數字只描述程式行為（點數、樹幹數、成功率、耗時）。", "",
             f"Scans: {', '.join(scans) if scans else '(none)'}", ""]
    if not scans:
        open(a.out, "w").write("\n".join(lines) + "\n")
        print("no completed runs")
        return

    lines += ["## Shared stages 1-4 (per scan, identical for all profiles / versions / seeds)", "",
              "| scan | " + " | ".join(f"{n} pts" for n, _, _ in read_shared_stages(
                  os.path.join(a.runs, scans[0], "summary.md"))) + " | stage 1-4 time [s] |",
              "|---|" + "---|" * (len(read_shared_stages(os.path.join(a.runs, scans[0], "summary.md"))) + 1)]
    for s in scans:
        st = read_shared_stages(os.path.join(a.runs, s, "summary.md"))
        lines.append(f"| {s} | " + " | ".join(str(p) for _, p, _ in st) + f" | {sum(t for *_, t in st) / 1000:.1f} |")
    lines.append("")

    for prof, ptitle in PROFILES:
        lines += [f"## {ptitle}", ""]
        for tab, ttitle in TABLES:
            per = {}
            labels = None
            for s in scans:
                p = os.path.join(a.runs, s, prof, f"summary_{tab}.csv")
                if os.path.exists(p):
                    per[s], labels = read_summary_csv(p)
            if not per:
                continue
            lines += [f"### {prof} / {ttitle}", "",
                      "Per station: mean ± std over seeds.", "",
                      "| scan | version | kept points | clusters | trees | RANSAC success | main failures (mean count) | stage 5-8 [ms] |",
                      "|---|---|---|---|---|---|---|---|"]
            for s, d in per.items():
                for l in labels:
                    fails = sorted(((k[len("fail: "):], v[l][0]) for k, v in d.items() if k.startswith("fail: ")),
                                   key=lambda x: -x[1])
                    top = ", ".join(f"{k} {v:.1f}" for k, v in fails[:2] if v > 0) or "none"
                    lines.append(f"| {s} | {l} | {fmt(*d['kept points'][l], 0)} | {fmt(*d['clusters'][l], 1)} | "
                                 f"{fmt(*d['trees'][l], 1)} | {fmt(*d['RANSAC success rate'][l], 3)} | {top} | "
                                 f"{fmt(*d['time total 5-8 [ms]'][l], 0)} |")
            lines += ["", f"Across stations (n = {len(per)}): mean ± std of the per-station seed means.", "",
                      "| version | kept points | trees | RANSAC success | stage 5-8 [ms] |", "|---|---|---|---|---|"]
            for l in labels:
                cols = []
                for metric, p in (("kept points", 0), ("trees", 1), ("RANSAC success rate", 3),
                                  ("time total 5-8 [ms]", 0)):
                    cols.append(fmt(*mean_std([d[metric][l][0] for d in per.values()]), p))
                lines.append(f"| {l} | " + " | ".join(cols) + " |")
            lines.append("")
    open(a.out, "w").write("\n".join(lines) + "\n")
    print(f"wrote {a.out} ({len(scans)} scans)")


if __name__ == "__main__":
    main()
