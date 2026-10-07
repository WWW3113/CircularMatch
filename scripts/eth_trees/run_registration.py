#!/usr/bin/env python3
"""Run CN-descriptor registration (cm_register) on all ETH Trees pairs and report.

For profile in (baseline, improved), for each overlapping pair sA-sB in pairs.txt:
  cm_register --src_trees=<runs>/sA/<profile>/<trees> --tgt_trees=<runs>/sB/<profile>/<trees>
              --src_cloud=<raw>/sA.ply --gt=<raw>/groundtruth/sA-sB.tfm --thresholds=0.05,0.10,0.15
Writes results/eth_trees/registration/<profile>_<pair>.csv and registration_report.md.
Threshold 0.05 m is the paper's setting; 0.10 and 0.15 m are a sensitivity analysis.
"""
import argparse
import csv
import math
import os
import statistics as st
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PAPER_ETH = "論文 Table I（ETH-tree，Ours）：e_p = 0.153427 m，成功率 100%（論文未說明成功標準）"


def f(x, p=3):
    try:
        v = float(x)
    except (TypeError, ValueError):
        return "–"
    return "–" if math.isnan(v) else f"{v:.{p}f}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", default=os.path.join(ROOT, "results/eth_trees/runs"))
    ap.add_argument("--raw", default=os.path.join(ROOT, "data/eth_trees/raw/trees"))
    ap.add_argument("--trees", default="step_raw_seed1_trees.csv")
    ap.add_argument("--thresholds", default="0.05,0.10,0.15")
    ap.add_argument("--bin", default=os.path.join(ROOT, "build/cm_register"))
    ap.add_argument("--out", default=os.path.join(ROOT, "results/eth_trees/registration"))
    ap.add_argument("--report", default=os.path.join(ROOT, "results/eth_trees/registration_report.md"))
    ap.add_argument("--skip_run", action="store_true", help="only rebuild the report from existing CSVs")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    pairs = [l.split() for l in open(os.path.join(a.raw, "pairs.txt")) if l.strip()]
    rows = []
    for prof in ("baseline", "improved"):
        for sa, sb in pairs:
            out = os.path.join(a.out, f"{prof}_{sa}-{sb}.csv")
            if not a.skip_run:
                cmd = [a.bin, f"--src_trees={os.path.join(a.runs, sa, prof, a.trees)}",
                       f"--tgt_trees={os.path.join(a.runs, sb, prof, a.trees)}",
                       f"--src_cloud={os.path.join(a.raw, sa + '.ply')}",
                       f"--gt={os.path.join(a.raw, 'groundtruth', f'{sa}-{sb}.tfm')}",
                       f"--thresholds={a.thresholds}", f"--label={prof}_{sa}-{sb}", f"--out={out}"]
                print("RUN", " ".join(cmd).replace(ROOT + "/", ""), flush=True)
                subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            for r in csv.DictReader(open(out)):
                r["profile"], r["pair"] = prof, f"{sa}-{sb}"
                rows.append(r)

    L = ["# ETH Trees — CN 描述子配準結果（coarse，無 ICP）", "",
         "- keypoints：各站 `" + a.trees + "`（baseline 與 improved 兩組）；描述子與匹配依論文 §II-A/B 與 Algorithm 1，"
         "論文未指定之處見 `docs/cn_ambiguities.md`。",
         "- **距離閾值 5 cm 是論文設定；10 與 15 cm 是敏感度分析，不是論文設定。**",
         "- 求轉換：`svd`（Kabsch）與 `svd+ransac` 兩種，**論文未指定求轉換的方法**。",
         "- 成功：旋轉誤差 < 2° 且平移誤差 < 0.5 m（論文未給標準，D10）。e_p（式 8）對 source scan 的所有點計算。",
         "- 正確配對數：配對的 source 樹位經 GT 轉換後與 target 樹位的 2D 距離 < 0.3 m（只作診斷）。",
         f"- {PAPER_ETH}", ""]
    thresholds = sorted({float(r["threshold"]) for r in rows})
    # Summary table
    L += ["## 摘要（10 對）", "",
          "| keypoints | 閾值 | 設定 | 求轉換 | 成功 | e_p 中位數 [m] | 旋轉誤差中位數 [°] | 平移誤差中位數 [m] | 配對數中位數 | 正確配對比例中位數 | 用到 triangle |",
          "|---|---|---|---|---|---|---|---|---|---|---|"]
    for thr in thresholds:
        for prof in ("baseline", "improved"):
            for solver in ("svd", "svd+ransac"):
                sel = [r for r in rows if r["profile"] == prof and float(r["threshold"]) == thr and r["solver"] == solver]
                if not sel:
                    continue
                med = lambda k: st.median([float(r[k]) for r in sel if not math.isnan(float(r[k]))]) \
                    if any(not math.isnan(float(r[k])) for r in sel) else float("nan")
                ratio = [int(r["n_correct_matches"]) / int(r["n_matches"]) for r in sel if int(r["n_matches"]) > 0]
                L.append(f"| {prof} | {thr * 100:.0f} cm | {'論文' if abs(thr - 0.05) < 1e-9 else '敏感度（非論文）'} | "
                         f"{solver} | {sum(int(r['success']) for r in sel)}/{len(sel)} | {f(med('e_p_m'))} | "
                         f"{f(med('rot_err_deg'), 2)} | {f(med('trans_err_m'))} | "
                         f"{st.median(int(r['n_matches']) for r in sel):.0f} | "
                         f"{(st.median(ratio) if ratio else float('nan')):.2f} | "
                         f"{sum(int(r['used_triangle']) for r in sel)}/{len(sel)} |")
    L.append("")
    # Per-pair raw numbers
    for thr in thresholds:
        tag = "論文設定" if abs(thr - 0.05) < 1e-9 else "敏感度分析，非論文設定"
        L += [f"## 每一對的原始數字：閾值 {thr * 100:.0f} cm（{tag}）", "",
              "| keypoints | pair | 求轉換 | 樹位數 src/tgt | 候選對 | 最佳分數 (DC, FM) | triangle | 配對數 | 正確配對 | RANSAC inliers | 旋轉誤差 [°] | 平移誤差 [m] | e_p [m] | 成功 |",
              "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
        for prof in ("baseline", "improved"):
            for solver in ("svd", "svd+ransac"):
                for r in rows:
                    if r["profile"] != prof or float(r["threshold"]) != thr or r["solver"] != solver:
                        continue
                    L.append(f"| {prof} | {r['pair']} | {solver} | {r['n_src_kp']}/{r['n_tgt_kp']} | {r['candidates']} | "
                             f"{f(r['best_score'], 0)} ({r['best_dc']}, {r['best_fm']}) | "
                             f"{'是' if r['used_triangle'] == '1' else '否'} | {r['n_matches']} | {r['n_correct_matches']} | "
                             f"{r['ransac_inliers'] if solver != 'svd' else '–'} | {f(r['rot_err_deg'], 2)} | "
                             f"{f(r['trans_err_m'])} | {f(r['e_p_m'])} | {'✓' if r['success'] == '1' else '✗'} |")
        L.append("")
    # D12 statistics (paper setting)
    L += ["## D12：encode2 / encode3 的 sector 0 最近點為 p1（或 p2）的比例（5 cm，描述子與閾值無關）", "",
          "| keypoints | 描述子總數（src+tgt，10 對） | encode2 sector 0 = p1 | encode3 sector 0 ∈ {p1, p2} |", "|---|---|---|---|"]
    for prof in ("baseline", "improved"):
        sel = [r for r in rows if r["profile"] == prof and abs(float(r["threshold"]) - 0.05) < 1e-9 and r["solver"] == "svd"]
        n = sum(int(r["descriptors_src"]) + int(r["descriptors_tgt"]) for r in sel)
        e2 = sum(int(r["d12_enc2_s0_is_p1"]) for r in sel)
        e3 = sum(int(r["d12_enc3_s0_is_p1p2"]) for r in sel)
        L.append(f"| {prof} | {n} | {e2} ({e2 / n:.1%}) | {e3} ({e3 / n:.1%}) |")
    L.append("")
    open(a.report, "w").write("\n".join(L) + "\n")
    print("wrote", a.report)


if __name__ == "__main__":
    main()
