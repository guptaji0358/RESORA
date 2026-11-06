"""Detailed properties of one image (header-only, read-only).

`read_properties(path)` never raises for a bad image. It returns a plain dict:
every group is either filled or `None`/`unavailable`; absent metadata is never
reported as 0. Basic facts (format, displayed size, aspect, classification) come
from `analyze_file`, so they always agree with scan results; this module adds
what a scan does not need (colour, ICC, DPI, EXIF, stored size, frames).
"""
from __future__ import annotations

import io
import os
from pathlib import Path
from typing import Any

from PIL import Image

from .analyzer import analyze_file
from .exif_fields import orientation_label, read_exif
from .metadata import no_pixel_limit
from .records import STATUS_OK

MODE_NAMES = {
    "1": "Bilevel (1-bit)", "L": "Grayscale", "LA": "Grayscale + alpha", "P": "Indexed palette",
    "PA": "Palette + alpha", "RGB": "RGB", "RGBA": "RGB + alpha", "RGBX": "RGB (padded)",
    "CMYK": "CMYK", "YCbCr": "YCbCr", "LAB": "CIE Lab", "HSV": "HSV",
    "I": "Grayscale (32-bit)", "I;16": "Grayscale (16-bit)", "F": "Grayscale (float)",
}
BITS_PER_CHANNEL = {"1": 1, "L": 8, "LA": 8, "P": 8, "PA": 8, "RGB": 8, "RGBA": 8, "RGBX": 8,
                    "CMYK": 8, "YCbCr": 8, "LAB": 8, "HSV": 8, "I": 32, "I;16": 16, "F": 32}
ALPHA_MODES = {"LA", "PA", "RGBA"}


def _icc(img: Image.Image) -> dict[str, Any] | None:
    data = img.info.get("icc_profile")
    if not data:
        return None
    info: dict[str, Any] = {"description": None, "size": len(data)}
    try:
        from PIL import ImageCms

        profile = ImageCms.ImageCmsProfile(io.BytesIO(data))
        info["description"] = (ImageCms.getProfileDescription(profile) or "").strip() or None
    except Exception:  # embedded but unparseable: still report it is present
        pass
    return info


def _positive_pair(value: Any) -> tuple[float, float] | None:
    try:
        x, y = float(value[0]), float(value[1])
    except (TypeError, ValueError, IndexError):
        return None
    return (round(x, 2), round(y, 2)) if x > 0 and y > 0 else None


def _dpi(img: Image.Image, exif_info: dict[str, Any]) -> dict[str, Any] | None:
    """Header density (JFIF/PNG pHYs/TIFF/EXIF resolution) in dots per inch, else None."""
    pair = _positive_pair(img.info.get("dpi"))
    if pair:
        return {"x": pair[0], "y": pair[1], "source": "file header"}
    values = {f["label"]: f["value"] for f in exif_info["fields"]}
    try:
        x, y = float(values["XResolution"]), float(values["YResolution"])
        unit = int(float(values.get("ResolutionUnit", 2)))
    except (KeyError, ValueError):
        return None
    if x <= 0 or y <= 0 or unit not in (2, 3):
        return None  # unit 1 = "no absolute unit": not a DPI
    scale = 2.54 if unit == 3 else 1.0
    return {"x": round(x * scale, 2), "y": round(y * scale, 2), "source": "EXIF"}


def _color(img: Image.Image) -> dict[str, Any]:
    mode = img.mode
    has_alpha = mode in ALPHA_MODES or (mode == "P" and "transparency" in img.info)
    return {
        "mode": mode,
        "mode_name": MODE_NAMES.get(mode, mode),
        "channels": len(img.getbands()),
        "bits_per_channel": BITS_PER_CHANNEL.get(mode),
        "has_alpha": has_alpha,
        "icc_profile": _icc(img),
    }


def _frames(img: Image.Image) -> int | None:
    count = getattr(img, "n_frames", None)  # may need to scan the container, but never decodes pixels
    return count if isinstance(count, int) and count > 0 else None


def _details(path: Path, record: dict[str, Any]) -> dict[str, Any]:
    with no_pixel_limit(), Image.open(path) as img:
        exif = read_exif(img)
        stored_w, stored_h = img.size
        orient = exif["orientation"]
        return {
            "color": _color(img),
            "dpi": _dpi(img, exif),
            "exif": exif,
            "frames": _frames(img),
            "dimensions": {
                "stored_width": stored_w,
                "stored_height": stored_h,
                "width": record["width"],
                "height": record["height"],
                "orientation_tag": orient,
                "orientation_label": orientation_label(orient),
                "exif_rotated": bool(record["exif_rotated"]),
            },
        }


def read_properties(path: Path) -> dict[str, Any]:
    record = analyze_file(path).to_dict()
    out: dict[str, Any] = {
        "path": record["path"],
        "name": os.path.basename(record["path"]),
        "status": record["status"],
        "error": record["error"],
        "file_size": record["file_size"],
        "format": record["format"],
        "warnings": [],
    }
    if record["status"] != STATUS_OK:
        return out
    out.update({
        "layout": record["orientation"],  # portrait / landscape / square
        "aspect_ratio": record["aspect_ratio"],
        "aspect_label": record["aspect_label"],
        "megapixels": record["megapixels"],
        "resolution": record["resolution"],
        "resolution_match": record["resolution_match"],
        "resolution_tier": record["resolution_tier"],
    })
    try:
        out.update(_details(path, record))
    except Exception as exc:  # basics are valid; the extras are best-effort
        out["warnings"].append(f"extended metadata unavailable: {type(exc).__name__}: {exc}")
        out.update({"color": None, "dpi": None, "exif": None, "frames": None, "dimensions": None})
    return out
