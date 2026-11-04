"""Aspect ratio and orientation from pixel dimensions. Pure; no Pillow."""
from __future__ import annotations

# Named ratios as (long, short); a measured ratio within 1% of one is reported by name.
COMMON_RATIOS = ((1, 1), (5, 4), (4, 3), (3, 2), (16, 10), (16, 9), (21, 9), (2, 1))
RATIO_TOLERANCE = 0.01


def orientation(width: int, height: int) -> str:
    """'landscape', 'portrait' or 'square' for the displayed dimensions."""
    if width == height:
        return "square"
    return "landscape" if width > height else "portrait"


def aspect_ratio(width: int, height: int) -> float:
    """width / height (below 1 for portrait), rounded to 3 decimals."""
    return round(width / height, 3)


def aspect_label(width: int, height: int) -> str:
    """Human label such as '16:9' / '9:16'; falls back to '1.90:1' / '1:1.90'
    when no common ratio is within 1%."""
    long_, short = max(width, height), min(width, height)
    ratio = long_ / short
    for a, b in COMMON_RATIOS:
        if abs(ratio - a / b) / (a / b) <= RATIO_TOLERANCE:
            return f"{a}:{b}" if width >= height else f"{b}:{a}"
    return f"{ratio:.2f}:1" if width >= height else f"1:{ratio:.2f}"
