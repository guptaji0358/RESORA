"""Duplicate scan orchestration: sources -> exact groups -> similar groups -> result payload.

Read-only: files are only stat-ed, hashed and decoded for a tiny fingerprint, one at a
time (bounded memory). Per-file problems are data in `skipped`, never job failures."""
from __future__ import annotations

import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable

from ..imaging.analyzer import analyze_file
from .cache import DSTATUS_FLAT, DSTATUS_OK, FingerprintCache
from .exact import FileStat, find_exact
from .perceptual import FingerprintError, fingerprint, similarity
from .savings import exact_group_bytes, exact_group_savings
from .similar import Item, category, cluster
from .sources import Sources

ALGORITHM = "dhash-128"
DEFAULT_THRESHOLD = 90.0
MIN_THRESHOLD = 50.0
MAX_SKIPPED_LISTED = 200
PROGRESS_INTERVAL = 0.1


@dataclass(frozen=True)
class Settings:
    threshold: float = DEFAULT_THRESHOLD  # minimum similarity percent
    similar: bool = True
    cache_path: str | None = None


def validate_threshold(value: Any) -> float:
    try:
        number = float(value)
    except (TypeError, ValueError) as exc:
        raise ValueError("threshold must be a number") from exc
    if not MIN_THRESHOLD <= number <= 100.0:
        raise ValueError(f"threshold must be between {MIN_THRESHOLD:g} and 100")
    return number


class _Throttle:
    def __init__(self, progress: Callable[[int, int, str], None], phase: str) -> None:
        self._progress, self._phase, self._last = progress, phase, 0.0

    def __call__(self, done: int, total: int, name: str) -> None:
        now = time.monotonic()
        if done == total or now - self._last >= PROGRESS_INTERVAL:
            self._progress(done, total, f"{self._phase}: {name}")
            self._last = now


def _entry(path: Path, size: int, cache: dict[Path, dict]) -> dict[str, Any]:
    if path not in cache:
        r = analyze_file(path).to_dict()
        cache[path] = {
            "path": str(path), "name": path.name, "folder": str(path.parent), "status": r["status"],
            "format": r["format"], "width": r["width"], "height": r["height"], "resolution": r["resolution"],
            "file_size": size,
        }
    return dict(cache[path])


def find_duplicates(
    sources: Sources,
    settings: Settings,
    check: Callable[[], None],
    progress: Callable[[int, int, str], None],
) -> dict[str, Any]:
    skipped: list[dict[str, str]] = []
    stats: list[FileStat] = []
    empty = 0
    for path in sources.files:
        check()
        try:
            st = path.stat()
        except OSError as exc:
            skipped.append({"path": str(path), "status": "unreadable", "error": exc.strerror or str(exc)})
            continue
        if st.st_size == 0:
            empty += 1
        else:
            stats.append(FileStat(path, st.st_size, st.st_mtime_ns))
    sizes = {f.path: f.size for f in stats}
    cache = FingerprintCache(settings.cache_path)
    try:
        exact = find_exact(stats, cache, check, _Throttle(progress, "Hashing"))
        for path, reason in exact.errors:
            skipped.append({"path": str(path), "status": "unreadable", "error": reason})
        exact_paths = {f.path for members in exact.groups.values() for f in members}
        meta: dict[Path, dict] = {}
        exact_groups = []
        for number, (digest, members) in enumerate(
                sorted(exact.groups.items(), key=lambda kv: (-exact_group_savings(kv[1][0].size, len(kv[1])),
                                                              str(kv[1][0].path).casefold())), start=1):
            size, count = members[0].size, len(members)
            exact_groups.append({
                "id": f"e{number}", "sha256": digest, "size": size, "count": count,
                "total_bytes": exact_group_bytes(size, count), "savings_bytes": exact_group_savings(size, count),
                "files": [_entry(f.path, f.size, meta) for f in members],
            })
        similar_groups: list[dict[str, Any]] = []
        fingerprinted = fp_hits = flat = 0
        if settings.similar:
            similar_groups, fingerprinted, fp_hits, flat = _similar(
                stats, exact, sizes, cache, settings.threshold, meta, skipped, check, _Throttle(progress, "Comparing"))
    finally:
        cache.close()
    exact_savings = sum(g["savings_bytes"] for g in exact_groups)
    exact_bytes = sum(g["total_bytes"] for g in exact_groups)
    similar_bytes = sum(m["file_size"] for g in similar_groups for m in g["files"])
    return {
        "settings": {"algorithm": ALGORITHM, "threshold": settings.threshold, "similar": settings.similar,
                     "cache": settings.cache_path is not None and cache.warning is None},
        "stats": {
            "total_files": len(sources.files), "empty_files": empty, "hashed": exact.hashed,
            "cache_hits": exact.cache_hits + fp_hits, "fingerprinted": fingerprinted, "flat_images": flat,
            "exact_groups": len(exact_groups), "exact_files": len(exact_paths),
            "exact_extra_files": sum(g["count"] - 1 for g in exact_groups),
            "exact_bytes": exact_bytes, "exact_savings_bytes": exact_savings,
            "similar_groups": len(similar_groups),
            "similar_files": sum(len(g["files"]) for g in similar_groups), "similar_bytes": similar_bytes,
            "skipped": len(skipped), "duplicates_ignored": sources.duplicates,
        },
        "exact_groups": exact_groups,
        "similar_groups": similar_groups,
        "skipped": skipped[:MAX_SKIPPED_LISTED],
        "skipped_truncated": len(skipped) > MAX_SKIPPED_LISTED,
        "cache_warning": cache.warning,
    }


def _similar(stats, exact, sizes, cache, threshold, meta, skipped, check, progress):
    """Fingerprint one representative per distinct content, then cluster."""
    representatives: list[FileStat] = []
    copies: dict[Path, int] = {}  # representative -> number of other byte-identical files
    taken: set[str] = set()
    for f in stats:
        digest = exact.digest_of.get(f.path)
        if digest in exact.groups:
            if digest in taken:
                continue
            taken.add(digest)
            copies[f.path] = len(exact.groups[digest]) - 1
        representatives.append(f)
    items: list[Item] = []
    fingerprinted = hits = flat = 0
    for index, f in enumerate(representatives, start=1):
        check()
        cached = cache.get(f.path, f.size, f.mtime)
        bits: int | None
        if cached and cached["dstatus"] in (DSTATUS_OK, DSTATUS_FLAT):
            bits, hits = cached["dhash"], hits + 1
        else:
            try:
                bits = fingerprint(f.path)
            except FingerprintError as exc:
                skipped.append({"path": str(f.path), "status": exc.status, "error": exc.message})
                progress(index, len(representatives), f.path.name)
                continue
            cache.put(f.path, f.size, f.mtime, dhash=bits, dstatus=DSTATUS_FLAT if bits is None else DSTATUS_OK)
            fingerprinted += 1
        progress(index, len(representatives), f.path.name)
        if bits is None:
            flat += 1
            continue
        info = _entry(f.path, f.size, meta)
        if info["status"] != "ok" or not info["width"] or not info["height"]:
            continue
        items.append(Item(index - 1, bits, info["width"] / info["height"]))  # key = position in `representatives`
    groups = []
    for number, matches in enumerate(sorted(
            cluster(items, threshold, check), key=lambda g: -sum(representatives[m.key].size for m in g)), start=1):
        files, lowest = [], 100.0
        for m in matches:
            f = representatives[m.key]
            score = 100.0 if m.distance == 0 and m is matches[0] else similarity(m.distance)
            lowest = min(lowest, score)
            row = _entry(f.path, f.size, meta)
            row.update({"similarity": score, "distance": m.distance, "category": category(score),
                        "anchor": m is matches[0], "identical_copies": copies.get(f.path, 0)})
            files.append(row)
        groups.append({"id": f"s{number}", "count": len(files), "category": category(lowest),
                       "lowest_similarity": lowest, "total_bytes": sum(r["file_size"] for r in files), "files": files})
    return groups, fingerprinted, hits, flat
