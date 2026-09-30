> Synthetic/experimental output. Implementation choices (not specified by the paper) are marked in effective_config.ini as [UNSPECIFIED].

Shared stages 1-4 (computed once, identical for all profiles/versions/seeds):

| stage | points | time [ms] |
|---|---|---|
| 0 input | 21612153 | 0 |
| 1 dtm+height | 18210844 | 4517.01 |
| 2 voxel | 3920799 | 4109.56 |
| 3 normals | 3886372 | 23191.5 |
| 4 verticality | 425506 | 73.9616 |

## 未啟用額外檢查的基準設定 (baseline: normal-consistency and arc-coverage checks OFF)

### baseline / Raw (P(d) as defined)

| metric | step | linear_a | linear_mid | physical |
|---|---|---|---|---|
| kept points | 347076.0 ± 196.6 | 378111.7 ± 160.3 | 346101.7 ± 198.6 | 287181.0 ± 132.6 |
| clusters | 754.90 ± 4.23 | 785.60 ± 2.07 | 753.00 ± 3.09 | 690.40 ± 5.46 |
| trees | 376.10 ± 7.82 | 382.60 ± 5.87 | 379.30 ± 8.76 | 364.70 ± 6.24 |
| RANSAC success rate | 0.498 ± 0.011 | 0.487 ± 0.008 | 0.504 ± 0.011 | 0.528 ± 0.007 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 22.80 ± 3.74 | 23.40 ± 3.89 | 21.00 ± 4.08 | 14.50 ± 2.72 |
| fail: radius_out_of_range | 254.50 ± 9.49 | 270.20 ± 6.49 | 252.10 ± 6.82 | 227.90 ± 6.59 |
| fail: tilt_too_large | 87.10 ± 10.34 | 95.00 ± 8.73 | 86.20 ± 6.68 | 68.90 ± 5.04 |
| fail: normal_inconsistent | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: arc_coverage_low | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_dtm_intersection | 14.40 ± 2.72 | 14.40 ± 2.72 | 14.40 ± 2.72 | 14.40 ± 2.72 |
| time 5 retention [ms] | 2.83 ± 2.68 | 1.86 ± 0.12 | 2.18 ± 0.07 | 2.39 ± 0.08 |
| time 6 clustering [ms] | 2425.43 ± 14.94 | 2989.13 ± 27.58 | 2407.12 ± 32.69 | 1683.63 ± 38.74 |
| time 7 ransac [ms] | 636.86 ± 16.09 | 700.66 ± 24.18 | 625.43 ± 9.00 | 511.25 ± 29.42 |
| time 8 position [ms] | 2.98 ± 4.72 | 2.23 ± 2.32 | 1.44 ± 0.03 | 1.31 ± 0.06 |
| time total 5-8 [ms] | 3068.10 ± 28.34 | 3693.89 ± 44.99 | 3036.17 ± 35.62 | 2198.59 ± 42.07 |

mean ± sample std over 10 seeds.

### baseline / Budget-matched (kept count = step's per seed; linear_a and linear_mid coincide as linear*)

| metric | step | linear* | physical* |
|---|---|---|---|
| kept points | 347076.0 ± 196.6 | 347156.2 ± 187.1 | 347003.1 ± 141.3 |
| clusters | 754.90 ± 4.23 | 754.00 ± 3.46 | 761.20 ± 3.55 |
| trees | 376.10 ± 7.82 | 377.20 ± 8.18 | 378.00 ± 9.23 |
| RANSAC success rate | 0.498 ± 0.011 | 0.500 ± 0.011 | 0.497 ± 0.013 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 22.80 ± 3.74 | 22.00 ± 3.59 | 20.60 ± 3.53 |
| fail: radius_out_of_range | 254.50 ± 9.49 | 253.10 ± 6.08 | 262.70 ± 8.59 |
| fail: tilt_too_large | 87.10 ± 10.34 | 87.30 ± 7.38 | 85.50 ± 7.28 |
| fail: normal_inconsistent | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: arc_coverage_low | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_dtm_intersection | 14.40 ± 2.72 | 14.40 ± 2.72 | 14.40 ± 2.72 |
| time 5 retention [ms] | 2.83 ± 2.68 | 1.91 ± 0.04 | 1.60 ± 0.04 |
| time 6 clustering [ms] | 2425.43 ± 14.94 | 2416.93 ± 22.47 | 2602.97 ± 17.26 |
| time 7 ransac [ms] | 636.86 ± 16.09 | 636.86 ± 18.68 | 648.11 ± 27.73 |
| time 8 position [ms] | 2.98 ± 4.72 | 1.64 ± 0.63 | 1.66 ± 0.67 |
| time total 5-8 [ms] | 3068.10 ± 28.34 | 3057.35 ± 32.03 | 3254.34 ± 31.27 |
| budget |kept - target| | – | 99.2 ± 53.6 | 132.5 ± 88.3 |

mean ± sample std over 10 seeds.


## 啟用檢查的改良設定 (improved: normal-consistency and arc-coverage checks ON)

### improved / Raw (P(d) as defined)

| metric | step | linear_a | linear_mid | physical |
|---|---|---|---|---|
| kept points | 347076.0 ± 196.6 | 378111.7 ± 160.3 | 346101.7 ± 198.6 | 287181.0 ± 132.6 |
| clusters | 754.90 ± 4.23 | 785.60 ± 2.07 | 753.00 ± 3.09 | 690.40 ± 5.46 |
| trees | 116.00 ± 3.40 | 117.20 ± 3.65 | 116.10 ± 2.47 | 115.60 ± 3.34 |
| RANSAC success rate | 0.154 ± 0.004 | 0.149 ± 0.005 | 0.154 ± 0.003 | 0.167 ± 0.005 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 22.80 ± 3.74 | 23.40 ± 3.89 | 21.00 ± 4.08 | 14.50 ± 2.72 |
| fail: radius_out_of_range | 254.50 ± 9.49 | 270.20 ± 6.49 | 252.10 ± 6.82 | 227.90 ± 6.59 |
| fail: tilt_too_large | 87.10 ± 10.34 | 95.00 ± 8.73 | 86.20 ± 6.68 | 68.90 ± 5.04 |
| fail: normal_inconsistent | 168.70 ± 7.83 | 171.90 ± 5.76 | 171.50 ± 7.50 | 159.50 ± 6.80 |
| fail: arc_coverage_low | 103.40 ± 5.58 | 105.50 ± 7.44 | 103.70 ± 7.86 | 101.60 ± 6.79 |
| fail: no_dtm_intersection | 2.40 ± 1.17 | 2.40 ± 1.17 | 2.40 ± 1.17 | 2.40 ± 1.17 |
| time 5 retention [ms] | 4.28 ± 4.88 | 1.92 ± 0.19 | 4.74 ± 7.95 | 2.38 ± 0.05 |
| time 6 clustering [ms] | 2440.47 ± 29.59 | 2992.14 ± 35.26 | 2435.08 ± 49.44 | 1687.79 ± 34.20 |
| time 7 ransac [ms] | 643.14 ± 15.17 | 735.08 ± 68.35 | 640.47 ± 23.75 | 507.11 ± 15.47 |
| time 8 position [ms] | 0.98 ± 0.14 | 1.81 ± 2.59 | 1.04 ± 0.51 | 0.94 ± 0.45 |
| time total 5-8 [ms] | 3088.87 ± 41.42 | 3730.96 ± 84.47 | 3081.33 ± 69.50 | 2198.22 ± 39.85 |

mean ± sample std over 10 seeds.

### improved / Budget-matched (kept count = step's per seed; linear_a and linear_mid coincide as linear*)

| metric | step | linear* | physical* |
|---|---|---|---|
| kept points | 347076.0 ± 196.6 | 347156.2 ± 187.1 | 347003.1 ± 141.3 |
| clusters | 754.90 ± 4.23 | 754.00 ± 3.46 | 761.20 ± 3.55 |
| trees | 116.00 ± 3.40 | 116.10 ± 3.48 | 116.40 ± 3.10 |
| RANSAC success rate | 0.154 ± 0.004 | 0.154 ± 0.005 | 0.153 ± 0.004 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 22.80 ± 3.74 | 22.00 ± 3.59 | 20.60 ± 3.53 |
| fail: radius_out_of_range | 254.50 ± 9.49 | 253.10 ± 6.08 | 262.70 ± 8.59 |
| fail: tilt_too_large | 87.10 ± 10.34 | 87.30 ± 7.38 | 85.50 ± 7.28 |
| fail: normal_inconsistent | 168.70 ± 7.83 | 170.10 ± 7.16 | 169.60 ± 8.53 |
| fail: arc_coverage_low | 103.40 ± 5.58 | 103.00 ± 7.60 | 104.00 ± 6.72 |
| fail: no_dtm_intersection | 2.40 ± 1.17 | 2.40 ± 1.17 | 2.40 ± 1.17 |
| time 5 retention [ms] | 4.28 ± 4.88 | 2.66 ± 2.31 | 2.26 ± 1.54 |
| time 6 clustering [ms] | 2440.47 ± 29.59 | 2435.30 ± 23.70 | 2645.95 ± 98.99 |
| time 7 ransac [ms] | 643.14 ± 15.17 | 645.04 ± 23.68 | 716.81 ± 124.94 |
| time 8 position [ms] | 0.98 ± 0.14 | 1.06 ± 0.52 | 1.13 ± 0.58 |
| time total 5-8 [ms] | 3088.87 ± 41.42 | 3084.06 ± 44.92 | 3366.15 ± 216.28 |
| budget |kept - target| | – | 99.2 ± 53.6 | 132.5 ± 88.3 |

mean ± sample std over 10 seeds.

