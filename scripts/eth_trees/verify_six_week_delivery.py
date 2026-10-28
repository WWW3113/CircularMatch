#!/usr/bin/env python3
"""Verify immutable GitHub downloads against the validated local deliverables."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re
from urllib.parse import quote
from urllib.request import urlopen
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'results/six_week_reports_20261028'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--commit', required=True)
    args = parser.parse_args()
    assert re.fullmatch(r'[0-9a-f]{40}', args.commit)
    validation = json.loads((OUT / 'validation.json').read_text())
    files = ['報告安排總覽.txt']
    for report in validation['reports']:
        for kind in ['txt', 'xlsx']:
            name = report[kind]
            assert hashlib.sha256((OUT / name).read_bytes()).hexdigest() == report[kind + '_sha256']
            files.append(name)
    files.append('six_week_reports_20261028.zip')
    assert len(files) == 14
    assert hashlib.sha256((OUT / files[-1]).read_bytes()).hexdigest() == validation['zip_sha256']

    def check(name):
        url = 'https://raw.githubusercontent.com/WWW3113/CircularMatch/' + args.commit + '/' + quote(str((OUT / name).relative_to(ROOT)))
        with urlopen(url, timeout=60) as response:
            status = response.status
            content = response.read()
        expected = hashlib.sha256((OUT / name).read_bytes()).hexdigest()
        assert hashlib.sha256(content).hexdigest() == expected, name
        return name, dict(url=url, sha256=expected, bytes=len(content), http_status=status, verified=True)

    with ThreadPoolExecutor(max_workers=4) as executor:
        verified = dict(executor.map(check, files))
    metadata = dict(commit=args.commit, verified_at=datetime.now(ZoneInfo('Asia/Taipei')).isoformat(), distributed_files=13, downloads=verified)
    (OUT / 'DELIVERY.json').write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(metadata, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
