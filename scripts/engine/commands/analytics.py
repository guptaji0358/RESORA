"""`analytics`: collection statistics for a folder or an explicit file list.

Same inputs as `scan` (folder [+ --recursive], or params key "files"), but no
reports are written. Headers only; originals are never modified. Definitions:
docs/ANALYTICS.md.

Result data: {"source": "folder"|"files", "folder", "selected", "duplicates_ignored",
              "recursive", "skipped_non_images", "discovery_errors": [..], "analytics": {...}}
"""
from __future__ import annotations

import argparse
from typing import Any

from ..command import Command, Context
from ..imaging.analytics import compute_analytics
from ..imaging.collection import analyze_all, gather
from ..protocol import EXIT_INVALID_INPUT, BackendError

MAX_DISCOVERY_ERRORS = 50


def add_arguments(p: argparse.ArgumentParser) -> None:
    p.add_argument("folder", nargs="?", help="folder to analyse")
    p.add_argument("--recursive", action="store_true", default=None, help="include subfolders")


def run(args: argparse.Namespace, ctx: Context) -> dict[str, Any]:
    files_arg = ctx.params.get("files")
    recursive = bool(args.recursive if args.recursive is not None else ctx.params.get("recursive")) and files_arg is None
    folder = args.folder or ctx.params.get("folder")
    ctx.emit.progress(0, None, "discovering files")
    try:
        found = gather(folder, files_arg, recursive, ctx.cancel.check)
    except ValueError as exc:
        raise BackendError("invalid_input", str(exc), EXIT_INVALID_INPUT) from exc
    records = analyze_all(found.files, ctx.cancel.check, ctx.emit.progress)
    return {
        "source": "folder" if found.root else "files",
        "folder": str(found.root) if found.root else None,
        "selected": None if found.root else len(found.files),
        "duplicates_ignored": found.duplicates,
        "recursive": recursive,
        "skipped_non_images": found.skipped,
        "discovery_errors": found.errors[:MAX_DISCOVERY_ERRORS],
        "analytics": compute_analytics(records, found.duplicates),
    }


COMMAND = Command(name="analytics", help="statistics for an image collection", run=run, add_arguments=add_arguments)
