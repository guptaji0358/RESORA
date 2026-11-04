"""The `scan` command: in-process orchestration plus one run through the real CLI."""
from __future__ import annotations

import argparse
import csv
import io
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image

from scripts.engine.command import Context
from scripts.engine.commands import scan
from scripts.engine.protocol import EXIT_INVALID_INPUT, BackendError, CancelToken, Cancelled, Emitter

ROOT = Path(__file__).resolve().parents[1]


def make_tree(root: Path) -> None:
    (root / "sub").mkdir()
    Image.new("RGB", (1920, 1080)).save(root / "a.png")
    Image.new("RGB", (3840, 2160)).save(root / "sub" / "b.jpg")
    (root / "bad.jpg").write_bytes(b"junk")
    (root / "readme.txt").write_text("hi")


def run_scan(argv: list[str], params=None, cancel: CancelToken | None = None):
    parser = argparse.ArgumentParser()
    scan.add_arguments(parser)
    out = io.StringIO()
    ctx = Context(emit=Emitter(out), cancel=cancel or CancelToken(), params=params or {})
    data = scan.run(parser.parse_args(argv), ctx)
    return data, [json.loads(line) for line in out.getvalue().splitlines()]


class ScanCommand(unittest.TestCase):
    def test_non_recursive_by_default_and_failures_do_not_stop_scan(self):
        with tempfile.TemporaryDirectory() as tmp:
            make_tree(Path(tmp))
            data, events = run_scan([tmp])
            self.assertEqual((data["summary"]["total"], data["summary"]["analyzed"]), (2, 1))
            self.assertEqual(data["summary"]["by_status"], {"corrupted": 1, "ok": 1})
            self.assertEqual(data["skipped_non_images"], 1)
            self.assertEqual(events[-1]["type"], "progress")
            self.assertEqual((events[-1]["done"], events[-1]["total"]), (2, 2))

    def test_recursive_and_reports(self):
        with tempfile.TemporaryDirectory() as tmp, tempfile.TemporaryDirectory() as outdir:
            make_tree(Path(tmp))
            csv_path, txt_path = Path(outdir) / "r.csv", Path(outdir) / "r.txt"
            data, _ = run_scan([tmp, "--recursive", "--csv", str(csv_path), "--txt", str(txt_path)])
            self.assertEqual(data["summary"]["total"], 3)
            self.assertEqual(data["reports"]["csv"], str(csv_path))
            with open(csv_path, encoding="utf-8-sig", newline="") as fh:
                rows = {Path(r["path"]).name: r for r in csv.DictReader(fh)}
            self.assertEqual(rows["b.jpg"]["resolution"], "UHD 4K")
            self.assertEqual(rows["bad.jpg"]["status"], "corrupted")
            self.assertEqual(rows["a.png"]["path"], str(Path(tmp) / "a.png"))
            self.assertIn("UHD 4K", txt_path.read_text(encoding="utf-8"))

    def test_params_file_values_used_when_flags_absent(self):
        with tempfile.TemporaryDirectory() as tmp:
            make_tree(Path(tmp))
            data, _ = run_scan([], params={"folder": tmp, "recursive": True})
            self.assertEqual(data["summary"]["total"], 3)

    def test_invalid_folder(self):
        for argv in ([], ["Z:\\definitely\\missing"]):
            with self.assertRaises(BackendError) as cm:
                run_scan(argv)
            self.assertEqual((cm.exception.code, cm.exception.exit_code), ("invalid_input", EXIT_INVALID_INPUT))

    def test_unwritable_report_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            make_tree(Path(tmp))
            with self.assertRaises(BackendError) as cm:
                run_scan([tmp, "--csv", str(Path(tmp) / "a.png" / "x.csv")])  # parent is a file
            self.assertEqual(cm.exception.code, "report_failed")

    def test_cancel(self):
        token = CancelToken()
        token.cancel()
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(Cancelled):
                run_scan([tmp], cancel=token)

    def test_empty_folder(self):
        with tempfile.TemporaryDirectory() as tmp:
            data, _ = run_scan([tmp])
            self.assertEqual(data["summary"]["total"], 0)


class ScanSelectedFiles(unittest.TestCase):
    def test_single_and_multiple_files_with_reports_and_failures(self):
        with tempfile.TemporaryDirectory() as tmp, tempfile.TemporaryDirectory() as outdir:
            root = Path(tmp)
            make_tree(root)
            files = [str(root / "a.png"), str(root / "sub" / "b.jpg"), str(root / "bad.jpg"),
                     str(root / "readme.txt"), str(root / "missing.png"), str(root / "A.PNG")]
            csv_path, txt_path = Path(outdir) / "r.csv", Path(outdir) / "r.txt"
            data, events = run_scan(["--csv", str(csv_path), "--txt", str(txt_path)],
                                    params={"files": files, "recursive": True})
            self.assertEqual((data["source"], data["folder"], data["recursive"]), ("files", None, False))
            self.assertEqual(data["selected"], 5)
            self.assertEqual(data["duplicates_ignored"], 1 if sys.platform == "win32" else 0)
            self.assertEqual(data["summary"]["analyzed"], 2)
            by_status = data["summary"]["by_status"]
            self.assertEqual(by_status.get("corrupted"), 1)
            self.assertEqual(by_status.get("unreadable"), 1)  # missing file is reported, not dropped
            self.assertEqual(by_status.get("unsupported"), 1)  # readme.txt
            self.assertEqual(events[-1]["total"], data["selected"])
            with open(csv_path, encoding="utf-8-sig", newline="") as fh:
                self.assertEqual(len(list(csv.DictReader(fh))), data["summary"]["total"])
            self.assertIn("5 selected file(s)", txt_path.read_text(encoding="utf-8"))

    def test_single_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            make_tree(Path(tmp))
            data, _ = run_scan([], params={"files": [str(Path(tmp) / "a.png")]})
            self.assertEqual((data["selected"], data["summary"]["analyzed"]), (1, 1))

    def test_folder_result_has_source_folder(self):
        with tempfile.TemporaryDirectory() as tmp:
            data, _ = run_scan([tmp])
            self.assertEqual((data["source"], data["selected"]), ("folder", None))

    def test_invalid_file_lists(self):
        for params in ({"files": []}, {"files": "a.png"}, {"files": [""]}, {"files": [3]},
                       {"files": ["a.png"], "folder": "."}):
            with self.assertRaises(BackendError, msg=str(params)) as cm:
                run_scan([], params=params)
            self.assertEqual(cm.exception.exit_code, EXIT_INVALID_INPUT)

    def test_cancel_file_list(self):
        token = CancelToken()
        token.cancel()
        with self.assertRaises(Cancelled):
            run_scan([], params={"files": ["a.png"]}, cancel=token)

    def test_many_files_through_cli_params_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            img = Path(tmp) / "a.png"
            Image.new("RGB", (64, 48)).save(img)
            long_dir = Path(tmp) / ("d" * 40) / ("e" * 40)
            long_dir.mkdir(parents=True)
            Image.new("RGB", (64, 48)).save(long_dir / "x.png")
            params = Path(tmp) / "p.json"
            params.write_text(json.dumps({"files": [str(img), str(long_dir / "x.png")] * 3000}), encoding="utf-8")
            r = subprocess.run([sys.executable, "-m", "scripts.engine", "--params-file", str(params), "scan"],
                               cwd=ROOT, capture_output=True, text=True, encoding="utf-8", timeout=120)
            self.assertEqual(r.returncode, 0, r.stderr)
            data = json.loads(r.stdout.splitlines()[-1])["data"]
            self.assertEqual((data["selected"], data["duplicates_ignored"]), (2, 5998))


class ThroughCli(unittest.TestCase):
    def test_subprocess_envelopes(self):
        with tempfile.TemporaryDirectory() as tmp:
            make_tree(Path(tmp))
            r = subprocess.run([sys.executable, "-m", "scripts.engine", "scan", tmp, "--recursive"], cwd=ROOT,
                               capture_output=True, text=True, encoding="utf-8", timeout=60)
            self.assertEqual(r.returncode, 0, r.stderr)
            lines = [json.loads(line) for line in r.stdout.splitlines()]
            self.assertEqual(lines[-1]["type"], "result")
            self.assertEqual(lines[-1]["data"]["summary"]["total"], 3)

    def test_missing_folder_exit_code(self):
        r = subprocess.run([sys.executable, "-m", "scripts.engine", "scan", "Z:\\nope"], cwd=ROOT,
                           capture_output=True, text=True, encoding="utf-8", timeout=60)
        self.assertEqual(r.returncode, 3)
        self.assertEqual(json.loads(r.stdout.splitlines()[-1])["code"], "invalid_input")


if __name__ == "__main__":
    unittest.main()
