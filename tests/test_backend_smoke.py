import json
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class BackendSmoke(unittest.TestCase):
    def test_selfcheck_reports_ok_json(self):
        r = subprocess.run([sys.executable, "-m", "scripts.engine", "--selfcheck"],
                           cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertTrue(json.loads(r.stdout)["data"]["ok"])


if __name__ == "__main__":
    unittest.main()
