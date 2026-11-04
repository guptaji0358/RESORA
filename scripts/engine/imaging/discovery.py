"""Folder discovery: which files under a root look like images. No Pillow, no
reports. Symlinked directories are not followed (avoids loops)."""
from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Collection


@dataclass
class Discovery:
    files: list[Path] = field(default_factory=list)  # sorted, as rooted at the given root
    skipped: int = 0  # files with a non-image extension
    errors: list[str] = field(default_factory=list)  # directories/entries that could not be listed


def discover(
    root: Path,
    recursive: bool,
    extensions: Collection[str],
    check: Callable[[], None] = lambda: None,
) -> Discovery:
    """`extensions` are lowercase with a leading dot. `check` is called per
    directory so the caller can cancel."""
    result = Discovery()
    pending = [root]
    while pending:
        check()
        directory = pending.pop()
        try:
            with os.scandir(directory) as it:
                entries = sorted(it, key=lambda e: e.name.casefold())
        except OSError as exc:
            result.errors.append(f"{directory}: {exc.strerror or exc}")
            continue
        for entry in entries:
            try:
                if entry.is_dir(follow_symlinks=False):
                    if recursive:
                        pending.append(Path(entry.path))
                elif entry.is_file():
                    if os.path.splitext(entry.name)[1].lower() in extensions:
                        result.files.append(Path(entry.path))
                    else:
                        result.skipped += 1
            except OSError as exc:
                result.errors.append(f"{entry.path}: {exc.strerror or exc}")
    result.files.sort(key=lambda p: str(p).casefold())
    return result
