"""Generate the SteamSwitch logo/icon assets with Pillow.

    python assets/make_icon.py

Produces, in this folder:
  steamswitch.ico      multi-size Windows icon (title bar / taskbar / .exe)
  steamswitch.png      256px app icon (sidebar logo, window icon, docs, etc.)

The mark is two opposing arrows (the universal "switch / swap" glyph) in
Steam-blue on a dark rounded tile — drawn purely from primitives, no font needed.
Pillow is only needed to BUILD these assets (it's bundled into the .exe build);
the app loads the finished PNG/ICO at runtime.
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent

# Palette (dark Steam-blue tile)
GRAD_TOP = (39, 56, 78)      # #27384e
GRAD_BOTTOM = (22, 31, 43)   # #161f2b
ARROW_A = (102, 192, 244)    # #66c0f4  Steam light-blue
ARROW_B = (214, 236, 251)    # #d6ecfb  near-white blue
BORDER = (255, 255, 255, 38)

S = 1024                     # supersampled canvas; downscaled for crisp edges
R = int(0.22 * S)            # corner radius


def _lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def _tile() -> Image.Image:
    """Dark rounded-square tile with a subtle vertical gradient."""
    grad = Image.new("RGB", (S, S))
    gd = ImageDraw.Draw(grad)
    for y in range(S):
        gd.line([(0, y), (S, y)], fill=_lerp(GRAD_TOP, GRAD_BOTTOM, y / S))

    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, S - 1, S - 1], radius=R, fill=255)

    tile = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    tile.paste(grad, (0, 0), mask)
    # faint inner highlight for a little depth
    ImageDraw.Draw(tile).rounded_rectangle(
        [6, 6, S - 7, S - 7], radius=R - 6, outline=BORDER, width=4)
    return tile


def _arrow(draw, y, *, point_right, color):
    """One horizontal arrow centred on row `y`, spanning the middle of the tile."""
    t = 0.105 * S            # shaft thickness
    hl = 0.15 * S            # head length
    hh = 0.165 * S           # head half-height (wider than the shaft)
    xl, xr = 0.23 * S, 0.77 * S
    if point_right:
        draw.rounded_rectangle([xl, y - t / 2, xr - hl, y + t / 2],
                               radius=t / 2, fill=color)
        draw.polygon([(xr, y), (xr - hl, y - hh), (xr - hl, y + hh)], fill=color)
    else:
        draw.rounded_rectangle([xl + hl, y - t / 2, xr, y + t / 2],
                               radius=t / 2, fill=color)
        draw.polygon([(xl, y), (xl + hl, y - hh), (xl + hl, y + hh)], fill=color)


def build() -> Image.Image:
    tile = _tile()
    d = ImageDraw.Draw(tile)
    _arrow(d, 0.385 * S, point_right=True, color=ARROW_A)   # top → switch to…
    _arrow(d, 0.615 * S, point_right=False, color=ARROW_B)  # bottom ← …and back
    return tile


def main() -> None:
    art = build()
    png = art.resize((256, 256), Image.LANCZOS)
    png.save(HERE / "steamswitch.png")
    png.save(HERE / "steamswitch.ico",
             sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    print("Wrote steamswitch.png, steamswitch.ico to", HERE)


if __name__ == "__main__":
    main()
