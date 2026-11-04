"""Write a text file via a temp name + rename, so a killed run never leaves a
half-written report under the final name (docs/BACKEND_CONTRACT.md section 6)."""
from __future__ import annotations

import os
from pathlib import Path
from typing import Callable, TextIO


def atomic_write(path: Path, write: Callable[[TextIO], None], encoding: str = "utf-8", newline: str | None = None) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(path.name + ".tmp")
    try:
        with open(temp, "w", encoding=encoding, newline=newline) as fh:
            write(fh)
        os.replace(temp, path)
    except BaseException:
        temp.unlink(missing_ok=True)
        raise
