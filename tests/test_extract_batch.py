"""CLI integration: cached preprocessing must preserve single-run CSV bytes."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
cloud=Path('/workspace/.circularmatch-env/run')
wrapper=os.environ.get('CM_TOOLCHAIN_WRAPPER',str(cloud) if cloud.exists() else '')


def run(tool,args):
    command=([wrapper] if wrapper else [])+[str(ROOT/'build'/tool),*args]
    return subprocess.run(command,cwd=ROOT,capture_output=True,text=True)


class BatchExtractionTests(unittest.TestCase):
    def test_two_seeds_match_individual_extractions(self):
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);scene=d/'scene.las'
            generated=run('cm_synth',[f'--out={scene}'])
            self.assertEqual(generated.returncode,0,generated.stderr)
            common=[f'--input={scene}','--config=config/default.ini','--cluster.tolerance=0.20']
            result=run('cm_extract_batch',common+[f'--out={d}/batch','--experiment.seed_base=11','--experiment.n_seeds=2'])
            self.assertEqual(result.returncode,0,result.stderr)
            for seed in [11,12]:
                single=run('cm_extract',common+[f'--out={d}/single',f'--seed={seed}'])
                self.assertEqual(single.returncode,0,single.stderr)
                for suffix in ['trees','clusters','retention_bins']:
                    name=f'step_seed{seed}_{suffix}.csv'
                    self.assertEqual((d/'batch'/name).read_bytes(),(d/'single'/name).read_bytes(),name)

    def test_empty_seed_range_is_rejected_before_reading_input(self):
        result=run('cm_extract_batch',['--input=missing.las','--out=unused','--experiment.n_seeds=0'])
        self.assertNotEqual(result.returncode,0)
        self.assertIn('seed range must be nonempty',result.stderr)


if __name__=='__main__':unittest.main()
