"""Exact duplicate detection: identical bytes, whatever the name, extension or folder.

File size is only a cheap pre-filter: a file whose size is unique cannot have an
exact twin and is never read. Files that share a size are hashed (SHA-256, cache
reused when path/size/mtime are unchanged); only equal digests make a group."""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Sequence

from .cache import FingerprintCache
from .hashing import sha256_file


@dataclass(frozen=True)
class FileStat:
    path: Path
    size: int
    mtime: int  # ns


@dataclass
class ExactResult:
    groups: dict[str, list[FileStat]] = field(default_factory=dict)  # digest -> >= 2 files, sorted by path
    digest_of: dict[Path, str] = field(default_factory=dict)  # every hashed file
    errors: list[tuple[Path, str]] = field(default_factory=list)  # (path, reason): could not be read
    hashed: int = 0
    cache_hits: int = 0


def find_exact(
    files: Sequence[FileStat],
    cache: FingerprintCache,
    check: Callable[[], None],
    progress: Callable[[int, int, str], None],
) -> ExactResult:
    """`files` must not contain empty (0-byte) files."""
    result = ExactResult()
    by_size: dict[int, list[FileStat]] = {}
    for item in files:
        by_size.setdefault(item.size, []).append(item)
    todo = [f for group in by_size.values() if len(group) > 1 for f in group]
    by_digest: dict[str, list[FileStat]] = {}
    for index, item in enumerate(todo, start=1):
        check()
        cached = cache.get(item.path, item.size, item.mtime)
        digest = cached["sha"] if cached and cached["sha"] else None
        if digest:
            result.cache_hits += 1
        else:
            try:
                digest = sha256_file(item.path, check)
            except OSError as exc:
                result.errors.append((item.path, exc.strerror or str(exc)))
                progress(index, len(todo), item.path.name)
                continue
            cache.put(item.path, item.size, item.mtime, sha=digest)
            result.hashed += 1
        result.digest_of[item.path] = digest
        by_digest.setdefault(digest, []).append(item)
        progress(index, len(todo), item.path.name)
    for digest, members in by_digest.items():
        if len(members) > 1:
            result.groups[digest] = sorted(members, key=lambda f: str(f.path).casefold())
    return result
