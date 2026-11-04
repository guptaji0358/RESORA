"""Discovery, metadata and per-file analysis, against real generated images."""
from __future__ import annotations

import hashlib
import tempfile
import unittest
from pathlib import Path

from PIL import Image

from scripts.engine.imaging.analyzer import analyze_file
from scripts.engine.imaging.discovery import discover
from scripts.engine.imaging.metadata import ImageReadError, candidate_extensions, read_metadata, supported_extensions
from scripts.engine.protocol import Cancelled


def make(path: Path, size=(40, 20), fmt=None, **save) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.new("RGB", size, (10, 20, 30)).save(path, fmt, **save)
    return path


class Discovery(unittest.TestCase):
    def test_recursive_flag_and_non_images(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            make(root / "a.png")
            make(root / "sub" / "deep" / "b.JPG")
            (root / "notes.txt").write_text("x")
            ext = candidate_extensions()
            flat = discover(root, False, ext)
            self.assertEqual([p.name for p in flat.files], ["a.png"])
            self.assertEqual(flat.skipped, 1)
            deep = discover(root, True, ext)
            self.assertEqual(sorted(p.name for p in deep.files), ["a.png", "b.JPG"])

    def test_cancel_propagates(self):
        def stop():
            raise Cancelled()

        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(Cancelled):
                discover(Path(tmp), True, {".png"}, stop)

    def test_does_not_import_pillow(self):
        import scripts.engine.imaging.discovery as mod

        self.assertNotIn("PIL", vars(mod))


class Metadata(unittest.TestCase):
    def test_common_formats(self):
        with tempfile.TemporaryDirectory() as tmp:
            for name, fmt in (("a.jpg", "JPEG"), ("a.png", "PNG"), ("a.webp", "WEBP"), ("a.bmp", "BMP"),
                              ("a.tif", "TIFF"), ("a.gif", "GIF")):
                m = read_metadata(make(Path(tmp) / name, (64, 32)))
                self.assertEqual((m.format, m.width, m.height), (fmt, 64, 32), name)

    def test_supported_extensions_cover_required_formats(self):
        self.assertTrue({".jpg", ".jpeg", ".png", ".webp", ".bmp", ".tif", ".tiff", ".gif"} <= supported_extensions())

    def test_exif_orientation_swaps_dimensions(self):
        with tempfile.TemporaryDirectory() as tmp:
            for orientation, swapped in ((1, False), (3, False), (6, True), (8, True), (5, True)):
                exif = Image.Exif()
                exif[0x0112] = orientation
                path = make(Path(tmp) / f"o{orientation}.jpg", (60, 30), exif=exif)
                m = read_metadata(path)
                self.assertEqual((m.width, m.height, m.exif_rotated), (30, 60, True) if swapped else (60, 30, False))

    def test_animated_gif_reports_primary_frame(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "anim.gif"
            frames = [Image.new("RGB", (50, 40), c) for c in ("red", "green", "blue")]
            frames[0].save(path, save_all=True, append_images=frames[1:])
            m = read_metadata(path)
            self.assertEqual((m.width, m.height), (50, 40))

    def test_large_image_is_not_rejected_as_decompression_bomb(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = make(Path(tmp) / "huge.png", (20000, 10000))  # 200 MP, above Pillow's 2x bomb limit
            self.assertEqual(read_metadata(path).width, 20000)
            self.assertIsNotNone(Image.MAX_IMAGE_PIXELS)  # limit restored


class Failures(unittest.TestCase):
    def test_statuses(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "garbage.jpg").write_bytes(b"not an image at all")
            (root / "empty.png").write_bytes(b"")
            (root / "photo.heic").write_bytes(b"\x00\x00\x00\x18ftypheic")
            good = make(root / "good.png")
            truncated = root / "trunc.png"
            truncated.write_bytes(good.read_bytes()[:12])
            self.assertEqual(analyze_file(root / "garbage.jpg").status, "corrupted")
            self.assertEqual(analyze_file(root / "empty.png").status, "corrupted")
            self.assertEqual(analyze_file(truncated).status, "corrupted")
            self.assertEqual(analyze_file(root / "photo.heic").status, "unsupported")
            missing = analyze_file(root / "gone.png")
            self.assertEqual(missing.status, "unreadable")
            self.assertIsNone(missing.file_size)
            self.assertTrue(analyze_file(root / "garbage.jpg").error)

    def test_read_metadata_raises_typed_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            bad = Path(tmp) / "x.png"
            bad.write_bytes(b"zzz")
            with self.assertRaises(ImageReadError) as cm:
                read_metadata(bad)
            self.assertEqual(cm.exception.status, "corrupted")


class Analyze(unittest.TestCase):
    def test_record_fields_and_source_untouched(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = make(Path(tmp) / "p.png", (1080, 1920))
            before = hashlib.sha256(path.read_bytes()).hexdigest()
            r = analyze_file(path)
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), before)
            self.assertEqual((r.status, r.format, r.width, r.height), ("ok", "PNG", 1080, 1920))
            self.assertEqual((r.orientation, r.aspect_label, r.resolution), ("portrait", "9:16", "Full HD"))
            self.assertEqual(r.path, str(path))
            self.assertEqual(r.file_size, path.stat().st_size)


if __name__ == "__main__":
    unittest.main()
