"""Analyse one file: metadata + aspect + classification -> ImageRecord.
Never raises for a bad image; the failure is recorded instead."""
from __future__ import annotations

import os
from pathlib import Path

from .aspect import aspect_label, aspect_ratio, orientation
from .classification import classify
from .metadata import ImageReadError, read_metadata
from .records import STATUS_OK, ImageRecord


def _file_size(path: Path) -> int | None:
    try:
        return os.stat(path).st_size
    except OSError:
        return None


def analyze_file(path: Path) -> ImageRecord:
    full = str(path)
    size = _file_size(path)
    try:
        meta = read_metadata(path)
    except ImageReadError as exc:
        return ImageRecord(path=full, status=exc.status, error=exc.message, file_size=size)
    result = classify(meta.width, meta.height)
    return ImageRecord(
        path=full,
        status=STATUS_OK,
        format=meta.format,
        width=meta.width,
        height=meta.height,
        orientation=orientation(meta.width, meta.height),
        aspect_ratio=aspect_ratio(meta.width, meta.height),
        aspect_label=aspect_label(meta.width, meta.height),
        megapixels=round(meta.width * meta.height / 1_000_000, 2),
        resolution=result.label,
        resolution_match=result.match,
        resolution_tier=result.tier,
        file_size=size,
        exif_rotated=meta.exif_rotated,
    )
