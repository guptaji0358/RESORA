"""Resolution analytics: pure calculations, the `analytics` command, scan integration."""
from __future__ import annotations

import argparse
import io
import json
import tempfile
import unittest
from pathlib import Path

from PIL import Image

from scripts.engine.command import Context
from scripts.engine.commands import analytics as cmd, scan
from scripts.engine.imaging.analytics import CATEGORY_ORDER, compute_analytics
from scripts.engine.imaging.analyzer import analyze_file
from scripts.engine.imaging.classification import classify
from scripts.engine.protocol import BackendError, CancelToken, Emitter


def rec(path, w, h, size=1000, fmt="PNG", status="ok", **extra):
    base = {"path": path, "status": status, "error": None if status == "ok" else "bad",
            "format": fmt if status == "ok" else None,
            "width": w if status == "ok" else None, "height": h if status == "ok" else None, "file_size": size}
    if status == "ok" and w and h and w > 0 and h > 0:
        c = classify(w, h)
        base.update(resolution=c.label, resolution_match=c.match, resolution_tier=c.tier)
    base.update(extra)
    return base


def by_name(rows):
    return {r["name"]: r for r in rows}


class Calculations(unittest.TestCase):
    def setUp(self):
        self.a = compute_analytics([
            rec("a.png", 1920, 1080, 2000, "PNG"),
            rec("b.jpg", 3840, 2160, 6000, "JPEG"),
            rec("c.jpg", 1080, 1920, 4000, "MPO"),
            rec("d.webp", 1000, 1000, 1000, "WEBP"),
            rec("bad.jpg", None, None, 500, status="corrupted"),
        ])

    def test_collection_and_storage(self):
        c = self.a["collection"]
        self.assertEqual((c["total"], c["analyzed"], c["failed"]), (5, 4, 1))
        self.assertEqual(c["failed_by_status"], {"corrupted": 1})
        self.assertEqual(c["total_bytes"], 13000)  # analyzed only; the failed file's 500 B is separate
        self.assertEqual(c["failed_bytes"], 500)
        self.assertEqual(c["average_bytes"], 3250.0)
        self.assertEqual((c["largest"]["name"], c["smallest"]["name"]), ("b.jpg", "d.webp"))

    def test_resolution_averages_and_extremes(self):
        r = self.a["resolution"]
        self.assertEqual(r["average_width"], (1920 + 3840 + 1080 + 1000) / 4)
        self.assertEqual(r["average_height"], (1080 + 2160 + 1920 + 1000) / 4)
        total_px = 1920 * 1080 + 3840 * 2160 + 1080 * 1920 + 1000 * 1000
        self.assertEqual(r["total_pixels"], total_px)
        self.assertAlmostEqual(r["total_megapixels"], total_px / 1e6, places=3)
        self.assertAlmostEqual(r["average_megapixels"], total_px / 4 / 1e6, places=3)
        self.assertNotAlmostEqual(r["total_megapixels"], r["average_megapixels"])
        self.assertEqual((r["highest"]["name"], r["highest"]["width"], r["highest"]["height"]), ("b.jpg", 3840, 2160))
        self.assertEqual(r["lowest"]["name"], "d.webp")

    def test_most_common_prefers_count_then_pixels(self):
        a = compute_analytics([rec("1", 100, 100), rec("2", 100, 100), rec("3", 200, 200), rec("4", 200, 200), rec("5", 50, 50)])
        m = a["resolution"]["most_common"]
        self.assertEqual((m["width"], m["height"], m["count"], m["tied"]), (200, 200, 2, 2))

    def test_extremes_ties_are_deterministic_and_counted(self):
        a = compute_analytics([rec("z.png", 10, 10), rec("a.png", 10, 10), rec("m.png", 20, 20)])
        self.assertEqual(a["resolution"]["lowest"]["name"], "a.png")
        self.assertEqual(a["resolution"]["lowest"]["tied"], 2)
        self.assertEqual(a["resolution"]["highest"]["tied"], 1)

    def test_category_distribution_reuses_classifier(self):
        cats = by_name(self.a["categories"])
        self.assertEqual([r["name"] for r in self.a["categories"]], list(CATEGORY_ORDER))
        self.assertEqual(cats["Full HD"]["count"], 2)  # 1920x1080 and portrait 1080x1920
        self.assertEqual(cats["UHD 4K"]["count"], 1)
        self.assertEqual(cats["Below HD / Other"]["count"], 1)  # 1000x1000 matches no standard
        self.assertEqual(cats["Full HD"]["percent"], 50.0)
        self.assertEqual(sum(c["count"] for c in self.a["categories"]), 4)

    def test_above_8k_and_all_buckets_present(self):
        a = compute_analytics([rec("x", 20000, 12000), rec("y", 7680, 4320), rec("z", 1280, 720), rec("w", 640, 480)])
        cats = by_name(a["categories"])
        self.assertEqual((cats["Above 8K"]["count"], cats["8K"]["count"], cats["HD"]["count"]), (1, 1, 1))
        self.assertEqual(cats["Below HD / Other"]["count"], 1)
        self.assertEqual(len(a["categories"]), 10)

    def test_orientation(self):
        o = by_name(self.a["orientation"])
        self.assertEqual((o["landscape"]["count"], o["portrait"]["count"], o["square"]["count"]), (2, 1, 1))
        self.assertEqual(o["square"]["percent"], 25.0)

    def test_format_normalisation(self):
        f = by_name(self.a["formats"])
        self.assertEqual(f["JPEG"]["count"], 2)  # JPEG + MPO
        self.assertEqual(f["WebP"]["count"], 1)
        self.assertEqual(f["JPEG"]["bytes"], 10000)
        self.assertEqual(f["JPEG"]["percent"], 50.0)
        self.assertEqual(f["JPEG"]["bytes_percent"], round(10000 * 100 / 13000, 2))
        other = compute_analytics([rec("a", 10, 10, fmt="ICO"), rec("b", 10, 10, fmt="PPM"), rec("c", 10, 10, fmt="PNG")])
        self.assertEqual([r["name"] for r in other["formats"]], ["PNG", "Other"])
        self.assertEqual(other["other_formats"], {"ICO": 1, "PPM": 1})

    def test_empty_collection(self):
        a = compute_analytics([])
        self.assertEqual((a["collection"]["total"], a["denominator"]), (0, 0))
        self.assertIsNone(a["resolution"]["average_width"])
        self.assertIsNone(a["resolution"]["highest"])
        self.assertIsNone(a["collection"]["average_bytes"])
        self.assertTrue(all(c["count"] == 0 and c["percent"] is None for c in a["categories"]))

    def test_all_failed(self):
        a = compute_analytics([rec("a", 0, 0, status="corrupted"), rec("b", 0, 0, status="unsupported")])
        self.assertEqual((a["collection"]["analyzed"], a["collection"]["failed"]), (0, 2))
        self.assertIsNone(a["resolution"]["average_megapixels"])
        self.assertEqual(a["formats"], [])

    def test_invalid_and_missing_metadata_are_failed_not_zero(self):
        a = compute_analytics([
            rec("ok.png", 100, 100),
            {"path": "zero.png", "status": "ok", "width": 0, "height": 50, "format": "PNG", "file_size": 5},
            {"path": "none.png", "status": "ok", "width": None, "height": None, "format": "PNG"},
            {"path": "neg.png", "status": "ok", "width": -3, "height": 4, "format": "PNG"},
        ])
        self.assertEqual((a["collection"]["analyzed"], a["collection"]["failed"]), (1, 3))
        self.assertEqual(a["collection"]["failed_by_status"], {"invalid_metadata": 3})
        self.assertEqual(a["resolution"]["average_width"], 100.0)
        self.assertEqual(len(a["failures"]), 3)

    def test_missing_file_size_is_unknown_not_zero(self):
        a = compute_analytics([rec("a", 10, 10, size=None), rec("b", 10, 10, size=100)])
        c = a["collection"]
        self.assertEqual((c["size_known"], c["size_unknown"], c["total_bytes"], c["average_bytes"]), (1, 1, 100, 100.0))
        self.assertEqual(c["smallest"]["name"], "b")

    def test_duplicate_paths_counted_once(self):
        a = compute_analytics([rec("a.png", 10, 10), rec("a.png", 10, 10)], duplicates_ignored=1)
        self.assertEqual((a["collection"]["total"], a["collection"]["duplicates_ignored"]), (1, 2))

    def test_large_collection_no_overflow(self):
        n = 20000
        px = 40000 * 30000  # 1.2 gigapixel per image
        a = compute_analytics(rec(f"i{i}.tif", 40000, 30000, size=3 * 1024 ** 3) for i in range(n))
        self.assertEqual(a["resolution"]["total_pixels"], px * n)
        self.assertGreater(a["resolution"]["total_pixels"], 2 ** 32)
        self.assertEqual(a["collection"]["total_bytes"], 3 * 1024 ** 3 * n)
        self.assertEqual(a["categories"][-2]["count"], n)  # Above 8K
        self.assertEqual(a["resolution"]["highest"]["tied"], n)

    def test_failures_are_capped(self):
        a = compute_analytics(rec(f"b{i}", 0, 0, status="corrupted") for i in range(120))
        self.assertEqual((len(a["failures"]), a["failures_truncated"], a["collection"]["failed"]), (50, True, 120))

    def test_json_serialisable(self):
        json.dumps(self.a)


def run_cmd(argv, params=None):
    p = argparse.ArgumentParser()
    cmd.add_arguments(p)
    out = io.StringIO()
    ctx = Context(emit=Emitter(out), cancel=CancelToken(), params=params or {})
    return cmd.run(p.parse_args(argv), ctx), [json.loads(line) for line in out.getvalue().splitlines()]


class CommandTests(unittest.TestCase):
    def setUp(self):
        self._t = tempfile.TemporaryDirectory()
        self.d = Path(self._t.name)
        Image.new("RGB", (1920, 1080)).save(self.d / "a.png")
        Image.new("RGB", (600, 900)).save(self.d / "b.jpg")
        (self.d / "bad.png").write_bytes(b"junk")
        (self.d / "sub").mkdir()
        Image.new("RGB", (3840, 2160)).save(self.d / "sub" / "c.png")

    def tearDown(self):
        self._t.cleanup()

    def test_folder_and_failed_images(self):
        data, events = run_cmd([str(self.d)])
        c = data["analytics"]["collection"]
        self.assertEqual((c["total"], c["analyzed"], c["failed"]), (3, 2, 1))
        self.assertEqual(events[-1]["type"], "progress")
        data, _ = run_cmd([str(self.d), "--recursive"])
        self.assertEqual(data["analytics"]["collection"]["analyzed"], 3)

    def test_file_list_with_duplicates(self):
        files = [str(self.d / "a.png"), str(self.d / "a.png"), str(self.d / "missing.png")]
        data, _ = run_cmd([], {"files": files})
        a = data["analytics"]
        self.assertEqual((a["collection"]["total"], a["collection"]["analyzed"], data["duplicates_ignored"]), (2, 1, 1))
        self.assertEqual(sum(a["collection"]["failed_by_status"].values()), 1)

    def test_matches_analyze_file(self):
        data, _ = run_cmd([], {"files": [str(self.d / "a.png")]})
        r = analyze_file(self.d / "a.png")
        self.assertEqual(data["analytics"]["resolution"]["highest"]["resolution"], r.resolution)
        self.assertEqual(data["analytics"]["collection"]["total_bytes"], r.file_size)

    def test_input_errors(self):
        with self.assertRaises(BackendError):
            run_cmd([])
        with self.assertRaises(BackendError):
            run_cmd([str(self.d / "nope")])

    def test_scan_embeds_same_analytics_and_txt_section_is_opt_in(self):
        p = argparse.ArgumentParser()
        scan.add_arguments(p)

        def go(argv):
            ctx = Context(emit=Emitter(io.StringIO()), cancel=CancelToken(), params={})
            return scan.run(p.parse_args(argv), ctx)

        out = Path(tempfile.mkdtemp())
        plain, withsec = out / "p.txt", out / "s.txt"
        data = go([str(self.d), "--recursive", "--txt", str(plain)])
        self.assertEqual(data["analytics"], run_cmd([str(self.d), "--recursive"])[0]["analytics"])
        go([str(self.d), "--recursive", "--txt", str(withsec), "--txt-analytics"])
        base = plain.read_text(encoding="utf-8")
        text = withsec.read_text(encoding="utf-8")
        self.assertNotIn("Analytics summary", base)
        self.assertIn("Analytics summary", text)
        self.assertIn("Highest resolution:  3840x2160", text)
        # Timestamps differ between runs, so compare everything after the header block.
        self.assertIn(base.split("\n\n", 1)[1].rstrip("\n"), text)


if __name__ == "__main__":
    unittest.main()
