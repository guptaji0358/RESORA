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
    --txt-analytics  append the analytics summary to the TXT report
                  (params: "txt_analytics": bool); default off, TXT layout
                  is otherwise unchanged. CSV is never affected.

Result data (small; the per-file table lives in the CSV):
    {"source": "folder"|"files", "folder": path|null, "selected": N|null,
     "duplicates_ignored": N, "recursive", "summary": {total, analyzed, failed, by_status,
     by_resolution, by_format, total_bytes}, "skipped_non_images",
     "discovery_errors": [..first 50..], "analytics": {imaging.analytics}, "reports": {"csv": path|null, "txt": path|null}}

Bad images never fail the job; they are rows with status != "ok". Errors:
invalid_input (exit 3) for a missing folder, report_failed (exit 1) when a
report cannot be written.
"""
from __future__ import annotations

import argparse
import os
from datetime import datetime
from pathlib import Path
from typing import Any

from ..command import Command, Context
from ..imaging.analytics import compute_analytics
from ..imaging.collection import analyze_all, gather
from ..protocol import EXIT_INVALID_INPUT, BackendError
from ..reports.csv_report import write_csv
from ..reports.summary import summarize
from ..reports.txt_report import write_txt

MAX_DISCOVERY_ERRORS = 50


def add_arguments(p: argparse.ArgumentParser) -> None:
    p.add_argument("folder", nargs="?", help="folder to scan")
    p.add_argument("--recursive", action="store_true", default=None, help="include subfolders")
    p.add_argument("--csv", help="write a CSV report to this path")
    p.add_argument("--txt", help="write a TXT report to this path")
    p.add_argument("--txt-analytics", action="store_true", default=None, help="add the analytics summary to the TXT report")


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


def run(args: argparse.Namespace, ctx: Context) -> dict[str, Any]:
    files_arg = ctx.params.get("files")
    recursive = bool(_option(args, ctx, "recursive")) and files_arg is None
    csv_path, txt_path = _report_path(_option(args, ctx, "csv")), _report_path(_option(args, ctx, "txt"))

    ctx.emit.progress(0, None, "discovering files")
    try:
        found = gather(_option(args, ctx, "folder"), files_arg, recursive, ctx.cancel.check)
    except ValueError as exc:
        raise BackendError("invalid_input", str(exc), EXIT_INVALID_INPUT) from exc
    files, root = found.files, found.root
    records = analyze_all(files, ctx.cancel.check, ctx.emit.progress)

    summary = summarize(records)
    analytics = compute_analytics(records, found.duplicates)
    if csv_path:
        _write_report("CSV", lambda: write_csv(records, csv_path))
    if txt_path:
        generated = datetime.now().isoformat(timespec="seconds")
        _write_report("TXT", lambda: write_txt(
            records, summary, txt_path, str(root) if root else "", recursive, generated,
            selected_files=None if root else len(files),
            analytics=analytics if _option(args, ctx, "txt_analytics") else None))

    return {
        "source": "folder" if root else "files",
        "folder": str(root) if root else None,
        "selected": None if root else len(files),
        "duplicates_ignored": found.duplicates,
        "recursive": recursive,
        "summary": summary,
        "skipped_non_images": found.skipped,
        "discovery_errors": found.errors[:MAX_DISCOVERY_ERRORS],
        "analytics": analytics,
        "reports": {"csv": str(csv_path) if csv_path else None, "txt": str(txt_path) if txt_path else None},
    }


COMMAND = Command(name="scan", help="analyse image resolutions in a folder", run=run, add_arguments=add_arguments)
