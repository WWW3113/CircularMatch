Six-report delivery checkpoint (2026-10-28 Asia/Taipei)

All native experiments COMPLETE. See ../report_experiments_20261028/COMPLETE.json. No native reruns needed.

Rebuild/validate/package: python scripts/eth_trees/finalize_six_week_reports.py
The generator reuses complete scalar CSVs. Every main comparison uses seeds31-40, Tongji Plot1-4 grouped as one dataset, ETH as another.
The ZIP contains exactly13 requested files: overviewTXT +6 talksTXT +6 workbooksXLSX.
Week1: 5cm/10cm × SVD/RANSAC, recoveryOFF; 20 groups and2200 rows complete. report_01_completeness.json records corrected omissions versus the old cc9fbea workbook and unavailable paper details.
Week3: full smooth vs original; separate fixed-axis-only controls. Week4: full DTM×refinement2×2 includes smooth+refinement547/550 vs original546/550; do not suppress the better branch.
Week5 retains v2 regression528/550. Week6 v3 result550/550; known-benchmark result, no unseen-forest/general-best claim.
Paper original executable remains unavailable (public repo README only); paper values are reference, not same-environment author rerun.

After finalizer exits successfully, compare validation hashes and every ZIP member to current files, render sample pages, commit/push scalar evidence and13files/manifests/ZIP, verify immutable GitHub downloads and write DELIVERY.json. Do not upload private runs/raw clouds/tree coordinates/Drive IDs.

FINAL VALIDATION COMPLETE: 2026-10-28T10:43:32.236691+08:00;2624 cell/formula checks,125 independently reaggregated groups,12 charts. Reopened/recalculated all six workbooks with Calc. ZIP13/current bytes identical. First-week rendered with no ###;8 Plot1/3 full-success cells visible. See validation.json and report_01_completeness.json. Only publish/download verification metadata remains.
