"""Turn "a folder or an explicit file list" into analysed records. Shared by the
`scan` and `analytics` commands (commands never import each other). No reports,
no protocol: input problems raise ValueError, cancellation propagates from `check`."""
from __future__ import annotations

import os
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Sequence

from .analyzer import analyze_file
from .discovery import discover
from .metadata import candidate_extensions
from .selection import select_files

PROGRESS_INTERVAL = 0.1  # seconds; the contract allows ~10 envelopes/s


@dataclass
class Gathered:
    files: list[Path]
    root: Path | None  # folder source; None for a file list
    skipped: int = 0
    errors: list[str] = field(default_factory=list)
    duplicates: int = 0


def gather(folder: Any, files: Any, recursive: bool, check: Callable[[], None]) -> Gathered:
    """Raises ValueError for a bad combination, a bad list or a missing folder."""
    if folder and files is not None:
        raise ValueError("give either a folder or a file list, not both")
    if files is not None:
        if not isinstance(files, list) or not files:
            raise ValueError("'files' must be a non-empty list of paths")
        chosen = select_files(files, check)
        return Gathered(chosen.files, None, duplicates=chosen.duplicates)
    if not folder:
        raise ValueError("a folder or a file list is required")
    root = Path(os.path.abspath(folder))
    if not root.is_dir():
        raise ValueError(f"not a folder: {root}")
    found = discover(root, recursive, candidate_extensions(), check)
    return Gathered(found.files, root, found.skipped, found.errors)


def analyze_all(
    files: Sequence[Path],
    check: Callable[[], None],
    progress: Callable[[int, int, str], None],
) -> list[dict[str, Any]]:
    """Header-only analysis of every file, in order; progress is throttled."""
    records: list[dict[str, Any]] = []
    total, last = len(files), time.monotonic()
    for index, path in enumerate(files, start=1):
        check()
        records.append(analyze_file(path).to_dict())
        now = time.monotonic()
        if index == total or now - last >= PROGRESS_INTERVAL:
            progress(index, total, path.name)
            last = now
    return records
