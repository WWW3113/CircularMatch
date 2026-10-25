# 原論文與我們目前方法的比較

paper_vs_ours.xlsx以白底灰色表頭呈現，先讀「先看結論」及「改了哪些參數」。其他頁保留調整證據、各配對結果、新seed逐次數字、來源限制與600列完整驗證資料。

論文ETH報告100%成功率、e_p=0.153427m；我們的新seed21–30、全部15配對為150/150成功，e_p平均0.07615435m。表內明確區分平均值與中位數、同機重跑與文章報告值、論文參數與未指定的實作細節。配對集合、成功門檻及計時尚未完全對齊，因此不能宣稱嚴格超越原論文。

重建：`OPENBLAS_NUM_THREADS=1 python3 scripts/eth_trees/make_paper_comparison.py`。原論文以使用者上傳PDF的Table I、Table II及第II/III節為來源；我們的數字由既有完整驗證CSV計算，不需要重跑實驗。

做完本檔後，另執行了 [WHU-Park外部場景測試](../whu_park/transfer_20261025/README.md)。固定ETH設定0/310成功，依點間距調整法向量半徑後僅2/310成功；因此尚無可靠泛化效果。此後續測試另附cross_dataset_park.xlsx，未混入本檔ETH與論文的數字。
