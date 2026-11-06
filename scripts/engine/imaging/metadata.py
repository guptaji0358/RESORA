"""Header-only image metadata via Pillow. Opens files read-only and never
decodes pixel data; sources are never modified.

Multi-frame images (animated GIF/WebP, multi-page TIFF, MPO): the primary
(first) frame's dimensions are reported. Frames are deliberately not counted or
seeked, since that can require decoding.
"""
from __future__ import annotations

import functools
import warnings
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator

from PIL import Image, UnidentifiedImageError

from .records import STATUS_CORRUPTED, STATUS_UNREADABLE, STATUS_UNSUPPORTED

EXIF_ORIENTATION_TAG = 0x0112
SWAPPING_ORIENTATIONS = {5, 6, 7, 8}  # transpose / 90-degree rotations
STUB_FORMATS = {"BUFR", "GRIB", "HDF5"}  # Pillow identifies these but cannot give real image sizes
# Common image extensions Pillow may lack a decoder for; scanned so they are reported as unsupported.
KNOWN_EXTRA_EXTENSIONS = frozenset({".avif", ".heic", ".heif", ".jxl"})


class ImageReadError(Exception):
    """A file that could not be analysed; `status` is one of the records.STATUS_* values."""

    def __init__(self, status: str, message: str) -> None:
        super().__init__(message)
        self.status = status
        self.message = message


@dataclass(frozen=True)
class ImageMetadata:
    format: str
    width: int  # displayed dimensions (EXIF orientation applied)
    height: int
    exif_rotated: bool


@functools.lru_cache(maxsize=1)
def supported_extensions() -> frozenset[str]:
    """Extensions of the formats this Pillow build can open."""
    return frozenset(
        ext for ext, fmt in Image.registered_extensions().items() if fmt in Image.OPEN and fmt not in STUB_FORMATS
    )


def candidate_extensions() -> frozenset[str]:
    return supported_extensions() | KNOWN_EXTRA_EXTENSIONS


@contextmanager
def _no_pixel_limit() -> Iterator[None]:
    """Header reads are cheap, so Pillow's decompression-bomb guard (meant for
    decoding) must not reject huge but valid images. Restored on exit."""
    previous = Image.MAX_IMAGE_PIXELS
    Image.MAX_IMAGE_PIXELS = None
    try:
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            yield
    finally:
        Image.MAX_IMAGE_PIXELS = previous


no_pixel_limit = _no_pixel_limit  # public name for sibling modules that also only read headers


def _exif_orientation(img: Image.Image) -> int | None:
    try:
        return img.getexif().get(EXIF_ORIENTATION_TAG)
    except Exception:  # damaged EXIF must not fail an otherwise readable image
        return None


def read_metadata(path: Path) -> ImageMetadata:
    try:
        with _no_pixel_limit(), Image.open(path) as img:
            fmt = img.format or "UNKNOWN"
            width, height = img.size
            rotated = _exif_orientation(img) in SWAPPING_ORIENTATIONS
    except UnidentifiedImageError as exc:
        known = path.suffix.lower() in supported_extensions()
        status = STATUS_CORRUPTED if known else STATUS_UNSUPPORTED
        reason = "not a valid image" if known else "format not supported by this Pillow build"
        raise ImageReadError(status, f"{reason} ({exc})") from exc
    except OSError as exc:
        # errno set => the OS refused (permission, missing, locked); otherwise Pillow rejected the content.
        status = STATUS_UNREADABLE if exc.errno is not None else STATUS_CORRUPTED
        raise ImageReadError(status, str(exc)) from exc
    except Exception as exc:  # Pillow raises SyntaxError/ValueError/struct.error/... on damaged headers
        raise ImageReadError(STATUS_CORRUPTED, f"{type(exc).__name__}: {exc}") from exc
    if width <= 0 or height <= 0:
        raise ImageReadError(STATUS_CORRUPTED, f"invalid dimensions {width}x{height}")
    if rotated:
        width, height = height, width
    return ImageMetadata(format=fmt, width=width, height=height, exif_rotated=rotated)
