#!/usr/bin/env python3
"""Registration comparison of the four P(d) versions on ETH Trees (standard library only).

Input: cm_compare output with several seeds (default results/eth_trees/dtm_slope/seeds/runs/<scan>/<profile>/
<version>_seed<k>_trees.csv). For every profile, merge radius, version, seed and scan pair (all 15 pairs
sA-sB, A < B) it runs CN registration at the paper's 5 cm (cm_register --batch) and reports:
  - success rate (15 pairs; also the official 10) as mean +- sd over seeds,
  - e_p median over successful runs,
  - paired comparison with step (same pair, same seed; common random numbers): runs where the
    version succeeds and step fails vs the opposite.

  python3 scripts/eth_trees/compare_retention.py [--seeds 10] [--jobs 3]
Writes results/eth_trees/dtm_slope/retention_registration.{md,csv}; per-run CSVs go to <runs>/../registration (git-ignored).
"""
import argparse
import csv
import itertools
import math
import os
import statistics as st
import subprocess
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RAW = os.path.join(ROOT, "data/eth_trees/raw/trees")
SCANS = ["s1", "s2", "s3", "s4", "s5", "s6"]
# file prefix, label
VERSIONS = [("step_raw", "step（論文式 5，基準）"), ("linear_a_raw", "linear_a = min(1, 0.5 + 0.05d)"),
            ("linear_mid_raw", "linear_mid = min(1, 0.375 + 0.05d)"), ("physical_raw", "physical = clip((d/d0)², Pmin, 1)"),
            ("linear_matched", "linear（保留點數與 step 相同）"), ("physical_matched", "physical（保留點數與 step 相同）")]
PROFILES = [("baseline", 0.0), ("improved", 0.0), ("improved", 0.3)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", default=os.path.join(ROOT, "results/eth_trees/dtm_slope/seeds/runs"))
    ap.add_argument("--seeds", type=int, default=10)
    ap.add_argument("--jobs", type=int, default=3)
    ap.add_argument("--out", default=os.path.join(ROOT, "results/eth_trees/dtm_slope/retention_registration"))
    ap.add_argument("--profiles", nargs="+", default=[f"{p}:{m}" for p, m in PROFILES],
                    help="profile:merge_radius, e.g. improved:0.0")
    a = ap.parse_args()
    profiles = [(x.split(':')[0], float(x.split(':')[1])) for x in a.profiles]
    official = {tuple(l.split()) for l in open(os.path.join(RAW, "pairs.txt")) if l.strip()}
    pairs = list(itertools.combinations(SCANS, 2))
    regdir = os.path.join(a.runs, "registration")
    os.makedirs(regdir, exist_ok=True)

    jobs = {s: [] for s in SCANS}  # grouped by source cloud
    keys = []
    for (prof, merge), (ver, _), seed, (sa, sb) in itertools.product(profiles, VERSIONS, range(1, a.seeds + 1), pairs):
        tf = lambda s: os.path.join(a.runs, s, prof, f"{ver}_seed{seed}_trees.csv")
        out = os.path.join(regdir, f"{prof}_m{merge}_{ver}_seed{seed}_{sa}-{sb}.csv")
        keys.append((prof, merge, ver, seed, sa, sb, out))
        if not os.path.exists(out):
            jobs[sa].append(f"{tf(sa)} {tf(sb)} {RAW}/{sa}.ply {RAW}/groundtruth/{sa}-{sb}.tfm {out} "
                            f"{prof}_m{merge}_{ver}_seed{seed}_{sa}-{sb}")

    def run(s):
        if not jobs[s]:
            return
        merges = sorted({float(l.split()[4].split("_m")[1].split("_")[0]) for l in jobs[s]})
        for m in merges:  # one batch per (source cloud, merge radius)
            lines = [l for l in jobs[s] if f"_m{m}_" in l.split()[4]]
            path = os.path.join(regdir, f"batch_{s}_m{m}.txt")
            open(path, "w").write("\n".join(lines) + "\n")
            subprocess.run([os.path.join(ROOT, "build/cm_register"), f"--batch={path}", "--thresholds=0.05",
                            f"--merge_radius={m}"], check=True, stdout=subprocess.DEVNULL)
    with ThreadPoolExecutor(a.jobs) as ex:
        list(ex.map(run, SCANS))

    rows = []
    for prof, merge, ver, seed, sa, sb, out in keys:
        for r in csv.DictReader(open(out)):
            rows.append(dict(profile=prof, merge=merge, version=ver, seed=seed, pair=f"{sa}-{sb}",
                             official=int((sa, sb) in official), solver=r["solver"], n_src_kp=r["n_src_kp"],
                             n_tgt_kp=r["n_tgt_kp"], n_matches=r["n_matches"], n_correct=r["n_correct_matches"],
                             transform_ok=r["transform_ok"], rot_err_deg=r["rot_err_deg"],
                             trans_err_m=r["trans_err_m"], e_p_m=r["e_p_m"], success=int(r["success"])))
    with open(a.out + ".csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)

    idx = {(r["profile"], r["merge"], r["version"], r["seed"], r["pair"], r["solver"]): r for r in rows}
    seeds = range(1, a.seeds + 1)
    pk = [f"{x}-{y}" for x, y in pairs]
    pk10 = [f"{x}-{y}" for x, y in pairs if (x, y) in official]
    ms = lambda v: f"{st.mean(v):.1f} ± {st.stdev(v):.1f}" if len(v) > 1 else f"{v[0]:.1f}"
    L = ["# 四種 P(d) 的配準比較（ETH Trees，新 DTM，論文設定 5 cm，SVD）", "",
         f"- 每個版本：{a.seeds} 個 seed × 15 對（6 站兩兩）= {a.seeds * 15} 次配準；另列官方 10 對。",
         "- 成功：旋轉 < 2° 且平移 < 0.5 m（D10，我們定的）。成功對數為各 seed 的平均 ± 標準差。",
         "- 成對比較：同一對、同一 seed（共用隨機數 CRN）下，「該版本成功、step 失敗」與「step 成功、該版本失敗」的次數。"
         "兩者相近代表差異只是隨機變動。",
         "- step 是論文式 (5)；其他三種是我們的修改。budget-matched 版本的保留點數調成與 step 相同（linear_a 與 linear_mid 調整後相同）。",
         "- improved 與合併重複樹位都不是論文的方法（見 `../variants/findings.md`）。", ""]
    for prof, merge in profiles:
        title = prof + ("" if merge == 0 else f" + 合併 {merge} m")
        L += [f"## {title}", "",
              "| P(d) | 每站平均樹位數 | 成功對數 / 15 | 成功對數 / 官方 10 | e_p 中位數（成功）[m] | 正確配對比例 | 勝 step | 輸 step |",
              "|---|---|---|---|---|---|---|---|"]
        for ver, vlabel in VERSIONS:
            s15 = [sum(idx[(prof, merge, ver, s, p, "svd")]["success"] for p in pk) for s in seeds]
            s10 = [sum(idx[(prof, merge, ver, s, p, "svd")]["success"] for p in pk10) for s in seeds]
            ok = [float(idx[(prof, merge, ver, s, p, "svd")]["e_p_m"]) for s in seeds for p in pk
                  if idx[(prof, merge, ver, s, p, "svd")]["success"]]
            nm = sum(int(idx[(prof, merge, ver, s, p, "svd")]["n_matches"]) for s in seeds for p in pk)
            nc = sum(int(idx[(prof, merge, ver, s, p, "svd")]["n_correct"]) for s in seeds for p in pk)
            ntree = st.mean(int(idx[(prof, merge, ver, s, f"s1-{t}", "svd")]["n_tgt_kp"]) for s in seeds
                            for t in SCANS[1:]) if True else 0
            win = sum(idx[(prof, merge, ver, s, p, "svd")]["success"] and not idx[(prof, merge, "step_raw", s, p, "svd")]["success"]
                      for s in seeds for p in pk)
            lose = sum(idx[(prof, merge, "step_raw", s, p, "svd")]["success"] and not idx[(prof, merge, ver, s, p, "svd")]["success"]
                       for s in seeds for p in pk)
            L.append(f"| {vlabel} | {ntree:.0f} | {ms(s15)} | {ms(s10)} | {st.median(ok) if ok else float('nan'):.3f} | "
                     f"{nc / nm if nm else float('nan'):.2f} | {'–' if ver == 'step_raw' else win} | "
                     f"{'–' if ver == 'step_raw' else lose} |")
        L.append("")
    L += ["每站平均樹位數：s2–s6 五站、各 seed 的平均（不含 s1）；有合併時是合併後、送進匹配的數字。", ""]
    open(a.out + ".md", "w").write("\n".join(L) + "\n")
    print("\n".join(L))


if __name__ == "__main__":
    main()
