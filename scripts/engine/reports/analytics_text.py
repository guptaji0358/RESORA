"""Plain-text lines for the optional "Analytics summary" section of the TXT
report. Pure; size formatting is injected so this module imports nothing."""
from __future__ import annotations

from typing import Any, Callable, Mapping


def _pct(value: float | None) -> str:
    return "-" if value is None else f"{value:.1f}%"


def _num(value: float | None, unit: str = "") -> str:
    return "n/a" if value is None else f"{value:,.2f}{unit}"


def _img(entry: Mapping[str, Any] | None, size: Callable[[int | None], str]) -> str:
    if not entry:
        return "n/a"
    return f"{entry['width']}x{entry['height']} ({entry['megapixels']:.2f} MP, {size(entry.get('file_size'))})  {entry['path']}"


def analytics_lines(a: Mapping[str, Any], size: Callable[[int | None], str]) -> list[str]:
    c, r, n = a["collection"], a["resolution"], a["denominator"]
    out = ["", "Analytics summary", "-----------------",
           f"Percentages are of the {n} analyzed image(s); failed files are excluded.",
           f"Average file size:   {size(int(c['average_bytes'])) if c['average_bytes'] is not None else 'n/a'}",
           f"Largest file:        {_img(c['largest'], size)}",
           f"Smallest file:       {_img(c['smallest'], size)}",
           f"Average width x height: {_num(r['average_width'])} x {_num(r['average_height'])} px",
           f"Average megapixels:  {_num(r['average_megapixels'], ' MP')}",
           f"Total megapixels:    {_num(r['total_megapixels'], ' MP')}",
           f"Highest resolution:  {_img(r['highest'], size)}",
           f"Lowest resolution:   {_img(r['lowest'], size)}"]
    m = r["most_common"]
    out.append("Most common size:    " + (f"{m['width']}x{m['height']} ({m['count']} image(s), {_pct(m['percent'])})" if m else "n/a"))
    for title, rows in (("Resolution categories", a["categories"]), ("Orientation", a["orientation"]), ("Formats", a["formats"])):
        out += ["", title]
        out += [f"  {x['name']:<28} {x['count']:>7}  {_pct(x['percent']):>7}" for x in rows]
    if c["failed"]:
        out += ["", f"Failed images: {c['failed']} (" + ", ".join(f"{k} {v}" for k, v in c["failed_by_status"].items()) + ")"]
    return out
