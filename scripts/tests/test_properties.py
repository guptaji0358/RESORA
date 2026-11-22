"""Advanced image properties: module behaviour plus the `properties` command."""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import tempfile
import unittest
from pathlib import Path

from PIL import Image

from scripts.engine.command import Context
from scripts.engine.commands import properties as cmd
from scripts.engine.imaging.properties import read_properties
from scripts.engine.protocol import BackendError, CancelToken, Emitter


def exif_bytes(orientation: int | None = None, make: str | None = None, dpi=None) -> bytes:
    exif = Image.Exif()
    if orientation:
        exif[0x0112] = orientation
    if make:
        exif[0x010F] = make
    return exif.tobytes()


def run_cmd(paths, params=None):
    parser = argparse.ArgumentParser()
    cmd.add_arguments(parser)
    out = io.StringIO()
    ctx = Context(emit=Emitter(out), cancel=CancelToken(), params=params or {})
    return cmd.run(parser.parse_args(paths), ctx), [json.loads(l) for l in out.getvalue().splitlines()]


class Properties(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self._tmp.name)

    def tearDown(self):
        self._tmp.cleanup()

    def test_basic_png_without_metadata_reports_unavailable_not_zero(self):
        p = self.dir / "plain.png"
        Image.new("RGB", (1920, 1080)).save(p)
        r = read_properties(p)
        self.assertEqual((r["status"], r["format"], r["megapixels"]), ("ok", "PNG", 2.07))
        self.assertEqual(r["aspect_label"], "16:9")
        self.assertEqual(r["layout"], "landscape")
        self.assertIsNone(r["dpi"])
        self.assertIsNone(r["color"]["icc_profile"])
        self.assertFalse(r["exif"]["available"])
        self.assertIsNone(r["dimensions"]["orientation_tag"])
        self.assertEqual(r["color"]["mode"], "RGB")
        self.assertEqual(r["color"]["bits_per_channel"], 8)
        self.assertFalse(r["color"]["has_alpha"])
        self.assertEqual(r["file_size"], p.stat().st_size)

    def test_dpi_from_header(self):
        p = self.dir / "d.jpg"
        Image.new("RGB", (100, 50)).save(p, dpi=(300, 300))
        self.assertEqual(read_properties(p)["dpi"], {"x": 300.0, "y": 300.0, "source": "file header"})

    def test_exif_orientation_swaps_displayed_dimensions(self):
        p = self.dir / "o.jpg"
        Image.new("RGB", (400, 200)).save(p, exif=exif_bytes(orientation=6, make="TestCam"))
        r = read_properties(p)
        d = r["dimensions"]
        self.assertEqual((d["stored_width"], d["stored_height"], d["width"], d["height"]), (400, 200, 200, 400))
        self.assertEqual((d["orientation_tag"], d["orientation_label"], d["exif_rotated"]), (6, "Rotated 90° clockwise", True))
        self.assertEqual(r["layout"], "portrait")
        self.assertTrue(r["exif"]["available"])
        self.assertIn(("Make", "TestCam"), [(f["label"], f["value"]) for f in r["exif"]["fields"]])

    def test_alpha_and_mode(self):
        p = self.dir / "a.png"
        Image.new("RGBA", (10, 10)).save(p)
        c = read_properties(p)["color"]
        self.assertTrue(c["has_alpha"])
        self.assertEqual(c["channels"], 4)

    def test_icc_profile_present(self):
        from PIL import ImageCms

        p = self.dir / "icc.png"
        profile = ImageCms.ImageCmsProfile(ImageCms.createProfile("sRGB")).tobytes()
        Image.new("RGB", (10, 10)).save(p, icc_profile=profile)
        icc = read_properties(p)["color"]["icc_profile"]
        self.assertGreater(icc["size"], 0)
        self.assertTrue(icc["description"])

    def test_corrupted_and_missing_files_are_entries_not_crashes(self):
        bad = self.dir / "bad.jpg"
        bad.write_bytes(b"junk")
        r = read_properties(bad)
        self.assertEqual(r["status"], "corrupted")
        self.assertEqual(r["file_size"], 4)
        self.assertNotIn("color", r)
        missing = read_properties(self.dir / "nope.png")
        self.assertNotEqual(missing["status"], "ok")
        self.assertIsNone(missing["file_size"])

    def test_damaged_exif_does_not_fail_image(self):
        p = self.dir / "x.jpg"
        Image.new("RGB", (20, 20)).save(p)
        data = bytearray(p.read_bytes())
        # Splice a bogus APP1 EXIF segment right after SOI.
        bogus = b"Exif\x00\x00II*\x00\xff\xff\xff\xff"
        seg = b"\xff\xe1" + (len(bogus) + 2).to_bytes(2, "big") + bogus
        p.write_bytes(bytes(data[:2]) + seg + bytes(data[2:]))
        r = read_properties(p)
        self.assertEqual(r["status"], "ok")
        self.assertFalse(r["exif"]["available"])

    def test_file_is_never_modified(self):
        p = self.dir / "ro.jpg"
        Image.new("RGB", (64, 64)).save(p, exif=exif_bytes(orientation=3))
        before = hashlib.sha256(p.read_bytes()).hexdigest()
        read_properties(p)
        self.assertEqual(hashlib.sha256(p.read_bytes()).hexdigest(), before)


class PropertiesCommand(unittest.TestCase):
    def test_multiple_paths_duplicates_and_progress(self):
        with tempfile.TemporaryDirectory() as tmp:
            a, b = Path(tmp) / "a.png", Path(tmp) / "b.png"
            Image.new("RGB", (8, 8)).save(a)
            Image.new("RGB", (16, 8)).save(b)
            data, events = run_cmd([str(a), str(b), str(a)])
            self.assertEqual([i["name"] for i in data["images"]], ["a.png", "b.png"])
            self.assertEqual(data["duplicates_ignored"], 1)
            self.assertEqual(events[-1]["done"], 2)

    def test_params_file_list_and_empty_request(self):
        with tempfile.TemporaryDirectory() as tmp:
            a = Path(tmp) / "a.png"
            Image.new("RGB", (8, 8)).save(a)
            data, _ = run_cmd([], params={"files": [str(a)]})
            self.assertEqual(len(data["images"]), 1)
        with self.assertRaises(BackendError):
            run_cmd([])


if __name__ == "__main__":
    unittest.main()
