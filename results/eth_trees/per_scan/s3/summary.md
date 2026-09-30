> Synthetic/experimental output. Implementation choices (not specified by the paper) are marked in effective_config.ini as [UNSPECIFIED].

Shared stages 1-4 (computed once, identical for all profiles/versions/seeds):

| stage | points | time [ms] |
|---|---|---|
| 0 input | 19767841 | 0 |
| 1 dtm+height | 17292553 | 4506.33 |
| 2 voxel | 3858526 | 4127.35 |
| 3 normals | 3815378 | 21871 |
| 4 verticality | 387190 | 77.2309 |

## 未啟用額外檢查的基準設定 (baseline: normal-consistency and arc-coverage checks OFF)

### baseline / Raw (P(d) as defined)

| metric | step | linear_a | linear_mid | physical |
|---|---|---|---|---|
| kept points | 318387.9 ± 181.3 | 342780.8 ± 136.3 | 314624.8 ± 219.8 | 264455.3 ± 184.1 |
| clusters | 1013.80 ± 3.08 | 1063.10 ± 4.12 | 1017.70 ± 4.52 | 893.10 ± 5.28 |
| trees | 302.20 ± 9.66 | 310.80 ± 11.12 | 304.60 ± 6.77 | 285.70 ± 11.45 |
| RANSAC success rate | 0.298 ± 0.010 | 0.292 ± 0.010 | 0.299 ± 0.007 | 0.320 ± 0.012 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 25.00 ± 3.20 | 24.30 ± 2.41 | 25.10 ± 3.60 | 19.40 ± 2.84 |
| fail: radius_out_of_range | 553.90 ± 9.68 | 584.90 ± 12.94 | 555.90 ± 6.26 | 483.70 ± 10.81 |
| fail: tilt_too_large | 116.50 ± 6.02 | 126.90 ± 9.62 | 115.90 ± 6.66 | 88.10 ± 4.77 |
| fail: normal_inconsistent | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: arc_coverage_low | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_dtm_intersection | 16.20 ± 2.35 | 16.20 ± 2.35 | 16.20 ± 2.35 | 16.20 ± 2.35 |
| time 5 retention [ms] | 2.99 ± 3.05 | 1.75 ± 0.11 | 1.98 ± 0.03 | 2.03 ± 0.03 |
| time 6 clustering [ms] | 1971.87 ± 14.76 | 2296.54 ± 11.83 | 1896.30 ± 13.64 | 1432.75 ± 14.10 |
| time 7 ransac [ms] | 600.22 ± 24.34 | 665.99 ± 24.59 | 591.58 ± 21.42 | 465.69 ± 19.31 |
| time 8 position [ms] | 1.66 ± 1.51 | 1.25 ± 0.07 | 1.42 ± 0.89 | 1.03 ± 0.05 |
| time total 5-8 [ms] | 2576.75 ± 33.83 | 2965.53 ± 32.98 | 2491.28 ± 32.11 | 1901.49 ± 24.55 |

mean ± sample std over 10 seeds.

### baseline / Budget-matched (kept count = step's per seed; linear_a and linear_mid coincide as linear*)

| metric | step | linear* | physical* |
|---|---|---|---|
| kept points | 318387.9 ± 181.3 | 318513.2 ± 184.5 | 318465.5 ± 130.1 |
| clusters | 1013.80 ± 3.08 | 1025.70 ± 4.79 | 1018.70 ± 4.08 |
| trees | 302.20 ± 9.66 | 306.50 ± 5.15 | 306.90 ± 9.57 |
| RANSAC success rate | 0.298 ± 0.010 | 0.299 ± 0.006 | 0.301 ± 0.009 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 25.00 ± 3.20 | 25.20 ± 2.25 | 23.00 ± 2.54 |
| fail: radius_out_of_range | 553.90 ± 9.68 | 562.20 ± 9.05 | 560.50 ± 9.86 |
| fail: tilt_too_large | 116.50 ± 6.02 | 115.60 ± 7.07 | 112.10 ± 9.48 |
| fail: normal_inconsistent | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: arc_coverage_low | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_dtm_intersection | 16.20 ± 2.35 | 16.20 ± 2.35 | 16.20 ± 2.35 |
| time 5 retention [ms] | 2.99 ± 3.05 | 1.75 ± 0.03 | 1.39 ± 0.03 |
| time 6 clustering [ms] | 1971.87 ± 14.76 | 1944.97 ± 12.96 | 2109.39 ± 33.23 |
| time 7 ransac [ms] | 600.22 ± 24.34 | 602.73 ± 13.51 | 598.26 ± 31.11 |
| time 8 position [ms] | 1.66 ± 1.51 | 1.16 ± 0.05 | 1.18 ± 0.06 |
| time total 5-8 [ms] | 2576.75 ± 33.83 | 2550.61 ± 23.66 | 2710.21 ± 63.57 |
| budget |kept - target| | – | 125.3 ± 62.9 | 127.4 ± 104.4 |

mean ± sample std over 10 seeds.


## 啟用檢查的改良設定 (improved: normal-consistency and arc-coverage checks ON)

### improved / Raw (P(d) as defined)

| metric | step | linear_a | linear_mid | physical |
|---|---|---|---|---|
| kept points | 318387.9 ± 181.3 | 342780.8 ± 136.3 | 314624.8 ± 219.8 | 264455.3 ± 184.1 |
| clusters | 1013.80 ± 3.08 | 1063.10 ± 4.12 | 1017.70 ± 4.52 | 893.10 ± 5.28 |
| trees | 55.10 ± 2.42 | 55.20 ± 2.62 | 55.00 ± 2.54 | 54.60 ± 2.37 |
| RANSAC success rate | 0.054 ± 0.002 | 0.052 ± 0.002 | 0.054 ± 0.002 | 0.061 ± 0.003 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 25.00 ± 3.20 | 24.30 ± 2.41 | 25.10 ± 3.60 | 19.40 ± 2.84 |
| fail: radius_out_of_range | 553.90 ± 9.68 | 584.90 ± 12.94 | 555.90 ± 6.26 | 483.70 ± 10.81 |
| fail: tilt_too_large | 116.50 ± 6.02 | 126.90 ± 9.62 | 115.90 ± 6.66 | 88.10 ± 4.77 |
| fail: normal_inconsistent | 164.50 ± 6.82 | 170.20 ± 11.61 | 165.70 ± 9.49 | 151.50 ± 11.25 |
| fail: arc_coverage_low | 97.00 ± 8.22 | 99.80 ± 11.68 | 98.30 ± 9.04 | 94.00 ± 7.26 |
| fail: no_dtm_intersection | 1.80 ± 1.14 | 1.80 ± 1.14 | 1.80 ± 1.14 | 1.80 ± 1.14 |
| time 5 retention [ms] | 1.92 ± 0.15 | 1.71 ± 0.04 | 2.03 ± 0.13 | 2.00 ± 0.05 |
| time 6 clustering [ms] | 1972.68 ± 13.82 | 2295.88 ± 10.23 | 1894.64 ± 14.23 | 1426.81 ± 6.02 |
| time 7 ransac [ms] | 596.37 ± 17.81 | 660.43 ± 12.80 | 595.14 ± 28.74 | 467.04 ± 11.51 |
| time 8 position [ms] | 0.65 ± 0.03 | 0.79 ± 0.27 | 0.63 ± 0.04 | 0.58 ± 0.02 |
| time total 5-8 [ms] | 2571.62 ± 26.41 | 2958.81 ± 21.46 | 2492.45 ± 34.09 | 1896.43 ± 15.26 |

mean ± sample std over 10 seeds.

### improved / Budget-matched (kept count = step's per seed; linear_a and linear_mid coincide as linear*)

| metric | step | linear* | physical* |
|---|---|---|---|
| kept points | 318387.9 ± 181.3 | 318513.2 ± 184.5 | 318465.5 ± 130.1 |
| clusters | 1013.80 ± 3.08 | 1025.70 ± 4.79 | 1018.70 ± 4.08 |
| trees | 55.10 ± 2.42 | 54.70 ± 2.58 | 54.60 ± 2.46 |
| RANSAC success rate | 0.054 ± 0.002 | 0.053 ± 0.003 | 0.054 ± 0.002 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 25.00 ± 3.20 | 25.20 ± 2.25 | 23.00 ± 2.54 |
| fail: radius_out_of_range | 553.90 ± 9.68 | 562.20 ± 9.05 | 560.50 ± 9.86 |
| fail: tilt_too_large | 116.50 ± 6.02 | 115.60 ± 7.07 | 112.10 ± 9.48 |
| fail: normal_inconsistent | 164.50 ± 6.82 | 167.90 ± 7.92 | 166.00 ± 9.92 |
| fail: arc_coverage_low | 97.00 ± 8.22 | 98.30 ± 7.35 | 100.70 ± 7.41 |
| fail: no_dtm_intersection | 1.80 ± 1.14 | 1.80 ± 1.14 | 1.80 ± 1.14 |
| time 5 retention [ms] | 1.92 ± 0.15 | 1.71 ± 0.06 | 1.38 ± 0.05 |
| time 6 clustering [ms] | 1972.68 ± 13.82 | 1947.50 ± 29.65 | 2095.43 ± 12.26 |
| time 7 ransac [ms] | 596.37 ± 17.81 | 598.93 ± 15.45 | 591.36 ± 21.52 |
| time 8 position [ms] | 0.65 ± 0.03 | 0.63 ± 0.02 | 0.72 ± 0.31 |
| time total 5-8 [ms] | 2571.62 ± 26.41 | 2548.78 ± 43.06 | 2688.90 ± 23.13 |
| budget |kept - target| | – | 125.3 ± 62.9 | 127.4 ± 104.4 |

mean ± sample std over 10 seeds.

