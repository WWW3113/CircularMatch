> Synthetic/experimental output. Implementation choices (not specified by the paper) are marked in effective_config.ini as [UNSPECIFIED].

Shared stages 1-4 (computed once, identical for all profiles/versions/seeds):

| stage | points | time [ms] |
|---|---|---|
| 0 input | 20392949 | 0 |
| 1 dtm+height | 17060268 | 4441.97 |
| 2 voxel | 4166117 | 5484.28 |
| 3 normals | 4128524 | 25359.3 |
| 4 verticality | 370916 | 149.305 |

## 未啟用額外檢查的基準設定 (baseline: normal-consistency and arc-coverage checks OFF)

### baseline / Raw (P(d) as defined)

| metric | step | linear_a | linear_mid | physical |
|---|---|---|---|---|
| kept points | 296698.6 ± 295.0 | 330645.1 ± 261.9 | 301280.4 ± 311.6 | 251975.7 ± 241.7 |
| clusters | 728.30 ± 4.92 | 765.00 ± 3.37 | 729.40 ± 4.27 | 651.10 ± 3.18 |
| trees | 289.50 ± 6.62 | 297.90 ± 11.94 | 292.00 ± 6.78 | 278.50 ± 7.69 |
| RANSAC success rate | 0.398 ± 0.009 | 0.389 ± 0.016 | 0.400 ± 0.009 | 0.428 ± 0.012 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 28.80 ± 3.19 | 31.40 ± 3.60 | 27.10 ± 2.56 | 18.40 ± 3.72 |
| fail: radius_out_of_range | 306.90 ± 7.99 | 327.40 ± 11.89 | 306.30 ± 7.76 | 271.90 ± 8.54 |
| fail: tilt_too_large | 100.40 ± 7.41 | 105.60 ± 6.98 | 101.30 ± 6.33 | 79.60 ± 5.08 |
| fail: normal_inconsistent | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: arc_coverage_low | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_dtm_intersection | 2.70 ± 1.57 | 2.70 ± 1.57 | 2.70 ± 1.57 | 2.70 ± 1.57 |
| time 5 retention [ms] | 1.85 ± 0.05 | 1.74 ± 0.17 | 5.49 ± 11.03 | 5.69 ± 8.20 |
| time 6 clustering [ms] | 1880.85 ± 10.13 | 2474.99 ± 26.12 | 1983.16 ± 44.99 | 1459.01 ± 59.71 |
| time 7 ransac [ms] | 596.89 ± 24.95 | 703.38 ± 77.43 | 615.35 ± 62.95 | 455.21 ± 29.00 |
| time 8 position [ms] | 1.36 ± 0.52 | 2.59 ± 4.41 | 1.98 ± 2.15 | 1.99 ± 2.59 |
| time total 5-8 [ms] | 2480.95 ± 26.86 | 3182.70 ± 103.23 | 2605.98 ± 85.84 | 1921.91 ± 81.97 |

mean ± sample std over 10 seeds.

### baseline / Budget-matched (kept count = step's per seed; linear_a and linear_mid coincide as linear*)

| metric | step | linear* | physical* |
|---|---|---|---|
| kept points | 296698.6 ± 295.0 | 296560.2 ± 327.4 | 296684.7 ± 342.1 |
| clusters | 728.30 ± 4.92 | 721.80 ± 6.09 | 709.80 ± 3.16 |
| trees | 289.50 ± 6.62 | 293.50 ± 7.91 | 289.80 ± 7.55 |
| RANSAC success rate | 0.398 ± 0.009 | 0.407 ± 0.010 | 0.408 ± 0.011 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 28.80 ± 3.19 | 26.70 ± 2.91 | 25.00 ± 4.03 |
| fail: radius_out_of_range | 306.90 ± 7.99 | 298.90 ± 5.84 | 299.60 ± 7.99 |
| fail: tilt_too_large | 100.40 ± 7.41 | 100.00 ± 5.08 | 92.70 ± 5.91 |
| fail: normal_inconsistent | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: arc_coverage_low | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_dtm_intersection | 2.70 ± 1.57 | 2.70 ± 1.57 | 2.70 ± 1.57 |
| time 5 retention [ms] | 1.85 ± 0.05 | 3.24 ± 2.57 | 3.58 ± 5.22 |
| time 6 clustering [ms] | 1880.85 ± 10.13 | 1948.56 ± 90.47 | 2056.34 ± 10.26 |
| time 7 ransac [ms] | 596.89 ± 24.95 | 593.50 ± 41.25 | 545.63 ± 46.92 |
| time 8 position [ms] | 1.36 ± 0.52 | 1.95 ± 2.25 | 1.09 ± 0.04 |
| time total 5-8 [ms] | 2480.95 ± 26.86 | 2547.25 ± 125.22 | 2606.64 ± 51.39 |
| budget |kept - target| | – | 142.8 ± 93.7 | 117.3 ± 90.3 |

mean ± sample std over 10 seeds.


## 啟用檢查的改良設定 (improved: normal-consistency and arc-coverage checks ON)

### improved / Raw (P(d) as defined)

| metric | step | linear_a | linear_mid | physical |
|---|---|---|---|---|
| kept points | 296698.6 ± 295.0 | 330645.1 ± 261.9 | 301280.4 ± 311.6 | 251975.7 ± 241.7 |
| clusters | 728.30 ± 4.92 | 765.00 ± 3.37 | 729.40 ± 4.27 | 651.10 ± 3.18 |
| trees | 68.90 ± 2.02 | 69.50 ± 2.17 | 69.30 ± 1.57 | 67.10 ± 2.92 |
| RANSAC success rate | 0.095 ± 0.003 | 0.091 ± 0.003 | 0.095 ± 0.002 | 0.103 ± 0.005 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 28.80 ± 3.19 | 31.40 ± 3.60 | 27.10 ± 2.56 | 18.40 ± 3.72 |
| fail: radius_out_of_range | 306.90 ± 7.99 | 327.40 ± 11.89 | 306.30 ± 7.76 | 271.90 ± 8.54 |
| fail: tilt_too_large | 100.40 ± 7.41 | 105.60 ± 6.98 | 101.30 ± 6.33 | 79.60 ± 5.08 |
| fail: normal_inconsistent | 141.30 ± 4.90 | 145.80 ± 9.43 | 143.40 ± 4.97 | 133.80 ± 6.49 |
| fail: arc_coverage_low | 81.50 ± 5.54 | 84.80 ± 6.53 | 81.50 ± 5.50 | 79.80 ± 5.92 |
| fail: no_dtm_intersection | 0.50 ± 0.53 | 0.50 ± 0.53 | 0.50 ± 0.53 | 0.50 ± 0.53 |
| time 5 retention [ms] | 2.59 ± 2.42 | 1.72 ± 0.14 | 2.39 ± 1.24 | 1.92 ± 0.05 |
| time 6 clustering [ms] | 1890.81 ± 20.57 | 2490.89 ± 48.79 | 1962.18 ± 8.23 | 1430.00 ± 11.56 |
| time 7 ransac [ms] | 581.44 ± 13.57 | 652.91 ± 14.31 | 584.61 ± 22.57 | 437.95 ± 8.29 |
| time 8 position [ms] | 0.69 ± 0.05 | 0.69 ± 0.02 | 0.70 ± 0.05 | 0.60 ± 0.04 |
| time total 5-8 [ms] | 2475.52 ± 26.49 | 3146.20 ± 59.25 | 2549.89 ± 28.26 | 1870.46 ± 13.45 |

mean ± sample std over 10 seeds.

### improved / Budget-matched (kept count = step's per seed; linear_a and linear_mid coincide as linear*)

| metric | step | linear* | physical* |
|---|---|---|---|
| kept points | 296698.6 ± 295.0 | 296560.2 ± 327.4 | 296684.7 ± 342.1 |
| clusters | 728.30 ± 4.92 | 721.80 ± 6.09 | 709.80 ± 3.16 |
| trees | 68.90 ± 2.02 | 67.90 ± 2.47 | 67.70 ± 2.16 |
| RANSAC success rate | 0.095 ± 0.003 | 0.094 ± 0.004 | 0.095 ± 0.003 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 28.80 ± 3.19 | 26.70 ± 2.91 | 25.00 ± 4.03 |
| fail: radius_out_of_range | 306.90 ± 7.99 | 298.90 ± 5.84 | 299.60 ± 7.99 |
| fail: tilt_too_large | 100.40 ± 7.41 | 100.00 ± 5.08 | 92.70 ± 5.91 |
| fail: normal_inconsistent | 141.30 ± 4.90 | 143.50 ± 6.06 | 141.30 ± 8.34 |
| fail: arc_coverage_low | 81.50 ± 5.54 | 84.30 ± 4.95 | 83.00 ± 4.47 |
| fail: no_dtm_intersection | 0.50 ± 0.53 | 0.50 ± 0.53 | 0.50 ± 0.53 |
| time 5 retention [ms] | 2.59 ± 2.42 | 1.80 ± 0.04 | 1.51 ± 0.06 |
| time 6 clustering [ms] | 1890.81 ± 20.57 | 1897.93 ± 11.76 | 2072.40 ± 56.27 |
| time 7 ransac [ms] | 581.44 ± 13.57 | 574.33 ± 11.55 | 534.04 ± 21.53 |
| time 8 position [ms] | 0.69 ± 0.05 | 0.67 ± 0.04 | 0.74 ± 0.28 |
| time total 5-8 [ms] | 2475.52 ± 26.49 | 2474.73 ± 20.45 | 2608.69 ± 72.29 |
| budget |kept - target| | – | 142.8 ± 93.7 | 117.3 ± 90.3 |

mean ± sample std over 10 seeds.

