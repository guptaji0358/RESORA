"""Verifies the backend CLI contract (docs/BACKEND_CONTRACT.md) end to end by
spawning the backend as a separate process, the way the Qt GUI does."""
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CT = "_contract-test"


def run_backend(*args: str, cwd: Path | None = None, timeout: float = 30):
    r = subprocess.run([sys.executable, "-m", "scripts.engine", *args], cwd=cwd or ROOT,
                       capture_output=True, text=True, encoding="utf-8", timeout=timeout)
    lines = [json.loads(line) for line in r.stdout.splitlines()]  # every stdout line must be JSON
    return r, lines


class Envelope(unittest.TestCase):
    def assertWellFormed(self, lines):
        self.assertTrue(lines)
        for env in lines:
            self.assertEqual(env["v"], 1)
            self.assertIn(env["type"], ("progress", "result", "error"))
        self.assertIn(lines[-1]["type"], ("result", "error"))
        self.assertEqual(sum(e["type"] in ("result", "error") for e in lines), 1)


class Contract(Envelope):
    def test_selfcheck_command_and_legacy_flag(self):
        for args in (["selfcheck"], ["--selfcheck"]):
            r, lines = run_backend(*args)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertWellFormed(lines)
            self.assertTrue(lines[-1]["data"]["ok"])

    def test_independent_of_working_directory(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = subprocess.run([sys.executable, "-m", "scripts.engine", "selfcheck"], cwd=tmp,
                               capture_output=True, text=True, env={**__import__("os").environ, "PYTHONPATH": str(ROOT)})
            self.assertEqual(r.returncode, 0, r.stderr)

    def test_backend_does_not_import_gui_toolkits(self):
        code = ("import sys, scripts.engine.main, scripts.engine.registry as r; r.build_registry();"
                "bad=[m for m in sys.modules if m.split('.')[0] in ('PyQt5','PyQt6','PySide2','PySide6','tkinter')];"
                "sys.exit(1 if bad else 0)")
        r = subprocess.run([sys.executable, "-c", code], cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)

    def test_params_file_round_trip(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "p.json"
            p.write_text(json.dumps({"folder": "C:\\ünï", "n": 1}), encoding="utf-8")
            r, lines = run_backend("--params-file", str(p), CT, "ok")
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertEqual(lines[-1]["data"]["params"], {"folder": "C:\\ünï", "n": 1})

    def test_progress_precedes_single_result(self):
        r, lines = run_backend(CT, "progress", "--steps", "4")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertWellFormed(lines)
        self.assertEqual([e["done"] for e in lines if e["type"] == "progress"], [1, 2, 3, 4])

    def test_large_output_goes_to_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "big.json"
            r, lines = run_backend(CT, "large", "--output", str(out), "--items", "50000")
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertLess(len(r.stdout), 1000)
            self.assertEqual(len(json.loads(out.read_text(encoding="utf-8"))), 50000)
            self.assertEqual(lines[-1]["data"]["items"], 50000)


class Errors(Envelope):
    def check(self, args, exit_code, code):
        r, lines = run_backend(*args)
        self.assertEqual(r.returncode, exit_code, r.stderr)
        self.assertWellFormed(lines)
        self.assertEqual(lines[-1]["type"], "error")
        self.assertEqual(lines[-1]["code"], code)
        self.assertTrue(r.stderr.strip(), "failures must also explain themselves on stderr")

    def test_runtime_failure(self):
        self.check([CT, "fail"], 1, "test_failure")

    def test_invalid_input(self):
        self.check([CT, "invalid"], 3, "invalid_input")

    def test_unexpected_exception_is_internal_error(self):
        self.check([CT, "crash"], 1, "internal")

    def test_usage_errors(self):
        self.check([], 2, "usage")
        self.check(["nope"], 2, "usage")
        self.check([CT, "bogus-mode"], 2, "usage")

    def test_bad_params_file(self):
        self.check(["--params-file", str(ROOT / "does-not-exist.json"), CT, "ok"], 3, "invalid_input")


class Cancellation(Envelope):
    def spawn(self):
        return subprocess.Popen([sys.executable, "-m", "scripts.engine", "--cancel-on-stdin", CT, "progress",
                                 "--steps", "200", "--delay", "0.05"], cwd=ROOT, stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8")

    def test_cancel_line_stops_with_130(self):
        p = self.spawn()
        self.assertEqual(json.loads(p.stdout.readline())["type"], "progress")  # running
        p.stdin.write("cancel\n")
        p.stdin.flush()
        out, _ = p.communicate(timeout=10)
        self.assertEqual(p.returncode, 130)
        last = json.loads(out.splitlines()[-1])
        self.assertEqual((last["type"], last["code"]), ("error", "cancelled"))

    def test_stdin_eof_cancels_orphaned_backend(self):
        p = self.spawn()
        p.stdout.readline()
        p.stdin.close()
        p.communicate(timeout=10)
        self.assertEqual(p.returncode, 130)

    def test_hard_kill_is_prompt(self):
        p = self.spawn()
        p.stdout.readline()
        start = time.monotonic()
        p.kill()
        p.communicate(timeout=10)
        self.assertLess(time.monotonic() - start, 5)


if __name__ == "__main__":
    unittest.main()
