"""`properties`: detailed, read-only properties of one or more image files.

Arguments (positional paths and/or params-file key "files"; both are merged):
    paths         image file paths

Result data: {"images": [<imaging.properties.read_properties dict>, ...],
              "duplicates_ignored": N}
A missing/corrupted/unsupported file is an entry with status != "ok"; the job
only fails (invalid_input, exit 3) when no path is given.
"""
from __future__ import annotations

import argparse
from pathlib import Path
from typing import Any

from ..command import Command, Context
from ..imaging.properties import read_properties
from ..imaging.selection import select_files
from ..protocol import EXIT_INVALID_INPUT, BackendError

MAX_FILES = 500


def add_arguments(p: argparse.ArgumentParser) -> None:
    p.add_argument("paths", nargs="*", help="image files")


def run(args: argparse.Namespace, ctx: Context) -> dict[str, Any]:
    raw = list(args.paths or [])
    extra = ctx.params.get("files")
    if extra is not None:
        if not isinstance(extra, list):
            raise BackendError("invalid_input", "'files' must be a list of paths", EXIT_INVALID_INPUT)
        raw += extra
    if not raw:
        raise BackendError("invalid_input", "at least one image path is required", EXIT_INVALID_INPUT)
    if len(raw) > MAX_FILES:
        raise BackendError("invalid_input", f"at most {MAX_FILES} images per request", EXIT_INVALID_INPUT)
    try:
        chosen = select_files(raw, ctx.cancel.check)
    except ValueError as exc:
        raise BackendError("invalid_input", str(exc), EXIT_INVALID_INPUT) from exc

    images = []
    total = len(chosen.files)
    for index, path in enumerate(chosen.files, start=1):
        ctx.cancel.check()
        images.append(read_properties(Path(path)))
        ctx.emit.progress(index, total, path.name)
    return {"images": images, "duplicates_ignored": chosen.duplicates}


COMMAND = Command(name="properties", help="detailed properties of image files", run=run, add_arguments=add_arguments)
