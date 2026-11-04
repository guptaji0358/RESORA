"""`scan`: analyse the images in a folder, or in an explicit list of files,
and optionally write CSV/TXT reports.

Arguments (each may also be given as a key of the --params-file object; the
command line wins):
    folder        folder to scan (positional)
    files         (params only) list of image file paths to scan instead of a
                  folder; mutually exclusive with folder. Every listed file
                  becomes a row (a missing/unsupported/corrupted one with
                  status != "ok"); exact duplicates are collapsed and counted.
    --recursive   include subfolders (params: "recursive": bool); default off;
                  ignored (reported false) for a file list
    --csv PATH    write the per-file CSV report
    --txt PATH    write the human-readable TXT report

Result data (small; the per-file table lives in the CSV):
    {"source": "folder"|"files", "folder": path|null, "selected": N|null,
     "duplicates_ignored": N, "recursive", "summary": {total, analyzed, failed, by_status,
     by_resolution, by_format, total_bytes}, "skipped_non_images",
     "discovery_errors": [..first 50..], "reports": {"csv": path|null, "txt": path|null}}

Bad images never fail the job; they are rows with status != "ok". Errors:
invalid_input (exit 3) for a missing folder, report_failed (exit 1) when a
report cannot be written.
"""
from __future__ import annotations

import argparse
import os
import time
from datetime import datetime
from pathlib import Path
from typing import Any

from ..command import Command, Context
from ..imaging.analyzer import analyze_file
from ..imaging.discovery import discover
from ..imaging.metadata import candidate_extensions
from ..imaging.selection import select_files
from ..protocol import EXIT_INVALID_INPUT, BackendError
from ..reports.csv_report import write_csv
from ..reports.summary import summarize
from ..reports.txt_report import write_txt

PROGRESS_INTERVAL = 0.1  # seconds; contract allows ~10 envelopes/s
MAX_DISCOVERY_ERRORS = 50


def add_arguments(p: argparse.ArgumentParser) -> None:
    p.add_argument("folder", nargs="?", help="folder to scan")
    p.add_argument("--recursive", action="store_true", default=None, help="include subfolders")
    p.add_argument("--csv", help="write a CSV report to this path")
    p.add_argument("--txt", help="write a TXT report to this path")


def _option(args: argparse.Namespace, ctx: Context, name: str) -> Any:
    value = getattr(args, name)
    return value if value is not None else ctx.params.get(name)


def _report_path(value: Any) -> Path | None:
    return Path(os.path.abspath(value)) if value else None


def _write_report(kind: str, write) -> None:
    try:
        write()
    except OSError as exc:
        raise BackendError("report_failed", f"cannot write {kind} report: {exc}") from exc


def _gather(folder_arg: Any, files_arg: Any, recursive: bool, ctx: Context):
    """Returns (files, root|None, skipped, discovery_errors, duplicates)."""
    if folder_arg and files_arg is not None:
        raise BackendError("invalid_input", "give either a folder or a file list, not both", EXIT_INVALID_INPUT)
    if files_arg is not None:
        if not isinstance(files_arg, list) or not files_arg:
            raise BackendError("invalid_input", "'files' must be a non-empty list of paths", EXIT_INVALID_INPUT)
        try:
            chosen = select_files(files_arg, ctx.cancel.check)
        except ValueError as exc:
            raise BackendError("invalid_input", str(exc), EXIT_INVALID_INPUT) from exc
        return chosen.files, None, 0, [], chosen.duplicates
    if not folder_arg:
        raise BackendError("invalid_input", "a folder or a file list is required", EXIT_INVALID_INPUT)
    root = Path(os.path.abspath(folder_arg))
    if not root.is_dir():
        raise BackendError("invalid_input", f"not a folder: {root}", EXIT_INVALID_INPUT)
    found = discover(root, recursive, candidate_extensions(), ctx.cancel.check)
    return found.files, root, found.skipped, found.errors, 0


def run(args: argparse.Namespace, ctx: Context) -> dict[str, Any]:
    files_arg = ctx.params.get("files")
    recursive = bool(_option(args, ctx, "recursive")) and files_arg is None
    csv_path, txt_path = _report_path(_option(args, ctx, "csv")), _report_path(_option(args, ctx, "txt"))

    ctx.emit.progress(0, None, "discovering files")
    files, root, skipped, errors, duplicates = _gather(_option(args, ctx, "folder"), files_arg, recursive, ctx)
    records: list[dict[str, Any]] = []
    total, last = len(files), time.monotonic()
    for index, path in enumerate(files, start=1):
        ctx.cancel.check()
        records.append(analyze_file(path).to_dict())
        now = time.monotonic()
        if index == total or now - last >= PROGRESS_INTERVAL:
            ctx.emit.progress(index, total, path.name)
            last = now

    summary = summarize(records)
    if csv_path:
        _write_report("CSV", lambda: write_csv(records, csv_path))
    if txt_path:
        generated = datetime.now().isoformat(timespec="seconds")
        _write_report("TXT", lambda: write_txt(
            records, summary, txt_path, str(root) if root else "", recursive, generated,
            selected_files=None if root else len(files)))

    return {
        "source": "folder" if root else "files",
        "folder": str(root) if root else None,
        "selected": None if root else len(files),
        "duplicates_ignored": duplicates,
        "recursive": recursive,
        "summary": summary,
        "skipped_non_images": skipped,
        "discovery_errors": errors[:MAX_DISCOVERY_ERRORS],
        "reports": {"csv": str(csv_path) if csv_path else None, "txt": str(txt_path) if txt_path else None},
    }


COMMAND = Command(name="scan", help="analyse image resolutions in a folder", run=run, add_arguments=add_arguments)
