"""Generates resora.ico from the same geometry as gui/src/ui/brand.cpp (needs Pillow)."""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

CELLS = [
    (0, 0, 8, 0),
    (8, 0, 4, 1), (12, 0, 4, 2), (8, 4, 4, 2), (12, 4, 4, 3),
    (0, 8, 4, 1), (4, 8, 4, 2), (0, 12, 4, 2), (4, 12, 4, 3),
    (8, 8, 4, 3), (12, 8, 4, 3), (8, 12, 4, 3),
    (12, 12, 2, 3), (14, 12, 2, 3), (12, 14, 2, 3), (14, 14, 2, 4),
]
ALPHA = [46, 96, 158, 230]
INK, PAPER, EMBER = (13, 12, 11), (239, 233, 221), (255, 91, 46)
GAP = 0.6


def render(size: int) -> Image.Image:
    scale = 8
    big = size * scale
    img = Image.new("RGBA", (big, big), INK + (255,))
    d = ImageDraw.Draw(img)
    pad = big * 0.12
    u = (big - 2 * pad) / 16
    for x, y, s, tone in CELLS:
        if tone == 4:
            col = EMBER
        else:
            a = ALPHA[tone] / 255
            col = tuple(round(INK[i] + (PAPER[i] - INK[i]) * a) for i in range(3))
        d.rectangle([pad + (x + GAP / 2) * u, pad + (y + GAP / 2) * u,
                     pad + (x + s - GAP / 2) * u - 1, pad + (y + s - GAP / 2) * u - 1], fill=col)
    return img.resize((size, size), Image.LANCZOS)


if __name__ == "__main__":
    out = Path(__file__).with_name("resora.ico")
    render(256).save(out, format="ICO", sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])
    print(out)
