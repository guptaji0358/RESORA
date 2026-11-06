"""Storage arithmetic, kept separate so it is easy to test.

An exact group of n identical files of `size` bytes occupies n*size, but one
copy must stay, so at most (n-1)*size is recoverable. Visually similar groups
only report their combined size: they are different files, never "safe to remove"."""
from __future__ import annotations


def exact_group_bytes(size: int, count: int) -> int:
    return size * count


def exact_group_savings(size: int, count: int) -> int:
    return size * max(count - 1, 0)
