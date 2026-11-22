"""Classification and aspect modules: pure logic, tested directly."""
from __future__ import annotations

import unittest

from scripts.engine.imaging.aspect import aspect_label, aspect_ratio, orientation
from scripts.engine.imaging.classification import classify


class Standards(unittest.TestCase):
    def test_exact_standards(self):
        expected = {
            (1280, 720): "HD",
            (1920, 1080): "Full HD",
            (2048, 1080): "DCI 2K",
            (2560, 1440): "QHD / 1440p",
            (3840, 2160): "UHD 4K",
            (4096, 2160): "DCI 4K",
            (5120, 2880): "5K",
            (7680, 4320): "8K",
        }
        for (w, h), name in expected.items():
            c = classify(w, h)
            self.assertEqual((c.label, c.standard, c.match), (name, name, "exact"), (w, h))

    def test_dci_distinct_from_neighbours(self):
        self.assertNotEqual(classify(2048, 1080).label, classify(2560, 1440).label)
        self.assertNotEqual(classify(3840, 2160).label, classify(4096, 2160).label)

    def test_portrait_matches_landscape(self):
        for w, h in ((1080, 1920), (2160, 3840), (2160, 4096), (1440, 2560)):
            self.assertEqual(classify(w, h), classify(h, w))
        self.assertEqual(classify(2160, 3840).label, "UHD 4K")

    def test_near_match_is_marked_approximate(self):
        c = classify(1920, 1088)  # codec-padded Full HD
        self.assertEqual((c.label, c.standard, c.match), ("Full HD (approx.)", "Full HD", "near"))

    def test_width_alone_is_not_enough(self):
        self.assertEqual(classify(1920, 600).match, "none")
        self.assertEqual(classify(3840, 1000).match, "none")


class NonStandard(unittest.TestCase):
    def test_custom_uses_largest_fitting_tier(self):
        c = classify(3000, 2000)
        self.assertEqual((c.label, c.standard, c.match), ("Custom (>= QHD / 1440p)", None, "none"))

    def test_below_hd(self):
        self.assertEqual(classify(640, 480).label, "Custom (below HD)")
        self.assertEqual(classify(1, 1).tier, "below HD")

    def test_panorama_never_gets_a_standard_name(self):
        c = classify(12000, 1000)
        self.assertEqual((c.label, c.standard, c.match), ("Panorama (>= HD)", None, "none"))
        self.assertTrue(classify(1000, 12000).label.startswith("Panorama"))

    def test_square(self):
        self.assertEqual(classify(3000, 3000).label, "Custom (>= QHD / 1440p)")

    def test_above_8k(self):
        for w, h in ((16000, 9000), (9000, 16000), (8192, 4608)):
            c = classify(w, h)
            self.assertEqual((c.label, c.standard), ("Above 8K", None), (w, h))
        self.assertEqual(classify(8192, 4320).label, "Custom (>= 8K)")  # DCI 8K is not "Above 8K"

    def test_rejects_non_positive(self):
        for w, h in ((0, 10), (10, 0), (-1, 5)):
            with self.assertRaises(ValueError):
                classify(w, h)


class Aspect(unittest.TestCase):
    def test_labels(self):
        self.assertEqual(aspect_label(1920, 1080), "16:9")
        self.assertEqual(aspect_label(1080, 1920), "9:16")
        self.assertEqual(aspect_label(1000, 1000), "1:1")
        self.assertEqual(aspect_label(4000, 3000), "4:3")
        self.assertEqual(aspect_label(4096, 2160), "1.90:1")
        self.assertEqual(aspect_label(2160, 4096), "1:1.90")

    def test_ratio_and_orientation(self):
        self.assertEqual(aspect_ratio(1920, 1080), 1.778)
        self.assertEqual(aspect_ratio(1080, 1920), 0.562)
        self.assertEqual(orientation(2, 1), "landscape")
        self.assertEqual(orientation(1, 2), "portrait")
        self.assertEqual(orientation(2, 2), "square")


if __name__ == "__main__":
    unittest.main()
