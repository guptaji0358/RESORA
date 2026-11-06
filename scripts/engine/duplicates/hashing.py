"""Streaming SHA-256 of a file's bytes. Read-only; memory is bounded by CHUNK."""
from __future__ import annotations

import hashlib
from pathlib import Path
from typing import Callable

CHUNK = 1024 * 1024


def sha256_file(path: Path, check: Callable[[], None] = lambda: None) -> str:
    """Hex digest. Raises OSError if the file cannot be read; `check` (cancel) runs per chunk."""
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        while True:
            check()
            block = handle.read(CHUNK)
            if not block:
                return digest.hexdigest()
            digest.update(block)
