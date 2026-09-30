> Synthetic/experimental output. Implementation choices (not specified by the paper) are marked in effective_config.ini as [UNSPECIFIED].

Shared stages 1-4 (computed once, identical for all profiles/versions/seeds):

| stage | points | time [ms] |
|---|---|---|
| 0 input | 19633280 | 0 |
| 1 dtm+height | 17185222 | 4367.22 |
| 2 voxel | 3919798 | 4015.01 |
| 3 normals | 3880005 | 23345.4 |
| 4 verticality | 374797 | 71.6453 |

## 未啟用額外檢查的基準設定 (baseline: normal-consistency and arc-coverage checks OFF)

### baseline / Raw (P(d) as defined)

| metric | step | linear_a | linear_mid | physical |
|---|---|---|---|---|
| kept points | 324549.0 ± 196.8 | 340377.2 ± 166.7 | 316264.7 ± 153.7 | 272718.3 ± 150.1 |
| clusters | 1054.40 ± 4.03 | 1099.50 ± 3.92 | 1045.90 ± 5.17 | 938.10 ± 3.14 |
| trees | 340.20 ± 9.16 | 350.10 ± 8.89 | 341.30 ± 7.86 | 321.20 ± 11.67 |
| RANSAC success rate | 0.323 ± 0.009 | 0.318 ± 0.008 | 0.326 ± 0.009 | 0.342 ± 0.012 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 17.80 ± 3.39 | 18.60 ± 4.48 | 16.00 ± 3.80 | 15.60 ± 3.06 |
| fail: radius_out_of_range | 564.50 ± 8.81 | 593.90 ± 8.90 | 557.80 ± 10.29 | 500.50 ± 10.17 |
| fail: tilt_too_large | 118.00 ± 8.55 | 123.00 ± 9.45 | 116.90 ± 7.29 | 86.90 ± 7.98 |
| fail: normal_inconsistent | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: arc_coverage_low | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_dtm_intersection | 13.90 ± 2.60 | 13.90 ± 2.60 | 13.90 ± 2.60 | 13.90 ± 2.60 |
| time 5 retention [ms] | 1.68 ± 0.07 | 4.27 ± 8.12 | 2.04 ± 0.23 | 1.92 ± 0.04 |
| time 6 clustering [ms] | 1944.85 ± 18.40 | 2117.35 ± 39.63 | 1779.12 ± 36.32 | 1346.85 ± 7.91 |
| time 7 ransac [ms] | 640.23 ± 94.99 | 647.99 ± 15.87 | 598.66 ± 18.54 | 480.72 ± 9.27 |
| time 8 position [ms] | 1.62 ± 1.16 | 1.27 ± 0.07 | 1.26 ± 0.03 | 1.09 ± 0.05 |
| time total 5-8 [ms] | 2588.38 ± 113.24 | 2770.88 ± 57.58 | 2381.08 ± 48.18 | 1830.58 ± 9.50 |

mean ± sample std over 10 seeds.

### baseline / Budget-matched (kept count = step's per seed; linear_a and linear_mid coincide as linear*)

| metric | step | linear* | physical* |
|---|---|---|---|
| kept points | 324549.0 ± 196.8 | 324410.8 ± 310.5 | 324654.9 ± 171.0 |
| clusters | 1054.40 ± 4.03 | 1066.30 ± 4.30 | 1057.30 ± 4.06 |
| trees | 340.20 ± 9.16 | 341.60 ± 9.83 | 338.50 ± 7.62 |
| RANSAC success rate | 0.323 ± 0.009 | 0.320 ± 0.009 | 0.320 ± 0.007 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 17.80 ± 3.39 | 17.80 ± 4.96 | 18.80 ± 3.79 |
| fail: radius_out_of_range | 564.50 ± 8.81 | 573.80 ± 11.03 | 578.80 ± 4.10 |
| fail: tilt_too_large | 118.00 ± 8.55 | 119.20 ± 8.53 | 107.30 ± 7.90 |
| fail: normal_inconsistent | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: arc_coverage_low | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_dtm_intersection | 13.90 ± 2.60 | 13.90 ± 2.60 | 13.90 ± 2.60 |
| time 5 retention [ms] | 1.68 ± 0.07 | 1.53 ± 0.04 | 1.46 ± 0.05 |
| time 6 clustering [ms] | 1944.85 ± 18.40 | 1861.01 ± 9.30 | 1959.88 ± 19.94 |
| time 7 ransac [ms] | 640.23 ± 94.99 | 611.12 ± 17.17 | 611.98 ± 31.46 |
| time 8 position [ms] | 1.62 ± 1.16 | 1.22 ± 0.10 | 1.78 ± 1.62 |
| time total 5-8 [ms] | 2588.38 ± 113.24 | 2474.88 ± 21.35 | 2575.10 ± 47.59 |
| budget |kept - target| | – | 212.4 ± 54.8 | 135.3 ± 80.5 |

mean ± sample std over 10 seeds.


## 啟用檢查的改良設定 (improved: normal-consistency and arc-coverage checks ON)

### improved / Raw (P(d) as defined)

| metric | step | linear_a | linear_mid | physical |
|---|---|---|---|---|
| kept points | 324549.0 ± 196.8 | 340377.2 ± 166.7 | 316264.7 ± 153.7 | 272718.3 ± 150.1 |
| clusters | 1054.40 ± 4.03 | 1099.50 ± 3.92 | 1045.90 ± 5.17 | 938.10 ± 3.14 |
| trees | 68.10 ± 2.60 | 67.70 ± 2.26 | 67.50 ± 2.01 | 67.20 ± 2.25 |
| RANSAC success rate | 0.065 ± 0.002 | 0.062 ± 0.002 | 0.065 ± 0.002 | 0.072 ± 0.002 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 17.80 ± 3.39 | 18.60 ± 4.48 | 16.00 ± 3.80 | 15.60 ± 3.06 |
| fail: radius_out_of_range | 564.50 ± 8.81 | 593.90 ± 8.90 | 557.80 ± 10.29 | 500.50 ± 10.17 |
| fail: tilt_too_large | 118.00 ± 8.55 | 123.00 ± 9.45 | 116.90 ± 7.29 | 86.90 ± 7.98 |
| fail: normal_inconsistent | 182.60 ± 7.09 | 190.60 ± 8.41 | 183.70 ± 4.74 | 166.90 ± 9.77 |
| fail: arc_coverage_low | 101.80 ± 4.10 | 104.10 ± 4.15 | 102.40 ± 4.79 | 99.40 ± 3.89 |
| fail: no_dtm_intersection | 1.60 ± 1.58 | 1.60 ± 1.58 | 1.60 ± 1.58 | 1.60 ± 1.58 |
| time 5 retention [ms] | 1.70 ± 0.07 | 1.61 ± 0.08 | 3.88 ± 6.36 | 1.95 ± 0.04 |
| time 6 clustering [ms] | 1944.60 ± 14.07 | 2110.83 ± 50.70 | 1766.37 ± 14.32 | 1346.97 ± 10.74 |
| time 7 ransac [ms] | 624.45 ± 28.00 | 655.02 ± 23.84 | 590.99 ± 14.41 | 487.33 ± 12.17 |
| time 8 position [ms] | 2.50 ± 3.73 | 0.90 ± 0.34 | 0.94 ± 0.66 | 0.65 ± 0.04 |
| time total 5-8 [ms] | 2573.26 ± 36.57 | 2768.36 ± 74.20 | 2362.18 ± 28.97 | 1836.91 ± 20.26 |

mean ± sample std over 10 seeds.

### improved / Budget-matched (kept count = step's per seed; linear_a and linear_mid coincide as linear*)

| metric | step | linear* | physical* |
|---|---|---|---|
| kept points | 324549.0 ± 196.8 | 324410.8 ± 310.5 | 324654.9 ± 171.0 |
| clusters | 1054.40 ± 4.03 | 1066.30 ± 4.30 | 1057.30 ± 4.06 |
| trees | 68.10 ± 2.60 | 67.30 ± 2.00 | 67.90 ± 2.02 |
| RANSAC success rate | 0.065 ± 0.002 | 0.063 ± 0.002 | 0.064 ± 0.002 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 17.80 ± 3.39 | 17.80 ± 4.96 | 18.80 ± 3.79 |
| fail: radius_out_of_range | 564.50 ± 8.81 | 573.80 ± 11.03 | 578.80 ± 4.10 |
| fail: tilt_too_large | 118.00 ± 8.55 | 119.20 ± 8.53 | 107.30 ± 7.90 |
| fail: normal_inconsistent | 182.60 ± 7.09 | 184.30 ± 9.88 | 178.50 ± 8.03 |
| fail: arc_coverage_low | 101.80 ± 4.10 | 102.30 ± 3.86 | 104.40 ± 3.17 |
| fail: no_dtm_intersection | 1.60 ± 1.58 | 1.60 ± 1.58 | 1.60 ± 1.58 |
| time 5 retention [ms] | 1.70 ± 0.07 | 1.55 ± 0.07 | 2.81 ± 4.18 |
| time 6 clustering [ms] | 1944.60 ± 14.07 | 1877.39 ± 21.58 | 1976.83 ± 34.64 |
| time 7 ransac [ms] | 624.45 ± 28.00 | 643.35 ± 66.23 | 617.34 ± 25.36 |
| time 8 position [ms] | 2.50 ± 3.73 | 2.71 ± 5.09 | 1.00 ± 0.88 |
| time total 5-8 [ms] | 2573.26 ± 36.57 | 2525.00 ± 78.88 | 2597.97 ± 56.67 |
| budget |kept - target| | – | 212.4 ± 54.8 | 135.3 ± 80.5 |

mean ± sample std over 10 seeds.

