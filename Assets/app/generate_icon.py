"""Generates resora.ico from Assets/branding/mark.svg, the single source of the mark geometry.

Needs Pillow. Parses the <rect> cells itself (no SVG renderer dependency): x, y, width, height,
fill and optional fill-opacity. Run from anywhere: python Assets/app/generate_icon.py
"""
from __future__ import annotations

import re
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
MARK = HERE.parent / "branding" / "mark.svg"
INK = (13, 12, 11)


def cells() -> list[tuple[float, float, float, float, tuple[int, int, int], float]]:
    out = []
    for tag in re.findall(r"<rect\b[^>]*/>", MARK.read_text(encoding="utf-8")):
        a = dict(re.findall(r'([\w-]+)="([^"]*)"', tag))
        h = a["fill"].lstrip("#")
        rgb = tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))
        out.append((float(a["x"]), float(a["y"]), float(a["width"]), float(a["height"]), rgb,
                    float(a.get("fill-opacity", 1))))
    return out


def render(size: int) -> Image.Image:
    scale = 8
    big = size * scale
    img = Image.new("RGBA", (big, big), INK + (255,))
    d = ImageDraw.Draw(img)
    pad = big * 0.12
    u = (big - 2 * pad) / 16
    for x, y, w, h, rgb, op in cells():
        col = tuple(round(INK[i] + (rgb[i] - INK[i]) * op) for i in range(3))
        d.rectangle([pad + x * u, pad + y * u, pad + (x + w) * u - 1, pad + (y + h) * u - 1], fill=col)
    return img.resize((size, size), Image.LANCZOS)


if __name__ == "__main__":
    out = HERE / "resora.ico"
    render(256).save(out, format="ICO", sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])
    print(out)
