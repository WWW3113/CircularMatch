# ETH Trees: keypoint extraction runs (execution statistics only)

> 程式成功執行 ≠ 樹位準確度已驗證。此資料集沒有人工標註的參考樹位，
> 因此不計算 precision、recall 或樹位誤差。數字只描述程式行為（點數、樹幹數、成功率、耗時）。

Scans: s1, s2, s3, s4, s5, s6

## Shared stages 1-4 (per scan, identical for all profiles / versions / seeds)

| scan | 0 input pts | 1 dtm+height pts | 2 voxel pts | 3 normals pts | 4 verticality pts | stage 1-4 time [s] |
|---|---|---|---|---|---|---|
| s1 | 19633280 | 17185222 | 3919798 | 3880005 | 374797 | 31.8 |
| s2 | 19602448 | 17099302 | 3959108 | 3918640 | 378636 | 34.2 |
| s3 | 19767841 | 17292553 | 3858526 | 3815378 | 387190 | 30.6 |
| s4 | 20392949 | 17060268 | 4166117 | 4128524 | 370916 | 35.4 |
| s5 | 20481295 | 17308012 | 3954380 | 3916041 | 382064 | 30.4 |
| s6 | 21612153 | 18210844 | 3920799 | 3886372 | 425506 | 31.9 |

## 未啟用額外檢查的基準設定 (baseline: extra checks OFF)

### baseline / Raw (P(d) as defined)

Per station: mean ± std over seeds.

| scan | version | kept points | clusters | trees | RANSAC success | main failures (mean count) | stage 5-8 [ms] |
|---|---|---|---|---|---|---|---|
| s1 | step | 324549 ± 197 | 1054.4 ± 4.0 | 340.2 ± 9.2 | 0.323 ± 0.009 | radius_out_of_range 564.5, tilt_too_large 118.0 | 2588 ± 113 |
| s1 | linear_a | 340377 ± 167 | 1099.5 ± 3.9 | 350.1 ± 8.9 | 0.318 ± 0.008 | radius_out_of_range 593.9, tilt_too_large 123.0 | 2771 ± 58 |
| s1 | linear_mid | 316265 ± 154 | 1045.9 ± 5.2 | 341.3 ± 7.9 | 0.326 ± 0.009 | radius_out_of_range 557.8, tilt_too_large 116.9 | 2381 ± 48 |
| s1 | physical | 272718 ± 150 | 938.1 ± 3.1 | 321.2 ± 11.7 | 0.342 ± 0.012 | radius_out_of_range 500.5, tilt_too_large 86.9 | 1831 ± 10 |
| s2 | step | 322079 ± 184 | 979.4 ± 5.2 | 339.1 ± 11.4 | 0.346 ± 0.012 | radius_out_of_range 501.5, tilt_too_large 111.8 | 2715 ± 25 |
| s2 | linear_a | 342888 ± 135 | 1031.9 ± 3.3 | 348.4 ± 8.9 | 0.338 ± 0.008 | radius_out_of_range 530.9, tilt_too_large 124.8 | 3051 ± 27 |
| s2 | linear_mid | 315722 ± 192 | 978.2 ± 5.0 | 341.7 ± 9.6 | 0.349 ± 0.010 | radius_out_of_range 498.6, tilt_too_large 111.2 | 2564 ± 23 |
| s2 | physical | 272845 ± 205 | 877.8 ± 5.3 | 317.4 ± 9.3 | 0.362 ± 0.011 | radius_out_of_range 449.3, tilt_too_large 88.7 | 2047 ± 34 |
| s3 | step | 318388 ± 181 | 1013.8 ± 3.1 | 302.2 ± 9.7 | 0.298 ± 0.010 | radius_out_of_range 553.9, tilt_too_large 116.5 | 2577 ± 34 |
| s3 | linear_a | 342781 ± 136 | 1063.1 ± 4.1 | 310.8 ± 11.1 | 0.292 ± 0.010 | radius_out_of_range 584.9, tilt_too_large 126.9 | 2966 ± 33 |
| s3 | linear_mid | 314625 ± 220 | 1017.7 ± 4.5 | 304.6 ± 6.8 | 0.299 ± 0.007 | radius_out_of_range 555.9, tilt_too_large 115.9 | 2491 ± 32 |
| s3 | physical | 264455 ± 184 | 893.1 ± 5.3 | 285.7 ± 11.5 | 0.320 ± 0.012 | radius_out_of_range 483.7, tilt_too_large 88.1 | 1901 ± 25 |
| s4 | step | 296699 ± 295 | 728.3 ± 4.9 | 289.5 ± 6.6 | 0.398 ± 0.009 | radius_out_of_range 306.9, tilt_too_large 100.4 | 2481 ± 27 |
| s4 | linear_a | 330645 ± 262 | 765.0 ± 3.4 | 297.9 ± 11.9 | 0.389 ± 0.016 | radius_out_of_range 327.4, tilt_too_large 105.6 | 3183 ± 103 |
| s4 | linear_mid | 301280 ± 312 | 729.4 ± 4.3 | 292.0 ± 6.8 | 0.400 ± 0.009 | radius_out_of_range 306.3, tilt_too_large 101.3 | 2606 ± 86 |
| s4 | physical | 251976 ± 242 | 651.1 ± 3.2 | 278.5 ± 7.7 | 0.428 ± 0.012 | radius_out_of_range 271.9, tilt_too_large 79.6 | 1922 ± 82 |
| s5 | step | 318071 ± 106 | 732.4 ± 4.7 | 292.2 ± 8.7 | 0.399 ± 0.012 | radius_out_of_range 328.5, tilt_too_large 85.6 | 2846 ± 25 |
| s5 | linear_a | 345091 ± 115 | 764.4 ± 2.0 | 295.7 ± 8.3 | 0.387 ± 0.011 | radius_out_of_range 346.6, tilt_too_large 94.3 | 3401 ± 78 |
| s5 | linear_mid | 318925 ± 150 | 731.1 ± 4.9 | 290.4 ± 6.2 | 0.397 ± 0.007 | radius_out_of_range 327.2, tilt_too_large 87.6 | 2896 ± 36 |
| s5 | physical | 276714 ± 131 | 668.3 ± 3.1 | 283.7 ± 7.5 | 0.424 ± 0.010 | radius_out_of_range 293.9, tilt_too_large 70.9 | 2320 ± 44 |
| s6 | step | 347076 ± 197 | 754.9 ± 4.2 | 376.1 ± 7.8 | 0.498 ± 0.011 | radius_out_of_range 254.5, tilt_too_large 87.1 | 3068 ± 28 |
| s6 | linear_a | 378112 ± 160 | 785.6 ± 2.1 | 382.6 ± 5.9 | 0.487 ± 0.008 | radius_out_of_range 270.2, tilt_too_large 95.0 | 3694 ± 45 |
| s6 | linear_mid | 346102 ± 199 | 753.0 ± 3.1 | 379.3 ± 8.8 | 0.504 ± 0.011 | radius_out_of_range 252.1, tilt_too_large 86.2 | 3036 ± 36 |
| s6 | physical | 287181 ± 133 | 690.4 ± 5.5 | 364.7 ± 6.2 | 0.528 ± 0.007 | radius_out_of_range 227.9, tilt_too_large 68.9 | 2199 ± 42 |

Across stations (n = 6): mean ± std of the per-station seed means.

| version | kept points | trees | RANSAC success | stage 5-8 [ms] |
|---|---|---|---|---|
| step | 321144 ± 16122 | 323.2 ± 34.3 | 0.377 ± 0.072 | 2713 ± 215 |
| linear_a | 346649 ± 16229 | 330.9 ± 34.9 | 0.369 ± 0.069 | 3177 ± 329 |
| linear_mid | 318820 ± 14735 | 324.9 ± 35.2 | 0.379 ± 0.073 | 2663 ± 251 |
| physical | 270981 ± 11879 | 308.5 ± 33.0 | 0.401 ± 0.076 | 2037 ± 190 |

### baseline / Budget-matched (kept count = step's)

Per station: mean ± std over seeds.

| scan | version | kept points | clusters | trees | RANSAC success | main failures (mean count) | stage 5-8 [ms] |
|---|---|---|---|---|---|---|---|
| s1 | step | 324549 ± 197 | 1054.4 ± 4.0 | 340.2 ± 9.2 | 0.323 ± 0.009 | radius_out_of_range 564.5, tilt_too_large 118.0 | 2588 ± 113 |
| s1 | linear* | 324411 ± 310 | 1066.3 ± 4.3 | 341.6 ± 9.8 | 0.320 ± 0.009 | radius_out_of_range 573.8, tilt_too_large 119.2 | 2475 ± 21 |
| s1 | physical* | 324655 ± 171 | 1057.3 ± 4.1 | 338.5 ± 7.6 | 0.320 ± 0.007 | radius_out_of_range 578.8, tilt_too_large 107.3 | 2575 ± 48 |
| s2 | step | 322079 ± 184 | 979.4 ± 5.2 | 339.1 ± 11.4 | 0.346 ± 0.012 | radius_out_of_range 501.5, tilt_too_large 111.8 | 2715 ± 25 |
| s2 | linear* | 322158 ± 293 | 989.2 ± 5.0 | 341.2 ± 8.8 | 0.345 ± 0.009 | radius_out_of_range 507.9, tilt_too_large 113.2 | 2679 ± 43 |
| s2 | physical* | 321999 ± 111 | 964.4 ± 3.7 | 333.9 ± 7.8 | 0.346 ± 0.008 | radius_out_of_range 500.9, tilt_too_large 103.4 | 2804 ± 126 |
| s3 | step | 318388 ± 181 | 1013.8 ± 3.1 | 302.2 ± 9.7 | 0.298 ± 0.010 | radius_out_of_range 553.9, tilt_too_large 116.5 | 2577 ± 34 |
| s3 | linear* | 318513 ± 185 | 1025.7 ± 4.8 | 306.5 ± 5.1 | 0.299 ± 0.006 | radius_out_of_range 562.2, tilt_too_large 115.6 | 2551 ± 24 |
| s3 | physical* | 318466 ± 130 | 1018.7 ± 4.1 | 306.9 ± 9.6 | 0.301 ± 0.009 | radius_out_of_range 560.5, tilt_too_large 112.1 | 2710 ± 64 |
| s4 | step | 296699 ± 295 | 728.3 ± 4.9 | 289.5 ± 6.6 | 0.398 ± 0.009 | radius_out_of_range 306.9, tilt_too_large 100.4 | 2481 ± 27 |
| s4 | linear* | 296560 ± 327 | 721.8 ± 6.1 | 293.5 ± 7.9 | 0.407 ± 0.010 | radius_out_of_range 298.9, tilt_too_large 100.0 | 2547 ± 125 |
| s4 | physical* | 296685 ± 342 | 709.8 ± 3.2 | 289.8 ± 7.6 | 0.408 ± 0.011 | radius_out_of_range 299.6, tilt_too_large 92.7 | 2607 ± 51 |
| s5 | step | 318071 ± 106 | 732.4 ± 4.7 | 292.2 ± 8.7 | 0.399 ± 0.012 | radius_out_of_range 328.5, tilt_too_large 85.6 | 2846 ± 25 |
| s5 | linear* | 318073 ± 261 | 730.2 ± 5.8 | 292.0 ± 8.1 | 0.400 ± 0.010 | radius_out_of_range 325.7, tilt_too_large 86.5 | 2854 ± 28 |
| s5 | physical* | 318148 ± 91 | 717.7 ± 1.4 | 292.6 ± 5.2 | 0.408 ± 0.007 | radius_out_of_range 319.9, tilt_too_large 80.4 | 3030 ± 27 |
| s6 | step | 347076 ± 197 | 754.9 ± 4.2 | 376.1 ± 7.8 | 0.498 ± 0.011 | radius_out_of_range 254.5, tilt_too_large 87.1 | 3068 ± 28 |
| s6 | linear* | 347156 ± 187 | 754.0 ± 3.5 | 377.2 ± 8.2 | 0.500 ± 0.011 | radius_out_of_range 253.1, tilt_too_large 87.3 | 3057 ± 32 |
| s6 | physical* | 347003 ± 141 | 761.2 ± 3.6 | 378.0 ± 9.2 | 0.497 ± 0.013 | radius_out_of_range 262.7, tilt_too_large 85.5 | 3254 ± 31 |

Across stations (n = 6): mean ± std of the per-station seed means.

| version | kept points | trees | RANSAC success | stage 5-8 [ms] |
|---|---|---|---|---|
| step | 321144 ± 16122 | 323.2 ± 34.3 | 0.377 ± 0.072 | 2713 ± 215 |
| linear* | 321145 ± 16181 | 325.3 ± 33.7 | 0.378 ± 0.073 | 2694 ± 223 |
| physical* | 321159 ± 16101 | 323.3 ± 33.7 | 0.380 ± 0.072 | 2830 ± 265 |

## 啟用檢查的改良設定 (improved: extra checks ON)

### improved / Raw (P(d) as defined)

Per station: mean ± std over seeds.

| scan | version | kept points | clusters | trees | RANSAC success | main failures (mean count) | stage 5-8 [ms] |
|---|---|---|---|---|---|---|---|
| s1 | step | 324549 ± 197 | 1054.4 ± 4.0 | 68.1 ± 2.6 | 0.065 ± 0.002 | radius_out_of_range 564.5, normal_inconsistent 182.6 | 2573 ± 37 |
| s1 | linear_a | 340377 ± 167 | 1099.5 ± 3.9 | 67.7 ± 2.3 | 0.062 ± 0.002 | radius_out_of_range 593.9, normal_inconsistent 190.6 | 2768 ± 74 |
| s1 | linear_mid | 316265 ± 154 | 1045.9 ± 5.2 | 67.5 ± 2.0 | 0.065 ± 0.002 | radius_out_of_range 557.8, normal_inconsistent 183.7 | 2362 ± 29 |
| s1 | physical | 272718 ± 150 | 938.1 ± 3.1 | 67.2 ± 2.3 | 0.072 ± 0.002 | radius_out_of_range 500.5, normal_inconsistent 166.9 | 1837 ± 20 |
| s2 | step | 322079 ± 184 | 979.4 ± 5.2 | 75.9 ± 3.0 | 0.077 ± 0.003 | radius_out_of_range 501.5, normal_inconsistent 176.0 | 2710 ± 21 |
| s2 | linear_a | 342888 ± 135 | 1031.9 ± 3.3 | 76.3 ± 2.5 | 0.074 ± 0.003 | radius_out_of_range 530.9, normal_inconsistent 185.2 | 3058 ± 30 |
| s2 | linear_mid | 315722 ± 192 | 978.2 ± 5.0 | 76.5 ± 3.4 | 0.078 ± 0.003 | radius_out_of_range 498.6, normal_inconsistent 177.1 | 2588 ± 54 |
| s2 | physical | 272845 ± 205 | 877.8 ± 5.3 | 74.2 ± 3.0 | 0.085 ± 0.003 | radius_out_of_range 449.3, normal_inconsistent 160.5 | 2045 ± 39 |
| s3 | step | 318388 ± 181 | 1013.8 ± 3.1 | 55.1 ± 2.4 | 0.054 ± 0.002 | radius_out_of_range 553.9, normal_inconsistent 164.5 | 2572 ± 26 |
| s3 | linear_a | 342781 ± 136 | 1063.1 ± 4.1 | 55.2 ± 2.6 | 0.052 ± 0.002 | radius_out_of_range 584.9, normal_inconsistent 170.2 | 2959 ± 21 |
| s3 | linear_mid | 314625 ± 220 | 1017.7 ± 4.5 | 55.0 ± 2.5 | 0.054 ± 0.002 | radius_out_of_range 555.9, normal_inconsistent 165.7 | 2492 ± 34 |
| s3 | physical | 264455 ± 184 | 893.1 ± 5.3 | 54.6 ± 2.4 | 0.061 ± 0.003 | radius_out_of_range 483.7, normal_inconsistent 151.5 | 1896 ± 15 |
| s4 | step | 296699 ± 295 | 728.3 ± 4.9 | 68.9 ± 2.0 | 0.095 ± 0.003 | radius_out_of_range 306.9, normal_inconsistent 141.3 | 2476 ± 26 |
| s4 | linear_a | 330645 ± 262 | 765.0 ± 3.4 | 69.5 ± 2.2 | 0.091 ± 0.003 | radius_out_of_range 327.4, normal_inconsistent 145.8 | 3146 ± 59 |
| s4 | linear_mid | 301280 ± 312 | 729.4 ± 4.3 | 69.3 ± 1.6 | 0.095 ± 0.002 | radius_out_of_range 306.3, normal_inconsistent 143.4 | 2550 ± 28 |
| s4 | physical | 251976 ± 242 | 651.1 ± 3.2 | 67.1 ± 2.9 | 0.103 ± 0.005 | radius_out_of_range 271.9, normal_inconsistent 133.8 | 1870 ± 13 |
| s5 | step | 318071 ± 106 | 732.4 ± 4.7 | 87.3 ± 2.8 | 0.119 ± 0.004 | radius_out_of_range 328.5, normal_inconsistent 133.8 | 2876 ± 38 |
| s5 | linear_a | 345091 ± 115 | 764.4 ± 2.0 | 87.7 ± 3.4 | 0.115 ± 0.004 | radius_out_of_range 346.6, normal_inconsistent 135.0 | 3418 ± 58 |
| s5 | linear_mid | 318925 ± 150 | 731.1 ± 4.9 | 86.8 ± 3.2 | 0.119 ± 0.004 | radius_out_of_range 327.2, normal_inconsistent 132.6 | 2915 ± 87 |
| s5 | physical | 276714 ± 131 | 668.3 ± 3.1 | 86.5 ± 2.9 | 0.129 ± 0.005 | radius_out_of_range 293.9, normal_inconsistent 127.9 | 2382 ± 122 |
| s6 | step | 347076 ± 197 | 754.9 ± 4.2 | 116.0 ± 3.4 | 0.154 ± 0.004 | radius_out_of_range 254.5, normal_inconsistent 168.7 | 3089 ± 41 |
| s6 | linear_a | 378112 ± 160 | 785.6 ± 2.1 | 117.2 ± 3.6 | 0.149 ± 0.005 | radius_out_of_range 270.2, normal_inconsistent 171.9 | 3731 ± 84 |
| s6 | linear_mid | 346102 ± 199 | 753.0 ± 3.1 | 116.1 ± 2.5 | 0.154 ± 0.003 | radius_out_of_range 252.1, normal_inconsistent 171.5 | 3081 ± 70 |
| s6 | physical | 287181 ± 133 | 690.4 ± 5.5 | 115.6 ± 3.3 | 0.167 ± 0.005 | radius_out_of_range 227.9, normal_inconsistent 159.5 | 2198 ± 40 |

Across stations (n = 6): mean ± std of the per-station seed means.

| version | kept points | trees | RANSAC success | stage 5-8 [ms] |
|---|---|---|---|---|
| step | 321144 ± 16122 | 78.5 ± 21.2 | 0.094 ± 0.037 | 2716 ± 230 |
| linear_a | 346649 ± 16229 | 78.9 ± 21.6 | 0.090 ± 0.036 | 3180 ± 345 |
| linear_mid | 318820 ± 14735 | 78.5 ± 21.2 | 0.094 ± 0.037 | 2665 ± 274 |
| physical | 270981 ± 11879 | 77.5 ± 21.4 | 0.103 ± 0.040 | 2038 ± 216 |

### improved / Budget-matched (kept count = step's)

Per station: mean ± std over seeds.

| scan | version | kept points | clusters | trees | RANSAC success | main failures (mean count) | stage 5-8 [ms] |
|---|---|---|---|---|---|---|---|
| s1 | step | 324549 ± 197 | 1054.4 ± 4.0 | 68.1 ± 2.6 | 0.065 ± 0.002 | radius_out_of_range 564.5, normal_inconsistent 182.6 | 2573 ± 37 |
| s1 | linear* | 324411 ± 310 | 1066.3 ± 4.3 | 67.3 ± 2.0 | 0.063 ± 0.002 | radius_out_of_range 573.8, normal_inconsistent 184.3 | 2525 ± 79 |
| s1 | physical* | 324655 ± 171 | 1057.3 ± 4.1 | 67.9 ± 2.0 | 0.064 ± 0.002 | radius_out_of_range 578.8, normal_inconsistent 178.5 | 2598 ± 57 |
| s2 | step | 322079 ± 184 | 979.4 ± 5.2 | 75.9 ± 3.0 | 0.077 ± 0.003 | radius_out_of_range 501.5, normal_inconsistent 176.0 | 2710 ± 21 |
| s2 | linear* | 322158 ± 293 | 989.2 ± 5.0 | 76.2 ± 2.5 | 0.077 ± 0.003 | radius_out_of_range 507.9, normal_inconsistent 178.3 | 2671 ± 27 |
| s2 | physical* | 321999 ± 111 | 964.4 ± 3.7 | 75.4 ± 3.4 | 0.078 ± 0.004 | radius_out_of_range 500.9, normal_inconsistent 172.3 | 2764 ± 11 |
| s3 | step | 318388 ± 181 | 1013.8 ± 3.1 | 55.1 ± 2.4 | 0.054 ± 0.002 | radius_out_of_range 553.9, normal_inconsistent 164.5 | 2572 ± 26 |
| s3 | linear* | 318513 ± 185 | 1025.7 ± 4.8 | 54.7 ± 2.6 | 0.053 ± 0.003 | radius_out_of_range 562.2, normal_inconsistent 167.9 | 2549 ± 43 |
| s3 | physical* | 318466 ± 130 | 1018.7 ± 4.1 | 54.6 ± 2.5 | 0.054 ± 0.002 | radius_out_of_range 560.5, normal_inconsistent 166.0 | 2689 ± 23 |
| s4 | step | 296699 ± 295 | 728.3 ± 4.9 | 68.9 ± 2.0 | 0.095 ± 0.003 | radius_out_of_range 306.9, normal_inconsistent 141.3 | 2476 ± 26 |
| s4 | linear* | 296560 ± 327 | 721.8 ± 6.1 | 67.9 ± 2.5 | 0.094 ± 0.004 | radius_out_of_range 298.9, normal_inconsistent 143.5 | 2475 ± 20 |
| s4 | physical* | 296685 ± 342 | 709.8 ± 3.2 | 67.7 ± 2.2 | 0.095 ± 0.003 | radius_out_of_range 299.6, normal_inconsistent 141.3 | 2609 ± 72 |
| s5 | step | 318071 ± 106 | 732.4 ± 4.7 | 87.3 ± 2.8 | 0.119 ± 0.004 | radius_out_of_range 328.5, normal_inconsistent 133.8 | 2876 ± 38 |
| s5 | linear* | 318073 ± 261 | 730.2 ± 5.8 | 86.7 ± 3.0 | 0.119 ± 0.004 | radius_out_of_range 325.7, normal_inconsistent 133.8 | 2912 ± 74 |
| s5 | physical* | 318148 ± 91 | 717.7 ± 1.4 | 87.8 ± 3.3 | 0.122 ± 0.004 | radius_out_of_range 319.9, normal_inconsistent 134.3 | 3050 ± 45 |
| s6 | step | 347076 ± 197 | 754.9 ± 4.2 | 116.0 ± 3.4 | 0.154 ± 0.004 | radius_out_of_range 254.5, normal_inconsistent 168.7 | 3089 ± 41 |
| s6 | linear* | 347156 ± 187 | 754.0 ± 3.5 | 116.1 ± 3.5 | 0.154 ± 0.005 | radius_out_of_range 253.1, normal_inconsistent 170.1 | 3084 ± 45 |
| s6 | physical* | 347003 ± 141 | 761.2 ± 3.6 | 116.4 ± 3.1 | 0.153 ± 0.004 | radius_out_of_range 262.7, normal_inconsistent 169.6 | 3366 ± 216 |

Across stations (n = 6): mean ± std of the per-station seed means.

| version | kept points | trees | RANSAC success | stage 5-8 [ms] |
|---|---|---|---|---|
| step | 321144 ± 16122 | 78.5 ± 21.2 | 0.094 ± 0.037 | 2716 ± 230 |
| linear* | 321145 ± 16181 | 78.1 ± 21.4 | 0.093 ± 0.038 | 2703 ± 244 |
| physical* | 321159 ± 16101 | 78.3 ± 21.6 | 0.094 ± 0.038 | 2846 ± 304 |

