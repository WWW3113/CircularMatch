#!/usr/bin/env python3
"""Fetch public TreeTool derivatives of Liang's six sample plots and LHD refs.

These are prepared single PCDs, NOT the original per-station registration scans.
Published source: https://github.com/porteratzo/TreeTool (README dataset link).
No publisher hashes are supplied; computed SHA256s and PCD sizes are recorded.
"""
import concurrent.futures
import hashlib
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'data/liang_treetool/TreeToolSamples'
IDS=[
 ('1HKCyha5gJ0nyGgTKEKKimfXlhyM5iwnF','1DngRKGKQTPAwFafQWGZ_zUtoYZd9Vbte'),
 ('1h3olwx2-8dunqsFFGuP7z6HB0mlfv71V','1ncwgpcWqAZJF66bVPgVMJHEyEK0XltqY'),
 ('1iBy8JqRDnuPlnVws5yyv95bNzf_cYdbQ','1UQGXrrGT3Z2arkR69PYMWcjELQpcUR2t'),
 ('1KqDhqsmXeXYD5zsmzl02F3BPO_3MnrIn','1yt1xNo7-63LfQwxteKgGXo0zxntvDZz3'),
 ('1WSWto-Q3q5p306r92ag003V8J3UfEibP','1zT1ifpaDMPK8PADIRRL_7QB9H6Y0gmNB'),
 ('1oIuItFOYQmI_A-VNXj9-3DoAXTdX89ku','1292wtKKivUYk90q2Hv3QUiUXi8qDnwH2'),
]


def pcd_header(path):
    meta={}
    with path.open('rb') as f:
        for _ in range(200):
            raw=f.readline()
            if not raw: raise ValueError(f'{path}: incomplete PCD header')
            line=raw.decode('ascii').strip()
            if line and not line.startswith('#'):
                key,*values=line.split();meta[key]=values
                if key=='DATA':break
        else: raise ValueError(f'{path}: PCD header exceeds 200 lines')
        offset=f.tell()
    assert meta['FIELDS']==['x','y','z'] and meta['SIZE']==['4']*3 and meta['TYPE']==['F']*3
    assert meta['DATA']==['binary']
    assert path.stat().st_size==offset+int(meta['POINTS'][0])*12,(path,path.stat().st_size,meta)
    return meta


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    def one(job):
        plot,id,name=job;path=OUT/name
        if not path.exists():
            part=path.with_suffix(path.suffix+'.part')
            subprocess.run(['curl','-fsSL','--retry','2','-C','-','--max-time','300','-o',str(part),
                            f'https://drive.usercontent.google.com/download?id={id}&export=download&confirm=t'],check=True)
            part.rename(path)
        record=dict(plot=plot,name=name,bytes=path.stat().st_size,computed_sha256=hashlib.file_digest(path.open('rb'),'sha256').hexdigest(),public_file_id=id)
        if name.endswith('.pcd'):record['pcd']=pcd_header(path)
        else:
            rows=[line.split() for line in path.read_text().splitlines() if line.strip()]
            assert all(len(row)==5 for row in rows)
            record['reference_trees']=len(rows)
        print(name,record['bytes'],flush=True);return record
    jobs=[]
    for i,(cloud,ref) in enumerate(IDS,1):
        jobs.extend([(i,cloud,f'downsampledlesscloudEURO{i}.pcd'),(i,ref,f'TLS_Benchmarking_Plot_{i}_LHD.txt')])
    with concurrent.futures.ThreadPoolExecutor(3) as pool:records=list(pool.map(one,jobs))
    manifest=dict(source='https://github.com/porteratzo/TreeTool',original_release='https://laserscanning.fi/results-available-for-international-benchmarking-of-terrestrial-laser-scanning-methods/',
                  public_folder='https://drive.google.com/drive/folders/1AhYd8pwCrTAJCV4OIc9MWot8tjNgIJSx',
                  version='TreeTool downsampled PCD sample plots; not original scan pairs',publisher_checksums_available=False,files=records,
                  contains_per_station_registration_ground_truth=False)
    (OUT/'DOWNLOAD.json').write_text(json.dumps(manifest,indent=2)+'\n')


if __name__=='__main__':main()
