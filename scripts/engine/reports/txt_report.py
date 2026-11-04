"""Human-readable TXT report: header, summary, breakdowns, file table, problems."""
from __future__ import annotations

from pathlib import Path
from typing import Any, Mapping, Sequence, TextIO

from .atomic import atomic_write


def human_size(num: int | None) -> str:
    if num is None:
        return "?"
    if num < 1024:
        return f"{num} B"
    size = float(num)
    for unit in ("KB", "MB", "GB"):
        size /= 1024
        if size < 1024 or unit == "GB":
            return f"{size:.1f} {unit}"
    raise AssertionError("unreachable")


def _breakdown(fh: TextIO, title: str, counts: Mapping[str, int]) -> None:
    fh.write(f"\n{title}\n")
    for name, count in counts.items():
        fh.write(f"  {name:<28} {count}\n")


def write_txt(
    records: Sequence[Mapping[str, Any]],
    summary: Mapping[str, Any],
    path: Path,
    folder: str,
    recursive: bool,
    generated: str,
    selected_files: int | None = None,
) -> None:
    def write(fh: TextIO) -> None:
        fh.write("Image Resolution Report\n=======================\n")
        if selected_files is None:
            fh.write(f"Folder:    {folder}\nRecursive: {'yes' if recursive else 'no'}\n")
        else:
            fh.write(f"Source:    {selected_files} selected file(s)\n")
        fh.write(f"Generated: {generated}\n")
        fh.write(
            f"\nFiles scanned: {summary['total']}  (analyzed {summary['analyzed']}, failed {summary['failed']})\n"
            f"Total size:    {human_size(summary['total_bytes'])}\n"
        )
        _breakdown(fh, "By resolution", summary["by_resolution"])
        _breakdown(fh, "By format", summary["by_format"])
        if summary["failed"]:
            _breakdown(fh, "Failures by status", {k: v for k, v in summary["by_status"].items() if k != "ok"})

        ok = [r for r in records if r.get("status") == "ok"]
        fh.write("\nImages\n")
        fh.write(f"  {'Resolution':<26}{'Size (WxH)':<14}{'Aspect':<9}{'Format':<8}{'File size':<11}Path\n")
        for r in ok:
            dims = f"{r['width']}x{r['height']}"
            fh.write(
                f"  {r['resolution']:<26}{dims:<14}{r['aspect_label']:<9}{r['format']:<8}"
                f"{human_size(r.get('file_size')):<11}{r['path']}\n"
            )
        problems = [r for r in records if r.get("status") != "ok"]
        if problems:
            fh.write("\nProblem files\n")
            for r in problems:
                fh.write(f"  [{r['status']}] {r['path']}\n      {r.get('error') or ''}\n")

    atomic_write(path, write, encoding="utf-8")
