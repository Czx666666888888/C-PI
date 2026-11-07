import tempfile
import json
import subprocess
import sys
import pi_sync
import unittest
from pathlib import Path
from pi_sync import allowed, digest, plan, snapshot, write_files

class SyncTests(unittest.TestCase):
    def test_conflict_and_baseline(self):
        self.assertEqual(plan({'a': {'hash':'new'}}, {'a':{'hash':'independent'}}, {}), ([], ['a']))
        self.assertEqual(plan({'a': {'hash':'new'}}, {'a':{'hash':'old'}}, {'a':'old'}), (['a'], []))
    def test_secrets_and_paths(self):
        for p in ['../x','/a','.venv/x','x/.env','x/id_rsa','x/a.key','logs/x']:
            self.assertFalse(allowed(p), p)
    def test_remote_worker_bootstrap(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            (root/'a.py').write_text('payload')
            code = Path(pi_sync.__file__).read_text().split('def main():')[0].replace('ROOT = Path(__file__).resolve().parents[1]', 'ROOT = Path.cwd()') + '\nremote_worker()\n'
            result = subprocess.run([sys.executable, '-c', code], input=json.dumps({'root':d,'op':'read'}), text=True, capture_output=True, check=True)
            self.assertEqual(json.loads(result.stdout)['a.py']['hash'], digest(b'payload'))

    def test_write_and_concurrent_change(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            (root/'a.py').write_text('old')
            before = snapshot(root)
            (root/'a.py').write_text('new')
            after = snapshot(root)
            with self.assertRaises(RuntimeError): write_files(root, before, before, ['a.py'])
            self.assertEqual((root/'a.py').read_text(), 'new')
            write_files(root, before, after, ['a.py'])
            self.assertEqual((root/'a.py').read_text(), 'old')

if __name__ == '__main__': unittest.main()
