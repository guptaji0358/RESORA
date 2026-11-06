"""What to scan: one or more folders (optional subfolders, excluded folders) and/or
an explicit file list (an imported collection). Nothing else is ever walked.
Input problems raise ValueError; cancellation propagates from `check`."""
from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Sequence

from ..imaging.discovery import discover
from ..imaging.metadata import candidate_extensions
from ..imaging.selection import select_files


@dataclass
class Sources:
    files: list[Path] = field(default_factory=list)  # unique, sorted case-insensitively
    folders: list[str] = field(default_factory=list)
    skipped: int = 0  # non-image files met during discovery
    errors: list[str] = field(default_factory=list)
    duplicates: int = 0  # the same path reached twice (overlapping folders / repeated files)


def _paths(value: Any, name: str) -> list[str]:
    if value is None:
        return []
    if not isinstance(value, list) or any(not isinstance(v, str) or not v.strip() for v in value):
        raise ValueError(f"'{name}' must be a list of non-empty path strings")
    return value


def collect(folders: Any, files: Any, recursive: bool, exclude: Any, check: Callable[[], None]) -> Sources:
    folder_list, file_list, skip_list = _paths(folders, "folders"), _paths(files, "files"), _paths(exclude, "exclude")
    if not folder_list and not file_list:
        raise ValueError("at least one folder or file is required")
    excluded = [os.path.normcase(os.path.abspath(p)) for p in skip_list]
    out = Sources()
    seen: set[str] = set()

    def add(paths: Sequence[Path]) -> None:
        for path in paths:
            key = os.path.normcase(str(path))
            if key in seen:
                out.duplicates += 1
            elif any(key == e or key.startswith(e + os.sep) for e in excluded):
                continue
            else:
                seen.add(key)
                out.files.append(path)

    for raw in folder_list:
        root = Path(os.path.abspath(raw))
        if not root.is_dir():
            raise ValueError(f"not a folder: {root}")
        if os.path.normcase(str(root)) in excluded:
            out.errors.append(f"{root}: excluded, not scanned")
            continue
        found = discover(root, recursive, candidate_extensions(), check, excluded)
        out.folders.append(str(root))
        out.skipped += found.skipped
        out.errors.extend(found.errors)
        add(found.files)
    if file_list:
        chosen = select_files(file_list, check)
        out.duplicates += chosen.duplicates
        add(chosen.files)
    out.files.sort(key=lambda p: str(p).casefold())
    return out
