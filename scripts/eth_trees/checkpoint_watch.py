#!/usr/bin/env python3
"""Keep small results safe while long native jobs run; never publishes raw clouds."""
import time
import importlib
import unified_study as study

while not (study.OUT/'FINAL_COMPLETE.json').exists():
    try:
        importlib.reload(study)
        study.publish_checkpoint()
    except Exception as error:
        print('CHECKPOINT PUSH ERROR (local results retained):',repr(error),flush=True)
    time.sleep(120)
