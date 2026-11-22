"""Report modules operate on plain dictionaries only; no images involved."""
from __future__ import annotations

import csv
import tempfile
import unittest
from pathlib import Path

from scripts.engine.reports.csv_report import COLUMNS, write_csv
from scripts.engine.reports.summary import summarize
from scripts.engine.reports.txt_report import human_size, write_txt

OK = {"path": "C:\\p\\caf\u00e9, \"x\".png", "status": "ok", "format": "PNG", "width": 1920, "height": 1080,
      "orientation": "landscape", "aspect_ratio": 1.778, "aspect_label": "16:9", "megapixels": 2.07,
      "resolution": "Full HD", "resolution_match": "exact", "resolution_tier": "Full HD", "file_size": 2048,
      "exif_rotated": False, "error": None}
BAD = {"path": "C:\\p\\bad.jpg", "status": "corrupted", "error": "not a valid image", "file_size": 10}
RECORDS = [OK, BAD]


class Summary(unittest.TestCase):
    def test_counts(self):
        s = summarize(RECORDS)
        self.assertEqual((s["total"], s["analyzed"], s["failed"]), (2, 1, 1))
        self.assertEqual(s["by_resolution"], {"Full HD": 1})
        self.assertEqual(s["by_status"], {"corrupted": 1, "ok": 1})
        self.assertEqual(s["total_bytes"], 2058)

    def test_empty(self):
        self.assertEqual(summarize([])["total"], 0)


class Csv(unittest.TestCase):
    def test_round_trip_includes_failures_and_escapes(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "nested" / "r.csv"
            write_csv(RECORDS, out)
            self.assertFalse(out.with_name("r.csv.tmp").exists())
            with open(out, encoding="utf-8-sig", newline="") as fh:
                rows = list(csv.DictReader(fh))
            self.assertEqual(list(rows[0]), list(COLUMNS))
            self.assertEqual(rows[0]["path"], OK["path"])
            self.assertEqual((rows[0]["width"], rows[0]["error"]), ("1920", ""))
            self.assertEqual((rows[1]["status"], rows[1]["error"], rows[1]["width"]), ("corrupted", "not a valid image", ""))


class Txt(unittest.TestCase):
    def test_contains_sections(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "r.txt"
            write_txt(RECORDS, summarize(RECORDS), out, "C:\\p", True, "2026-01-01T00:00:00")
            text = out.read_text(encoding="utf-8")
            for needle in ("Image Resolution Report", "Recursive: yes", "Full HD", "1920x1080", "16:9",
                           "Problem files", "[corrupted]", "not a valid image"):
                self.assertIn(needle, text)

    def test_human_size(self):
        self.assertEqual((human_size(None), human_size(512), human_size(2048)), ("?", "512 B", "2.0 KB"))
        self.assertEqual(human_size(5 * 1024**3), "5.0 GB")
        self.assertEqual(human_size(5000 * 1024**3), "5000.0 GB")


if __name__ == "__main__":
    unittest.main()
