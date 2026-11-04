"""CSV report: one row per file, successes and failures alike. UTF-8 with BOM
so Excel opens non-ASCII paths correctly."""
from __future__ import annotations

import csv
from pathlib import Path
from typing import Any, Mapping, Sequence

from .atomic import atomic_write

COLUMNS = (
    "path", "status", "error", "format", "width", "height", "orientation", "aspect_ratio", "aspect_label",
    "megapixels", "resolution", "resolution_match", "resolution_tier", "file_size", "exif_rotated",
)


def write_csv(records: Sequence[Mapping[str, Any]], path: Path) -> None:
    def write(fh) -> None:
        writer = csv.DictWriter(fh, fieldnames=COLUMNS, extrasaction="ignore", restval="")
        writer.writeheader()
        for record in records:
            writer.writerow({k: "" if record.get(k) is None else record[k] for k in COLUMNS})

    atomic_write(path, write, encoding="utf-8-sig", newline="")
