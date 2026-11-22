"""Duplicate finder: exact and perceptual detection are tested separately."""
from __future__ import annotations

import argparse
import io
import os
import random
import tempfile
import unittest
from pathlib import Path

from PIL import Image, ImageDraw

from scripts.engine.command import Context
from scripts.engine.commands import duplicates as cmd
from scripts.engine.duplicates import savings
from scripts.engine.duplicates.cache import FingerprintCache
from scripts.engine.duplicates.exact import FileStat, find_exact
from scripts.engine.duplicates.finder import Settings, find_duplicates, validate_threshold
from scripts.engine.duplicates.hashing import sha256_file
from scripts.engine.duplicates.perceptual import distance, fingerprint, max_distance, similarity
from scripts.engine.duplicates.similar import Item, cluster
from scripts.engine.duplicates.sources import collect
from scripts.engine.protocol import BackendError, Cancelled, CancelToken, Emitter

NOOP = lambda *a: None  # noqa: E731
OK = lambda: None  # noqa: E731


def picture(seed: int, size=(320, 240)) -> Image.Image:
    rng = random.Random(seed)
    img = Image.new("RGB", size, (rng.randrange(256),) * 3)
    d = ImageDraw.Draw(img)
    for _ in range(14):
        x, y = rng.randrange(size[0]), rng.randrange(size[1])
        d.rectangle([x, y, x + rng.randrange(20, 120), y + rng.randrange(20, 120)],
                    fill=tuple(rng.randrange(256) for _ in range(3)))
    return img


class Base(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        self.addCleanup(self._tmp.cleanup)

    def save(self, img, rel, **kw) -> Path:
        p = self.root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        img.save(p, **kw)
        return p

    def scan(self, similar=True, threshold=90.0, cache=None, recursive=True, files=None, exclude=None):
        src = collect(None if files else [str(self.root)], files, recursive, exclude, OK)
        return find_duplicates(src, Settings(threshold, similar, cache), OK, NOOP)


class ExactDetection(Base):
    def test_same_content_different_names_and_folders(self):
        a = self.save(picture(1), "a.png")
        for rel in ("copy.png", "sub/other.png", "x/y/renamed.dat.png"):
            (self.root / rel).parent.mkdir(parents=True, exist_ok=True)
            (self.root / rel).write_bytes(a.read_bytes())
        r = self.scan(similar=False)
        self.assertEqual(r["stats"]["exact_groups"], 1)
        self.assertEqual(r["exact_groups"][0]["count"], 4)

    def test_different_extension_same_bytes(self):
        a = self.save(picture(1), "a.png")
        (self.root / "b.jpeg.png").write_bytes(a.read_bytes())
        self.assertEqual(self.scan(similar=False)["stats"]["exact_groups"], 1)

    def test_same_size_different_content_not_exact(self):
        a = self.root / "a.png"
        self.save(picture(1), "a.png")
        data = bytearray(a.read_bytes())
        data[-20] ^= 0xFF  # same size, different content
        (self.root / "b.png").write_bytes(bytes(data))
        r = self.scan(similar=False)
        self.assertEqual(r["stats"]["exact_groups"], 0)
        self.assertEqual(r["stats"]["hashed"], 2)  # size matched, so hashing decided

    def test_unique_sizes_never_hashed(self):
        self.save(picture(1, (100, 80)), "a.png")
        self.save(picture(2, (200, 160)), "b.png")
        self.assertEqual(self.scan(similar=False)["stats"]["hashed"], 0)

    def test_empty_folder_and_empty_files(self):
        self.assertEqual(self.scan()["stats"]["total_files"], 0)
        (self.root / "e1.png").write_bytes(b"")
        (self.root / "e2.png").write_bytes(b"")
        r = self.scan()
        self.assertEqual((r["stats"]["empty_files"], r["stats"]["exact_groups"]), (2, 0))

    def test_corrupted_identical_files_still_exact_but_skipped_for_similarity(self):
        (self.root / "a.png").write_bytes(b"not an image at all")
        (self.root / "b.png").write_bytes(b"not an image at all")
        r = self.scan()
        self.assertEqual(r["stats"]["exact_groups"], 1)
        self.assertEqual(r["similar_groups"], [])
        self.assertEqual(r["skipped"][0]["status"], "corrupted")

    def test_unreadable_file_reported(self):
        a = self.save(picture(1), "a.png")
        (self.root / "b.png").write_bytes(a.read_bytes())
        files = [FileStat(a, 5, 0), FileStat(self.root / "gone.png", 5, 0)]
        res = find_exact(files, FingerprintCache(None), OK, NOOP)
        self.assertEqual(len(res.errors), 1)

    def test_sha256_streaming_matches_hashlib(self):
        import hashlib
        p = self.root / "big.bin"
        p.write_bytes(os.urandom(3 * 1024 * 1024 + 7))
        self.assertEqual(sha256_file(p), hashlib.sha256(p.read_bytes()).hexdigest())


class Perceptual(Base):
    def test_resized_recompressed_and_reformatted_are_similar(self):
        base = picture(7, (640, 480))
        self.save(base, "orig.png")
        self.save(base.resize((320, 240)), "small.png")
        self.save(base, "re.jpg", quality=40)
        self.save(base.resize((800, 600)), "big.webp", quality=70)
        r = self.scan()
        self.assertEqual(r["stats"]["exact_groups"], 0)
        self.assertEqual(len(r["similar_groups"]), 1)
        self.assertEqual(r["similar_groups"][0]["count"], 4)
        for f in r["similar_groups"][0]["files"]:
            self.assertGreaterEqual(f["similarity"], 90.0)

    def test_unrelated_images_not_grouped(self):
        for i in range(6):
            self.save(picture(100 + i), f"u{i}.png")
        self.assertEqual(self.scan()["stats"]["similar_groups"], 0)

    def test_heavy_edit_and_crop_not_grouped(self):
        base = picture(9, (640, 480))
        self.save(base, "a.png")
        self.save(base.crop((0, 0, 200, 480)), "cropped.png")  # different aspect ratio
        self.assertEqual(self.scan()["stats"]["similar_groups"], 0)

    def test_threshold_behavior(self):
        a = picture(11)
        b = a.copy()
        ImageDraw.Draw(b).rectangle([0, 0, 120, 100], fill=(250, 5, 5))
        self.save(a, "a.png")
        self.save(b, "b.png")
        loose = self.scan(threshold=60.0)
        strict = self.scan(threshold=100.0)
        self.assertEqual(strict["stats"]["similar_groups"], 0)
        self.assertGreaterEqual(loose["stats"]["similar_groups"], 1)
        self.assertEqual(max_distance(100.0), 0)
        self.assertEqual(max_distance(90.0), 12)
        self.assertEqual(similarity(0), 100.0)
        with self.assertRaises(ValueError):
            validate_threshold(10)
        with self.assertRaises(ValueError):
            validate_threshold("x")

    def test_exact_copies_not_repeated_as_similar_members(self):
        a = self.save(picture(3), "a.png")
        (self.root / "a_copy.png").write_bytes(a.read_bytes())
        self.save(picture(3).resize((160, 120)), "a_small.png")
        r = self.scan()
        self.assertEqual(r["stats"]["exact_groups"], 1)
        self.assertEqual(r["stats"]["similar_groups"], 1)
        g = r["similar_groups"][0]
        self.assertEqual(g["count"], 2)  # one representative of the exact pair + the small one
        self.assertEqual(sorted(f["identical_copies"] for f in g["files"]), [0, 1])

    def test_flat_images_have_no_fingerprint(self):
        self.save(Image.new("RGB", (50, 50), "white"), "w1.png")
        self.save(Image.new("RGB", (60, 60), "white"), "w2.png")
        r = self.scan()
        self.assertEqual(r["stats"]["similar_groups"], 0)
        self.assertEqual(r["stats"]["flat_images"], 2)

    def test_clustering_does_not_chain(self):
        # a~b (12 bits), b~c (12 bits), a vs c 24 bits apart: c must not join a's group.
        a, b, c = 0, (1 << 12) - 1, ((1 << 12) - 1) ^ (((1 << 12) - 1) << 12)
        self.assertEqual((distance(a, b), distance(b, c), distance(a, c)), (12, 12, 24))
        groups = cluster([Item(0, a, 1.0), Item(1, b, 1.0), Item(2, c, 1.0)], 90.0)
        self.assertEqual([[m.key for m in g] for g in groups], [[0, 1]])

    def test_fingerprint_errors(self):
        (self.root / "bad.png").write_bytes(b"junk")
        from scripts.engine.duplicates.perceptual import FingerprintError
        with self.assertRaises(FingerprintError):
            fingerprint(self.root / "bad.png")
        with self.assertRaises(FingerprintError) as ctx:
            fingerprint(self.root / "missing.png")
        self.assertEqual(ctx.exception.status, "unreadable")


class SourcesAndCache(Base):
    def test_exclude_and_subfolder_option(self):
        self.save(picture(1), "top.png")
        self.save(picture(2), "sub/in.png")
        self.save(picture(3), "skip/out.png")
        self.assertEqual(len(collect([str(self.root)], None, False, None, OK).files), 1)
        self.assertEqual(len(collect([str(self.root)], None, True, None, OK).files), 3)
        ex = collect([str(self.root)], None, True, [str(self.root / "skip")], OK)
        self.assertEqual(sorted(p.name for p in ex.files), ["in.png", "top.png"])

    def test_multiple_folders_and_collection_files(self):
        a = self.save(picture(1), "f1/a.png")
        self.save(picture(1), "f2/a2.png")
        s = collect([str(self.root / "f1"), str(self.root / "f2")], [str(a)], False, None, OK)
        self.assertEqual((len(s.files), s.duplicates), (2, 1))

    def test_bad_inputs(self):
        for args in ((None, None), (["nope_dir"], None), ("x", None), (None, [3])):
            with self.assertRaises(ValueError):
                collect(args[0], args[1], False, None, OK)

    def test_cache_reuse_and_invalidation(self):
        a = self.save(picture(1), "a.png")
        (self.root / "b.png").write_bytes(a.read_bytes())
        db = str(self.root.parent / (self.root.name + "_cache.sqlite"))
        self.addCleanup(lambda: os.path.exists(db) and os.remove(db))
        first = self.scan(cache=db)
        second = self.scan(cache=db)
        self.assertEqual(first["stats"]["hashed"], 2)
        self.assertEqual(second["stats"]["hashed"], 0)
        self.assertGreaterEqual(second["stats"]["cache_hits"], 2)
        a.write_bytes(a.read_bytes() + b"x")  # changed size/mtime -> must be re-hashed
        self.assertEqual(self.scan(cache=db)["stats"]["exact_groups"], 0)

    def test_broken_cache_path_warns_not_fails(self):
        blocker = self.root / "file.txt"
        blocker.write_text("x")
        r = self.scan(cache=str(blocker / "sub" / "c.sqlite"))
        self.assertIn("cache disabled", r["cache_warning"])


class Savings(Base):
    def test_arithmetic(self):
        self.assertEqual(savings.exact_group_bytes(100, 3), 300)
        self.assertEqual(savings.exact_group_savings(100, 3), 200)
        self.assertEqual(savings.exact_group_savings(100, 1), 0)

    def test_keeps_one_copy_and_similar_not_counted_as_savings(self):
        a = self.save(picture(1), "a.png")
        for n in ("b.png", "c.png"):
            (self.root / n).write_bytes(a.read_bytes())
        self.save(picture(1).resize((160, 120)), "small.png")
        size = a.stat().st_size
        r = self.scan()["stats"]
        self.assertEqual(r["exact_bytes"], 3 * size)
        self.assertEqual(r["exact_savings_bytes"], 2 * size)
        self.assertEqual(r["exact_extra_files"], 2)
        self.assertGreater(r["similar_bytes"], 0)


class Scale(Base):
    def test_large_collection(self):
        base = picture(5, (64, 48))
        for i in range(300):
            p = self.root / f"d{i % 10}" / f"{i}.png"
            p.parent.mkdir(exist_ok=True)
            picture(1000 + i, (64, 48)).save(p)
        for i in range(5):  # 5 exact copies of one file
            (self.root / f"copy{i}.png").write_bytes((self.root / "d0" / "0.png").read_bytes())
        r = self.scan()
        self.assertEqual(r["stats"]["total_files"], 305)
        self.assertEqual(r["stats"]["exact_groups"], 1)
        del base


class CancelAndCommand(Base):
    def test_cancel_during_scan(self):
        for i in range(5):
            self.save(picture(i), f"{i}.png")
        token = CancelToken()
        calls = {"n": 0}

        def check():
            calls["n"] += 1
            if calls["n"] > 3:
                token.cancel()
            token.check()

        src = collect([str(self.root)], None, True, None, OK)
        with self.assertRaises(Cancelled):
            find_duplicates(src, Settings(), check, NOOP)

    def run_cmd(self, params, argv=()):
        p = argparse.ArgumentParser()
        cmd.add_arguments(p)
        out = io.StringIO()
        return cmd.run(p.parse_args(list(argv)), Context(Emitter(out), CancelToken(), params)), out.getvalue()

    def test_command_result_and_progress(self):
        a = self.save(picture(1), "a.png")
        (self.root / "b.png").write_bytes(a.read_bytes())
        data, out = self.run_cmd({"folders": [str(self.root)], "recursive": True})
        self.assertEqual(data["stats"]["exact_groups"], 1)
        self.assertIn('"progress"', out)

    def test_command_invalid_input(self):
        for params in ({}, {"folders": [str(self.root)], "threshold": 5}):
            with self.assertRaises(BackendError):
                self.run_cmd(params)

    def test_originals_never_modified(self):
        a = self.save(picture(1), "a.png")
        (self.root / "b.png").write_bytes(a.read_bytes())
        before = {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.root.iterdir()}
        self.scan()
        after = {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.root.iterdir()}
        self.assertEqual(before, after)


if __name__ == "__main__":
    unittest.main()
