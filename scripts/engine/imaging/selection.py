"""Explicitly selected files (as opposed to folder discovery). No Pillow, no
reports. Every entry is kept so that nothing is silently dropped: bad paths
become rows later (`unreadable` / `unsupported` / `corrupted`); only exact
duplicates are collapsed, and counted."""
from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Sequence


@dataclass
class Selection:
    files: list[Path] = field(default_factory=list)  # unique, in the order given
    duplicates: int = 0


def select_files(paths: Sequence[Any], check: Callable[[], None] = lambda: None) -> Selection:
    """Absolutise and de-duplicate (case-insensitively on Windows). Raises
    ValueError for an entry that is not a non-empty string."""
    result = Selection()
    seen: set[str] = set()
    for index, raw in enumerate(paths):
        if index % 1000 == 0:
            check()
        if not isinstance(raw, str) or not raw.strip():
            raise ValueError(f"files[{index}] must be a non-empty path string")
        full = os.path.abspath(raw)
        key = os.path.normcase(full)
        if key in seen:
            result.duplicates += 1
            continue
        seen.add(key)
        result.files.append(Path(full))
    return result
