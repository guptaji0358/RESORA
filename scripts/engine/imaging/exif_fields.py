"""EXIF extraction for the properties view. Pure Pillow, read-only, tolerant:
a damaged tag is skipped, never an error. GPS coordinates are deliberately not
exposed (only whether a GPS block exists)."""
from __future__ import annotations

from typing import Any

from PIL import ExifTags, Image

EXIF_IFD, GPS_IFD = 0x8769, 0x8825
ORIENTATION_TAG = 0x0112
SKIPPED_TAGS = {EXIF_IFD, GPS_IFD, 0xA005, 0x927C, 0x02BC, 0x8773}  # pointers, MakerNote, XMP, ICC
MAX_TEXT = 200
MAX_BINARY = 32
MAX_FIELDS = 150

ORIENTATION_LABELS = {
    1: "Normal",
    2: "Mirrored horizontally",
    3: "Rotated 180°",
    4: "Mirrored vertically",
    5: "Mirrored, rotated 90° clockwise",
    6: "Rotated 90° clockwise",
    7: "Mirrored, rotated 90° counter-clockwise",
    8: "Rotated 90° counter-clockwise",
}


def orientation_label(value: int | None) -> str | None:
    return None if value is None else ORIENTATION_LABELS.get(value, f"Unknown ({value})")


def _format(value: Any) -> str | None:
    """A short display string, or None when the value is not worth showing."""
    if isinstance(value, bytes):
        if len(value) > MAX_BINARY:
            return None
        text = value.rstrip(b"\x00").decode("ascii", errors="ignore").strip()
        return text if text and text.isprintable() else None
    if isinstance(value, str):
        text = value.replace("\x00", "").strip()
        return text[:MAX_TEXT] if text else None
    if isinstance(value, float):
        return f"{value:.6g}"
    if isinstance(value, (tuple, list)):
        parts = [_format(v) for v in value[:8]]
        return ", ".join(p for p in parts if p) or None
    try:
        return str(float(value)) if hasattr(value, "numerator") and not isinstance(value, int) else str(value)
    except (TypeError, ValueError, ZeroDivisionError):
        return None


def _orientation(exif: Image.Exif) -> int | None:
    value = exif.get(ORIENTATION_TAG)
    return value if isinstance(value, int) and 1 <= value <= 8 else None


def read_exif(img: Image.Image) -> dict[str, Any]:
    """{"available", "fields": [{"tag", "label", "value"}], "gps_present", "orientation"}."""
    try:
        exif = img.getexif()
        if not exif:
            return {"available": False, "fields": [], "gps_present": False, "orientation": None}
        merged: dict[int, Any] = dict(exif)
        try:
            merged.update(exif.get_ifd(EXIF_IFD))
        except Exception:
            pass  # a broken Exif sub-block must not hide the main tags
        gps = bool(merged.get(GPS_IFD))
        try:
            gps = gps or bool(exif.get_ifd(GPS_IFD))
        except Exception:
            pass
        fields = []
        for tag in sorted(merged):
            if tag in SKIPPED_TAGS:
                continue
            text = _format(merged[tag])
            if text is None:
                continue
            fields.append({"tag": f"0x{tag:04X}", "label": ExifTags.TAGS.get(tag, f"Tag 0x{tag:04X}"), "value": text})
            if len(fields) >= MAX_FIELDS:
                break
        return {"available": bool(fields) or gps, "fields": fields, "gps_present": gps, "orientation": _orientation(exif)}
    except Exception:  # damaged EXIF is "not available", not a failure
        return {"available": False, "fields": [], "gps_present": False, "orientation": None}
