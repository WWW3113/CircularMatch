# 向教授分次報告

建議4次，每次10～15分鐘，以方法改動及證據串接；每次各有完整TXT口述稿與3工作表XLSX。請先讀[報告安排總覽](報告安排總覽.txt)。

| 次數 | 主題 | 文字稿 | 比較表 |
|---|---|---|---|
| 1 | 匹配容差與弱配對補救 | [TXT](report_01_%E5%8C%B9%E9%85%8D%E5%AE%B9%E5%B7%AE%E8%88%87%E5%BC%B1%E9%85%8D%E5%B0%8D%E8%A3%9C%E6%95%91.txt) | [XLSX](report_01_%E5%8C%B9%E9%85%8D%E5%AE%B9%E5%B7%AE%E8%88%87%E5%BC%B1%E9%85%8D%E5%B0%8D%E8%A3%9C%E6%95%91.xlsx) |
| 2 | DTM平滑的局部改善與代價 | [TXT](report_02_DTM%E5%B9%B3%E6%BB%91%E7%9A%84%E5%B1%80%E9%83%A8%E6%94%B9%E5%96%84%E8%88%87%E4%BB%A3%E5%83%B9.txt) | [XLSX](report_02_DTM%E5%B9%B3%E6%BB%91%E7%9A%84%E5%B1%80%E9%83%A8%E6%94%B9%E5%96%84%E8%88%87%E4%BB%A3%E5%83%B9.xlsx) |
| 3 | 加入多尺度幾何微調 | [TXT](report_03_%E5%8A%A0%E5%85%A5%E5%A4%9A%E5%B0%BA%E5%BA%A6%E5%B9%BE%E4%BD%95%E5%BE%AE%E8%AA%BF.txt) | [XLSX](report_03_%E5%8A%A0%E5%85%A5%E5%A4%9A%E5%B0%BA%E5%BA%A6%E5%B9%BE%E4%BD%95%E5%BE%AE%E8%AA%BF.xlsx) |
| 4 | 多候選與垂直結構驗證 | [TXT](report_04_%E5%A4%9A%E5%80%99%E9%81%B8%E8%88%87%E5%9E%82%E7%9B%B4%E7%B5%90%E6%A7%8B%E9%A9%97%E8%AD%89.txt) | [XLSX](report_04_%E5%A4%9A%E5%80%99%E9%81%B8%E8%88%87%E5%9E%82%E7%9B%B4%E7%B5%90%E6%A7%8B%E9%A9%97%E8%AD%89.xlsx) |

報告依序涵蓋粗配準、DTM的局部改善與退步、幾何微調、多候選及結構驗證。Tongji四個Plot是一個dataset，ETH是另一個。沒有把兩來源混算成一個e_p平均。

原作者baseline尚未完成；本地5cm版標為參考實作。缺少或無法公平比較的資料標待補。DTM完整新流程只有Plot2；最終v3的垂直篩選、驗證gate及接受判斷是整組修改，尚無單因素因果結論。舊cohort和新cohort分開呈現，保留負面結果。

- `實驗盤點與數據口徑.txt`：其他已完成探索、Park失敗、多站圖及提取限定支線，說明未獨立拆篇原因。
- `source_manifest.json`：每張表、每個資料群的CSV列號、篩選、metric、來源檔SHA256；P為上傳論文，PDF實體p4 Table I/II、p5 Table III。
- `evidence/`：原始scalar結果CSV/設定的逐byte快照，包含以前忽略於runs的結果，方便離線追溯；沒有原始點雲。
- `validation.json`：重新開啟XLSX、公式快取、圖表快取、缺值與數字核驗。公式已由LibreOffice重算，檔案不依賴外部Excel連結。
- 全部成果另打包為 `professor_reports_20261026.zip`；不含內部render、完整原論文或原始點雲。

重建程式在repository的 `scripts/eth_trees/`：make_professor_reports.py → write_professor_talks.py → professor_inventory.py。XLSX來源先寫入忽略的runs，需以LibreOffice重算至本目錄，再執行validate_professor_reports.py。缺少歷史原始CSV時會使用evidence快照；可用CIRCULARMATCH_PAPER指定同一上傳PDF，否則沿用既有PDF SHA與視讀紀錄。不得用openpyxl再次存檔重算後的XLSX，否則會丟失公式快取。
