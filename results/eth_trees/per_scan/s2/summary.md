> Synthetic/experimental output. Implementation choices (not specified by the paper) are marked in effective_config.ini as [UNSPECIFIED].

Shared stages 1-4 (computed once, identical for all profiles/versions/seeds):

| stage | points | time [ms] |
|---|---|---|
| 0 input | 19602448 | 0 |
| 1 dtm+height | 17099302 | 4656.82 |
| 2 voxel | 3959108 | 4914.66 |
| 3 normals | 3918640 | 24445.9 |
| 4 verticality | 378636 | 185.561 |

## 未啟用額外檢查的基準設定 (baseline: normal-consistency and arc-coverage checks OFF)

### baseline / Raw (P(d) as defined)

| metric | step | linear_a | linear_mid | physical |
|---|---|---|---|---|
| kept points | 322079.0 ± 184.0 | 342888.5 ± 135.0 | 315722.3 ± 191.9 | 272844.6 ± 205.2 |
| clusters | 979.40 ± 5.21 | 1031.90 ± 3.31 | 978.20 ± 5.03 | 877.80 ± 5.33 |
| trees | 339.10 ± 11.41 | 348.40 ± 8.93 | 341.70 ± 9.58 | 317.40 ± 9.26 |
| RANSAC success rate | 0.346 ± 0.012 | 0.338 ± 0.008 | 0.349 ± 0.010 | 0.362 ± 0.011 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 19.90 ± 4.33 | 20.70 ± 2.98 | 19.60 ± 3.31 | 15.30 ± 3.62 |
| fail: radius_out_of_range | 501.50 ± 11.89 | 530.90 ± 7.96 | 498.60 ± 7.21 | 449.30 ± 6.11 |
| fail: tilt_too_large | 111.80 ± 10.39 | 124.80 ± 7.98 | 111.20 ± 8.82 | 88.70 ± 10.08 |
| fail: normal_inconsistent | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: arc_coverage_low | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_dtm_intersection | 7.10 ± 1.66 | 7.10 ± 1.66 | 7.10 ± 1.66 | 7.10 ± 1.66 |
| time 5 retention [ms] | 1.85 ± 0.58 | 1.60 ± 0.03 | 1.92 ± 0.04 | 2.04 ± 0.05 |
| time 6 clustering [ms] | 2120.13 ± 12.96 | 2397.51 ± 7.42 | 1985.28 ± 18.71 | 1569.62 ± 30.30 |
| time 7 ransac [ms] | 591.78 ± 18.58 | 649.57 ± 20.77 | 575.94 ± 16.58 | 474.32 ± 8.62 |
| time 8 position [ms] | 1.74 ± 1.29 | 2.32 ± 3.55 | 1.12 ± 0.02 | 1.04 ± 0.04 |
| time total 5-8 [ms] | 2715.50 ± 24.59 | 3051.00 ± 27.22 | 2564.26 ± 22.83 | 2047.02 ± 34.00 |

mean ± sample std over 10 seeds.

### baseline / Budget-matched (kept count = step's per seed; linear_a and linear_mid coincide as linear*)

| metric | step | linear* | physical* |
|---|---|---|---|
| kept points | 322079.0 ± 184.0 | 322157.5 ± 292.7 | 321998.7 ± 110.6 |
| clusters | 979.40 ± 5.21 | 989.20 ± 4.96 | 964.40 ± 3.72 |
| trees | 339.10 ± 11.41 | 341.20 ± 8.84 | 333.90 ± 7.81 |
| RANSAC success rate | 0.346 ± 0.012 | 0.345 ± 0.009 | 0.346 ± 0.008 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 19.90 ± 4.33 | 19.80 ± 3.68 | 19.10 ± 3.18 |
| fail: radius_out_of_range | 501.50 ± 11.89 | 507.90 ± 11.70 | 500.90 ± 8.85 |
| fail: tilt_too_large | 111.80 ± 10.39 | 113.20 ± 7.48 | 103.40 ± 9.91 |
| fail: normal_inconsistent | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: arc_coverage_low | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_dtm_intersection | 7.10 ± 1.66 | 7.10 ± 1.66 | 7.10 ± 1.66 |
| time 5 retention [ms] | 1.85 ± 0.58 | 1.63 ± 0.17 | 1.52 ± 0.04 |
| time 6 clustering [ms] | 2120.13 ± 12.96 | 2079.31 ± 16.19 | 2210.69 ± 44.97 |
| time 7 ransac [ms] | 591.78 ± 18.58 | 597.17 ± 31.32 | 589.58 ± 80.26 |
| time 8 position [ms] | 1.74 ± 1.29 | 1.15 ± 0.03 | 1.74 ± 1.91 |
| time total 5-8 [ms] | 2715.50 ± 24.59 | 2679.26 ± 42.80 | 2803.54 ± 125.55 |
| budget |kept - target| | – | 187.7 ± 115.1 | 167.3 ± 83.9 |

mean ± sample std over 10 seeds.


## 啟用檢查的改良設定 (improved: normal-consistency and arc-coverage checks ON)

### improved / Raw (P(d) as defined)

| metric | step | linear_a | linear_mid | physical |
|---|---|---|---|---|
| kept points | 322079.0 ± 184.0 | 342888.5 ± 135.0 | 315722.3 ± 191.9 | 272844.6 ± 205.2 |
| clusters | 979.40 ± 5.21 | 1031.90 ± 3.31 | 978.20 ± 5.03 | 877.80 ± 5.33 |
| trees | 75.90 ± 3.03 | 76.30 ± 2.54 | 76.50 ± 3.37 | 74.20 ± 3.05 |
| RANSAC success rate | 0.077 ± 0.003 | 0.074 ± 0.003 | 0.078 ± 0.003 | 0.085 ± 0.003 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 19.90 ± 4.33 | 20.70 ± 2.98 | 19.60 ± 3.31 | 15.30 ± 3.62 |
| fail: radius_out_of_range | 501.50 ± 11.89 | 530.90 ± 7.96 | 498.60 ± 7.21 | 449.30 ± 6.11 |
| fail: tilt_too_large | 111.80 ± 10.39 | 124.80 ± 7.98 | 111.20 ± 8.82 | 88.70 ± 10.08 |
| fail: normal_inconsistent | 176.00 ± 6.18 | 185.20 ± 5.75 | 177.10 ± 8.65 | 160.50 ± 5.70 |
| fail: arc_coverage_low | 94.00 ± 6.25 | 93.70 ± 4.52 | 94.90 ± 4.68 | 89.50 ± 6.38 |
| fail: no_dtm_intersection | 0.30 ± 0.48 | 0.30 ± 0.48 | 0.30 ± 0.48 | 0.30 ± 0.48 |
| time 5 retention [ms] | 1.72 ± 0.07 | 1.61 ± 0.07 | 1.96 ± 0.09 | 2.41 ± 1.21 |
| time 6 clustering [ms] | 2115.34 ± 9.88 | 2403.78 ± 17.55 | 2007.18 ± 48.18 | 1566.94 ± 29.04 |
| time 7 ransac [ms] | 592.33 ± 12.35 | 652.21 ± 14.50 | 578.26 ± 12.18 | 475.38 ± 12.39 |
| time 8 position [ms] | 0.68 ± 0.05 | 0.79 ± 0.17 | 0.68 ± 0.03 | 0.66 ± 0.10 |
| time total 5-8 [ms] | 2710.06 ± 21.01 | 3058.39 ± 29.54 | 2588.08 ± 54.39 | 2045.38 ± 39.08 |

mean ± sample std over 10 seeds.

### improved / Budget-matched (kept count = step's per seed; linear_a and linear_mid coincide as linear*)

| metric | step | linear* | physical* |
|---|---|---|---|
| kept points | 322079.0 ± 184.0 | 322157.5 ± 292.7 | 321998.7 ± 110.6 |
| clusters | 979.40 ± 5.21 | 989.20 ± 4.96 | 964.40 ± 3.72 |
| trees | 75.90 ± 3.03 | 76.20 ± 2.49 | 75.40 ± 3.41 |
| RANSAC success rate | 0.077 ± 0.003 | 0.077 ± 0.003 | 0.078 ± 0.004 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 19.90 ± 4.33 | 19.80 ± 3.68 | 19.10 ± 3.18 |
| fail: radius_out_of_range | 501.50 ± 11.89 | 507.90 ± 11.70 | 500.90 ± 8.85 |
| fail: tilt_too_large | 111.80 ± 10.39 | 113.20 ± 7.48 | 103.40 ± 9.91 |
| fail: normal_inconsistent | 176.00 ± 6.18 | 178.30 ± 7.57 | 172.30 ± 5.14 |
| fail: arc_coverage_low | 94.00 ± 6.25 | 93.50 ± 6.60 | 93.00 ± 3.74 |
| fail: no_dtm_intersection | 0.30 ± 0.48 | 0.30 ± 0.48 | 0.30 ± 0.48 |
| time 5 retention [ms] | 1.72 ± 0.07 | 1.60 ± 0.04 | 1.54 ± 0.04 |
| time 6 clustering [ms] | 2115.34 ± 9.88 | 2077.37 ± 23.07 | 2196.03 ± 7.13 |
| time 7 ransac [ms] | 592.33 ± 12.35 | 591.36 ± 11.15 | 566.09 ± 9.13 |
| time 8 position [ms] | 0.68 ± 0.05 | 1.10 ± 1.16 | 0.67 ± 0.02 |
| time total 5-8 [ms] | 2710.06 ± 21.01 | 2671.43 ± 27.04 | 2764.32 ± 11.26 |
| budget |kept - target| | – | 187.7 ± 115.1 | 167.3 ± 83.9 |

mean ± sample std over 10 seeds.

