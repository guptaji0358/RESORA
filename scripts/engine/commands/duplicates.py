"""`duplicates`: exact (SHA-256) and visually similar (dHash) image groups.

Sources: folders (positional and/or params "folders"), params "files" (an imported
collection), optional "exclude" folders and "recursive". Read-only: nothing is deleted,
moved or modified. Definitions and result shape: docs/DUPLICATES.md.
"""
from __future__ import annotations

import argparse
from typing import Any

from ..command import Command, Context
from ..duplicates.finder import DEFAULT_THRESHOLD, Settings, find_duplicates, validate_threshold
from ..duplicates.sources import collect
from ..protocol import EXIT_INVALID_INPUT, BackendError


def add_arguments(p: argparse.ArgumentParser) -> None:
    p.add_argument("folders", nargs="*", help="folders to scan")
    p.add_argument("--recursive", action="store_true", default=None, help="include subfolders")
    p.add_argument("--exclude", action="append", default=None, help="folder not to scan (repeatable)")
    p.add_argument("--threshold", type=float, default=None,
                   help=f"minimum visual similarity in percent (50-100, default {DEFAULT_THRESHOLD:g})")
    p.add_argument("--no-similar", action="store_true", default=None, help="exact duplicates only")
    p.add_argument("--cache", default=None, help="fingerprint cache file (SQLite)")


def run(args: argparse.Namespace, ctx: Context) -> dict[str, Any]:
    params = ctx.params
    folders = list(args.folders) or params.get("folders")
    recursive = bool(args.recursive if args.recursive is not None else params.get("recursive", False))
    exclude = args.exclude if args.exclude is not None else params.get("exclude")
    similar = not args.no_similar if args.no_similar is not None else bool(params.get("similar", True))
    cache = args.cache or params.get("cache_path")
    ctx.emit.progress(0, None, "Discovering files")
    try:
        threshold = validate_threshold(args.threshold if args.threshold is not None
                                       else params.get("threshold", DEFAULT_THRESHOLD))
        sources = collect(folders or None, params.get("files"), recursive, exclude, ctx.cancel.check)
        data = find_duplicates(sources, Settings(threshold, similar, cache), ctx.cancel.check, ctx.emit.progress)
    except ValueError as exc:
        raise BackendError("invalid_input", str(exc), EXIT_INVALID_INPUT) from exc
    data["sources"] = {"folders": sources.folders, "files": len(params.get("files") or []),
                       "recursive": recursive, "exclude": list(exclude or []),
                       "skipped_non_images": sources.skipped, "discovery_errors": sources.errors[:50]}
    return data


COMMAND = Command(name="duplicates", help="find exact and visually similar duplicate images",
                  run=run, add_arguments=add_arguments)
