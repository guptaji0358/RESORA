"""The per-file analysis result. A plain value type; no behaviour beyond `to_dict`."""
from __future__ import annotations

from dataclasses import asdict, dataclass

STATUS_OK = "ok"
STATUS_UNSUPPORTED = "unsupported"  # image-like extension Pillow cannot open here
STATUS_CORRUPTED = "corrupted"  # content is not a valid image (or unusable dimensions)
STATUS_UNREADABLE = "unreadable"  # OS-level failure (permissions, locked, vanished)


@dataclass(frozen=True)
class ImageRecord:
    path: str
    status: str
    error: str | None = None
    format: str | None = None
    width: int | None = None  # displayed dimensions (EXIF orientation applied)
    height: int | None = None
    orientation: str | None = None
    aspect_ratio: float | None = None
    aspect_label: str | None = None
    megapixels: float | None = None
    resolution: str | None = None
    resolution_match: str | None = None
    resolution_tier: str | None = None
    file_size: int | None = None
    exif_rotated: bool | None = None

    def to_dict(self) -> dict:
        return asdict(self)
