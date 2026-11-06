"""Perceptual fingerprint: a 128-bit difference hash (dHash).

The image is flattened (alpha composited on white), converted to greyscale and
shrunk to 9x9. The first 64 bits say "this pixel is brighter than its right
neighbour", the second 64 bits "brighter than the pixel below". Resizing,
recompression, format changes and mild colour shifts barely change those
gradients; different pictures differ in about half the bits. Pure functions
plus one Pillow loader; the original file is only opened for reading.

Not a proof of identity: similar != identical. Flat images (almost no detail)
carry no usable fingerprint and give `None`.
"""
from __future__ import annotations

import warnings
from pathlib import Path

from PIL import Image, ImageOps

from ..imaging.metadata import no_pixel_limit

HASH_BITS = 128
GRID = 9
MIN_DETAIL = 3.0  # std-dev of the 9x9 greyscale; below it the hash would be meaningless


class FingerprintError(Exception):
    """The file could not be decoded; `status` is "unreadable" or "corrupted"."""

    def __init__(self, status: str, message: str) -> None:
        super().__init__(message)
        self.status = status
        self.message = message


def distance(a: int, b: int) -> int:
    return (a ^ b).bit_count()


def similarity(dist: int) -> float:
    """0..100; 100 = identical fingerprints."""
    return round(100.0 * (1.0 - dist / HASH_BITS), 1)


def max_distance(min_similarity: float) -> int:
    """Largest Hamming distance that still meets `min_similarity` (percent)."""
    return int(HASH_BITS * (1.0 - min_similarity / 100.0) + 1e-9)


def hash_pixels(pixels: list[int]) -> int | None:
    """`pixels`: GRID*GRID greyscale values, row-major. None when the image is flat."""
    mean = sum(pixels) / len(pixels)
    if (sum((p - mean) ** 2 for p in pixels) / len(pixels)) ** 0.5 < MIN_DETAIL:
        return None
    value = 0
    for y in range(GRID - 1):
        for x in range(GRID - 1):
            value = (value << 1) | (pixels[y * GRID + x] > pixels[y * GRID + x + 1])
    for y in range(GRID - 1):
        for x in range(GRID - 1):
            value = (value << 1) | (pixels[y * GRID + x] > pixels[(y + 1) * GRID + x])
    return value


def fingerprint(path: Path) -> int | None:
    """dHash of the displayed first frame; None for a flat image. Raises FingerprintError."""
    try:
        with no_pixel_limit(), Image.open(path) as opened:
            opened.draft("RGB", (64, 64))  # JPEG: decode at reduced scale, far cheaper
            img = ImageOps.exif_transpose(opened)
            if img.mode in ("RGBA", "LA", "P") or "transparency" in img.info:
                rgba = img.convert("RGBA")
                img = Image.new("RGB", rgba.size, "white")
                img.paste(rgba, mask=rgba.getchannel("A"))
            small = img.convert("L").resize((GRID, GRID), Image.Resampling.BOX)
            return hash_pixels(list(small.getdata()))
    except OSError as exc:
        raise FingerprintError("unreadable" if exc.errno is not None else "corrupted", str(exc)) from exc
    except Exception as exc:  # Pillow raises many types on damaged data
        raise FingerprintError("corrupted", f"{type(exc).__name__}: {exc}") from exc
