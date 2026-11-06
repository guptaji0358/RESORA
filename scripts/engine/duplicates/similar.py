"""Group images whose fingerprints are close. Pure logic on plain values.

Candidate search uses the pigeonhole principle: if two 128-bit hashes differ in
at most d bits, then when the hash is cut into d+1 slices at least one slice is
identical. Only items sharing a slice are compared, so large collections do not
need all-pairs work.

Grouping is anchor-based, not chained: the first unassigned item becomes the
anchor and takes every unassigned item within the threshold *of the anchor*.
A -> B -> C chains (A~B, B~C, A not~C) therefore never merge into one group.
The anchor is only the comparison base; it is not a "preferred copy".

Items with clearly different aspect ratios are never matched, so crops and
heavily edited variants are not presented as similar."""
from __future__ import annotations

from dataclasses import dataclass

from .perceptual import HASH_BITS, distance, max_distance

ASPECT_TOLERANCE = 0.03  # relative difference of width/height allowed between matches
NEAR_IDENTICAL, VERY_SIMILAR = 97.0, 93.0  # similarity percent boundaries of the categories
CATEGORY_NEAR, CATEGORY_VERY, CATEGORY_SIMILAR = "Nearly identical", "Very similar", "Similar"


@dataclass(frozen=True)
class Item:
    key: int       # caller's index
    bits: int      # fingerprint
    aspect: float  # width / height


@dataclass(frozen=True)
class Match:
    key: int
    distance: int  # to the anchor (0 for the anchor itself)


def category(similarity_percent: float) -> str:
    if similarity_percent >= NEAR_IDENTICAL:
        return CATEGORY_NEAR
    return CATEGORY_VERY if similarity_percent >= VERY_SIMILAR else CATEGORY_SIMILAR


def _aspect_ok(a: float, b: float) -> bool:
    return abs(a - b) / max(a, b) <= ASPECT_TOLERANCE


def cluster(items: list[Item], min_similarity: float, check=lambda: None) -> list[list[Match]]:
    """Groups of >= 2 items, anchor first, then by ascending distance."""
    limit = max_distance(min_similarity)
    slices = min(limit + 1, HASH_BITS)
    width = -(-HASH_BITS // slices)
    mask = (1 << width) - 1
    buckets: dict[tuple[int, int], list[int]] = {}
    for position, item in enumerate(items):
        for s in range(slices):
            buckets.setdefault((s, (item.bits >> (s * width)) & mask), []).append(position)
    taken = [False] * len(items)
    groups: list[list[Match]] = []
    for position, anchor in enumerate(items):
        if taken[position]:
            continue
        if position % 256 == 0:
            check()
        near: dict[int, int] = {}
        for s in range(slices):
            for other in buckets[(s, (anchor.bits >> (s * width)) & mask)]:
                if other == position or taken[other] or other in near:
                    continue
                d = distance(anchor.bits, items[other].bits)
                if d <= limit and _aspect_ok(anchor.aspect, items[other].aspect):
                    near[other] = d
        if not near:
            continue
        taken[position] = True
        for other in near:
            taken[other] = True
        members = sorted(near.items(), key=lambda kv: (kv[1], kv[0]))
        groups.append([Match(anchor.key, 0)] + [Match(items[o].key, d) for o, d in members])
    return groups
