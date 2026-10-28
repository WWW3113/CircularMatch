#!/usr/bin/env python3
"""Single resumable controller; starts no duplicate controller on this machine."""
from datetime import datetime,timezone
import fcntl
import json
from pathlib import Path
import subprocess
import unified_study as s

s.RUNS.mkdir(parents=True,exist_ok=True)
with (s.RUNS/'controller.lock').open('a') as lock:
    fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    now=datetime.now(timezone.utc)
    cpu=next((line.split(':',1)[1].strip() for line in Path('/proc/cpuinfo').read_text().splitlines() if line.startswith('model name')),'unknown')
    epoch=dict(id=now.strftime('%Y%m%dT%H%M%SZ'),started_utc=now.isoformat(),cpu_model=cpu,cpu_quota=Path('/sys/fs/cgroup/cpu.max').read_text().strip(),memory_limit=Path('/sys/fs/cgroup/memory.max').read_text().strip(),note='Runtime restarted; legacy timing retained as historical, fair v3 controls measured in current epoch')
    (s.OUT/'current_epoch.json').write_text(json.dumps(epoch,indent=2)+'\n')
    s.restore_checkpoint()
    s.publish_checkpoint()
    for command in [[str(s.TOOL),'/usr/bin/cmake','-S','.','-B','build'],[str(s.TOOL),'/usr/bin/cmake','--build','build','--target','cm_geometry_register_v3','cm_geometry_structural_tests','-j2'],[str(s.TOOL),str(s.ROOT/'build/cm_geometry_structural_tests')]]:
        subprocess.run(command,cwd=s.ROOT,check=True)
    s.freeze_v3()
    s.publish_checkpoint()
    s.infer_v3()
    s.publish_checkpoint()
    s.evaluate('v3')
    s.publish_checkpoint()
    s.cold_benchmark()
    s.publish_checkpoint()
    s.feature_benchmark()
    s.publish_checkpoint()
    print('ACCURACY AND COST PHASES COMPLETE; run unified_report.py and audit outputs',flush=True)
