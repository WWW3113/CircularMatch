> Synthetic/experimental output. Implementation choices (not specified by the paper) are marked in effective_config.ini as [UNSPECIFIED].

Shared stages 1-4 (computed once, identical for all profiles/versions/seeds):

| stage | points | time [ms] |
|---|---|---|
| 0 input | 20481295 | 0 |
| 1 dtm+height | 17308012 | 4307.89 |
| 2 voxel | 3954380 | 4041.41 |
| 3 normals | 3916041 | 21941.5 |
| 4 verticality | 382064 | 87.1991 |

## 未啟用額外檢查的基準設定 (baseline: normal-consistency and arc-coverage checks OFF)

### baseline / Raw (P(d) as defined)

| metric | step | linear_a | linear_mid | physical |
|---|---|---|---|---|
| kept points | 318070.6 ± 105.9 | 345090.6 ± 114.8 | 318924.9 ± 149.7 | 276713.7 ± 131.3 |
| clusters | 732.40 ± 4.67 | 764.40 ± 1.96 | 731.10 ± 4.89 | 668.30 ± 3.06 |
| trees | 292.20 ± 8.74 | 295.70 ± 8.26 | 290.40 ± 6.19 | 283.70 ± 7.45 |
| RANSAC success rate | 0.399 ± 0.012 | 0.387 ± 0.011 | 0.397 ± 0.007 | 0.424 ± 0.010 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 22.70 ± 3.09 | 24.40 ± 4.60 | 22.50 ± 2.72 | 16.40 ± 4.35 |
| fail: radius_out_of_range | 328.50 ± 9.17 | 346.60 ± 8.18 | 327.20 ± 10.25 | 293.90 ± 7.06 |
| fail: tilt_too_large | 85.60 ± 6.26 | 94.30 ± 7.42 | 87.60 ± 6.47 | 70.90 ± 6.98 |
| fail: normal_inconsistent | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: arc_coverage_low | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_dtm_intersection | 3.40 ± 0.52 | 3.40 ± 0.52 | 3.40 ± 0.52 | 3.40 ± 0.52 |
| time 5 retention [ms] | 1.75 ± 0.08 | 1.59 ± 0.12 | 3.90 ± 6.41 | 1.93 ± 0.06 |
| time 6 clustering [ms] | 2265.31 ± 10.05 | 2732.56 ± 50.29 | 2302.91 ± 30.70 | 1844.94 ± 42.85 |
| time 7 ransac [ms] | 577.33 ± 21.62 | 665.28 ± 34.23 | 588.30 ± 13.25 | 471.52 ± 23.00 |
| time 8 position [ms] | 1.49 ± 0.89 | 1.37 ± 0.57 | 1.13 ± 0.06 | 1.57 ± 1.51 |
| time total 5-8 [ms] | 2845.89 ± 25.15 | 3400.79 ± 77.59 | 2896.24 ± 36.03 | 2319.96 ± 44.43 |

mean ± sample std over 10 seeds.

### baseline / Budget-matched (kept count = step's per seed; linear_a and linear_mid coincide as linear*)

| metric | step | linear* | physical* |
|---|---|---|---|
| kept points | 318070.6 ± 105.9 | 318073.4 ± 261.4 | 318148.1 ± 91.4 |
| clusters | 732.40 ± 4.67 | 730.20 ± 5.79 | 717.70 ± 1.42 |
| trees | 292.20 ± 8.74 | 292.00 ± 8.11 | 292.60 ± 5.21 |
| RANSAC success rate | 0.399 ± 0.012 | 0.400 ± 0.010 | 0.408 ± 0.007 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 22.70 ± 3.09 | 22.60 ± 3.41 | 21.40 ± 3.13 |
| fail: radius_out_of_range | 328.50 ± 9.17 | 325.70 ± 9.84 | 319.90 ± 3.18 |
| fail: tilt_too_large | 85.60 ± 6.26 | 86.50 ± 3.95 | 80.40 ± 4.53 |
| fail: normal_inconsistent | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: arc_coverage_low | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_dtm_intersection | 3.40 ± 0.52 | 3.40 ± 0.52 | 3.40 ± 0.52 |
| time 5 retention [ms] | 1.75 ± 0.08 | 1.67 ± 0.03 | 1.26 ± 0.03 |
| time 6 clustering [ms] | 2265.31 ± 10.05 | 2255.66 ± 7.53 | 2470.86 ± 14.91 |
| time 7 ransac [ms] | 577.33 ± 21.62 | 595.18 ± 24.49 | 556.45 ± 23.77 |
| time 8 position [ms] | 1.49 ± 0.89 | 1.16 ± 0.06 | 1.15 ± 0.02 |
| time total 5-8 [ms] | 2845.89 ± 25.15 | 2853.66 ± 28.27 | 3029.73 ± 27.08 |
| budget |kept - target| | – | 243.6 ± 95.7 | 117.5 ± 96.3 |

mean ± sample std over 10 seeds.


## 啟用檢查的改良設定 (improved: normal-consistency and arc-coverage checks ON)

### improved / Raw (P(d) as defined)

| metric | step | linear_a | linear_mid | physical |
|---|---|---|---|---|
| kept points | 318070.6 ± 105.9 | 345090.6 ± 114.8 | 318924.9 ± 149.7 | 276713.7 ± 131.3 |
| clusters | 732.40 ± 4.67 | 764.40 ± 1.96 | 731.10 ± 4.89 | 668.30 ± 3.06 |
| trees | 87.30 ± 2.75 | 87.70 ± 3.43 | 86.80 ± 3.22 | 86.50 ± 2.88 |
| RANSAC success rate | 0.119 ± 0.004 | 0.115 ± 0.004 | 0.119 ± 0.004 | 0.129 ± 0.005 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 22.70 ± 3.09 | 24.40 ± 4.60 | 22.50 ± 2.72 | 16.40 ± 4.35 |
| fail: radius_out_of_range | 328.50 ± 9.17 | 346.60 ± 8.18 | 327.20 ± 10.25 | 293.90 ± 7.06 |
| fail: tilt_too_large | 85.60 ± 6.26 | 94.30 ± 7.42 | 87.60 ± 6.47 | 70.90 ± 6.98 |
| fail: normal_inconsistent | 133.80 ± 7.55 | 135.00 ± 6.82 | 132.60 ± 7.01 | 127.90 ± 5.26 |
| fail: arc_coverage_low | 74.50 ± 7.03 | 76.40 ± 5.83 | 74.40 ± 3.41 | 72.70 ± 6.67 |
| fail: no_dtm_intersection | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| time 5 retention [ms] | 1.97 ± 0.66 | 4.92 ± 10.26 | 1.88 ± 0.04 | 1.91 ± 0.03 |
| time 6 clustering [ms] | 2267.65 ± 9.46 | 2743.74 ± 47.74 | 2289.53 ± 14.31 | 1892.72 ± 85.25 |
| time 7 ransac [ms] | 604.09 ± 35.42 | 668.30 ± 27.43 | 623.31 ± 78.54 | 485.62 ± 38.24 |
| time 8 position [ms] | 2.71 ± 3.50 | 1.11 ± 0.94 | 0.77 ± 0.04 | 1.40 ± 1.48 |
| time total 5-8 [ms] | 2876.42 ± 38.37 | 3418.07 ± 57.81 | 2915.49 ± 87.00 | 2381.65 ± 122.26 |

mean ± sample std over 10 seeds.

### improved / Budget-matched (kept count = step's per seed; linear_a and linear_mid coincide as linear*)

| metric | step | linear* | physical* |
|---|---|---|---|
| kept points | 318070.6 ± 105.9 | 318073.4 ± 261.4 | 318148.1 ± 91.4 |
| clusters | 732.40 ± 4.67 | 730.20 ± 5.79 | 717.70 ± 1.42 |
| trees | 87.30 ± 2.75 | 86.70 ± 2.98 | 87.80 ± 3.26 |
| RANSAC success rate | 0.119 ± 0.004 | 0.119 ± 0.004 | 0.122 ± 0.004 |
| fail: too_few_points | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: no_model | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| fail: low_inlier_ratio | 22.70 ± 3.09 | 22.60 ± 3.41 | 21.40 ± 3.13 |
| fail: radius_out_of_range | 328.50 ± 9.17 | 325.70 ± 9.84 | 319.90 ± 3.18 |
| fail: tilt_too_large | 85.60 ± 6.26 | 86.50 ± 3.95 | 80.40 ± 4.53 |
| fail: normal_inconsistent | 133.80 ± 7.55 | 133.80 ± 6.80 | 134.30 ± 4.57 |
| fail: arc_coverage_low | 74.50 ± 7.03 | 74.90 ± 5.61 | 73.90 ± 5.30 |
| fail: no_dtm_intersection | 0.00 ± 0.00 | 0.00 ± 0.00 | 0.00 ± 0.00 |
| time 5 retention [ms] | 1.97 ± 0.66 | 3.31 ± 2.63 | 1.29 ± 0.03 |
| time 6 clustering [ms] | 2267.65 ± 9.46 | 2303.04 ± 52.67 | 2481.84 ± 16.76 |
| time 7 ransac [ms] | 604.09 ± 35.42 | 604.93 ± 28.13 | 566.41 ± 35.76 |
| time 8 position [ms] | 2.71 ± 3.50 | 0.82 ± 0.13 | 0.75 ± 0.03 |
| time total 5-8 [ms] | 2876.42 ± 38.37 | 2912.10 ± 73.83 | 3050.30 ± 45.05 |
| budget |kept - target| | – | 243.6 ± 95.7 | 117.5 ± 96.3 |

mean ± sample std over 10 seeds.

