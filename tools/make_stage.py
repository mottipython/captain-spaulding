#!/usr/bin/env python3
"""The stage, from Pollock's toy-theatre scenes for "Jack the Giant Killer" (London,
about 1850; the Metropolitan Museum's prints, CC0, from Wikimedia Commons):

  - the backdrop is Scene 4, a sunrise over mountains and a river: its picture,
    without the printed border and lettering;
  - the curtains are the two red drapes of Scene 11, cut out by their colour (the
    red cloth and its gold fringe), without the coin and the sky between them.

    tools/make_stage.py MET_DIR

MET_DIR holds scene-4.jpg and scene-11.jpg. Writes assets/art/backdrop.jpg,
drape_l.png and drape_r.png. Needs Pillow and numpy.
"""
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

OUT = "assets/art"
# the picture inside the printed border, as parts of the sheet's width and height
BACKDROP = (0.080, 0.085, 0.905, 0.935)
# each drape: the part of the sheet it lies in, and how far in from the outer edge the
# fringe can be (the coin is gold too)
DRAPES = {"l": ((0.072, 0.065, 0.300, 0.940), 0.255), "r": ((0.700, 0.065, 0.942, 0.940), 0.745)}

def cut_drape(sheet, box, fringe_x, side):
    W, H = sheet.size
    x0, y0, x1, y1 = (round(box[0] * W), round(box[1] * H), round(box[2] * W), round(box[3] * H))
    part = sheet.crop((x0, y0, x1, y1)).convert("RGB")
    a = np.asarray(part).astype(int)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    h, w = r.shape
    xs = (np.arange(w) + x0) / W
    ys = (np.arange(h)[:, None] + y0) / H
    red = (r > g + 45) & (r > b + 35)
    gold = (r > 150) & (g > 110) & (b < 120) & (r - b > 70) & (ys > 0.78)
    gold &= (xs < fringe_x)[None, :] if side == "l" else (xs > fringe_x)[None, :]
    m = Image.fromarray(((red | gold) * 255).astype(np.uint8))
    # close over the engraved lines in the cloth, then fill any holes left inside
    m = m.filter(ImageFilter.MaxFilter(17)).filter(ImageFilter.MinFilter(17))
    m = m.filter(ImageFilter.MinFilter(5)).filter(ImageFilter.MaxFilter(5))     # drop specks
    outside = m.copy()
    edge = (w - 1, h // 2) if side == "l" else (0, h // 2)
    ImageDraw.floodfill(outside, edge, 128)
    for pt in ((w - 1, 0), (w - 1, h - 1), (0, 0), (0, h - 1)):
        if outside.getpixel(pt) == 0: ImageDraw.floodfill(outside, pt, 128)
    mask = np.asarray(outside) != 128
    # keep only the drape: the biggest shape, which reaches the outer edge
    alpha = Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2))
    rgba = part.convert("RGBA")
    rgba.putalpha(alpha)
    return rgba

def main(met):
    scene4 = Image.open(f"{met}/scene-4.jpg")
    W, H = scene4.size
    bx = BACKDROP
    back = scene4.crop((round(bx[0] * W), round(bx[1] * H), round(bx[2] * W), round(bx[3] * H))).convert("RGB")
    back.thumbnail((2400, 2400), Image.LANCZOS)
    back.save(f"{OUT}/backdrop.jpg", quality=88)
    print("backdrop.jpg", back.size)
    scene11 = Image.open(f"{met}/scene-11.jpg")
    for side, (box, fringe_x) in DRAPES.items():
        d = cut_drape(scene11, box, fringe_x, side)
        d.thumbnail((900, 2400), Image.LANCZOS)
        d.save(f"{OUT}/drape_{side}.png", optimize=True)
        print(f"drape_{side}.png", d.size)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
