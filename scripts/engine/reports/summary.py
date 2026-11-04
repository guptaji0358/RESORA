"""Aggregate counts over record dictionaries. Pure."""
from __future__ import annotations

from collections import Counter
from typing import Any, Mapping, Sequence


def _ranked(counter: Counter) -> dict[str, int]:
    return dict(sorted(counter.items(), key=lambda kv: (-kv[1], kv[0])))


def summarize(records: Sequence[Mapping[str, Any]]) -> dict[str, Any]:
    ok = [r for r in records if r.get("status") == "ok"]
    return {
        "total": len(records),
        "analyzed": len(ok),
        "failed": len(records) - len(ok),
        "by_status": _ranked(Counter(r.get("status") for r in records)),
        "by_resolution": _ranked(Counter(r["resolution"] for r in ok)),
        "by_format": _ranked(Counter(r["format"] for r in ok)),
        "total_bytes": sum(r.get("file_size") or 0 for r in records),
    }
