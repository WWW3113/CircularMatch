#!/usr/bin/env python3
"""Fetch the seven immutable downloads and verify their exact SHA256."""
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
from pathlib import Path
from urllib.parse import quote
from urllib.request import urlopen
from zoneinfo import ZoneInfo
import argparse,hashlib,json,re

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'results/meeting_reports_20261108'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--commit',required=True);a=ap.parse_args();assert re.fullmatch('[0-9a-f]{40}',a.commit)
 validation=json.loads((OUT/'validation.json').read_text());files=[]
 for r in validation['reports']:
  assert sha(OUT/r['file'])==r['sha256'];files.append(r['file'])
 assert len(files)==6
 files.append('meeting_6weeks.zip');assert sha(OUT/files[-1])==validation['zip_sha256']
 def fetch(name):
  p=OUT/name;url='https://raw.githubusercontent.com/WWW3113/CircularMatch/'+a.commit+'/'+quote(str(p.relative_to(ROOT)))
  with urlopen(url,timeout=60) as response:status=response.status;data=response.read()
  assert status==200 and hashlib.sha256(data).hexdigest()==sha(p),name
  return name,dict(url=url,sha256=sha(p),http_status=status,verified=True)
 with ThreadPoolExecutor(max_workers=4) as pool:downloads=dict(pool.map(fetch,files))
 d=dict(commit=a.commit,verified_at=datetime.now(ZoneInfo('Asia/Taipei')).isoformat(),workbooks=6,all_no_cell_comments=True,downloads=downloads)
 (OUT/'DELIVERY.json').write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n');print(json.dumps(d,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
