Confirmed deliverables: 6 talks TXT + 6 compact XLSX + overview TXT (13 total) and ZIP. Both datasets required; every week compares paper, local5cm, previous week, current. Missing experiments must be run. Never upload raw clouds or extracted tree coordinates.

Live missing-experiment driver: python scripts/eth_trees/complete_report_experiments.py all ; tool session 75696. Frozen plan results/report_experiments_20261028/protocol.json. Resume-safe station complete.json and per-case CSVs. ETH coarse600 completed (5cm/SVD114,5cm/R121,10cm/SVD138,10cm/R144; seeds31–40); ETH fixed-axis smooth300 completed (primary R149/150 vs baseline150). Full smooth is running for missing21 stations, reuses Plot2 five stations. Do not interrupt or rerun based on GT failure.

First week already delivered to user via immutable commit cc9fbea917165fa6ae47d63444e52f5df0a3b6b5 TXT+XLSX, includes both datasets and seed31–40. Later style changes may regenerate same contents. Prior ETH118/123/135/142 were seed11–20; keep as historical cohort only.

Generation/finish code implemented:
  python scripts/eth_trees/make_six_week_reports.py --report 1  (compact 4-combination matrices)
  reports2/5/6 already generated; 3/4 need full_dtm_smooth_rows.csv (1100 rows) from running experiment.
  python scripts/eth_trees/finalize_six_week_reports.py  (builds all6, writes overview, source-independent reaggregation, LibreOffice recompute validation, 13-file ZIP)

Need wait for experiment finish, run finalizer, render sample Excel pages and inspect layout; verify TXT/Excel and negative results. Commit new scalar evidence, scripts, reports, validation and ZIP; do not include runs/private derived trees. Push branch experiment/eth-tuning-20261025; verify raw.githubusercontent.com immutable links HTTP200/SHA and write DELIVERY.json. Final response point to ZIP+overview and first-week TXT/XLSX.

Week sequence: 1 threshold×solver, recoveryOFF; 2 add recovery; 3 frozen smooth full pipeline, with separate fixed-axis controls, on both datasets; 4 compare previous smooth-full to ORIGINAL DTM+refinement (two changes), and also ORIGINAL raw/refined control to isolate backend; 5 previous single init vs multi v2, with v1→v2 negative control; 6 v2→v3 structural verification, same-backend controls and cold representative cases. Explicitly show DTMsmooth→original restoration in week4. All main cases55pairs×10seed31–40, no per-dataset parameter switching.

Author repo reaudited 2026-10-27T20:34 UTC, still README only; author code baseline impossible, paper values are reference. New experiment wall times have concurrent reporting work; not exclusive benchmarks. GT used in development, not per-pair pose selection.
