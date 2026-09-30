#!/usr/bin/env python3
"""Run cm_compare on every scan in stations.csv that has a documented scanner
position. Standard library only (Linux / WSL2).

Usage:
  python3 scripts/eth_trees/run_all.py [--stations scripts/eth_trees/stations.csv]
      [--out results/eth_trees/runs] [--seeds 10] [--only SCAN ...] [--bin build]

Every scan uses the same parameters: config/default.ini + --experiment.n_seeds.
cm_compare runs both profiles (baseline: extra checks off; improved: on) and
the four P(d) versions (raw + budget-matched). No --ref is ever passed: the
ETH Trees ground truth is pairwise scan transformations, not tree positions,
so no tree precision / recall / position error is computed.

Per scan it writes <out>/<scan>/ (cm_compare output, stdout.txt) and appends a
row to results/eth_trees/run_status.csv (wall time, peak RSS, exit code).
Scans without a documented station are listed as skipped, never run.
"""
import argparse
import csv
import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def read_stations(path):
    with open(path, newline="") as f:
        rows = [r for r in csv.DictReader(line for line in f if not line.startswith("#"))]
    return rows


def eligible(row):
    missing = [k for k in ("x0", "y0", "z0", "source") if not (row.get(k) or "").strip()]
    return missing


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stations", default=os.path.join(ROOT, "scripts/eth_trees/stations.csv"))
    ap.add_argument("--out", default=os.path.join(ROOT, "results/eth_trees/runs"))
    ap.add_argument("--status", default=os.path.join(ROOT, "results/eth_trees/run_status.csv"))
    ap.add_argument("--config", default=os.path.join(ROOT, "config/default.ini"))
    ap.add_argument("--seeds", type=int, default=10)
    ap.add_argument("--bin", default=os.path.join(ROOT, "build"))
    ap.add_argument("--only", nargs="*", help="run only these scan names")
    ap.add_argument("extra", nargs="*", help="extra --section.key=value options for cm_compare")
    a = ap.parse_args()

    exe = os.path.join(a.bin, "cm_compare")
    if not os.access(exe, os.X_OK):
        sys.exit(f"cm_compare not found at {exe}; build first")
    rows = read_stations(a.stations)
    if a.only:
        rows = [r for r in rows if r["scan"] in a.only]
    os.makedirs(a.out, exist_ok=True)
    new_status = not os.path.exists(a.status)
    with open(a.status, "a", newline="") as sf:
        w = csv.writer(sf)
        if new_status:
            w.writerow(["scan", "status", "exit_code", "wall_s", "peak_rss_mb", "x0", "y0", "z0", "source",
                        "command", "note"])
        for r in rows:
            scan = r["scan"]
            missing = eligible(r)
            if missing:
                print(f"SKIP {scan}: missing {', '.join(missing)} (no documented scanner position)")
                w.writerow([scan, "skipped", "", "", "", r.get("x0"), r.get("y0"), r.get("z0"), r.get("source"),
                            "", "missing " + " ".join(missing)])
                sf.flush()
                continue
            out = os.path.join(a.out, scan)
            os.makedirs(out, exist_ok=True)
            cmd = [exe, f"--input={os.path.join(ROOT, r['path'])}", f"--out={out}", f"--config={a.config}",
                   f"--experiment.n_seeds={a.seeds}", f"--scanner.x0={r['x0']}", f"--scanner.y0={r['y0']}",
                   f"--scanner.z0={r['z0']}", "--quiet"] + a.extra
            print(f"RUN  {scan}: {' '.join(cmd)}", flush=True)
            t0 = time.time()
            with open(os.path.join(out, "stdout.txt"), "w") as log:
                p = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)
                _, status, ru = os.wait4(p.pid, 0)
            wall = time.time() - t0
            code = os.waitstatus_to_exitcode(status)
            rss_mb = ru.ru_maxrss / 1024.0  # Linux: KiB
            note = ""
            if code != 0:
                with open(os.path.join(out, "stdout.txt")) as f:
                    tail = f.read().strip().splitlines()[-3:]
                note = " | ".join(tail)[:300]
            print(f"     -> exit {code}, {wall:.1f} s, peak RSS {rss_mb:.0f} MB {note}", flush=True)
            w.writerow([scan, "ok" if code == 0 else "failed", code, f"{wall:.1f}", f"{rss_mb:.0f}", r["x0"],
                        r["y0"], r["z0"], r["source"], " ".join(cmd).replace(ROOT + "/", ""), note])
            sf.flush()


if __name__ == "__main__":
    main()
