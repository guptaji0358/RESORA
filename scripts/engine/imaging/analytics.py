"""Collection analytics: one streaming pass over per-file record dicts. Pure
(no I/O, no Pillow). Exact definitions are in docs/ANALYTICS.md.

Key rules
- An image is *analyzed* only if status == "ok" AND width/height are positive
  integers. Everything else is *failed* and never feeds any average, extreme
  or distribution. Missing values are skipped, never treated as 0.
- Resolution, category, orientation and format percentages use `analyzed` as
  the denominator; storage percentages use the known-size total.
- Category assignment reuses the classifier's output (`resolution_match`,
  `resolution_tier`); `classify` is only called for a record that lacks them.
- Totals are Python ints (no overflow); floats only for averages/percentages.
"""
from __future__ import annotations

import os
from collections import Counter
from typing import Any, Iterable, Mapping

from .aspect import orientation
from .classification import ABOVE_8K, STANDARDS, classify

OTHER_CATEGORY = "Below HD / Other"
CATEGORY_ORDER = tuple(s.name for s in STANDARDS) + (ABOVE_8K, OTHER_CATEGORY)
ORIENTATIONS = ("landscape", "portrait", "square")
MAIN_FORMATS = {"JPEG": "JPEG", "MPO": "JPEG", "PNG": "PNG", "WEBP": "WebP", "GIF": "GIF", "BMP": "BMP", "TIFF": "TIFF"}
OTHER_FORMAT = "Other"
MAX_FAILURES = 50
STATUS_INVALID = "invalid_metadata"  # status ok but dimensions missing/non-positive


def _valid_dims(r: Mapping[str, Any]) -> bool:
    w, h = r.get("width"), r.get("height")
    return (r.get("status") == "ok" and isinstance(w, int) and isinstance(h, int)
            and not isinstance(w, bool) and not isinstance(h, bool) and w > 0 and h > 0)


def category_of(r: Mapping[str, Any]) -> str:
    match, tier = r.get("resolution_match"), r.get("resolution_tier")
    if match is None or tier is None:
        result = classify(r["width"], r["height"])
        match, tier = result.match, result.tier
    if match in ("exact", "near") and tier in CATEGORY_ORDER:
        return tier
    return tier if tier == ABOVE_8K else OTHER_CATEGORY


def format_group(raw: Any) -> str:
    """Normalised display group; unknown formats fall into OTHER_FORMAT."""
    return MAIN_FORMATS.get(str(raw).upper(), OTHER_FORMAT) if raw else OTHER_FORMAT


def _percent(count: int, denominator: int) -> float | None:
    return round(count * 100 / denominator, 2) if denominator > 0 else None


def _image(r: Mapping[str, Any]) -> dict[str, Any]:
    w, h = r["width"], r["height"]
    return {"path": r["path"], "name": os.path.basename(r["path"]), "width": w, "height": h,
            "megapixels": round(w * h / 1_000_000, 3), "file_size": r.get("file_size"),
            "format": r.get("format"), "resolution": r.get("resolution")}


def _rows(counter: Mapping[str, int], order: Iterable[str], denominator: int) -> list[dict[str, Any]]:
    return [{"name": n, "count": counter.get(n, 0), "percent": _percent(counter.get(n, 0), denominator)} for n in order]


def compute_analytics(records: Iterable[Mapping[str, Any]], duplicates_ignored: int = 0) -> dict[str, Any]:
    seen: set[str] = set()
    total = analyzed = dup = 0
    by_status: Counter[str] = Counter()
    failures: list[dict[str, Any]] = []
    failed_bytes = 0
    sum_w = sum_h = sum_px = 0
    sum_bytes = size_known = 0
    categories: Counter[str] = Counter()
    orients: Counter[str] = Counter()
    dims: Counter[tuple[int, int]] = Counter()
    fmt_count: Counter[str] = Counter()
    fmt_bytes: Counter[str] = Counter()
    other_detail: Counter[str] = Counter()
    hi = lo = big = small = None  # (key, record)
    hi_n = lo_n = 0

    for r in records:
        key = os.path.normcase(os.path.abspath(str(r.get("path", ""))))
        if key in seen:
            dup += 1
            continue
        seen.add(key)
        total += 1
        size = r.get("file_size")
        size = size if isinstance(size, int) and not isinstance(size, bool) and size >= 0 else None
        if not _valid_dims(r):
            status = r.get("status") if r.get("status") != "ok" else STATUS_INVALID
            by_status[status] += 1
            failed_bytes += size or 0
            if len(failures) < MAX_FAILURES:
                failures.append({"path": r.get("path"), "status": status, "error": r.get("error")})
            continue
        analyzed += 1
        w, h = r["width"], r["height"]
        px = w * h
        sum_w, sum_h, sum_px = sum_w + w, sum_h + h, sum_px + px
        categories[category_of(r)] += 1
        orients[orientation(w, h)] += 1
        dims[(w, h)] += 1
        raw = r.get("format")
        group = format_group(raw)
        fmt_count[group] += 1
        if group == OTHER_FORMAT:
            other_detail[str(raw).upper() if raw else "Unknown"] += 1
        if size is not None:
            sum_bytes += size
            size_known += 1
            fmt_bytes[group] += size
            if big is None or size > big[0] or (size == big[0] and r["path"] < big[1]["path"]):
                big = (size, r)  # ties: smaller path, so the result is deterministic
            if small is None or size < small[0] or (size == small[0] and r["path"] < small[1]["path"]):
                small = (size, r)
        if hi is None or px > hi[0]:
            hi, hi_n = (px, r), 1
        elif px == hi[0]:
            hi_n += 1
            if r["path"] < hi[1]["path"]:
                hi = (px, r)
        if lo is None or px < lo[0]:
            lo, lo_n = (px, r), 1
        elif px == lo[0]:
            lo_n += 1
            if r["path"] < lo[1]["path"]:
                lo = (px, r)

    # Most common exact (width, height); ties: larger pixel count, then smaller width.
    common = None
    if dims:
        (cw, ch), cn = max(dims.items(), key=lambda kv: (kv[1], kv[0][0] * kv[0][1], -kv[0][0]))
        common = {"width": cw, "height": ch, "count": cn, "percent": _percent(cn, analyzed),
                  "tied": sum(1 for v in dims.values() if v == cn)}

    named = [n for n in sorted(fmt_count, key=lambda n: (-fmt_count[n], n)) if n != OTHER_FORMAT]
    if OTHER_FORMAT in fmt_count:
        named.append(OTHER_FORMAT)
    formats = [{"name": n, "count": fmt_count[n], "percent": _percent(fmt_count[n], analyzed),
                "bytes": fmt_bytes.get(n, 0), "bytes_percent": _percent(fmt_bytes.get(n, 0), sum_bytes)} for n in named]

    def extreme(item, tied):
        return None if item is None else {**_image(item[1]), "tied": tied}

    return {
        "denominator": analyzed,
        "collection": {
            "total": total, "analyzed": analyzed, "failed": total - analyzed,
            "failed_by_status": dict(sorted(by_status.items(), key=lambda kv: (-kv[1], kv[0]))),
            "duplicates_ignored": duplicates_ignored + dup,
            "total_bytes": sum_bytes, "size_known": size_known, "size_unknown": analyzed - size_known,
            "failed_bytes": failed_bytes,
            "average_bytes": round(sum_bytes / size_known, 1) if size_known else None,
            "largest": None if big is None else _image(big[1]),
            "smallest": None if small is None else _image(small[1]),
        },
        "resolution": {
            "total_pixels": sum_px,
            "total_megapixels": round(sum_px / 1_000_000, 3),
            "average_width": round(sum_w / analyzed, 1) if analyzed else None,
            "average_height": round(sum_h / analyzed, 1) if analyzed else None,
            "average_megapixels": round(sum_px / analyzed / 1_000_000, 3) if analyzed else None,
            "highest": extreme(hi, hi_n), "lowest": extreme(lo, lo_n), "most_common": common,
        },
        "categories": _rows(categories, CATEGORY_ORDER, analyzed),
        "orientation": _rows(orients, ORIENTATIONS, analyzed),
        "formats": formats,
        "other_formats": dict(sorted(other_detail.items(), key=lambda kv: (-kv[1], kv[0]))),
        "failures": failures,
        "failures_truncated": (total - analyzed) > len(failures),
    }
