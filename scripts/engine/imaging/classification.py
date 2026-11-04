"""Resolution classification. Pure: depends on nothing but the dimensions.

Strategy (full description in docs/IMAGE_ANALYSIS.md):
1. Dimensions are normalised to (long side, short side), so portrait and
   landscape images classify identically; width alone is never used.
2. `exact`: both sides equal a standard. `near`: both sides within
   NEAR_TOLERANCE of the closest standard (e.g. 1920x1088); labelled "(approx.)".
3. Otherwise the image is NOT given a standard's name:
   - aspect ratio > PANORAMA_ASPECT -> "Panorama (<tier>)"
   - larger than 8K by more than the tolerance in both sides -> "Above 8K"
   - else "Custom (<tier>)"
   where `tier` is the largest standard that fits entirely inside the image
   (both sides at least as large), or "below HD".
The caller always keeps and reports the real dimensions.
"""
from __future__ import annotations

from dataclasses import dataclass

NEAR_TOLERANCE = 0.02
PANORAMA_ASPECT = 2.4
BELOW_HD = "below HD"
ABOVE_8K = "Above 8K"


@dataclass(frozen=True)
class Standard:
    name: str
    long_side: int
    short_side: int


# Ascending by pixel count.
STANDARDS = (
    Standard("HD", 1280, 720),
    Standard("Full HD", 1920, 1080),
    Standard("DCI 2K", 2048, 1080),
    Standard("QHD / 1440p", 2560, 1440),
    Standard("UHD 4K", 3840, 2160),
    Standard("DCI 4K", 4096, 2160),
    Standard("5K", 5120, 2880),
    Standard("8K", 7680, 4320),
)


@dataclass(frozen=True)
class Classification:
    label: str  # what to display, e.g. "Full HD", "Full HD (approx.)", "Custom (>= QHD / 1440p)"
    standard: str | None  # standard's name for exact/near matches, else None
    match: str  # "exact" | "near" | "none"
    tier: str  # largest standard fully contained, ABOVE_8K, or BELOW_HD


def _closest(long_: int, short: int) -> tuple[Standard, float] | None:
    best: tuple[Standard, float] | None = None
    for s in STANDARDS:
        deviation = max(abs(long_ - s.long_side) / s.long_side, abs(short - s.short_side) / s.short_side)
        if deviation <= NEAR_TOLERANCE and (best is None or deviation < best[1]):
            best = (s, deviation)
    return best


def _tier(long_: int, short: int) -> str:
    top = STANDARDS[-1]
    limit = 1 + NEAR_TOLERANCE
    if long_ > top.long_side * limit and short > top.short_side * limit:
        return ABOVE_8K
    fitting = [s for s in STANDARDS if long_ >= s.long_side and short >= s.short_side]
    return fitting[-1].name if fitting else BELOW_HD


def classify(width: int, height: int) -> Classification:
    if width <= 0 or height <= 0:
        raise ValueError(f"dimensions must be positive, got {width}x{height}")
    long_, short = max(width, height), min(width, height)

    hit = _closest(long_, short)
    if hit is not None:
        standard, deviation = hit
        exact = deviation == 0
        return Classification(
            label=standard.name if exact else f"{standard.name} (approx.)",
            standard=standard.name,
            match="exact" if exact else "near",
            tier=standard.name,
        )

    tier = _tier(long_, short)
    qualifier = tier if tier == BELOW_HD else f">= {tier}"
    if long_ / short > PANORAMA_ASPECT:
        return Classification(f"Panorama ({qualifier})", None, "none", tier)
    if tier == ABOVE_8K:
        return Classification(ABOVE_8K, None, "none", tier)
    return Classification(f"Custom ({qualifier})", None, "none", tier)
